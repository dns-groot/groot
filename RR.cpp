#include "RR.h"
#include <cassert>
#include <iomanip>


ResourceRecord::ResourceRecord(string name, string type, uint16_t class_, uint32_t ttl, string rData) : _name(name), _type(convertRRtype(type)), _class(class_), _ttl(ttl), _rData(rData) {}

std::string ResourceRecord::GetName() const
{
	return _name;
}

rr_type ResourceRecord::GetType() const
{
	return _type;
}

uint16_t ResourceRecord::GetClass() const
{
	return _class;
}

uint32_t ResourceRecord::GetTimeToLive() const
{
	return _ttl;
}

string ResourceRecord::GetRData() const
{
	return _rData;
}

void ResourceRecord::SetName(string name)
{
	_name = name;
}

void ResourceRecord::SetType(string type)
{
	_type = convertRRtype(type);

}

void ResourceRecord::SetClass(uint16_t class_)
{
	_class = class_;
}

void ResourceRecord::SetTTL(uint32_t ttl)
{
	_ttl = ttl;
}



ostream& operator<<(ostream& os, const ResourceRecord rr)
{
	os << std::setw(20) << rr._name << std::setw(4) << rr._class << std::setw(8) << rr._ttl << std::setw(5) << rr._type << "  " << rr._rData << endl;
	return os;
}

rr_type	convertRRtype(string type) {
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
	return N;
}