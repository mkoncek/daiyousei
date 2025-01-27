#include <limits>

#include <bencode.hpp>
#include <testing.hpp>

using namespace bencode;
using namespace bencode::serializable;

std::initializer_list<testing::Test_case> testing::test_cases
{
Test_case("integer", []
{
	assert_eq("i0e", serialize(0).value());
	assert_eq("i25e", serialize(25).value());
	assert_eq("i-1e", serialize(-1).value());
	assert_eq("i9223372036854775807e", serialize(std::numeric_limits<bencode::Integer>::max()).value());
	assert_eq("i-9223372036854775808e", serialize(std::numeric_limits<bencode::Integer>::min()).value());
}),

Test_case("byte_string", []
{
	assert_eq("0:", serialize("").value());
	assert_eq("3:foo", serialize("foo").value());
	assert_eq("7:bencode", serialize("bencode").value());
	assert_eq("3:☺", serialize("☺").value());
	assert_eq("26:Lorem ipsum dolor sit amet", serialize("Lorem ipsum dolor sit amet").value());
	assert_eq(
		"123:Lorem ipsum dolor sit amet, consectetur adipiscing elit, sed do eiusmod tempor incididunt ut labore et dolore magna aliqua.", serialize(
		"Lorem ipsum dolor sit amet, consectetur adipiscing elit, sed do eiusmod tempor incididunt ut labore et dolore magna aliqua."
	).value());
}),

Test_case("list", []
{
	assert_eq("le", serialize(List()).value());
	assert_eq("l1:ae", serialize(List {Serializable("a")}).value());
	assert_eq("l1:a1:ae", serialize(List {Serializable("a"), Serializable("a")}).value());
	assert_eq("li-1e1:ai25e3:fooe", serialize(List {
		Serializable(-1), Serializable("a"), Serializable(25), Serializable("foo")
	}).value());
	{
		auto lst = List({Serializable("a"), Serializable("a")});
		assert_eq("ll1:a1:ael1:a1:aee", serialize(List {
			Serializable(lst), Serializable(lst)
		}).value());
	}
}),

Test_case("dictionary", []
{
	assert_eq("de", serialize(Dictionary()).value());
	{
		auto vec = Dictionary {Field("a", Serializable(1))};
		assert_eq("d1:ai1ee", serialize(Dictionary(vec)).value());
	}
	{
		auto vec = Dictionary {Field("a", Serializable(1)), Field("b", Serializable(2))};
		assert_eq("d1:ai1e1:bi2ee", serialize(Dictionary(vec)).value());
	}
	{
		auto vec = Dictionary {Field("b", Serializable(2)), Field("a", Serializable(1))};
		assert_eq("d1:ai1e1:bi2ee", serialize(Dictionary(vec)).value());
	}
}),
};
