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

extern char** environ;

constexpr std::string_view program_name = "daiyousei";

int main([[maybe_unused]] int argc, [[maybe_unused]] const char* const* argv)
{
	auto args = Arguments(std::span(argv, argc));
	
	if (auto result = args.parse(); not result)
	{
		std::clog << program_name << ": " << result.error().second << "\n";
		return result.error().first;
	}
	
	auto serializer = bencode::Serializer::create();
	auto message_serializer = serializer.push_list();
	
	{
		message_serializer.push_byte_string("argv");
		auto argv_serializer = message_serializer.push_list();
		auto start_forwarded_args = args.start_forwarded_args.value_or(args.size());
		for (std::size_t i = start_forwarded_args; i != args.size(); ++i)
		{
			argv_serializer.push_byte_string(args[i]);
		}
	}
	
	message_serializer.push_byte_string("cwd");
	message_serializer.push_byte_string(bencode::serializable::Byte_string(std::filesystem::current_path().string()));
	
	{
		message_serializer.push_byte_string("env");
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
		
		bencode::serialize(serializer.get(), bencode::serializable::Sorted_dictionary(std::move(env_dict)));
	}
	
	auto client = Client::create(args);
	if (not client)
	{
		std::clog << program_name << ": " << client.error().second << "\n";
		return client.error().first;
	}
	
	if (auto sent = client->send(serializer.take()); not sent)
	{
		std::clog << program_name << ": " << sent.error().second << "\n";
		return sent.error().first;
	}
	
	if (auto result = client->run())
	{
		return result.value();
	}
	else
	{
		std::clog << program_name << ": " << result.error().second << "\n";
		return result.error().first;
	}
}
