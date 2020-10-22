#include "equivalence-class.h"
#include "utils.h"

string EC::ToString() const
{
    string q = "";
    if (excluded) {
        q += "~{ }.";
    } else {
        q += "";
    }
    q += LabelUtils::LabelsToString(name);
    return q;
}

string EC::ToConcreteString() const
{
    string q = "";
    if (excluded) {
        q += "a.";
    } else {
        q += "";
    }
    if (name.size())
        q += LabelUtils::LabelsToString(name);
    return q;
}

bool EC::operator==(const EC &q) const
{
    if (name != q.name) {
        return false;
    }
    if (rrTypes != q.rrTypes) {
        return false;
    }
    if (excluded && !q.excluded) {
        return false;
    }
    if (!excluded && q.excluded) {
        return false;
    }
    return true;
}
