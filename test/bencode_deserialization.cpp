#include <bencode.hpp>

#define BOOST_TEST_DYN_LINK
#define BOOST_TEST_MAIN
#include <boost/test/unit_test.hpp>

using namespace bencode;
using namespace bencode::deserialized;

BOOST_AUTO_TEST_CASE(test_int)
{
	{
		auto de = Deserializer();
		de << "i0e";
		BOOST_TEST(0 == de.deserialize().value().get_integer().value);
		BOOST_TEST(de.empty());
	}
	{
		auto de = Deserializer();
		de << "i123456e";
		BOOST_TEST(123456 == de.deserialize().value().get_integer().value);
		BOOST_TEST(de.empty());
	}
	{
		auto de = Deserializer();
		de << "i-123456e";
		BOOST_TEST(-123456 == de.deserialize().value().get_integer().value);
		BOOST_TEST(de.empty());
	}
}

BOOST_AUTO_TEST_CASE(test_int_incomplete)
{
	{
		auto de = Deserializer();
		for (char c : std::string_view("i0e"))
		{
			BOOST_TEST(not de.deserialize().has_value());
			de << std::string_view(&c, 1);
		}
		BOOST_TEST(0 == de.deserialize().value().get_integer().value);
		BOOST_TEST(de.empty());
	}
	{
		auto de = Deserializer();
		for (char c : std::string_view("i123456e"))
		{
			BOOST_TEST(not de.deserialize().has_value());
			de << std::string_view(&c, 1);
		}
		BOOST_TEST(123456 == de.deserialize().value().get_integer().value);
		BOOST_TEST(de.empty());
	}
	{
		auto de = Deserializer();
		for (char c : std::string_view("i-123456e"))
		{
			BOOST_TEST(not de.deserialize().has_value());
			de << std::string_view(&c, 1);
		}
		BOOST_TEST(-123456 == de.deserialize().value().get_integer().value);
		BOOST_TEST(de.empty());
	}
}

BOOST_AUTO_TEST_CASE(test_byte_string)
{
	{
		auto de = Deserializer();
		de << "0:";
		BOOST_TEST("" == de.deserialize().value().get_byte_string());
		BOOST_TEST(de.empty());
	}
	{
		auto de = Deserializer();
		de << "3:foo";
		BOOST_TEST("foo" == de.deserialize().value().get_byte_string());
		BOOST_TEST(de.empty());
	}
	{
		auto de = Deserializer();
		de << "7:bencode";
		BOOST_TEST("bencode" == de.deserialize().value().get_byte_string());
		BOOST_TEST(de.empty());
	}
}

BOOST_AUTO_TEST_CASE(test_byte_string_incomplete)
{
	{
		auto de = Deserializer();
		for (char c : std::string_view("0:"))
		{
			BOOST_TEST(not de.deserialize().has_value());
			de << std::string_view(&c, 1);
		}
		BOOST_TEST("" == de.deserialize().value().get_byte_string());
		BOOST_TEST(de.empty());
	}
	{
		auto de = Deserializer();
		for (char c : std::string_view("3:foo"))
		{
			BOOST_TEST(not de.deserialize().has_value());
			de << std::string_view(&c, 1);
		}
		BOOST_TEST("foo" == de.deserialize().value().get_byte_string());
		BOOST_TEST(de.empty());
	}
	{
		auto de = Deserializer();
		for (char c : std::string_view("11:eoobarbazee"))
		{
			BOOST_TEST(not de.deserialize().has_value());
			de << std::string_view(&c, 1);
		}
		BOOST_TEST("eoobarbazee" == de.deserialize().value().get_byte_string());
		BOOST_TEST(de.empty());
	}
}

BOOST_AUTO_TEST_CASE(test_list)
{
	{
		auto de = Deserializer();
		de << "le";
		BOOST_TEST(de.deserialize().value().get_list().empty());
		BOOST_TEST(de.empty());
	}
	{
		auto de = Deserializer();
		de << "l1:ae";
		auto list = de.deserialize().value().get_list();
		auto it = list.begin();
		BOOST_TEST("a" == it++->get_byte_string());
		BOOST_TEST((list.end() == it));
		BOOST_TEST(de.empty());
	}
	{
		auto de = Deserializer();
		de << "l1:a1:ae";
		auto list = de.deserialize().value().get_list();
		auto it = list.begin();
		BOOST_TEST("a" == it++->get_byte_string());
		BOOST_TEST("a" == it++->get_byte_string());
		BOOST_TEST((list.end() == it));
		BOOST_TEST(de.empty());
	}
	{
		auto de = Deserializer();
		de << "l1:ai5elee";
		auto list = de.deserialize().value().get_list();
		auto it = list.begin();
		BOOST_TEST("a" == it++->get_byte_string());
		BOOST_TEST(5 == it++->get_integer());
		BOOST_TEST(it++->get_list().empty());
		BOOST_TEST((list.end() == it));
		BOOST_TEST(de.empty());
	}
	{
		auto de = Deserializer();
		de << "lllleeee";
		auto list = de.deserialize().value().get_list();
		BOOST_TEST(1 == list.size());
		list = std::move(list[0]).get_list();
		BOOST_TEST(1 == list.size());
		list = std::move(list[0]).get_list();
		BOOST_TEST(1 == list.size());
		list = std::move(list[0]).get_list();
		BOOST_TEST(list.empty());
		BOOST_TEST(de.empty());
	}
}

