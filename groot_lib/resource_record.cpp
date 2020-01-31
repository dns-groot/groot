#include <cassert>
#include <iomanip>
#include <iostream>
#include <set>
#include "resource_record.h"


ResourceRecord::ResourceRecord(string name, string type, uint16_t class_, uint32_t ttl, string rdata) : name_(GetLabels(name)), type_(ConvertToRRType(type)), class_(class_), ttl_(ttl), rdata_(rdata) {}
bool ResourceRecord::operator==(const ResourceRecord& l1)
{
	if (name_ == l1.get_name() && rdata_ == l1.get_rdata() &&  type_ == l1.get_type()) {
		// ignoring ttl_ == l1.get_ttl() 
		return true;
	}
	return false;
}


vector<Label> GetLabels(string name) {
	vector<Label> tokens;
	if (name.length() == 0) {
		return tokens;
	}
	if (name[name.length() - 1] != '.') {
		name += ".";
	}
	// boost::algorithm::split(labels, name, boost::is_any_of(".")); // Avoiding this for the case where . is written with \. and root zone.
	string previous = "";
	for (auto it = name.begin(); it < name.end(); ++it) {
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

ostream& operator<<(ostream& os, const ResourceRecord& rr)
{
	os << LabelsToString(rr.name_) << '\t' << rr.type_ << '\t' << rr.rdata_ << endl;
	return os;
}

string ResourceRecord::toString()
{
	std::bitset<RRType::N> rrTypes;
	rrTypes.set(type_);
	return LabelsToString(name_) + "   " + RRTypesToString(rrTypes) + "   " + rdata_;
}

std::string Label::get() const
{
	return n.get();
}

void Label::set(const std::string s)
{
	n = s;
}

vector<Label> ResourceRecord::get_name() const
{
	return name_;
}

RRType ResourceRecord::get_type() const
{
	return type_;
}

uint16_t ResourceRecord::get_class() const
{
	return class_;
}

uint32_t ResourceRecord::get_ttl() const
{
	return ttl_;
}

string ResourceRecord::get_rdata() const
{
	return rdata_;
}

void ResourceRecord::set_name(string name)
{
	name_ = GetLabels(name);
}

void ResourceRecord::set_type(string type)
{
	type_ = ConvertToRRType(type);

}

void ResourceRecord::set_class(uint16_t class_)
{
	class_ = class_;
}

void ResourceRecord::set_ttl(uint32_t ttl)
{
	ttl_ = ttl;
}




bool operator==(const Label& l1, const Label& l2)
{
	return l1.n == l2.n;
}

string LabelsToString(vector<Label> name) {
	string domain = "";
	if (name.size() == 0) {
		return ".";
	}
	else {
		for (auto& l : name) {
			domain = l.get() + "." + domain;
		}
	}
	return domain;
}


RRType	ConvertToRRType(string type) {
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

string RRTypesToString(std::bitset<RRType::N> rrTypes) {
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
