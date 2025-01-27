#pragma once

#include <initializer_list>
#include <ostream>
#include <source_location>
#include <type_traits>

namespace testing
{
namespace privates
{
struct Ostream_printer
{
	template<typename Type>
	constexpr Ostream_printer(const Type& value) noexcept
		:
		value_(&value),
		printer_([](std::ostream& os, const void* value) -> void
		{
			os << *static_cast<const Type*>(value);
		})
	{
	}
	
	friend std::ostream& operator<<(std::ostream& os, Ostream_printer printer)
	{
		printer.printer_(os, printer.value_);
		return os;
	}
	
private:
	const void* value_ = nullptr;
	void(*printer_)(std::ostream& os, const void* value) = nullptr;
};

void fail_eq(Ostream_printer expected, Ostream_printer actual, std::source_location location);
void fail_neq(Ostream_printer expected, Ostream_printer actual, std::source_location location);
} // namespace privates

struct Test_failure;

struct Test_case
{
	constexpr Test_case(const char* name, void(*test)(), std::source_location location = std::source_location::current()) noexcept
		:
		name_(name),
		test_(test),
		location_(location)
	{
	}
	
	void operator()() const;
	
private:
	[[maybe_unused]] const char* name_ = nullptr;
	void(*test_)() = nullptr;
	const std::source_location location_;
};

void assert_true(bool value, std::source_location location = std::source_location::current());
void assert_false(bool value, std::source_location location = std::source_location::current());

void assert_eq(const auto& expected, const auto& actual, std::source_location location = std::source_location::current())
{
	if (expected != actual)
	{
		privates::fail_eq(expected, actual, location);
	}
}

void assert_neq(const auto& expected, const auto& actual, std::source_location location = std::source_location::current())
{
	if (expected == actual)
	{
		privates::fail_neq(expected, actual, location);
	}
}

template<typename Type>
requires(std::is_floating_point_v<Type>)
struct Assert_floating_similar
{
	constexpr Assert_floating_similar(Type threshold) noexcept
		:
		threshold_(threshold)
	{
	}
	
	void operator()(Type expected, Type actual, std::source_location location = std::source_location::current());
	
private:
	Type threshold_;
};

extern std::initializer_list<Test_case> test_cases;
} // namespace testing
