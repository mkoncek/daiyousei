#include <bencode.hpp>
#include <testing.hpp>

using namespace bencode;
using namespace bencode::deserialized;

struct Test_streaming_deserializer : bencode::Streaming_deserializer, std::string
{
	void visit_integer(bencode::Integer value) override
	{
		bencode::serialize(*this, serializable::Integer(value));
	}
	
	void visit_list_end() override
	{
		*this += 'e';
	}
	
	void visit_dictionary_end() override
	{
		*this += 'e';
	}
	
	void visit_list_begin() override
	{
		*this += 'l';
	}
	
	void visit_dictionary_begin() override
	{
		*this += 'd';
	}
	
	void visit_byte_string(std::string_view value) override
	{
		bencode::serialize(*this, value);
	}
	
	void test_whole(std::string_view string, std::source_location location = std::source_location::current())
	{
		receive(string);
		testing::assert_eq(string, *this, location);
		testing::assert_true(finished(), location);
	}
	
	void test_incomplete(std::string_view string, std::source_location location = std::source_location::current())
	{
		for (char c : string)
		{
			receive(std::string_view(&c, 1));
		}
		testing::assert_eq(string, *this, location);
		testing::assert_true(finished(), location);
	}
};

std::initializer_list<testing::Test_case> testing::test_cases
{
Test_case("int", []
{
	Test_streaming_deserializer().test_whole("i0e");
	Test_streaming_deserializer().test_whole("i123456e");
	Test_streaming_deserializer().test_whole("i-123456e");
}),

Test_case("int incomplete", []
{
	Test_streaming_deserializer().test_incomplete("i0e");
	Test_streaming_deserializer().test_incomplete("i123456e");
	Test_streaming_deserializer().test_incomplete("i-123456e");
}),

Test_case("byte_string", []
{
	Test_streaming_deserializer().test_whole("0:");
	Test_streaming_deserializer().test_whole("3:foo");
	Test_streaming_deserializer().test_whole("7:bencode");
}),

Test_case("byte_string incomplete", []
{
	Test_streaming_deserializer().test_incomplete("0:");
	Test_streaming_deserializer().test_incomplete("3:foo");
	Test_streaming_deserializer().test_incomplete("11:eoobarbazee");
}),

Test_case("list", []
{
	Test_streaming_deserializer().test_whole("le");
	Test_streaming_deserializer().test_whole("l1:ae");
	Test_streaming_deserializer().test_whole("l1:a1:ae");
	Test_streaming_deserializer().test_whole("l1:ai5elee");
	Test_streaming_deserializer().test_whole("lllleeee");
}),

Test_case("list incomplete", []
{
	Test_streaming_deserializer().test_incomplete("le");
	Test_streaming_deserializer().test_incomplete("l1:ae");
	Test_streaming_deserializer().test_incomplete("l1:a1:ae");
	Test_streaming_deserializer().test_incomplete("l1:ai5elee");
	Test_streaming_deserializer().test_incomplete("lllleeee");
}),

Test_case("dictionary", []
{
	Test_streaming_deserializer().test_whole("de");
	Test_streaming_deserializer().test_whole("d1:ai1ee");
	Test_streaming_deserializer().test_whole("d1:ai1e1:bi1ee");
	try
	{
		Test_streaming_deserializer().test_whole("d1:ai1e1:ai1ee");
		fail_because("excpected an exception due to duplicate keys");
	}
	catch (bencode::Deserialization_exception& ex)
	{
	}
	catch (...)
	{
		throw;
	}
	try
	{
		Test_streaming_deserializer().test_whole("d1:bi1e1:ai1ee");
		fail_because("excpected an exception due to unsorted keys");
	}
	catch (bencode::Deserialization_exception& ex)
	{
	}
	catch (...)
	{
		throw;
	}
	Test_streaming_deserializer().test_whole("d1:a1:a1:ble1:cdee");
	Test_streaming_deserializer().test_whole("d1:a1:a1:bl2:abd2:abli456eeee1:cd1:ade1:bdeee");
}),

Test_case("dictionary incomplete", []
{
	Test_streaming_deserializer().test_incomplete("de");
	Test_streaming_deserializer().test_incomplete("d1:ai1ee");
	Test_streaming_deserializer().test_incomplete("d1:ai1e1:bi1ee");
	Test_streaming_deserializer().test_incomplete("d1:a1:a1:bl2:abd2:abli456eeee1:cd1:ade1:bdeee");
}),
};
