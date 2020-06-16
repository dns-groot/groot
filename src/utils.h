#ifndef UTILS_H_
#define UTILS_H_

#include <set>

#include "resource-record.h"

class LabelUtils
{
  public:
    static string LabelsToString(vector<NodeLabel>);
    static string LabelsToString(vector<vector<NodeLabel>>);
    static vector<NodeLabel> StringToLabels(string);
    static bool SubDomainCheck(const vector<NodeLabel> &, const vector<NodeLabel> &);
    static bool SubDomainCheck(const vector<vector<NodeLabel>> &, const vector<NodeLabel> &);
};

class TypeUtils
{
  public:
    static RRType StringToType(const string &);
    static string TypeToString(RRType type);
    static string TypesToString(std::bitset<RRType::N>);
};

using CommonSymDiff = tuple<vector<ResourceRecord>, vector<ResourceRecord>, vector<ResourceRecord>>;

class RRUtils
{
  public:
    static CommonSymDiff CompareRRs(vector<ResourceRecord>, vector<ResourceRecord>);
};

class LintUtils
{
  public:
    static void WriteIssueToFile(json &, bool);
};
#endif
