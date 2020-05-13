#ifndef EQUIVALENCE_CLASS_H
#define EQUIVALENCE_CLASS_H

#include "resource-record.h"

class EC
{

  private:
    friend class boost::serialization::access;
    template <typename Archive> void serialize(Archive &ar, const unsigned int version)
    {
        ar &name;
        ar &rrTypes;
        ar &excluded;
    }

  public:
    boost::optional<std::vector<NodeLabel>> excluded;
    vector<NodeLabel> name;
    std::bitset<RRType::N> rrTypes;
    bool nonExistent = false;

    string ToString() const;
    bool operator==(const EC &) const;
};

#endif