// For reference https://www.boost.org/doc/libs/1_70_0/libs/spirit/doc/html/spirit/lex/tutorials/lexer_quickstart1.html

#include <boost/config/warning_disable.hpp>
#include <boost/spirit/include/lex_lexertl.hpp>
#include <boost/bind.hpp>
#include <boost/ref.hpp>
#include <iostream>
#include <fstream>
#include <string>
#include <boost\assign\list_of.hpp>
#include <boost/algorithm/string/predicate.hpp>
#include <boost/algorithm/string.hpp>
#include "resource_record.h"
#include "zone.h"
#include "graph.h"


using namespace std;
namespace lex = boost::spirit::lex;

string gFileName = "";

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
			("\\(", ID_LPAREN)
			("\\)", ID_RPAREN)
			("[^ ;\t\r\n\\(\\)]+", ID_WORD)
			("\n", ID_EOL)
			("[ \t]+", ID_WHITESPACE)
			(".", ID_OTHER)
			;
	}
};

bool CheckForSubDomain(vector<Label>& domain, vector<Label>  queryLabels) {

	if (domain.size() > queryLabels.size()) {
		return false;
	}
	for (int i = 0; i < domain.size(); i++) {
		if (!(domain[i] == queryLabels[i])) {
			return false;
		}
	}
	return true;
}


struct Parser
{
	typedef bool result_type;
	//Can only take at max ten parameters
	template <typename Token>
	bool operator()(Token const& t, size_t& l, int& parenCount, string& relativeDomainSuffix, ResourceRecord& defaultValues, vector<string>& currentRecord, const VertexDescriptor& root, LabelGraph& g, Zone& z) const
	{
		switch (t.id()) {
		case ID_LPAREN:
			++parenCount;
			break;
		case ID_RPAREN:
			--parenCount;
			if (parenCount < 0)
			{
				Logger->error(fmt::format("Unmatched right parenthesis at line - {} in file - {}", l, gFileName));
				return false;
			}
			break;
		case ID_WORD:
		{
			std::string tokenvalue(t.value().begin(), t.value().end());
			currentRecord.push_back(std::move(tokenvalue));
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
			if (parenCount == 0 && currentRecord.size() > 0)
			{
				// Control entry $ORIGIN - Sets the origin for relative domain names
				if (currentRecord[0].compare("$ORIGIN") == 0) {
					relativeDomainSuffix = currentRecord[1];
					boost::to_lower(relativeDomainSuffix);
					currentRecord.clear();
					return true;
				}
				// Control entry $INCLUDE - Inserts the named file( currently unhandled)
				if (currentRecord[0].compare("$INCLUDE") == 0) {
					Logger->error(fmt::format("Found $INCLUDE entry at line - {} in file - {}", l, gFileName));
					return false;
				}
				// Control entry $TTL - The TTL for records without explicit TTL value
				if (currentRecord[0].compare("$TTL") == 0) {
					defaultValues.set_ttl(std::stoi(currentRecord[1]));
					currentRecord.clear();
					return true;
				}

				std::string name;
				// First symbol is "@" implies the owner name is the relative domain name.
				if (currentRecord[0].compare("@") == 0) {
					if (relativeDomainSuffix.length() > 0) {
						name = relativeDomainSuffix;
						currentRecord.erase(currentRecord.begin());
						defaultValues.set_name(relativeDomainSuffix);
					}
					else {
						Logger->error(fmt::format("Encountered @ symbol but relative domain is empty at line - {} in file - {}", l, gFileName));
						return false;
					}
				}

				// Search for the index where the RR type is found
				int typeIndex = GetTypeIndex(currentRecord);
				if (typeIndex == -1) {
					//Logger->warn(fmt::format("RR type not handled for the RR at line- {} in file- {}", l, gFileName));
					currentRecord.clear();
					return true;
				}
				string type = currentRecord[typeIndex];
				boost::to_upper(type);
				string rdata = "";
				uint16_t class_ = 1;
				uint32_t ttl = defaultValues.get_ttl();
				int i = 0;
				for (auto& field : currentRecord)
				{
					if (i > typeIndex) {
						rdata += field + " ";
					}
					if (i < typeIndex) {
						if (boost::iequals(field, "CH")) {
							Logger->warn(fmt::format("Found CH class for the RR at line - {} in file - {}", l, gFileName));
							class_ = 3;
						}
						else if (boost::iequals(field, "IN")) {
							class_ = 1;
						}
						else if (isInteger(field)) {
							ttl = std::stoi(field);
							if (defaultValues.get_ttl() == 0) defaultValues.set_ttl(ttl);
						}
						else {
							name = field;
							defaultValues.set_name(field);
						}
					}
					i++;
				}
				rdata = rdata.substr(0, rdata.size() - 1);
				if (name.length() == 0) {
					name = LabelsToString(defaultValues.get_name());
				}
				if (!boost::algorithm::ends_with(name, ".")) {
					name = name + "." + relativeDomainSuffix;
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
						rdata = rdata + "." + relativeDomainSuffix;
					}
				}
				boost::to_lower(rdata);
				boost::to_lower(name);
				ResourceRecord RR(name, type, class_, ttl, rdata);
				bool add = true;
				/*	if (!type.compare("SOA") && !CheckForSubDomain(z.origin, RR.get_name())) {
						add = false;
					}*/
				if (add) {
					boost::optional<ZoneVertexDescriptor> vertexid = ZoneGraphBuilder(RR, z);
					if (vertexid) {
						LabelGraphBuilder(RR, g, root, z.zoneId, vertexid.get());
					}

				}
				currentRecord.clear();
				Logger->debug(fmt::format("zone_parser.cpp (Parser) Parsed {} in file {}", l, gFileName));
			}
			break;
		case ID_OTHER:
			break;
		}

		// continue on
		return true;
	}

