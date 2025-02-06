#include <iostream>
#include <sstream>

#include <testing.hpp>

namespace testing
{
struct Test_failure : std::exception
{
	Test_failure(std::source_location location)
		:
		location_(location)
	{
		message_
		<< "["
		<< "\033[31;1m"
		<< "FAIL"
		<< "\033[m"
		<< "] "
		<< "\033[35m"
		<< location.file_name()
		<< "\033[m"
		<< ":"
		<< "\033[36m"
		<< location.line()
		<< "\033[m"
		<< ": "
		;
		message_.put('\0');
		message_.seekp(-1, std::ios_base::end);
	}
	
	const char* what() const noexcept override
	{
		return message_.rdbuf()->view().data();
	}
	
	Test_failure& operator<<(const auto& value) &
	{
		message_ << value;
		message_.put('\0');
		message_.seekp(-1, std::ios_base::end);
		return *this;
	}
	
	Test_failure&& operator<<(const auto& value) &&
	{
		return std::move(*this << value);
	}
	
private:
	const std::source_location location_;
	std::stringstream message_;
};

namespace privates
{
void fail_eq(Ostream_printer expected, Ostream_printer actual, std::source_location location)
{
	throw testing::Test_failure(location) << "expected: " << expected << " but was: " << actual;
}

void fail_neq(Ostream_printer expected, Ostream_printer actual, std::source_location location)
{
	throw testing::Test_failure(location) << "expected: not equal but " << expected << " == " << actual;
}
} // namespace privates

void testing::Test_case::operator()() const
{
	try
	{
		test_();
	}
	catch (...)
	{
		std::cout << "[" << "\033[91;1m" << "ERROR" << "\033[m" << "] an exception thrown from test case " << "\033[96;1m" << name_ << "\033[m" << ":\n";
		throw;
	}
}

void fail_because(std::string_view message, std::source_location location)
{
	throw testing::Test_failure(location) << message;
}

void assert_true(bool value, std::source_location location)
{
	if (not value)
	{
		throw testing::Test_failure(location) << "evaluated to false, expected true";
	}
}

void assert_false(bool value, std::source_location location)
{
	if (value)
	{
		throw testing::Test_failure(location) << "evaluated to true, expected false";
	}
}

template<typename Type>
requires(std::is_floating_point_v<Type>)
void Assert_floating_similar<Type>::operator()(Type expected, Type actual, std::source_location location)
{
	if (auto result = expected - actual; std::abs(result) > threshold_)
	{
		throw testing::Test_failure(location) << "expected: " << expected << " to be similar to " << actual
			<< " but their absolute difference is larger than "
			<< result
		;
	}
}

template struct Assert_floating_similar<float>;
template struct Assert_floating_similar<double>;
template struct Assert_floating_similar<long double>;
} // namespace testing

int main(int argc, const char** argv)
{
	const char* test_name = "<unknown>";
	
	if (argc > 0)
	{
		test_name = argv[0];
	}
	
	std::cout << "\033[36;1m" << "Running tests for " << "\033[0;35m" << test_name << "\033[m" << "\n";
	
	for (auto& test : testing::test_cases)
	{
		test();
	}
	
	std::cout << "\033[32;1m" << "All tests passed" << "\033[m" << "\n";
}
