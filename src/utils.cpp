#include "utils.h"

string LabelUtils::LabelsToString(vector<Label> domain_name)
{
	string domain = "";
	if (domain_name.size() == 0) {
		return ".";
	}
	else {
		for (auto& l : domain_name) {
			domain = l.get() + "." + domain;
		}
	}
	return domain;
}

vector<Label> LabelUtils::StringToLabels(string domain_name)
{
	vector<Label> tokens;
	if (domain_name.length() == 0) {
		return tokens;
	}
	if (domain_name[domain_name.length() - 1] != '.') {
		domain_name += ".";
	}
	// boost::algorithm::split(labels, name, boost::is_any_of(".")); // Avoiding this for the case where . is written with \. and root zone.
	string previous = "";
	for (auto it = domain_name.begin(); it < domain_name.end(); ++it) {
		if (*it == '.' && previous.length() > 0) {
			if (previous.back() == '\\') {
				previous += *it;
			}
			else {
				tokens.push_back(std::move(previous));
				previous = "";
			}
		}
		else {
			previous += *it;
		}
	}
	std::reverse(tokens.begin(), tokens.end());
	return tokens;
}

RRType TypeUtils::StringToType(string type)
{
	if (type == "A") {
		return A;
	}
	if (type == "NS") {
		return NS;
	}
	if (type == "CNAME") {
		return CNAME;
	}
	if (type == "DNAME") {
		return DNAME;
	}
	if (type == "SOA") {
		return SOA;
	}
	if (type == "PTR") {
		return PTR;
	}
	if (type == "MX") {
		return MX;
	}
	if (type == "TXT") {
		return TXT;
	}
	if (type == "AAAA") {
		return AAAA;
	}
	if (type == "SRV") {
		return SRV;
	}
	if (type == "RRSIG") {
		return RRSIG;
	}
	if (type == "NSEC") {
		return NSEC;
	}
	if (type == "SPF") {
		return SPF;
	}
	return N;
}

string TypeUtils::TypesToString(std::bitset<RRType::N> rrTypes)
{
	std::set<string> types;

	for (int i = 0; i < RRType::N; i++) {
		if (rrTypes[i] == 1) {
			switch (i) {
			case 0:
				types.insert("A");
				break;
			case 1:
				types.insert("NS");
				break;
			case 2:
				types.insert("CNAME");
				break;
			case 3:
				types.insert("DNAME");
				break;
			case 4:
				types.insert("SOA");
				break;
			case 5:
				types.insert("PTR");
				break;
			case 6:
				types.insert("MX");
				break;
			case 7:
				types.insert("TXT");
				break;
			case 8:
				types.insert("AAAA");
				break;
			case 9:
				types.insert("SRV");
				break;
			}
		}
	}
	string stypes = "";
	for (auto r : types) {
		stypes += r + " ";
	}
	return stypes;
}
