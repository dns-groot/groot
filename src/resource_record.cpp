#include <cassert>
#include <iomanip>
#include <iostream>
#include <set>

#include "resource_record.h"
#include "utils.h"

ResourceRecord::ResourceRecord(string name, string type, uint16_t class_, uint32_t ttl, string rdata) : name_(LabelUtils::StringToLabels(name)), type_(TypeUtils::StringToType(type)), class_(class_), ttl_(ttl), rdata_(rdata) {}

bool ResourceRecord::operator==(const ResourceRecord& l1)
{
	if (name_ == l1.get_name() && rdata_ == l1.get_rdata() &&  type_ == l1.get_type()) {
		// ignoring ttl_ == l1.get_ttl() 
		return true;
	}
	return false;
}


ostream& operator<<(ostream& os, const ResourceRecord& rr)
{
	os << LabelUtils::LabelsToString(rr.name_) << '\t' << rr.type_ << '\t' << rr.rdata_ << endl;
	return os;
}

string ResourceRecord::toString()
{
	std::bitset<RRType::N> rrTypes;
	rrTypes.set(type_);
	return LabelUtils::LabelsToString(name_) + "   " + TypeUtils::TypesToString(rrTypes) + "   " + rdata_;
}

std::string Label::get() const
{
	return n.get();
}

std::size_t hash_value(const Label& l1)
{
	boost::hash<boost::flyweight<std::string, boost::flyweights::no_tracking>> hasher;
	return hasher(l1.n);
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
	name_ = LabelUtils::StringToLabels(name);
}

void ResourceRecord::set_type(string type)
{
	type_ = TypeUtils::StringToType(type);
}

void ResourceRecord::set_class(uint16_t class_)
{
	class_ = class_;
}

void ResourceRecord::set_ttl(uint32_t ttl)
{
	ttl_ = ttl;
}

bool  Label::operator==(const Label& l) const
{
	return l.n == n;
}

