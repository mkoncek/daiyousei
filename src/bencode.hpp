#pragma once

#include <cstdint>
#include <cstring>

#include <algorithm>
#include <limits>
#include <charconv>
#include <system_error>
#include <string>
#include <string_view>
#include <span>
#include <variant>
#include <expected>
#include <vector>
#include <stdexcept>
#include <utility>
#include <typeinfo>

namespace bencode
{
struct Serializable;
using Integer = std::intmax_t;

// "i" + digits10 + round up + minus sign + "e"
constexpr std::size_t max_integer_length = 1 + std::numeric_limits<bencode::Integer>::digits10 + 1 + 1 + 1;

namespace serializable
{
struct Field;

struct Integer
{
	bencode::Integer value;
	
	constexpr Integer() noexcept = default;
	
	constexpr Integer(bencode::Integer value) noexcept : value(value) {}
	
	constexpr operator bencode::Integer() const noexcept
	{
		return value;
	}
};

struct Byte_string : std::string
{
	using std::string::string;
	
	Byte_string(std::string value) noexcept : std::string(std::move(value)) {}
};

struct List : std::vector<Serializable>
{
	using std::vector<Serializable>::vector;
	
	List(std::vector<Serializable> value) noexcept : std::vector<Serializable>(std::move(value)) {}
};

struct Dictionary : std::vector<Field>
{
	using std::vector<Field>::vector;
	
	Dictionary(std::vector<Field> value) noexcept : std::vector<Field>(std::move(value)) {}
};

struct Sorted_dictionary : private Dictionary
{
	Sorted_dictionary() noexcept = default;
	Sorted_dictionary(Dictionary value) noexcept;
	
	using Dictionary::begin;
	using Dictionary::end;
	using Dictionary::size;
	using Dictionary::data;
};

inline void sort_dictionary(std::span<Field> value) noexcept;
} // namespace serializable

inline void serialize(std::string& output, serializable::Integer value);
inline void serialize(std::string& output, const serializable::Byte_string& value);
inline void serialize(std::string& output, const serializable::List& value);
inline void serialize(std::string& output, const serializable::Sorted_dictionary& value);

struct Serializable : std::variant<serializable::Integer, serializable::Byte_string, serializable::List, serializable::Sorted_dictionary>
{
	using Base = std::variant<serializable::Integer, serializable::Byte_string, serializable::List, serializable::Sorted_dictionary>;
	
	Serializable(serializable::Integer value) noexcept : Base(value)
	{
	}
	
	Serializable(serializable::Byte_string value) noexcept : Base(std::move(value))
	{
	}
	
	Serializable(serializable::List value) noexcept : Base(std::move(value))
	{
	}
	
	Serializable(serializable::Sorted_dictionary value) noexcept : Base(std::move(value))
	{
	}
	
	void serialize(std::string& output) const
	{
		std::visit([&output](const auto& value) noexcept -> void
		{
			bencode::serialize(output, value);
		}, *this);
	}
	
