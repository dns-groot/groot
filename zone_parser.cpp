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
#include "RR.h"
#include "zone.h"

using namespace std;
namespace lex = boost::spirit::lex;


enum token_ids
{
	ID_COMMENT = 1000,		// a comment block
	ID_WORD,				// a collection of non-space/newline characters
	ID_EOL,					// a newline token
	ID_WHITESPACE,			// some collection of whitespace
	ID_LPAREN,				// a left open parenthesis
	ID_RPAREN,				// a right close parenthesis
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
			("[^ ;\t\r\n]+", ID_WORD)
			("\n", ID_EOL)
			("[ \t]+", ID_WHITESPACE)
			(".", ID_OTHER)
			;
	}
};

struct parser
{
	typedef bool result_type;

	template <typename Token>
	bool operator()(Token const& t, size_t& c, size_t& l, size_t& parenCount, string& relative_domain_suffix, ResourceRecord& default_values, vector<string>& current_record, vector<ResourceRecord>& records) const
	{
		switch (t.id()) {
		case ID_WORD:
		{
			std::string tokenvalue(t.value().begin(), t.value().end());
			current_record.push_back(tokenvalue);
			c += t.value().size();
			break;
		}
		case ID_LPAREN:
			++parenCount; ++c;
			break;
		case ID_RPAREN:
			--parenCount; ++c;
			if (parenCount < 0)
			{
				cout << "unmatched right parenthesis at line: " << l << ", column: " << c;
				return false;
			}
			break;
		case ID_EOL:
			++l; c = 0;
			if (parenCount == 0 && current_record.size() > 0)
			{
				// Control entry $ORIGIN - Sets the origin for relative domain names
				if (current_record[0].compare("$ORIGIN") == 0) {
					relative_domain_suffix = current_record[1];
					current_record.clear();
					return true;
				}
				// Control entry $INCLUDE - Inserts the named file( currently unhandled)
				if (current_record[0].compare("$INCLUDE") == 0) {
					cout << "Found $INCLUDE entry ";
					return false;
				}
				// Control entry $TTL - The TTL for records without explicit TTL value
				if (current_record[0].compare("$TTL") == 0) {
					default_values.SetTTL(std::stoi(current_record[1]));
					current_record.clear();
					return true;
				}

				std::string name;
				// First symbol is "@" implies the owner name is the relative domain name.
				if (current_record[0].compare("@") == 0) {
					if (relative_domain_suffix.length() > 0) {
						name = relative_domain_suffix;
						current_record.erase(current_record.begin());
						default_values.SetName(relative_domain_suffix);
					}
					else {
						cout << "Encountered @ symbol but relative domain is empty ";
						return false;
					}
				}

				// Search for the index where the RR type is found
				int typeIndex = getTypeIndex(current_record);
				if (typeIndex == -1) {
					//cout << "The RR type not found";
					return true;
				}
				string type = current_record[typeIndex];
				string rdata = "";
				uint16_t class_ = 1;
				uint32_t ttl = default_values.GetTimeToLive();
				int i = 0;
				for (auto& field : current_record)
				{
					if (i > typeIndex) {
						rdata += field + " ";
					}
					if (i < typeIndex) {
						if (field.compare("CH") == 0) {
							cout << "Found CH class? ";
							class_ = 3;
						}
						else if (field.compare("IN") == 0) {
							class_ = 1;
						}
						else if (isInteger(field)) {
							ttl = std::stoi(field);
						}
						else {
							name = field;
							default_values.SetName(field);
						}
					}
					i++;
				}
				rdata = rdata.substr(0, rdata.size() - 1);
				if (name.length() == 0) {
					name = default_values.GetName();
				}
				if (!boost::algorithm::ends_with(name, ".")) {
					name = name + "." + relative_domain_suffix;
				}
				if (!type.compare("NS") || !type.compare("CNAME") || !type.compare("DNAME") || !type.compare("MX")) {
					if (!boost::algorithm::ends_with(rdata, ".")) {
						if (relative_domain_suffix.length() > 0) {
							if (rdata.compare("@") == 0) {
								rdata = relative_domain_suffix;
							}
							else {
								rdata = rdata + "." + relative_domain_suffix;
							}
						}
						else {
							cout << "Encountered @ symbol but relative domain is empty for rdata";
							return false;
						}
					}
				}
				ResourceRecord RR(name, type, class_, ttl, rdata);
				records.push_back(RR);
				current_record.clear();
			}
			break;
		case ID_OTHER:
			++c;
			break;
		}

		// continue on
		return true;
	}

	int getTypeIndex(vector<string>& current_record) const {
		std::vector<string> types{ "A", "MX", "NS", "CNAME", "SOA", "PTR", "TXT", "AAAA", "SRV", "DNAME","RRSIG","NSEC" };

		int index = -1;
		int i = 0;
		for (auto& field : current_record)
		{
			if (std::find(std::begin(types), std::end(types), field) != std::end(types)) {
				return i;
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


inline string read_from_file(char const* infile)
{
	ifstream instream(infile);
	if (!instream.is_open()) {
		cerr << "Couldn't open file: " << infile << endl;
		exit(-1);
	}
	instream.unsetf(ios::skipws);
	return string(std::istreambuf_iterator<char>(instream.rdbuf()),
		istreambuf_iterator<char>());
}

vector<ResourceRecord> parse_zone_file(string& file)
{
	// get the zone file input as a string.
	string str(read_from_file(file.c_str()));
	char const* first = str.c_str();
	char const* last = &first[str.size()];

	// mutable state that will be updated by the parser.
	size_t c = 0, l = 0, parenCount = 0;
	string relative_domain;
	ResourceRecord default_values("", "", 0, 0, "");
	vector<string> current_record;
	vector<ResourceRecord> records;

	// parse the zone file from the given string.
	zone_file_tokens<lex::lexertl::lexer<> > zone_functor;
	auto parser_callback =
		boost::bind(parser(), _1,
			boost::ref(c),
			boost::ref(l),
			boost::ref(parenCount),
			boost::ref(relative_domain),
			boost::ref(default_values),
			boost::ref(current_record),
			boost::ref(records));

	auto r = lex::tokenize(first, last, zone_functor, parser_callback);

	// check if parsing was successful.
	if (!r)
	{
		cout << "failed to parse zone file\n";
	}
	return records;
}