#include "../src/resource-record.h"
#include "../src/utils.h"
#include <boost/test/unit_test.hpp>

BOOST_AUTO_TEST_SUITE(UtilTestSuite)

bool CheckSubdomain(string domain, string subdomain)
{
    auto d = LabelUtils::StringToLabels(domain);
    auto sd = LabelUtils::StringToLabels(subdomain);
    return LabelUtils::SubDomainCheck(d, sd);
}

BOOST_AUTO_TEST_CASE(type_conversions)
{
    for (int i = 0; i < RRType::N; i++) {
        std::bitset<RRType::N> b;
        b[i] = true;
        BOOST_CHECK_EQUAL(i, static_cast<int>(TypeUtils::StringToType(TypeUtils::TypesToString(b))));
    }
}

BOOST_AUTO_TEST_CASE(label_conversions_empty)
{
    auto empty = LabelUtils::StringToLabels("");
    BOOST_CHECK_EQUAL(empty.size(), 0);
}

BOOST_AUTO_TEST_CASE(label_conversions_root)
{
    auto empty = LabelUtils::StringToLabels(".");
    BOOST_CHECK_EQUAL(empty.size(), 0);
}

BOOST_AUTO_TEST_CASE(label_conversions)
{
    NodeLabel l1("uk");
    NodeLabel l2("co");
    vector<NodeLabel> domain_name{l1, l2};

    auto dn = LabelUtils::LabelsToString(domain_name);
    BOOST_CHECK_EQUAL("co.uk.", dn);

    auto labels = LabelUtils::StringToLabels(dn);
    BOOST_CHECK_EQUAL(labels[0].get(), "uk");
    BOOST_CHECK_EQUAL(labels[1].get(), "co");
}

BOOST_AUTO_TEST_CASE(subdomain_check)
{
    BOOST_TEST(CheckSubdomain(".", "com."));
    BOOST_TEST(CheckSubdomain(".", "org."));
    BOOST_TEST(CheckSubdomain("", "google.com."));
    BOOST_TEST(CheckSubdomain("bing.com", "search.bing.com"));
    BOOST_TEST(!CheckSubdomain("google.com", "com."));
    BOOST_TEST(!CheckSubdomain("google.com", "microsoft.com."));
    BOOST_TEST(!CheckSubdomain("google.com", "."));
    BOOST_TEST(!CheckSubdomain("google.com", "org"));
    BOOST_TEST(!CheckSubdomain("a.b.c", "a.c.c"));
}

BOOST_AUTO_TEST_SUITE_END()