	std::string serialize() const noexcept
	{
		auto result = std::string();
		result.reserve(32);
		serialize(result);
		return result;
	}
};

struct serializable::Field
{
	Byte_string name;
	Serializable value;
};

void serializable::sort_dictionary(std::span<Field> value) noexcept
{
	std::ranges::sort(value, [](auto lhs, auto rhs) noexcept -> bool
	{
		return lhs < rhs;
	}, [](const auto& value) noexcept -> std::string_view {return value.name;});
}

serializable::Sorted_dictionary::Sorted_dictionary(Dictionary value) noexcept : Dictionary(std::move(value))
{
	sort_dictionary(static_cast<Dictionary&>(*this));
}

void serialize(std::string& output, serializable::Integer value)
{
	auto prev_size = output.size();
	output.resize(prev_size + bencode::max_integer_length);
	output[prev_size] = 'i';
	++prev_size;
	auto [end, error] = std::to_chars(
		output.data() + prev_size,
		output.data() + output.size(),
		value.value
	);
	if (auto ec = std::make_error_code(error))
	{
		throw std::logic_error(ec.message());
	}
	*end = 'e';
	++end;
	output.resize(end - output.data());
}

inline void serialize(std::string& output, std::string_view value)
{
	auto prev_size = output.size();
	// digits10 + round up + ':' + string
	output.resize(prev_size + std::numeric_limits<std::size_t>::digits10 + 1 + 1 + value.size());
	auto [end, error] = std::to_chars(
		output.data() + prev_size,
		output.data() + output.size(),
		value.size()
	);
	if (auto ec = std::make_error_code(error))
	{
		throw std::logic_error(ec.message());
	}
	*end = ':';
	++end;
	auto [in, out] = std::ranges::copy(value, end);
	output.resize(out - output.data());
}

inline void serialize(std::string& output, const char* value)
{
	return serialize(output, std::string_view(value));
}

void serialize(std::string& output, const serializable::Byte_string& value)
{
	return serialize(output, std::string_view(value));
}

void serialize(std::string& output, const serializable::List& value)
{
	output.push_back('l');
	for (const auto& inner : value)
	{
		inner.serialize(output);
	}
	output.push_back('e');
}

void serialize(std::string& output, const serializable::Sorted_dictionary& value)
{
	output.push_back('d');
	for (const auto& [key, inner] : value)
	{
		serialize(output, key);
		inner.serialize(output);
	}
	output.push_back('e');
}

template<typename Type>
inline std::string serialize(const Type& value) noexcept
{
	auto result = std::string();
	result.reserve(32);
	serialize(result, value);
	return result;
}

struct Serializer : std::reference_wrapper<std::string>
{
	struct Serializer_buffer;
	
	inline static Serializer_buffer create() noexcept;
	
	void push_raw_data(std::string_view data)
	{
		get() += data;
	}
	
	void push_integer(bencode::Integer value)
	{
		return serialize(get(), value);
	}
	
	void push_byte_string(std::string_view value)
	{
		return serialize(get(), value);
	}
	
	void emplace_byte_string(auto&&... args)
	{
		return push_byte_string(std::string_view(std::forward<decltype(args)>(args)...));
	}
	
	struct List_serializer;
	
	inline List_serializer push_list();
	
	struct Dictionary_serializer;
	
	inline Dictionary_serializer push_dictionary();
	
	struct Take : std::reference_wrapper<std::string>
	{
		~Take()
		{
			get().clear();
		}
		
		operator std::string_view() noexcept
		{
			return std::string_view(get());
		}
	};
	
	Take take() noexcept
	{
		return Take(*this);
	}
};

struct Serializer::Serializer_buffer : Serializer
{
	std::string buffer_;
	
	Serializer_buffer() noexcept : Serializer(buffer_)
	{
	}
	
	Serializer_buffer(Serializer_buffer&&) : Serializer(buffer_)
	{
	}
};

Serializer::Serializer_buffer Serializer::create() noexcept
{
	return Serializer::Serializer_buffer();
}

struct Serializer::List_serializer : Serializer
{
	~List_serializer()
	{
		get().push_back('e');
	}
	
	List_serializer(Serializer parent) : Serializer(parent)
	{
		get().push_back('l');
	}
};

Serializer::List_serializer Serializer::push_list()
{
	return List_serializer(*this);
}

struct Serializer::Dictionary_serializer : Serializer
{
	~Dictionary_serializer()
	{
		get().push_back('e');
	}
	
	Dictionary_serializer(Serializer parent) : Serializer(parent)
	{
		get().push_back('d');
	}
};

Serializer::Dictionary_serializer Serializer::push_dictionary()
{
	return Dictionary_serializer(*this);
}

enum struct Deserialization_error
{
	unknown_type,
	wrong_type,
	incomplete_message,
	invalid_value,
	dictionary_unsorted,
	dictionary_duplicate_key,
};

struct Deserialized;

namespace deserialized
{
struct Field;
struct Integer;
struct Byte_string;
struct List;
struct Dictionary;;

struct Deserializable
{
	virtual ~Deserializable() = default;
	virtual bool is_complete() const noexcept {return false;}
	
