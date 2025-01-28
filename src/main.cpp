#include <unistd.h>
#include <fcntl.h>
#include <sys/epoll.h>
#include <sys/socket.h>

#include <cerrno>
#include <algorithm>
#include <cstring>
#include <iostream>
#include <filesystem>
#include <experimental/scope>

#include <client.hpp>

void checked_close(int fd)
{
	if (close(fd))
	{
		std::clog << "failed to close file descriptor " << fd << ": " << std::strerror(errno) << "\n";
	}
}

std::expected<std::size_t, std::error_code> read_into(int fd, std::string& output)
{
	auto total_read_bytes = std::size_t(0);
	
	while (true)
	{
		auto prev_size = output.size();
		output.resize(std::max(prev_size + 64, output.capacity()));
		auto read_bytes = read(fd, output.data() + prev_size, output.size() - prev_size);
		
		if (read_bytes == -1)
		{
			if (errno == EWOULDBLOCK or errno == EAGAIN)
			{
				read_bytes = 0;
			}
			else
			{
				return std::unexpected(std::make_error_code(std::errc(errno)));
			}
		}
		
		output.resize(prev_size + read_bytes);
		
		total_read_bytes += read_bytes;
		
		if (read_bytes == 0)
		{
			break;
		}
	}
	
	return total_read_bytes;
}

extern char** environ;

int main([[maybe_unused]] int argc, [[maybe_unused]] const char* const* argv)
{
	auto args = Arguments(std::span(argv, argc));
	
	if (auto result = args.parse(); not result)
	{
		std::clog << "daiyousei: " << result.error().second << "\n";
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
	
	auto client = Client::create(args);
	if (not client)
	{
		std::clog << "daiyousei: " << client.error().second << "\n";
		return client.error().first;
	}
	
	if (auto serialized = bencode::serialize(message); not serialized)
	{
		std::clog << "daiyousei: " << serialized.error().message() << "\n";
		return 255;
	}
	else if (auto sent = client->send(serialized.value()); not sent)
	{
		std::clog << "daiyousei: " << sent.error().second << "\n";
		return sent.error().first;
	}
	
	if (auto result = client->run())
	{
		return result.value();
	}
	else
	{
		std::clog << "daiyousei: " << result.error().second << "\n";
		return result.error().first;
	}
}
