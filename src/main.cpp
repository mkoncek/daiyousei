#include <unistd.h>
#include <fcntl.h>
#include <sys/epoll.h>

#include <cerrno>
#include <algorithm>
#include <cstring>
#include <iostream>
#include <filesystem>
#include <experimental/scope>

#include <bencode.hpp>

void checked_close(int fd)
{
	if (close(fd))
	{
		std::clog << "failed to close file descriptor " << fd << ": " << std::strerror(errno) << "\n";
	}
}

std::error_code read_into(int fd, std::string& output)
{
	while (true)
	{
		auto prev_size = output.size();
		output.resize(prev_size + 64);
		auto read_bytes = read(fd, output.data() + prev_size, output.size() - prev_size);
		
		if (read_bytes == -1)
		{
			return std::make_error_code(std::errc(errno));
		}
		
		output.resize(prev_size + read_bytes);
		
		if (read_bytes == 0)
		{
			break;
		}
	}
	
	return {};
}

extern char** environ;

struct Arguments : std::span<const char* const>
{
	std::optional<std::filesystem::path> unix_socket;
	std::optional<std::size_t> start_forwarded_args;
	
	std::expected<std::string_view, std::pair<int, std::string>> get_value(std::size_t& pos)
	{
		std::size_t pos_eq = 0;
		
		while ((*this)[pos][pos_eq] != '\n' and (*this)[pos][pos_eq] != '=')
		{
			++pos_eq;
		}
		
		if ((*this)[pos][pos_eq] == '=')
		{
			return std::string_view((*this)[pos] + pos_eq + 1);
		}
		else if (++pos, pos >= (*this).size())
		{
			return std::unexpected(std::pair(255, "expected a value"));
		}
		else
		{
			return std::string_view((*this)[pos]);
		}
	}
	
	std::expected<void, std::pair<int, std::string>> parse()
	{
		std::size_t pos = 1;
		while (pos != (*this).size())
		{
			auto arg = std::string_view((*this)[pos]);
			
			if (arg.starts_with("--unix-socket"))
			{
				if (auto value = get_value(pos))
				{
					unix_socket.emplace(value.value());
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
				return std::unexpected(std::pair(255, "unrecognized option"));
			}
		}
		
		return {};
	}
};

int main([[maybe_unused]] int argc, [[maybe_unused]] const char* const* argv)
{
	auto args = Arguments(std::span(argv, argc));
	
	if (auto result = args.parse(); not result)
	{
		std::clog << result.error().second << "\n";
		return result.error().first;
	}
	
	auto message = bencode::serializable::List();
	
	{
		message.emplace_back("argv");
		auto argv_list = bencode::serializable::List();
		auto start_forwarded_args = args.start_forwarded_args.value_or(args.size());
		argv_list.reserve(1 + args.size() - start_forwarded_args);
		std::cout << argc << "\n";
		argv_list.emplace_back(bencode::serializable::Byte_string(args[0]));
		for (std::size_t i = start_forwarded_args; i != args.size(); ++i)
		{
			std::cout << argv[i] << "\n";
			argv_list.emplace_back(bencode::serializable::Byte_string(argv[i]));
		}
		message.emplace_back(bencode::serializable::List(std::move(argv_list)));
	}
	
	message.emplace_back("cwd");
	message.emplace_back(bencode::serializable::Byte_string(std::filesystem::current_path().string()));
	
	{
		message.emplace_back("env");
		auto env_dict = bencode::serializable::Dictionary();
		
		for (const char* const* env = environ; *env != nullptr; ++env)
		{
			std::size_t mid= 0;
			while ((*env)[mid] != '=')
			{
				++mid;
			}
			std::size_t end = mid + 1;
			while ((*env)[end] != '\0')
			{
				++end;
			}
			
			env_dict.emplace_back(std::string(*env, mid), bencode::serializable::Byte_string(*env + mid + 1, *env + end));
		}
		
		message.emplace_back(bencode::serializable::Sorted_dictionary(std::move(env_dict)));
	}
	
	std::cout << bencode::serialize(message).value() << "\n";
	
	auto epoll_fd = std::experimental::make_unique_resource_checked(epoll_create1(0), -1, &checked_close);
	
	if (epoll_fd.get() == -1)
	{
		std::clog << "failed to create epoll file descriptor: " << std::strerror(errno) << "\n";
		return 1;
	}
	
	{
		auto event = epoll_event();
		event.events = EPOLLIN;
		event.data.fd = 0;
		
		if (epoll_ctl(epoll_fd.get(), EPOLL_CTL_ADD, 0, &event))
		{
			std::clog << "failed to add file descriptor to epoll: " << std::strerror(errno) << "\n";
			return 1;
		}
	}
	
	auto events = std::array<epoll_event, 8>();
	
	fcntl(0, F_SETFL, O_NONBLOCK);
	
	while (true)
	{
		auto ready_events = epoll_wait(epoll_fd.get(), events.data(), events.size(), -1);
		for (int i = 0; i != ready_events; ++i)
		{
			std::cout << "## " << events[i].events << "\n";
			std::cout << i << "\n";
			char buf[512];
			auto read_bytes = read(events[i].data.fd, &buf, std::size(buf));
			std::cout << read_bytes << "\n";
			if (read_bytes == -1)
			{
				std::clog << "read " << events[i].data.fd << " failed: " << std::strerror(errno) << "\n";
				return 1;
			}
			else if (read_bytes == 0)
			{
				std::clog << "input stream " << events[i].data.fd << " closed"  << "\n";
				return 0;
			}
		}
	}
}