	virtual std::expected<void, Deserialization_error> append(std::vector<Deserializable*>& stack, deserialized::Integer value);
	virtual std::expected<void, Deserialization_error> append(std::vector<Deserializable*>& stack, deserialized::Byte_string value);
	virtual std::expected<void, Deserialization_error> append(std::vector<Deserializable*>& stack, deserialized::List value);
	virtual std::expected<void, Deserialization_error> append(std::vector<Deserializable*>& stack, deserialized::Dictionary value);
	virtual std::expected<void, Deserialization_error> append(std::vector<Deserializable*>& stack, std::string_view value);
};

struct Integer : Deserializable
{
	bencode::Integer value;
	
	constexpr Integer() noexcept = default;
	
	constexpr Integer(bencode::Integer value) noexcept : value(value) {}
	
	constexpr operator bencode::Integer() const noexcept
	{
		return value;
	}
	
	bool is_complete() const noexcept final override {return true;}
};

struct Byte_string : Deserializable, std::string
{
	std::size_t complete_size_ = 0;
	
	operator std::string_view() const noexcept
	{
		return std::string_view(static_cast<const std::string&>(*this));
	}
	
	bool is_complete() const noexcept final override {return complete_size_ == this->size();}
	
	std::size_t remaining() const noexcept
	{
		return size() - complete_size_;
	}
	
	std::expected<void, Deserialization_error> append(std::vector<Deserializable*>& stack, std::string_view value) final override;
};

struct List : Deserializable, std::vector<bencode::Deserialized>
{
	bool is_complete_ = false;
	
	bool is_complete() const noexcept final override {return is_complete_;}
	
	std::expected<void, Deserialization_error> append(std::vector<Deserializable*>& stack, deserialized::Integer value) final override;
	std::expected<void, Deserialization_error> append(std::vector<Deserializable*>& stack, deserialized::Byte_string value) final override;
	std::expected<void, Deserialization_error> append(std::vector<Deserializable*>& stack, deserialized::List value) final override;
	std::expected<void, Deserialization_error> append(std::vector<Deserializable*>& stack, deserialized::Dictionary value) final override;
};

struct Dictionary : Deserializable, std::vector<Field>
{
	bool is_complete_ = false;
	
	bool is_complete() const noexcept final override {return is_complete_;}
	
	std::expected<void, Deserialization_error> append(std::vector<Deserializable*>& stack, deserialized::Byte_string value) final override;
};

std::expected<void, Deserialization_error> Deserializable::append(
	[[maybe_unused]] std::vector<Deserializable*>& stack, [[maybe_unused]] deserialized::Integer value)
{
	return std::unexpected(Deserialization_error::wrong_type);
}

std::expected<void, Deserialization_error> Deserializable::append(
	[[maybe_unused]] std::vector<Deserializable*>& stack, [[maybe_unused]] deserialized::Byte_string value)
{
	return std::unexpected(Deserialization_error::wrong_type);
}

std::expected<void, Deserialization_error> Deserializable::append(
	[[maybe_unused]] std::vector<Deserializable*>& stack, [[maybe_unused]] deserialized::List value)
{
	return std::unexpected(Deserialization_error::wrong_type);
}

std::expected<void, Deserialization_error> Deserializable::append(
	[[maybe_unused]] std::vector<Deserializable*>& stack, [[maybe_unused]] deserialized::Dictionary value)
{
	return std::unexpected(Deserialization_error::wrong_type);
}

std::expected<void, Deserialization_error> Deserializable::append(
	[[maybe_unused]] std::vector<Deserializable*>& stack, [[maybe_unused]] std::string_view value)
{
	return std::unexpected(Deserialization_error::wrong_type);
}
} // namespace deserialized

struct Deserialized : std::variant<deserialized::Deserializable, deserialized::Integer, deserialized::Byte_string, deserialized::List, deserialized::Dictionary>
{
	bool is_complete() const noexcept
	{
		return std::visit([]<typename Type>(Type& self) noexcept -> bool
		{
			return self.is_complete();
		}, *this);
	}
	
	deserialized::Integer& get_integer() &
	{
		if (auto* self = std::get_if<deserialized::Integer>(this))
		{
			return *self;
		}
		throw std::bad_variant_access();
	}
	
	const deserialized::Integer& get_integer() const&
	{
		if (auto* self = std::get_if<deserialized::Integer>(this))
		{
			return *self;
		}
		throw std::bad_variant_access();
	}
	
