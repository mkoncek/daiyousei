#include <bencode.hpp>
#include <testing.hpp>

using namespace bencode;
using namespace bencode::deserialized;

std::initializer_list<testing::Test_case> testing::test_cases
{
Test_case("int", []
{
	{
		auto de = Deserializer();
		de << "i0e";
		assert_eq(0, de.deserialize().value().get_integer().value);
		assert_true(de.empty());
	}
	{
		auto de = Deserializer();
		de << "i123456e";
		assert_eq(123456, de.deserialize().value().get_integer().value);
		assert_true(de.empty());
	}
	{
		auto de = Deserializer();
		de << "i-123456e";
		assert_eq(-123456, de.deserialize().value().get_integer().value);
		assert_true(de.empty());
	}
}),

Test_case("int incomplete", []
{
	{
		auto de = Deserializer();
		for (char c : std::string_view("i0e"))
		{
			assert_true(not de.deserialize().has_value());
			de << std::string_view(&c, 1);
		}
		assert_eq(0, de.deserialize().value().get_integer().value);
		assert_true(de.empty());
	}
	{
		auto de = Deserializer();
		for (char c : std::string_view("i123456e"))
		{
			assert_true(not de.deserialize().has_value());
			de << std::string_view(&c, 1);
		}
		assert_eq(123456, de.deserialize().value().get_integer().value);
		assert_true(de.empty());
	}
	{
		auto de = Deserializer();
		for (char c : std::string_view("i-123456e"))
		{
			assert_true(not de.deserialize().has_value());
			de << std::string_view(&c, 1);
		}
		assert_eq(-123456, de.deserialize().value().get_integer().value);
		assert_true(de.empty());
	}
}),

Test_case("byte_string", []
{
	{
		auto de = Deserializer();
		de << "0:";
		assert_eq("", de.deserialize().value().get_byte_string());
		assert_true(de.empty());
	}
	{
		auto de = Deserializer();
		de << "3:foo";
		assert_eq("foo", de.deserialize().value().get_byte_string());
		assert_true(de.empty());
	}
	{
		auto de = Deserializer();
		de << "7:bencode";
		assert_eq("bencode", de.deserialize().value().get_byte_string());
		assert_true(de.empty());
	}
}),

Test_case("byte_string incomplete", []
{
	{
		auto de = Deserializer();
		for (char c : std::string_view("0:"))
		{
			assert_true(not de.deserialize().has_value());
			de << std::string_view(&c, 1);
		}
		assert_eq("", de.deserialize().value().get_byte_string());
		assert_true(de.empty());
	}
	{
		auto de = Deserializer();
		for (char c : std::string_view("3:foo"))
		{
			assert_true(not de.deserialize().has_value());
			de << std::string_view(&c, 1);
		}
		assert_eq("foo", de.deserialize().value().get_byte_string());
		assert_true(de.empty());
	}
	{
		auto de = Deserializer();
		for (char c : std::string_view("11:eoobarbazee"))
		{
			assert_true(not de.deserialize().has_value());
			de << std::string_view(&c, 1);
		}
		assert_eq("eoobarbazee", de.deserialize().value().get_byte_string());
		assert_true(de.empty());
	}
}),

Test_case("list", []
{
	{
		auto de = Deserializer();
		de << "le";
		assert_true(de.deserialize().value().get_list().empty());
		assert_true(de.empty());
	}
	{
		auto de = Deserializer();
		de << "l1:ae";
		auto list = de.deserialize().value().get_list();
		auto it = list.begin();
		assert_eq("a", it++->get_byte_string());
		assert_true(list.end() == it);
		assert_true(de.empty());
	}
	{
		auto de = Deserializer();
		de << "l1:a1:ae";
		auto list = de.deserialize().value().get_list();
		auto it = list.begin();
		assert_eq("a", it++->get_byte_string());
		assert_eq("a", it++->get_byte_string());
		assert_true(list.end() == it);
		assert_true(de.empty());
	}
	{
		auto de = Deserializer();
		de << "l1:ai5elee";
		auto list = de.deserialize().value().get_list();
		auto it = list.begin();
		assert_eq("a", it++->get_byte_string());
		assert_eq(5, it++->get_integer());
		assert_true(it++->get_list().empty());
		assert_true(list.end() == it);
		assert_true(de.empty());
	}
	{
		auto de = Deserializer();
		de << "lllleeee";
		auto list = de.deserialize().value().get_list();
		assert_eq(1uz, list.size());
		list = auto(std::move(list[0]).get_list());
		assert_eq(1uz, list.size());
		list = auto(std::move(list[0]).get_list());
		assert_eq(1uz, list.size());
		list = auto(std::move(list[0]).get_list());
		assert_true(list.empty());
		assert_true(de.empty());
	}
}),

Test_case("list incomplete", []
{
	{
		auto de = Deserializer();
		for (char c : std::string_view("le"))
		{
			assert_true(not de.deserialize().has_value());
			de << std::string_view(&c, 1);
		}
		assert_true(de.deserialize().value().get_list().empty());
		assert_true(de.empty());
	}
	{
		auto de = Deserializer();
		for (char c : std::string_view("l1:ae"))
		{
			assert_true(not de.deserialize().has_value());
			de << std::string_view(&c, 1);
		}
		auto list = de.deserialize().value().get_list();
		auto it = list.begin();
		assert_eq("a", it++->get_byte_string());
		assert_true(list.end() == it);
		assert_true(de.empty());
	}
	{
		auto de = Deserializer();
		for (char c : std::string_view("l1:a1:ae"))
		{
			assert_true(not de.deserialize().has_value());
			de << std::string_view(&c, 1);
		}
		auto list = de.deserialize().value().get_list();
		auto it = list.begin();
		assert_eq("a", it++->get_byte_string());
		assert_eq("a", it++->get_byte_string());
		assert_true(list.end() == it);
		assert_true(de.empty());
	}
	{
		auto de = Deserializer();
		for (char c : std::string_view("l1:ai5elee"))
		{
			assert_true(not de.deserialize().has_value());
			de << std::string_view(&c, 1);
		}
		auto list = de.deserialize().value().get_list();
		auto it = list.begin();
		assert_eq("a", it++->get_byte_string());
		assert_eq(5, it++->get_integer());
		assert_true(it++->get_list().empty());
		assert_true(list.end() == it);
		assert_true(de.empty());
	}
	{
		auto de = Deserializer();
		for (char c : std::string_view("lllleeee"))
		{
			assert_true(not de.deserialize().has_value());
			de << std::string_view(&c, 1);
		}
		auto list = de.deserialize().value().get_list();
		assert_eq(1uz, list.size());
		list = auto(std::move(list[0]).get_list());
		assert_eq(1uz, list.size());
		list = auto(std::move(list[0]).get_list());
		assert_eq(1uz, list.size());
		list = auto(std::move(list[0]).get_list());
		assert_true(list.empty());
		assert_true(de.empty());
	}
}),

Test_case("dictionary", []
{
	{
		auto de = Deserializer();
		de << "de";
		assert_true(de.deserialize().value().get_dictionary().empty());
		assert_true(de.empty());
	}
	{
		auto de = Deserializer();
		de << "d1:ai1ee";
		auto dict = de.deserialize().value().get_dictionary();
		auto it = dict.begin();
		assert_eq("a", it->name);
		assert_eq(1, it->value.get_integer());
		++it;
		assert_true(dict.end() == it);
		assert_true(de.empty());
	}
	{
		auto de = Deserializer();
		de << "d1:ai1e1:bi1ee";
		auto dict = de.deserialize().value().get_dictionary();
		auto it = dict.begin();
		assert_eq("a", it->name);
		assert_eq(1, it->value.get_integer());
		++it;
		assert_eq("b", it->name);
		assert_eq(1, it->value.get_integer());
		++it;
		assert_true(dict.end() == it);
		assert_true(de.empty());
	}
	{
		auto de = Deserializer();
		de << "d1:ai1e1:ai1ee";
		assert_true(Deserialization_error::dictionary_duplicate_key == de.deserialize().error());
	}
	{
		auto de = Deserializer();
		de << "d1:bi1e1:ai1ee";
		assert_true(Deserialization_error::dictionary_unsorted == de.deserialize().error());
	}
	{
		auto de = Deserializer();
		de << "d1:a1:a1:ble1:cdee";
		auto dict = de.deserialize().value().get_dictionary();
		auto it = dict.begin();
		assert_eq("a", it->name);
		assert_eq("a", it->value.get_byte_string());
		++it;
		assert_eq("b", it->name);
		assert_true(it->value.get_list().empty());
		++it;
		assert_eq("c", it->name);
		assert_true(it->value.get_dictionary().empty());
		++it;
		assert_true(dict.end() == it);
		assert_true(de.empty());
	}
	{
		auto de = Deserializer();
		de << "d1:a1:a1:bl2:abd2:abli456eeee1:cd1:ade1:bdeee";
		auto dict = de.deserialize().value().get_dictionary();
		auto it = dict.begin();
		assert_eq("a", it->name);
		assert_eq("a", it->value.get_byte_string());
		++it;
		assert_eq("b", it->name);
		{
			auto& lst = it->value.get_list();
			auto lit = lst.begin();
			assert_eq("ab", lit++->get_byte_string());
			{
				auto& ddict = lit++->get_dictionary();
				auto dit = ddict.begin();
				assert_eq("ab", dit->name);
				{
					auto& llst = dit->value.get_list();
					auto llit = llst.begin();
					assert_eq(456, llit++->get_integer());
					assert_true(llst.end() == llit);
				}
				++dit;
				assert_true(ddict.end() == dit);
			}
			assert_true(lst.end() == lit);
		}
		++it;
		assert_eq("c", it->name);
		{
			auto& ddict = it->value.get_dictionary();
			auto dit = ddict.begin();
			assert_eq("a", dit->name);
			assert_true(dit->value.get_dictionary().empty());
			++dit;
			assert_eq("b", dit->name);
			assert_true(dit->value.get_dictionary().empty());
			++dit;
			assert_true(ddict.end() == dit);
		}
		++it;
		
		assert_true(dict.end() == it);
		assert_true(de.empty());
	}
}),

Test_case("dictionary incomplete", []
{
	{
		auto de = Deserializer();
		for (char c : std::string_view("de"))
		{
			assert_true(not de.deserialize().has_value());
			de << std::string_view(&c, 1);
		}
		assert_true(de.deserialize().value().get_dictionary().empty());
		assert_true(de.empty());
	}
	{
		auto de = Deserializer();
		for (char c : std::string_view("d1:ai1ee"))
		{
			assert_true(not de.deserialize().has_value());
			de << std::string_view(&c, 1);
		}
		auto dict = de.deserialize().value().get_dictionary();
		auto it = dict.begin();
		assert_eq("a", it->name);
		assert_eq(1, it->value.get_integer());
		++it;
		assert_true(dict.end() == it);
		assert_true(de.empty());
	}
	{
		auto de = Deserializer();
		for (char c : std::string_view("d1:ai1e1:bi1ee"))
		{
			assert_true(not de.deserialize().has_value());
			de << std::string_view(&c, 1);
		}
		auto dict = de.deserialize().value().get_dictionary();
		auto it = dict.begin();
		assert_eq("a", it->name);
		assert_eq(1, it->value.get_integer());
		++it;
		assert_eq("b", it->name);
		assert_eq(1, it->value.get_integer());
		++it;
		assert_true(dict.end() == it);
		assert_true(de.empty());
	}
	{
		auto de = Deserializer();
		for (char c : std::string_view("d1:a1:a1:bl2:abd2:abli456eeee1:cd1:ade1:bdeee"))
		{
			assert_true(not de.deserialize().has_value());
			de << std::string_view(&c, 1);
		}
		auto dict = de.deserialize().value().get_dictionary();
		auto it = dict.begin();
		assert_eq("a", it->name);
		assert_eq("a", it->value.get_byte_string());
		++it;
		assert_eq("b", it->name);
		{
			auto& lst = it->value.get_list();
			auto lit = lst.begin();
			assert_eq("ab", lit++->get_byte_string());
			{
				auto& ddict = lit++->get_dictionary();
				auto dit = ddict.begin();
				assert_eq("ab", dit->name);
				{
					auto& llst = dit->value.get_list();
					auto llit = llst.begin();
					assert_eq(456, llit++->get_integer());
					assert_true(llst.end() == llit);
				}
				++dit;
				assert_true(ddict.end() == dit);
			}
			assert_true(lst.end() == lit);
		}
		++it;
		assert_eq("c", it->name);
		{
			auto& ddict = it->value.get_dictionary();
			auto dit = ddict.begin();
			assert_eq("a", dit->name);
			assert_true(dit->value.get_dictionary().empty());
			++dit;
			assert_eq("b", dit->name);
			assert_true(dit->value.get_dictionary().empty());
			++dit;
			assert_true(ddict.end() == dit);
		}
		++it;
		
		assert_true(dict.end() == it);
		assert_true(de.empty());
	}
}),
};
