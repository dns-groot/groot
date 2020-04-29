#include <boost/algorithm/string/predicate.hpp>
#include <boost/algorithm/string.hpp>
#include <boost/bind.hpp>
#include <boost/config/warning_disable.hpp>
#include <boost/ref.hpp>
#include <boost/spirit/include/lex_lexertl.hpp>

#include "driver.h"
#include "utils.h"

using namespace std;
namespace lex = boost::spirit::lex;

struct MiniContext {
	string file_name = "";
	bool found_SOA = false;
	int rrs_parsed = 0;
	std::unordered_map<string, long> type_to_count;
};

enum TokenIds
{
	ID_COMMENT = 1000,		// a comment block
	ID_LPAREN,				// a left open parenthesis
	ID_RPAREN,				// a right close parenthesis
	ID_WORD,				// a collection of non-space/newline characters
	ID_EOL,					// a newline token
	ID_WHITESPACE,			// some collection of whitespace
	ID_OTHER				// any other character
};

template <typename Lexer>
struct zone_file_tokens : lex::lexer<Lexer>
{
	zone_file_tokens()
	{
		this->self.add
		(";[^\n]+", ID_COMMENT)
			("((\\\\\\()*(\\\\\\))*[^ ;\t\r\n\\(\\)]+)+", ID_WORD)
			("\\(", ID_LPAREN)
			("\\)", ID_RPAREN)
			("\n", ID_EOL)
			("[ \t]+", ID_WHITESPACE)
			(".", ID_OTHER)
			;
	}
};