	deserialized::Integer&& get_integer() &&
	{
		if (auto* self = std::get_if<deserialized::Integer>(this))
		{
			return std::move(*self);
		}
		throw std::bad_variant_access();
	}
	
	deserialized::Byte_string& get_byte_string() &
	{
		if (auto* self = std::get_if<deserialized::Byte_string>(this))
		{
			return *self;
		}
		throw std::bad_variant_access();
	}
	
	const deserialized::Byte_string& get_byte_string() const&
	{
		if (auto* self = std::get_if<deserialized::Byte_string>(this))
		{
			return *self;
		}
		throw std::bad_variant_access();
	}
	
	deserialized::Byte_string&& get_byte_string() &&
	{
		if (auto* self = std::get_if<deserialized::Byte_string>(this))
		{
			return std::move(*self);
		}
		throw std::bad_variant_access();
	}
	
	deserialized::List& get_list() &
	{
		if (auto* self = std::get_if<deserialized::List>(this))
		{
			return *self;
		}
		throw std::bad_variant_access();
	}
	
	const deserialized::List& get_list() const&
	{
		if (auto* self = std::get_if<deserialized::List>(this))
		{
			return *self;
		}
		throw std::bad_variant_access();
	}
	
	deserialized::List&& get_list() &&
	{
		if (auto* self = std::get_if<deserialized::List>(this))
		{
			return std::move(*self);
		}
		throw std::bad_variant_access();
	}
	
	deserialized::Dictionary& get_dictionary() &
	{
		if (auto* self = std::get_if<deserialized::Dictionary>(this))
		{
			return *self;
		}
		throw std::bad_variant_access();
	}
	
	const deserialized::Dictionary& get_dictionary() const&
	{
		if (auto* self = std::get_if<deserialized::Dictionary>(this))
		{
			return *self;
		}
		throw std::bad_variant_access();
	}
	
	deserialized::Dictionary&& get_dictionary() &&
	{
		if (auto* self = std::get_if<deserialized::Dictionary>(this))
		{
			return std::move(*self);
		}
		throw std::bad_variant_access();
	}
};

struct deserialized::Field : Deserializable
{
	Byte_string name;
	Deserialized value;
	
	bool is_complete() const noexcept final override
	{
		return name.is_complete() and value.is_complete();
	}
	
