#pragma once

#include <span>
#include <optional>
#include <expected>
#include <filesystem>

struct Arguments : std::span<const char* const>
{
	std::optional<std::filesystem::path> unix_socket;
	std::optional<std::size_t> start_forwarded_args;
	
	Arguments(std::span<const char* const> value) noexcept : std::span<const char* const>(value)
	{
	}
	
	static bool key_arg_equals(std::string_view arg, std::initializer_list<std::string_view> names) noexcept
	{
		std::size_t pos_eq = 0;
		
		while (pos_eq != arg.size() and arg[pos_eq] != '=')
		{
			++pos_eq;
		}
		
		for (auto name : names)
		{
			if (name == arg.substr(0, pos_eq))
			{
				return true;
			}
		}
		
		return false;
	}
	
	std::expected<std::string_view, std::pair<int, std::string>> get_value(std::size_t& pos)
	{
		std::size_t pos_eq = 0;
		
		while ((*this)[pos][pos_eq] != '\0' and (*this)[pos][pos_eq] != '=')
		{
			++pos_eq;
		}
		
		if ((*this)[pos][pos_eq] == '=')
		{
			return std::string_view((*this)[pos] + pos_eq + 1);
		}
		else if (++pos, pos >= (*this).size())
		{
			return std::unexpected(std::pair(255, std::string("expected a value following argument ") + (*this)[pos - 1]));
		}
		else
		{
			return std::string_view((*this)[pos]);
		}
	}
	
	std::expected<void, std::pair<int, std::string>> parse()
	{
		for (std::size_t pos = 1; pos != (*this).size(); ++pos)
		{
			auto arg = std::string_view((*this)[pos]);
			
			if (key_arg_equals(arg, {"--unix-socket"}))
			{
				if (auto value = get_value(pos))
				{
					unix_socket.emplace(value.value());
					continue;
				}
				else
				{
					return std::unexpected(value.error());
				}
			}
			else if (arg == "--")
			{
				start_forwarded_args.emplace(pos);
				break;
			}
			else
			{
				return std::unexpected(std::pair(255, std::string("unrecognized option: ") += arg));
			}
		}
		
		return {};
	}
};