struct Parser
{
	typedef bool result_type;
	//Can only take at max ten parameters
	template <typename Token>
	bool operator()(Token const& t, size_t& l, int& paren_count, string& relative_domain_suffix, ResourceRecord& default_values, vector<string>& current_record, label::Graph& label_graph, zone::Graph& z, MiniContext& mc)
	{
		switch (t.id()) {
		case ID_LPAREN:
			++paren_count;
			break;
		case ID_RPAREN:
			--paren_count;
			if (paren_count < 0)
			{
				Logger->error(fmt::format("zone-file-parser.cpp (Parser()) - Unmatched right parenthesis at line - {} in file - {}", l, mc.file_name));
				return false;
			}
			break;
		case ID_WORD:
		{
			std::string tokenvalue(t.value().begin(), t.value().end());
			current_record.push_back(std::move(tokenvalue));
			break;
		}
		case ID_WHITESPACE: {
			if (current_record.size() == 0)current_record.push_back("");
			break;
		}
		/*case ID_COMMENT:
		{
			std::string tokenvalue(t.value().begin(), t.value().end());
			if (tokenvalue.find("Default zone scope in zone") != std::string::npos) {
				vector<string> strs;
				boost::split(strs, tokenvalue, boost::is_any_of(" "));
				relativeDomainSuffix = strs.back();
			}
			break;
		}*/
		case ID_EOL:
			++l;
			if (paren_count == 0 && current_record.size() > 0)
			{
				// Control entry $ORIGIN - Sets the origin for relative domain names
				if (current_record[0].compare("$ORIGIN") == 0) {
					relative_domain_suffix = current_record[1];
					boost::to_lower(relative_domain_suffix);
					current_record.clear();
					return true;
				}
				// Control entry $INCLUDE - Inserts the named file( currently unhandled)
				if (current_record[0].compare("$INCLUDE") == 0) {
					Logger->error(fmt::format("zone-file-parser.cpp (Parser()) - Found $INCLUDE entry at line - {} in file - {}", l, mc.file_name));
					return false;
				}
				// Control entry $TTL - The TTL for records without explicit TTL value
				if (current_record[0].compare("$TTL") == 0) {
					default_values.set_ttl(std::stoi(current_record[1]));
					current_record.clear();
					return true;
				}

				std::string name;
				// First symbol is "@" implies the owner name is the relative domain name.
				if (current_record[0].compare("@") == 0) {
					if (relative_domain_suffix.length() > 0) {
						name = relative_domain_suffix;
						current_record.erase(current_record.begin());
						default_values.set_name(relative_domain_suffix);
					}
					else {
						Logger->error(fmt::format("zone-file-parser.cpp (Parser()) - Encountered @ symbol but relative domain is empty at line - {} in file - {}", l, mc.file_name));
						return false;
					}
				}

				// Search for the index where the RR type is found
				int typeIndex = GetTypeIndex(current_record);
				if (typeIndex == -1) {
					//Logger->warn(fmt::format("RR type not handled for the RR at line- {} in file- {}", l, mc.file_name));
					current_record.clear();
					return true;
				}
				string type = current_record[typeIndex];
				boost::to_upper(type);
				string rdata = "";
				uint16_t class_ = 1;
				uint32_t ttl = default_values.get_ttl();
				int i = 0;
				if (typeIndex > 3) {
					Logger->warn(fmt::format("zone-file-parser.cpp (Parser()) - RR at line {} in file {} is not following the DNS grammar (typeIndex > 3)", l, mc.file_name));
					current_record.clear();
					return true;
				}
				for (auto& field : current_record)
				{
					if (i > typeIndex) {
						rdata += field + " ";
					}
					if (i < typeIndex) {
						if (i == 0) {
							if (field.size() > 0) {
								name = field;
								default_values.set_name(field);
							}
						}
						else if (boost::iequals(field, "CH")) {
							Logger->warn(fmt::format("zone-file-parser.cpp (Parser()) - Found CH class for the RR at line {} in file {}", l, mc.file_name));
							class_ = 3;
						}
						else if (boost::iequals(field, "IN")) {
							class_ = 1;
						}
						else if (isInteger(field)) {
							ttl = std::stoi(field);
							if (default_values.get_ttl() == 0) default_values.set_ttl(ttl);
						}
						else {
							Logger->warn(fmt::format("zone-file-parser.cpp (Parser()) - RR at line {} in file {} is not following the DNS grammar", l, mc.file_name));
							current_record.clear();
							return true;
						}
					}
					i++;
				}
				rdata = rdata.substr(0, rdata.size() - 1);
				if (name.length() == 0) {
					name = LabelUtils::LabelsToString(default_values.get_name());
				}
				else if (type == "SOA") {
					mc.found_SOA = true;
					if (relative_domain_suffix.size() == 0) {
						relative_domain_suffix = name;
					}
				}
				if (!boost::algorithm::ends_with(name, ".")) {
					name = name + "." + relative_domain_suffix;
					default_values.set_name(name);
				}
				//compare returns zero when equal
				if (!type.compare("NS") || !type.compare("CNAME") || !type.compare("DNAME") || !type.compare("MX")) {
					if (!boost::algorithm::ends_with(rdata, ".")) {
						//if (relativeDomainSuffix.length() > 0) {
						//	if (rdata.compare("@") == 0) {
						//		rdata = relativeDomainSuffix;
						//	}
						//	else {
						//		rdata = rdata + "." + relativeDomainSuffix;
						//	}
						//}
						//else {
						//	cout << "Encountered @ symbol but relative domain is empty for rdata";
						//	currentRecord.clear();
						//	return false;
						//}
						rdata = rdata + "." + relative_domain_suffix;
					}
				}
				boost::to_lower(rdata);
				boost::to_lower(name);
				mc.rrs_parsed++;
				ResourceRecord RR(name, type, class_, ttl, rdata);
				bool add = true;
				/*	if (!type.compare("SOA") && !CheckForSubDomain(z.origin, RR.get_name())) {
						add = false;
					}*/
				if (add) {
					boost::optional<zone::Graph::VertexDescriptor> vertexid = z.AddResourceRecord(RR);
					if (vertexid) {
						label_graph.AddResourceRecord(RR, z.get_id(), vertexid.get());
					}
					if (mc.type_to_count.find(type) != mc.type_to_count.end()) {
						mc.type_to_count.insert({ type, 0 });
					}
					mc.type_to_count[type]++;
					if (boost::algorithm::starts_with(name, "*.")) {
						if (mc.type_to_count.find("wildcard") != mc.type_to_count.end()) {
							mc.type_to_count.insert({ "wildcard", 0 });
						}
						mc.type_to_count["wildcard"]++;
					}
				}
				current_record.clear();
				Logger->trace(fmt::format("zone-file-parser.cpp (Parser()) - Parsed line {} in file {}", l, mc.file_name));
			}
			break;
		case ID_OTHER:
			break;
		}
		// continue on
		return true;
	}

