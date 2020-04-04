#pragma once

#include "node-label.h"

enum RRClass
{
	CLASS_IN = 1,
	CLASS_CH = 3
};

enum RRType
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
	RRSIG,
	NSEC,
	SPF,
	N
};


class ResourceRecord
{
public:
	ResourceRecord(string name, string type, uint16_t class_, uint32_t ttl, string rdata);
	bool operator== (const ResourceRecord& l1) const;
	vector<NodeLabel> get_name() const;
	RRType get_type() const;
	uint16_t get_class() const;
	uint32_t get_ttl() const;
	string get_rdata() const;
	void set_name(string);
	void set_type(string);
	void set_class(uint16_t);
	void set_ttl(uint32_t);
	string toString();
	friend ostream& operator<<(ostream& os, const ResourceRecord& rr);

private:
	vector<NodeLabel> name_;
	RRType type_;
	uint16_t class_;
	uint32_t ttl_;
	string rdata_;
	friend class boost::serialization::access;
	template <typename Archive>
	void serialize(Archive& ar, const unsigned int version)
	{
		ar& name_;
		ar& type_;
		ar& class_;
		ar& ttl_;
		ar& rdata_;
	}
};