	int GetTypeIndex(vector<string>& current_record) const {
		std::vector<string> types{ "A", "MX", "NS", "CNAME", "SOA", "PTR", "TXT", "AAAA", "SRV", "DNAME","RRSIG","NSEC", "SPF" };

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
		Logger->critical(fmt::format("zone_parser.cpp - couldn't open file - {}", infile));
		exit(-1);
	}
	instream.unsetf(ios::skipws);
	return string(std::istreambuf_iterator<char>(instream.rdbuf()),
		istreambuf_iterator<char>());
}

void ParseZoneFile(string& file, LabelGraph& g, const VertexDescriptor& root, Zone& z)
{
	// get the zone file input as a string.
	string str(ReadFromFile(file.c_str()));
	char const* first = str.c_str();
	char const* last = &first[str.size()];

	gFileName = file;
	// mutable state that will be updated by the parser.
	size_t l = 0;
	int parenCount = 0;
	string relative_domain="";
	//if (boost::algorithm::ends_with(file, ".dns")) {
	//	//For the hotmail.com zone files
	//	vector<string> strs;
	//	boost::split(strs, file, boost::is_any_of("\\"));
	//	relative_domain = strs.back();
	//	relative_domain = relative_domain.substr(0, relative_domain.length() - 3);
	//	boost::to_lower(relative_domain);
	//}
	ResourceRecord defaultValues("", "", 0, 0, "");
	vector<string> currentRecord;

	// parse the zone file from the given string.
	zone_file_tokens<lex::lexertl::lexer<> > zone_functor;
	auto parserCallback =
		boost::bind(Parser(), _1,
			boost::ref(l),
			boost::ref(parenCount),
			boost::ref(relative_domain),
			boost::ref(defaultValues),
			boost::ref(currentRecord),
			boost::ref(root),
			boost::ref(g),
			boost::ref(z));

	auto r = lex::tokenize(first, last, zone_functor, parserCallback);

	// check if parsing was successful.
	if (!r)
	{
		Logger->error(fmt::format("Failed to completely parse zone file - {}", gFileName));
	}
	else {
		Logger->debug(fmt::format("Successfully parsed zone file - {}", gFileName));
	}
}
