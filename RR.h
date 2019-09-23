#pragma once
#include <cstdint>
#include <string>
#include <boost\serialization\access.hpp>
#include <boost/archive/text_oarchive.hpp>
#include <boost/archive/text_iarchive.hpp>


using namespace std;

#define MAX_LABELLEN     63
#define MAX_DOMAINLEN    255

enum rr_class
{
	CLASS_IN = 1,
	CLASS_CH = 3
};

enum rr_type
{
	A,
	NS,
	CNAME,
	DNAME,
	SOA,
	PTR,
	MX,
	TXT,
	AAAA,
	SRV,
	N
};

rr_type	convertRRtype(string type);

class ResourceRecord
{
public:	
	ResourceRecord(string name, string type, uint16_t class_, uint32_t ttl, string rData);
	string GetName() const;
	rr_type GetType() const;
	uint16_t GetClass() const;
	uint32_t GetTimeToLive() const;
	string GetRData() const;
	void SetName(string);
	void SetType(string);
	void SetClass(uint16_t);
	void SetTTL(uint32_t);
	friend ostream& operator<<(ostream& os, const ResourceRecord rr);

private:
	string _name;
	rr_type _type;
	uint16_t _class;
	uint32_t _ttl;
	string _rData;
	friend class boost::serialization::access;
	template <typename Archive>
	void serialize(Archive& ar, const unsigned int version)
	{
		ar& _name;
		ar& _type;
		ar& _class;
		ar& _ttl;
		ar& _rData;
	}
};


