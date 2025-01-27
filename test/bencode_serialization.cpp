#include <limits>

#include <bencode.hpp>

#define BOOST_TEST_DYN_LINK
#define BOOST_TEST_MAIN
#include <boost/test/unit_test.hpp>

using namespace bencode;
using namespace bencode::serializable;

BOOST_AUTO_TEST_CASE(test_int)
{
	BOOST_TEST("i0e" == serialize(0).value());
	BOOST_TEST("i25e" == serialize(25).value());
	BOOST_TEST("i-1e" == serialize(-1).value());
	BOOST_TEST("i9223372036854775807e" == serialize(std::numeric_limits<bencode::Integer>::max()).value());
	BOOST_TEST("i-9223372036854775808e" == serialize(std::numeric_limits<bencode::Integer>::min()).value());
}

BOOST_AUTO_TEST_CASE(test_byte_string)
{
	BOOST_TEST("0:" == serialize("").value());
	BOOST_TEST("3:foo" == serialize("foo").value());
	BOOST_TEST("7:bencode" == serialize("bencode").value());
	BOOST_TEST("3:☺" == serialize("☺").value());
	BOOST_TEST("26:Lorem ipsum dolor sit amet" == serialize("Lorem ipsum dolor sit amet").value());
	BOOST_TEST(
		"123:Lorem ipsum dolor sit amet, consectetur adipiscing elit, sed do eiusmod tempor incididunt ut labore et dolore magna aliqua." == serialize(
		"Lorem ipsum dolor sit amet, consectetur adipiscing elit, sed do eiusmod tempor incididunt ut labore et dolore magna aliqua."
	).value());
}

BOOST_AUTO_TEST_CASE(test_list)
{
	BOOST_TEST("le" == serialize(List()).value());
	BOOST_TEST("l1:ae" == serialize(List {Serializable("a")}).value());
	BOOST_TEST("l1:a1:ae" == serialize(List {Serializable("a"), Serializable("a")}).value());
	BOOST_TEST("li-1e1:ai25e3:fooe" == serialize(List {
		Serializable(-1), Serializable("a"), Serializable(25), Serializable("foo")
	}).value());
	{
		auto lst = List({Serializable("a"), Serializable("a")});
		BOOST_TEST("ll1:a1:ael1:a1:aee" == serialize(List {
			Serializable(lst), Serializable(lst)
		}).value());
	}
}

BOOST_AUTO_TEST_CASE(test_dictionary)
{
	BOOST_TEST("de" == serialize(Dictionary()).value());
	{
		auto vec = Dictionary {Field("a", Serializable(1))};
		BOOST_TEST("d1:ai1ee" == serialize(Dictionary(vec)).value());
	}
	{
		auto vec = Dictionary {Field("a", Serializable(1)), Field("b", Serializable(2))};
		BOOST_TEST("d1:ai1e1:bi2ee" == serialize(Dictionary(vec)).value());
	}
	{
		auto vec = Dictionary {Field("b", Serializable(2)), Field("a", Serializable(1))};
		BOOST_TEST("d1:ai1e1:bi2ee" == serialize(Dictionary(vec)).value());
	}
}
