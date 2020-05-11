#ifndef NODE_LABEL_H_
#define NODE_LABEL_H_

#include <iostream>
#include <string>
#include <vector>
#include <boost/archive/text_oarchive.hpp>
#include <boost/archive/text_iarchive.hpp>
#include <boost/dynamic_bitset.hpp>
#include <boost/flyweight.hpp>
#include <boost/flyweight/no_tracking.hpp>
#include <boost/flyweight/serialize.hpp>
#include <boost/graph/adj_list_serialize.hpp>
#include <boost/graph/adjacency_list.hpp>	
#include <boost/graph/graphviz.hpp>	
#include <boost/serialization/access.hpp>
#include <boost/serialization/bitset.hpp>
#include <boost/serialization/optional.hpp>
#include <boost/serialization/string.hpp>
#include <boost/serialization/vector.hpp>
#include <nlohmann/json.hpp>

#include "../concurrentqueue/concurrentqueue.h"
#include "my_logger.h"

using namespace std;
using json = nlohmann::json;

#define kHashMapThreshold	500
#define kMaxLabelLength     63
#define kMaxDomainLength    255

struct Empty {};

struct NodeLabel {
	boost::flyweight<std::string, boost::flyweights::tag<Empty>, boost::flyweights::no_tracking> n;
	NodeLabel(const std::string s) : n{ s } {};
	NodeLabel() : n{ "" } {};
	std::string get() const;
	void set(const std::string);
	bool operator== (const NodeLabel&) const;
private:
	friend class boost::serialization::access;
	template <typename Archive>
	void serialize(Archive& ar, const unsigned int version)
	{
		ar& n;
	}
};

std::size_t hash_value(const NodeLabel&);

#endif