#include "node-label.h"

std::string NodeLabel::get() const
{
	return n.get();
}

void NodeLabel::set(const std::string s)
{
	n = s;
}

bool NodeLabel::operator==(const NodeLabel& nl) const
{
	return nl.n == n;
}

std::size_t hash_value(const NodeLabel& nl)
{
	boost::hash<boost::flyweight<std::string, boost::flyweights::tag<Empty>, boost::flyweights::no_tracking>> hasher;
	return hasher(nl.n);
}
