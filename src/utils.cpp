#include "utils.h"

string LabelUtils::LabelsToString(vector<NodeLabel> domain_name)
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

vector<NodeLabel> LabelUtils::StringToLabels(string domain_name)
{
	vector<NodeLabel> tokens;
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

bool LabelUtils::SubDomainCheck(const vector<NodeLabel>& domain, const vector<NodeLabel>& subdomain)
{
	if (domain.size() > subdomain.size()) {
		return false;
	}
	for (int i = 0; i < domain.size(); i++) {
		if (!(domain[i] == subdomain[i])) {
			return false;
		}
	}
	return true;
}

RRType TypeUtils::StringToType(const string& type)
{
	if (type == "A") {
		return A;
	}
	else if (type == "NS") {
		return NS;
	}
	else if (type == "CNAME") {
		return CNAME;
	}
	else if (type == "DNAME") {
		return DNAME;
	}
	else if (type == "SOA") {
		return SOA;
	}
	else if (type == "PTR") {
		return PTR;
	}
	else if (type == "MX") {
		return MX;
	}
	else if (type == "TXT") {
		return TXT;
	}
	else if (type == "AAAA") {
		return AAAA;
	}
	else if (type == "SRV") {
		return SRV;
	}
	else if (type == "RRSIG") {
		return RRSIG;
	}
	else if (type == "NSEC") {
		return NSEC;
	}
	else if (type == "SPF") {
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

CommonSymDiff RRUtils::CompareRRs(vector<ResourceRecord> res_a, vector<ResourceRecord> res_b)
{
	//For the given pair of collection of resource records, return the common RR's, RR's present only in A and RR's present only in B.
	// Assumption: resA and resB has unique records (no two records in either vector are exactly the same)
	vector<ResourceRecord> common;
	auto it = res_a.begin();
	while (it != res_a.end()) {
		auto itb = res_b.begin();
		bool erased = false;
		while (itb != res_b.end()) {
			if (*it == *itb) {
				common.push_back(*it);
				it = res_a.erase(it);
				res_b.erase(itb);
				erased = true;
				break;
			}
			else {
				itb++;
			}
		}
		if (!erased)it++;
	}
	return std::make_tuple(common, res_a, res_b);
}