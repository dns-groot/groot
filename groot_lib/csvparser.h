#pragma once
#include <string>
#include <vector>
#include <boost/graph/adjacency_list.hpp>
#include <boost/graph/graphviz.hpp>
#include <boost/dynamic_bitset.hpp>
#include <boost/archive/text_oarchive.hpp>
#include <boost/archive/text_iarchive.hpp>
#include <boost/serialization/vector.hpp>
#include <boost/serialization/string.hpp>
#include <boost/serialization/optional.hpp>
#include <boost/serialization/bitset.hpp>
#include <boost/graph/adj_list_serialize.hpp>
#include <iostream>
#include <sys/stat.h>
#include <fstream>
#include <boost/filesystem.hpp>
#include <filesystem>
#include "resource_record.h"
#include "graph.h"
#include "label_graph.h"

typedef std::vector<std::string> Row;
void SOA_CSV_Parser(string, LabelGraph&, const VertexDescriptor, string outputDirectory);
void Generate_Zone_Files(string file, string type, LabelGraph& g, VertexDescriptor root, string);