	std::expected<void, Deserialization_error> append(std::vector<Deserializable*>& stack, deserialized::Integer value) final override;
	std::expected<void, Deserialization_error> append(std::vector<Deserializable*>& stack, deserialized::Byte_string value) final override;
	std::expected<void, Deserialization_error> append(std::vector<Deserializable*>& stack, deserialized::List value) final override;
	std::expected<void, Deserialization_error> append(std::vector<Deserializable*>& stack, deserialized::Dictionary value) final override;
};

std::expected<void, Deserialization_error> deserialized::Byte_string::append(
	[[maybe_unused]] std::vector<Deserializable*>& stack, std::string_view value)
{
	if (value.size() <= size() - complete_size_)
	{
		std::ranges::copy(value, data() + complete_size_);
		complete_size_ += value.size();
		return {};
	}
	
	return std::unexpected(Deserialization_error::wrong_type);
}

std::expected<void, Deserialization_error> deserialized::List::append(std::vector<Deserializable*>& stack, deserialized::Integer value)
{
	emplace_back();
	bool is_complete = value.is_complete();
	if (auto* ptr = &back().emplace<decltype(value)>(std::move(value)); not is_complete)
	{
		stack.push_back(ptr);
	}
	return {};
}

std::expected<void, Deserialization_error> deserialized::List::append(std::vector<Deserializable*>& stack, deserialized::Byte_string value)
{
	emplace_back();
	bool is_complete = value.is_complete();
	if (auto* ptr = &back().emplace<decltype(value)>(std::move(value)); not is_complete)
	{
		stack.push_back(ptr);
	}
	return {};
}

std::expected<void, Deserialization_error> deserialized::List::append(std::vector<Deserializable*>& stack, deserialized::List value)
{
	emplace_back();
	bool is_complete = value.is_complete();
	if (auto* ptr = &back().emplace<decltype(value)>(std::move(value)); not is_complete)
	{
		stack.push_back(ptr);
	}
	return {};
}

std::expected<void, Deserialization_error> deserialized::List::append(std::vector<Deserializable*>& stack, deserialized::Dictionary value)
{
	emplace_back();
	bool is_complete = value.is_complete();
	if (auto* ptr = &back().emplace<decltype(value)>(std::move(value)); not is_complete)
	{
		stack.push_back(ptr);
	}
	return {};
}

std::expected<void, Deserialization_error> deserialized::Dictionary::append(std::vector<Deserializable*>& stack, deserialized::Byte_string value)
{
	if (empty() or back().is_complete())
	{
		emplace_back();
		bool is_complete = value.is_complete();
		back().name = std::move(value);
		stack.push_back(&back());
		if (not is_complete)
		{
			stack.push_back(&back().name);
		}
		return {};
	}
	
	return std::unexpected(Deserialization_error::wrong_type);
}

std::expected<void, Deserialization_error> deserialized::Field::append(std::vector<Deserializable*>& stack, deserialized::Integer value)
{
	if (name.is_complete() and std::holds_alternative<deserialized::Deserializable>(this->value))
	{
		bool is_complete = value.is_complete();
		if (auto* ptr = &this->value.emplace<decltype(value)>(std::move(value)); not is_complete)
		{
			stack.push_back(ptr);
		}
		return {};
	}
	
	return std::unexpected(Deserialization_error::wrong_type);
}

std::expected<void, Deserialization_error> deserialized::Field::append(std::vector<Deserializable*>& stack, deserialized::Byte_string value)
{
	if (name.is_complete() and std::holds_alternative<deserialized::Deserializable>(this->value))
	{
		bool is_complete = value.is_complete();
		if (auto* ptr = &this->value.emplace<decltype(value)>(std::move(value)); not is_complete)
		{
			stack.push_back(ptr);
		}
		return {};
	}
	
	return std::unexpected(Deserialization_error::wrong_type);
}

std::expected<void, Deserialization_error> deserialized::Field::append(std::vector<Deserializable*>& stack, deserialized::List value)
{
	if (name.is_complete() and std::holds_alternative<deserialized::Deserializable>(this->value))
	{
		bool is_complete = value.is_complete();
		if (auto* ptr = &this->value.emplace<decltype(value)>(std::move(value)); not is_complete)
		{
			stack.push_back(ptr);
		}
		return {};
	}
	
	return std::unexpected(Deserialization_error::wrong_type);
}

std::expected<void, Deserialization_error> deserialized::Field::append(std::vector<Deserializable*>& stack, deserialized::Dictionary value)
{
	if (name.is_complete() and std::holds_alternative<deserialized::Deserializable>(this->value))
	{
		bool is_complete = value.is_complete();
		if (auto* ptr = &this->value.emplace<decltype(value)>(std::move(value)); not is_complete)
		{
			stack.push_back(ptr);
		}
		return {};
	}
	
	return std::unexpected(Deserialization_error::wrong_type);
}

struct Deserializer
{
	std::string data_;
	Deserialized in_progress_;
	std::vector<deserialized::Deserializable*> stack_;
	
	bool empty() const noexcept
	{
		return data_.empty();
	}
	
	Deserializer& operator<<(std::string_view data)
	{
		data_ += data;
		return *this;
	}
	
	void consume(std::size_t size)
	{
		std::copy(data_.begin() + size, data_.end(), data_.begin());
		data_.resize(data_.size() - size);
	}
	
	template<typename Type>
	std::expected<void, Deserialization_error> consume_value(Type value)
	{
		bool is_complete = value.is_complete();
		if (not stack_.empty())
		{
			if (auto result = stack_.back()->append(stack_, std::move(value)); not result)
			{
				return std::unexpected(result.error());
			}
		}
		else
		{
			in_progress_.emplace<Type>(std::move(value));
			
			if (not is_complete)
			{
				auto* ptr = std::visit([](auto& self) -> deserialized::Deserializable*
				{
					return &self;
				}, in_progress_);
				stack_.push_back(ptr);
			}
		}
		
		return {};
	}
	
