#define BOOST_TEST_MODULE mytests
#include <boost/test/included/unit_test.hpp>
#include "../DNS/zone.h"
#include "../DNS/RR.h"
#include <cassert>
#include <iomanip>
#include <vector>
#include "../DNS/graph.h"

BOOST_AUTO_TEST_CASE(my_boost_test)
{
  std::string file1("C:\\Users\\t-sikaka\\Desktop\\dns\\DNS\\DNS\\tests\\test1.txt");
  //auto v = convertRRtype(file);
  vector<std::string> records = getLabels(file1);
  labelGraph g;
  vertex_t root = boost::add_vertex(g);
  g[root].name = ".";
  rr_type c = CNAME;
  //label_graph_builder(records, g, root);
  BOOST_TEST(1 == 1);
  BOOST_TEST(true);
}

