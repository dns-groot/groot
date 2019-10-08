#pragma once
#include <cstdint>
#include <string>
#include <bitset>
#include <boost\serialization\access.hpp>
#include <boost/archive/text_oarchive.hpp>
#include <boost/archive/text_iarchive.hpp>
#include <boost/flyweight.hpp>
#include <boost/flyweight/serialize.hpp>
#include <boost/flyweight/no_locking.hpp>
#include <boost/flyweight/no_tracking.hpp>

using namespace std;

#define kMaxLabelLength     63
#define kMaxDomainLength    255

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
	N
};

struct Label {
	boost::flyweight<std::string, boost::flyweights::no_locking, boost::flyweights::no_tracking> n;
	Label(std::string s) : n{ s } {};
	Label() : n{ "" } {};
	std::string get() const;
	void set(const std::string s);
	friend bool operator== (const Label& l1, const Label& l2);
private:
	friend class boost::serialization::access;
	template <typename Archive>
	void serialize(Archive& ar, const unsigned int version)
	{
		ar& n;
	}
};

string LabelsToString(vector<Label> name);
RRType	ConvertToRRType(string type);
string RRTypesToString(std::bitset<RRType::N> rrTypes);
vector<Label> GetLabels(string name);

class ResourceRecord
{
public:	
	ResourceRecord(string name, string type, uint16_t class_, uint32_t ttl, string rdata);
	vector<Label> get_name() const;
	RRType get_type() const;
	uint16_t get_class() const;
	uint32_t get_ttl() const;
	string get_rdata() const;
	void set_name(string);
	void set_type(string);
	void set_class(uint16_t);
	void set_ttl(uint32_t);

private:
	vector<Label> name_;
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