	std::expected<Deserialized, Deserialization_error> deserialize()
	{
		while (true)
		{
			while (not stack_.empty() and stack_.back()->is_complete())
			{
				if (stack_.size() >= 3)
				{
					if (typeid(*stack_[stack_.size() - 1]) == typeid(deserialized::Byte_string&)
						and typeid(*stack_[stack_.size() - 2]) == typeid(deserialized::Field&))
					{
						auto& dict = static_cast<deserialized::Dictionary&>(*stack_[stack_.size() - 3]);
						if (dict.size() >= 2)
						{
							auto cmp = dict[dict.size() - 2].name <=> dict[dict.size() - 1].name;
							if (cmp == std::partial_ordering::equivalent)
							{
								return std::unexpected(Deserialization_error::dictionary_duplicate_key);
							}
							else if (cmp == std::partial_ordering::greater)
							{
								return std::unexpected(Deserialization_error::dictionary_unsorted);
							}
						}
					}
				}
				stack_.pop_back();
			}
			
			if (not std::holds_alternative<deserialized::Deserializable>(in_progress_) and stack_.empty())
			{
				return std::exchange(in_progress_, Deserialized());
			}
			
			if (data_.empty())
			{
				return std::unexpected(Deserialization_error::incomplete_message);
			}
			
			if (not stack_.empty())
			{
				auto& ref_type = typeid(*stack_.back());
				if (ref_type == typeid(deserialized::List&) or ref_type == typeid(deserialized::Dictionary&))
				{
					if (data_[0] == 'e')
					{
						if (ref_type == typeid(deserialized::List&))
						{
							static_cast<deserialized::List*>(stack_.back())->is_complete_ = true;
						}
						else if (ref_type == typeid(deserialized::Dictionary&))
						{
							static_cast<deserialized::Dictionary*>(stack_.back())->is_complete_ = true;
						}
						stack_.pop_back();
						consume(1);
						continue;
					}
				}
				else if (ref_type == typeid(deserialized::Byte_string&))
				{
					auto* string = static_cast<deserialized::Byte_string*>(stack_.back());
					auto len = std::min(string->remaining(), data_.size());
					if (auto result = string->append(stack_, std::string_view(data_.data(), len)); not result)
					{
						return std::unexpected(result.error());
					}
					consume(len);
					continue;
				}
			}
			
			if (data_[0] == 'i')
			{
				auto end = data_.find_first_of('e');
				if (end == std::string::npos)
				{
					return std::unexpected(Deserialization_error::incomplete_message);
				}
				auto integer = deserialized::Integer();
				auto [ptr, error] = std::from_chars(data_.data() + 1, data_.data() + end, integer.value);
				if (std::make_error_code(error))
				{
					return std::unexpected(Deserialization_error::invalid_value);
				}
				consume(end + 1);
				if (auto result = consume_value(integer); not result)
				{
					return std::unexpected(result.error());
				}
			}
			else if (std::isdigit(data_[0]))
			{
				auto end = data_.find_first_of(':');
				if (end == std::string::npos)
				{
					return std::unexpected(Deserialization_error::incomplete_message);
				}
				std::ptrdiff_t len;
				auto [ptr, error] = std::from_chars(data_.data(), data_.data() + end, len);
				if (std::make_error_code(error))
				{
					return std::unexpected(Deserialization_error::invalid_value);
				}
				auto byte_string = deserialized::Byte_string();
				byte_string.resize(len);
				if (len < 0)
				{
					return std::unexpected(Deserialization_error::invalid_value);
				}
				if (ptr != data_.data() + end)
				{
					return std::unexpected(Deserialization_error::invalid_value);
				}
				++end;
				consume(end);
				if (auto result = consume_value(std::move(byte_string)); not result)
				{
					return std::unexpected(result.error());
				}
			}
			else if (data_[0] == 'l')
			{
				consume(1);
				if (auto result = consume_value(deserialized::List()); not result)
				{
					return std::unexpected(result.error());
				}
			}
			else if (data_[0] == 'd')
			{
				consume(1);
				if (auto result = consume_value(deserialized::Dictionary()); not result)
				{
					return std::unexpected(result.error());
				}
			}
			else
			{
				return std::unexpected(Deserialization_error::unknown_type);
			}
		}
	}
};

struct Deserialization_exception : std::runtime_error
{
	using std::runtime_error::runtime_error;
};

struct Streaming_deserializer
{
protected:
	std::string data_;
private:
	std::optional<std::size_t> expected_byte_string_length_;
	std::string stack_;
	
