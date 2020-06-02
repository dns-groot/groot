#include "../src/equivalence-class.h"
#include "../src/utils.h"
#include <boost/test/unit_test.hpp>

BOOST_AUTO_TEST_SUITE(EcTestSuite)

BOOST_AUTO_TEST_CASE(ec_equality1)
{
    std::bitset<RRType::N> bs;
    bs.set(RRType::A);
    bs.set(RRType::TXT);

    EC ec1;
    ec1.name = LabelUtils::StringToLabels("a.b.c.");
    ec1.excluded = {};
    ec1.nonExistent = false;
    ec1.rrTypes = bs;

    EC ec2;
    ec2.name = LabelUtils::StringToLabels("a.b.c.");
    ec2.excluded = {};
    ec2.nonExistent = false;
    ec2.rrTypes = bs;

    BOOST_CHECK(ec1 == ec2);
}

BOOST_AUTO_TEST_CASE(ec_inequality1)
{
    EC ec1;
    ec1.name = LabelUtils::StringToLabels("a.b.c.");
    ec1.excluded = {};
    ec1.nonExistent = false;

    EC ec2;
    ec2.name = LabelUtils::StringToLabels("b.c.");
    ec2.excluded = {};
    ec2.nonExistent = false;

    BOOST_CHECK(!(ec1 == ec2));
}

BOOST_AUTO_TEST_CASE(ec_inequality2)
{
    std::bitset<RRType::N> bs1;
    bs1.set(RRType::A);

    std::bitset<RRType::N> bs2;
    bs2.set(RRType::A);
    bs2.set(RRType::TXT);

    EC ec1;
    ec1.name = LabelUtils::StringToLabels("a.b.c.");
    ec1.excluded = {};
    ec1.nonExistent = false;
    ec1.rrTypes = bs1;

    EC ec2;
    ec2.name = LabelUtils::StringToLabels("a.b.c.");
    ec2.excluded = {};
    ec2.nonExistent = false;
    ec2.rrTypes = bs2;

    BOOST_CHECK(!(ec1 == ec2));
}

BOOST_AUTO_TEST_CASE(ec_inequality3)
{
    std::bitset<RRType::N> bs1;
    bs1.set(RRType::A);
    bs1.set(RRType::TXT);

    std::bitset<RRType::N> bs2;
    bs2.set(RRType::A);

    EC ec1;
    ec1.name = LabelUtils::StringToLabels("a.b.c.");
    ec1.excluded = {};
    ec1.nonExistent = false;
    ec1.rrTypes = bs1;

    EC ec2;
    ec2.name = LabelUtils::StringToLabels("a.b.c.");
    ec2.excluded = {};
    ec2.nonExistent = false;
    ec2.rrTypes = bs2;

    BOOST_CHECK(!(ec1 == ec2));
}

BOOST_AUTO_TEST_SUITE_END()