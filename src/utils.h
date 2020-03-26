#pragma once

#include <set>

#include "resource_record.h"

class LabelUtils
{
public:
	static string LabelsToString(vector<Label> domain_name);
	static vector<Label> StringToLabels(string domain_name);
};

class TypeUtils
{
public:
	static RRType StringToType(string type);
	static string TypesToString(std::bitset<RRType::N> rrTypes);
};