	int GetTypeIndex(vector<string>& current_record) const {
		std::vector<string> types{ "A", "MX", "NS", "CNAME", "SOA", "PTR", "TXT", "AAAA", "SRV", "DNAME","RRSIG","NSEC", "DS","SPF" };

		int index = -1;
		int i = 0;
		for (auto& field : current_record)
		{
			string f(field);
			if (f.length() < 6) {
				boost::to_upper(f);
				if (std::find(std::begin(types), std::end(types), f) != std::end(types)) {
					return i;
				}
			}
			i++;
		}
		return index;
	}

	inline bool isInteger(const std::string& s) const
	{
		if (s.empty() || ((!isdigit(s[0])) && (s[0] != '-') && (s[0] != '+'))) return false;
		char* p;
		strtol(s.c_str(), &p, 10);
		return (*p == 0);
	}

};

inline string ReadFromFile(char const* infile)
{
	ifstream instream(infile);
	if (!instream.is_open()) {
		//cerr << "Couldn't open file: " << infile << endl;
		Logger->critical(fmt::format("zone-file-parser.cpp (Parser()) - couldn't open file {}", infile));
		exit(-1);
	}
	instream.unsetf(ios::skipws);
	return string(std::istreambuf_iterator<char>(instream.rdbuf()),
		istreambuf_iterator<char>());
}

int Driver::ParseZoneFileAndExtendGraphs(string file, string nameserver) {

	context_.zoneId_counter_++;
	int& zoneId = context_.zoneId_counter_;
	zone::Graph zone_graph(zoneId);

	MiniContext mc;
	mc.file_name = file;

	// get the zone file input as a string.
	string str(ReadFromFile(file.c_str()));
	char const* first = str.c_str();
	char const* last = &first[str.size()];

	string file_name = file;

	// mutable state that will be updated by the parser.
	size_t l = 0;
	int parenCount = 0;
	string relative_domain = "";
	ResourceRecord defaultValues("", "", 0, 0, "");
	vector<string> currentRecord;

	zone_file_tokens<lex::lexertl::lexer<> > zone_functor;
	auto parserCallback =
		boost::bind(Parser(), _1,
			boost::ref(l),
			boost::ref(parenCount),
			boost::ref(relative_domain),
			boost::ref(defaultValues),
			boost::ref(currentRecord),
			boost::ref(label_graph_),
			boost::ref(zone_graph),
			boost::ref(mc)
		);

	auto r = lex::tokenize(first, last, zone_functor, parserCallback);
	
	for (auto& [k, v] : mc.type_to_count) {
		if (context_.type_to_rr_count.find(k) == context_.type_to_rr_count.end()) {
			context_.type_to_rr_count.insert({ k, v });
		}
		else {
			context_.type_to_rr_count[k] += v;
		}
	}

	if (mc.found_SOA) {
		// check if parsing was successful.
		if (!r || parenCount != 0)
		{
			Logger->error(fmt::format("zone-file-parser.cpp (ParseZoneFileAndExtendGraphs) - Failed to completely parse zone file {}", file));
		}
		else {
			Logger->debug(fmt::format("zone-file-parser.cpp (ParseZoneFileAndExtendGraphs) - Successfully parsed zone file {}", file));
		}

		//Add the new zone graph to the context
		context_.zoneId_to_zone.insert({ zoneId, std::move(zone_graph) });
		auto it = context_.nameserver_zoneIds_map.find(nameserver);
		if (it == context_.nameserver_zoneIds_map.end()) {
			context_.nameserver_zoneIds_map.insert({ nameserver, std::vector<int>{} });
		}
		it = context_.nameserver_zoneIds_map.find(nameserver);
		if (it == context_.nameserver_zoneIds_map.end()) {
			Logger->critical(fmt::format("zone-file-parser.cpp (ParseZoneFileAndExtendGraphs) - Unable to insert into nameserver_zoneIds_map"));
			std::exit(EXIT_FAILURE);
		}
		else {
			it->second.push_back(zoneId);
		}
	}
	else
	{
		Logger->error(fmt::format("zone-file-parser.cpp (ParseZoneFileAndExtendGraphs) - {} file doesn't have a SOA record and is ignored.", file));
	}
	return mc.rrs_parsed;
}