BOOST_AUTO_TEST_CASE(test_list_incomplete)
{
	{
		auto de = Deserializer();
		for (char c : std::string_view("le"))
		{
			BOOST_TEST(not de.deserialize().has_value());
			de << std::string_view(&c, 1);
		}
		BOOST_TEST(de.deserialize().value().get_list().empty());
		BOOST_TEST(de.empty());
	}
	{
		auto de = Deserializer();
		for (char c : std::string_view("l1:ae"))
		{
			BOOST_TEST(not de.deserialize().has_value());
			de << std::string_view(&c, 1);
		}
		auto list = de.deserialize().value().get_list();
		auto it = list.begin();
		BOOST_TEST("a" == it++->get_byte_string());
		BOOST_TEST((list.end() == it));
		BOOST_TEST(de.empty());
	}
	{
		auto de = Deserializer();
		for (char c : std::string_view("l1:a1:ae"))
		{
			BOOST_TEST(not de.deserialize().has_value());
			de << std::string_view(&c, 1);
		}
		auto list = de.deserialize().value().get_list();
		auto it = list.begin();
		BOOST_TEST("a" == it++->get_byte_string());
		BOOST_TEST("a" == it++->get_byte_string());
		BOOST_TEST((list.end() == it));
		BOOST_TEST(de.empty());
	}
	{
		auto de = Deserializer();
		for (char c : std::string_view("l1:ai5elee"))
		{
			BOOST_TEST(not de.deserialize().has_value());
			de << std::string_view(&c, 1);
		}
		auto list = de.deserialize().value().get_list();
		auto it = list.begin();
		BOOST_TEST("a" == it++->get_byte_string());
		BOOST_TEST(5 == it++->get_integer());
		BOOST_TEST(it++->get_list().empty());
		BOOST_TEST((list.end() == it));
		BOOST_TEST(de.empty());
	}
	{
		auto de = Deserializer();
		for (char c : std::string_view("lllleeee"))
		{
			BOOST_TEST(not de.deserialize().has_value());
			de << std::string_view(&c, 1);
		}
		auto list = de.deserialize().value().get_list();
		BOOST_TEST(1 == list.size());
		list = std::move(list[0]).get_list();
		BOOST_TEST(1 == list.size());
		list = std::move(list[0]).get_list();
		BOOST_TEST(1 == list.size());
		list = std::move(list[0]).get_list();
		BOOST_TEST(list.empty());
		BOOST_TEST(de.empty());
	}
}

BOOST_AUTO_TEST_CASE(test_dictionary)
{
	{
		auto de = Deserializer();
		de << "de";
		BOOST_TEST(de.deserialize().value().get_dictionary().empty());
		BOOST_TEST(de.empty());
	}
	{
		auto de = Deserializer();
		de << "d1:ai1ee";
		auto dict = de.deserialize().value().get_dictionary();
		auto it = dict.begin();
		BOOST_TEST("a" == it->name);
		BOOST_TEST(1 == it->value.get_integer());
		++it;
		BOOST_TEST((dict.end() == it));
		BOOST_TEST(de.empty());
	}
	{
		auto de = Deserializer();
		de << "d1:ai1e1:bi1ee";
		auto dict = de.deserialize().value().get_dictionary();
		auto it = dict.begin();
		BOOST_TEST("a" == it->name);
		BOOST_TEST(1 == it->value.get_integer());
		++it;
		BOOST_TEST("b" == it->name);
		BOOST_TEST(1 == it->value.get_integer());
		++it;
		BOOST_TEST((dict.end() == it));
		BOOST_TEST(de.empty());
	}
	{
		auto de = Deserializer();
		de << "d1:ai1e1:ai1ee";
		BOOST_TEST((Deserialization_error::dictionary_duplicate_key == de.deserialize().error()));
	}
	{
		auto de = Deserializer();
		de << "d1:bi1e1:ai1ee";
		BOOST_TEST((Deserialization_error::dictionary_unsorted == de.deserialize().error()));
	}
}

BOOST_AUTO_TEST_CASE(test_dictionary_incomplete)
{
	{
		auto de = Deserializer();
		for (char c : std::string_view("de"))
		{
			BOOST_TEST(not de.deserialize().has_value());
			de << std::string_view(&c, 1);
		}
		BOOST_TEST(de.deserialize().value().get_dictionary().empty());
		BOOST_TEST(de.empty());
	}
	{
		auto de = Deserializer();
		for (char c : std::string_view("d1:ai1ee"))
		{
			BOOST_TEST(not de.deserialize().has_value());
			de << std::string_view(&c, 1);
		}
		auto dict = de.deserialize().value().get_dictionary();
		auto it = dict.begin();
		BOOST_TEST("a" == it->name);
		BOOST_TEST(1 == it->value.get_integer());
		++it;
		BOOST_TEST((dict.end() == it));
		BOOST_TEST(de.empty());
	}
	{
		auto de = Deserializer();
		for (char c : std::string_view("d1:ai1e1:bi1ee"))
		{
			BOOST_TEST(not de.deserialize().has_value());
			de << std::string_view(&c, 1);
		}
		auto dict = de.deserialize().value().get_dictionary();
		auto it = dict.begin();
		BOOST_TEST("a" == it->name);
		BOOST_TEST(1 == it->value.get_integer());
		++it;
		BOOST_TEST("b" == it->name);
		BOOST_TEST(1 == it->value.get_integer());
		++it;
		BOOST_TEST((dict.end() == it));
		BOOST_TEST(de.empty());
	}
}