	void consume(std::size_t size)
	{
		std::copy(data_.begin() + size, data_.end(), data_.begin());
		data_.resize(data_.size() - size);
	}
	
	virtual void visit_integer([[maybe_unused]] bencode::Integer value) {}
	
	virtual void visit_byte_string([[maybe_unused]] std::string_view value) {}
	
	virtual void visit_list_begin() {}
	virtual void visit_list_end() {}
	
	virtual void visit_dictionary_begin() {}
	virtual void visit_dictionary_end() {}
	
	void pre_visit_integer(bencode::Integer value)
	{
		try_finish_dictionary_value();
		visit_integer(value);
	}
	
	void pre_visit_byte_string(std::string_view value)
	{
		if (auto entry = Stack_entry::from(stack_))
		{
			if (entry->type_ == Stack_entry::Type::list)
			{
			}
			else if (entry->expecting_key_)
			{
				if (value.size() >= Stack_entry::npos)
				{
					throw Deserialization_exception(std::string("dictionary key too long"));
				}
				
				if (entry->length_ == Stack_entry::npos)
				{
					stack_ += value;
					stack_ += Stack_entry::new_dictionary_entry(value);
				}
				else
				{
					auto prev_key = std::string_view(stack_.data() + stack_.size() - sizeof(Stack_entry) - entry->length_, entry->length_);
					auto cmp = prev_key <=> value;
					if (cmp == std::strong_ordering::greater)
					{
						throw Deserialization_exception(std::string("unsorted dictionary: '") + std::string(prev_key) +  "' is greater than '" + std::string(value) + "'");
					}
					else if (cmp == std::strong_ordering::equal)
					{
						throw Deserialization_exception(std::string("duplicate key: '") + std::string(value) + "'");
					}
					
					stack_.resize(stack_.size() - sizeof(Stack_entry) - prev_key.size());
					stack_ += value;
					stack_ += Stack_entry::new_dictionary_entry(value);
				}
			}
			else
			{
				finish_dictionary_value(*entry);
			}
		}
		
		visit_byte_string(value);
	}
	
	void pre_visit_list_end()
	{
		try_finish_dictionary_value();
		visit_list_end();
	}
	
	void pre_visit_dictionary_end()
	{
		try_finish_dictionary_value();
		visit_dictionary_end();
	}
	
	struct Stack_entry
	{
		enum struct Type : std::uint8_t
		{
			list,
			dictionary,
		}
		type_ : 1;
		bool expecting_key_ : 1;
		/// npos if no key has yet been encountered
		std::uint16_t length_ : 14;
		
		static Stack_entry new_list_entry() noexcept
		{
			return Stack_entry {.type_ = Type::list, .expecting_key_ = false, .length_ = 0};
		}
		
		static Stack_entry new_dictionary_entry() noexcept
		{
			return Stack_entry {.type_ = Type::dictionary, .expecting_key_ = true, .length_ = npos};
		}
		
		static Stack_entry new_dictionary_entry(std::string_view key) noexcept
		{
			return Stack_entry {.type_ = Type::dictionary, .expecting_key_ = false, .length_ = std::uint16_t(key.size())};
		}
		
		constexpr static std::uint16_t npos = std::numeric_limits<std::uint16_t>::max() >> 2;
		
		operator std::string_view() const noexcept
		{
			return std::string_view(reinterpret_cast<const char*>(this), sizeof(Stack_entry));
		}
		
		static std::optional<Stack_entry> from(std::string_view stack) noexcept
		{
			if (stack.size() >= sizeof(Stack_entry))
			{
				auto result = Stack_entry();
				std::memcpy(&result, stack.data() + stack.size() - sizeof(Stack_entry), sizeof(Stack_entry));
				return result;
			}
			
			return {};
		}
	};
	
	void finish_dictionary_value(Stack_entry entry)
	{
		entry.expecting_key_ = true;
		stack_.resize(stack_.size() - sizeof(Stack_entry));
		stack_ += entry;
	}
	
