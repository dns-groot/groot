#include "utils.h"

string LabelUtils::LabelsToString(vector<NodeLabel> domain_name)
{
    string domain = "";
    if (domain_name.size() == 0) {
        return ".";
    } else {
        for (auto &l : domain_name) {
            domain = l.get() + "." + domain;
        }
    }
    return domain;
}

string LabelUtils::LabelsToString(vector<vector<NodeLabel>> domains)
{
    string result = "[";
    for (auto &d : domains) {
        result += LabelsToString(d) + ", ";
    }
    result.pop_back();
    result.pop_back();
    return result + "]";
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
    // boost::algorithm::split(labels, name, boost::is_any_of(".")); // Avoiding this for the case where . is written
    // with \. and root zone.
    string previous = "";
    for (auto it = domain_name.begin(); it < domain_name.end(); ++it) {
        if (*it == '.' && previous.length() > 0) {
            if (previous.back() == '\\') {
                previous += *it;
            } else {
                tokens.push_back(std::move(previous));
                previous = "";
            }
        } else {
            previous += *it;
        }
    }
    std::reverse(tokens.begin(), tokens.end());
    return tokens;
}

bool LabelUtils::SubDomainCheck(const vector<NodeLabel> &domain, const vector<NodeLabel> &subdomain)
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

bool LabelUtils::SubDomainCheck(const vector<vector<NodeLabel>> &allowed_domains, const vector<NodeLabel> &subdomain)
{
    bool any_subdomain = false;
    for (auto &d : allowed_domains) {
        any_subdomain |= SubDomainCheck(d, subdomain);
    }
    return any_subdomain;
}

RRType TypeUtils::StringToType(const string &type)
{
    if (type == "A") {
        return A;
    } else if (type == "NS") {
        return NS;
    } else if (type == "CNAME") {
        return CNAME;
    } else if (type == "DNAME") {
        return DNAME;
    } else if (type == "SOA") {
        return SOA;
    } else if (type == "PTR") {
        return PTR;
    } else if (type == "MX") {
        return MX;
    } else if (type == "TXT") {
        return TXT;
    } else if (type == "AAAA") {
        return AAAA;
    } else if (type == "SRV") {
        return SRV;
    } else if (type == "RRSIG") {
        return RRSIG;
    } else if (type == "NSEC") {
        return NSEC;
    } else if (type == "SPF") {
        return SPF;
    }
    return N;
}

string type_to_string[] = {"A", "NS", "CNAME", "DNAME", "SOA", "PTR", "MX", "TXT", "AAAA", "SRV", "RRSIG", "NSEC", "SPF"};

string TypeUtils::TypeToString(RRType type)
{
    return type_to_string[type];
}

string TypeUtils::TypesToString(std::bitset<RRType::N> rrTypes)
{
    std::set<string> types;

    for (int i = 0; i < RRType::N; i++) {
        if (rrTypes[i] == 1) {
            types.insert(type_to_string[i]);
        }
    }
    string stypes = "";
    for (auto r : types) {
        if (stypes.size() > 0) {
            stypes += " ";
        }
        stypes += r;
    }
    return stypes;
}

CommonSymDiff RRUtils::CompareRRs(vector<ResourceRecord> res_a, vector<ResourceRecord> res_b)
{
    // For the given pair of collection of resource records, return the common RR's, RR's present only in A and RR's
    // present only in B.
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
            } else {
                itb++;
            }
        }
        if (!erased)
            it++;
    }
    return std::make_tuple(common, res_a, res_b);
}