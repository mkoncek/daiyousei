#include <limits>

#include <bencode.hpp>
#include <testing.hpp>

using namespace bencode;
using namespace bencode::serializable;

std::initializer_list<testing::Test_case> testing::test_cases
{
Test_case("integer", []
{
	assert_eq("i0e", serialize(0));
	assert_eq("i25e", serialize(25));
	assert_eq("i-1e", serialize(-1));
	assert_eq("i9223372036854775807e", serialize(std::numeric_limits<bencode::Integer>::max()));
	assert_eq("i-9223372036854775808e", serialize(std::numeric_limits<bencode::Integer>::min()));
}),

Test_case("byte_string", []
{
	assert_eq("0:", serialize(""));
	assert_eq("3:foo", serialize("foo"));
	assert_eq("7:bencode", serialize("bencode"));
	assert_eq("3:☺", serialize("☺"));
	assert_eq("26:Lorem ipsum dolor sit amet", serialize("Lorem ipsum dolor sit amet"));
	assert_eq(
		"123:Lorem ipsum dolor sit amet, consectetur adipiscing elit, sed do eiusmod tempor incididunt ut labore et dolore magna aliqua.", serialize(
		"Lorem ipsum dolor sit amet, consectetur adipiscing elit, sed do eiusmod tempor incididunt ut labore et dolore magna aliqua."
	));
}),

Test_case("list", []
{
	assert_eq("le", serialize(List()));
	assert_eq("l1:ae", serialize(List {Serializable("a")}));
	assert_eq("l1:a1:ae", serialize(List {Serializable("a"), Serializable("a")}));
	assert_eq("li-1e1:ai25e3:fooe", serialize(List {
		Serializable(-1), Serializable("a"), Serializable(25), Serializable("foo")
	}));
	{
		auto lst = List({Serializable("a"), Serializable("a")});
		assert_eq("ll1:a1:ael1:a1:aee", serialize(List {
			Serializable(lst), Serializable(lst)
		}));
	}
}),

Test_case("dictionary", []
{
	assert_eq("de", serialize(Dictionary()));
	{
		auto vec = Dictionary {Field("a", Serializable(1))};
		assert_eq("d1:ai1ee", serialize(Dictionary(vec)));
	}
	{
		auto vec = Dictionary {Field("a", Serializable(1)), Field("b", Serializable(2))};
		assert_eq("d1:ai1e1:bi2ee", serialize(Dictionary(vec)));
	}
	{
		auto vec = Dictionary {Field("b", Serializable(2)), Field("a", Serializable(1))};
		assert_eq("d1:ai1e1:bi2ee", serialize(Dictionary(vec)));
	}
}),
};