	void try_finish_dictionary_value()
	{
		if (auto entry = Stack_entry::from(stack_))
		{
			if (entry->type_ == Stack_entry::Type::dictionary and entry->expecting_key_ == false)
			{
				finish_dictionary_value(*entry);
			}
		}
	}
	
public:
	virtual ~Streaming_deserializer() = default;
	
	bool finished() const noexcept
	{
		return data_.empty() and stack_.empty() and not expected_byte_string_length_.has_value();
	}
	
	void receive(std::string_view data)
	{
		data_.append(data);
		receive();
	}
	
	void receive()
	{
		while (true)
		{
			if (data_.empty())
			{
				break;
			}
			
			if (expected_byte_string_length_)
			{
				if (data_.size() >= *expected_byte_string_length_)
				{
					pre_visit_byte_string(std::string_view(data_.data(), *expected_byte_string_length_));
					consume(*expected_byte_string_length_);
					expected_byte_string_length_.reset();
					continue;
				}
				else
				{
					// wait for more data
					break;
				}
			}
			
			if (data_[0] == 'i')
			{
				if (auto entry = Stack_entry::from(stack_))
				{
					if (entry->expecting_key_)
					{
						throw Deserialization_exception(std::string("expected a dictionary key, found integer"));
					}
				}
				auto end = std::string::npos;
				for (std::size_t i = 1; i != data_.size(); ++i)
				{
					if (data_[i] == 'e')
					{
						end = i;
						break;
					}
					if (i == bencode::max_integer_length)
					{
						throw Deserialization_exception(std::string("integer too long"));
					}
				}
				if (end == std::string::npos)
				{
					// wait for more data
					break;
				}
				auto integer = bencode::Integer();
				auto [ptr, error] = std::from_chars(data_.data() + 1, data_.data() + end, integer);
				++end;
				if (auto ec = std::make_error_code(error))
				{
					throw Deserialization_exception(std::string("invalid integer value: '") + data_.substr(0, end) + "':" + ec.message());
				}
				consume(end);
				pre_visit_integer(integer);
			}
			else if (std::isdigit(data_[0]))
			{
				auto end = data_.find_first_of(':');
				if (end == std::string::npos)
				{
					// wait for more data
					break;
				}
				std::ptrdiff_t len;
				auto [ptr, error] = std::from_chars(data_.data(), data_.data() + end, len);
				++end;
				if (auto ec = std::make_error_code(error))
				{
					throw Deserialization_exception(std::string("invalid byte string length: '") + data_.substr(0, end) + "':" + ec.message());
				}
				if (len < 0)
				{
					throw Deserialization_exception(std::string("negative byte string length: '") + data_.substr(0, end) + "'");
				}
				if (len > 1024 * 1024)
				{
					throw Deserialization_exception(std::string("byte string length too large: '") + data_.substr(0, end) + "'");
				}
				if (data_.size() >= end + len)
				{
					pre_visit_byte_string(std::string_view(data_.data() + end, len));
					end += len;
				}
				else
				{
					expected_byte_string_length_.emplace(len);
				}
				consume(end);
			}
			else if (data_[0] == 'l')
			{
				consume(1);
				stack_ += Stack_entry::new_list_entry();
				visit_list_begin();
			}
			else if (data_[0] == 'd')
			{
				consume(1);
				stack_ += Stack_entry::new_dictionary_entry();
				visit_dictionary_begin();
			}
			else if (data_[0] == 'e')
			{
				consume(1);
				if (auto entry = Stack_entry::from(stack_))
				{
					stack_.resize(stack_.size() - sizeof(Stack_entry));
					if (entry->type_ == Stack_entry::Type::list)
					{
						pre_visit_list_end();
					}
					else if (entry->type_ == Stack_entry::Type::dictionary)
					{
						if (entry->length_ != Stack_entry::npos)
						{
							stack_.resize(stack_.size() - entry->length_ - sizeof(Stack_entry));
						}
						pre_visit_dictionary_end();
					}
				}
				else
				{
					throw Deserialization_exception(std::string("found end of collection 'e' with no previous beginning"));
				}
			}
			else
			{
				throw Deserialization_exception(std::string("unknown value type: '") + data_ + "'");
			}
		}
	}
};
} // namespace bencode
