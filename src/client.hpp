#pragma once

#include <unistd.h>
#include <fcntl.h>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <sys/un.h>

#include <cerrno>
#include <cstdlib>

#include <algorithm>
#include <cstring>
#include <iostream>
#include <filesystem>
#include <stdexcept>
#include <experimental/scope>
#include <experimental/array>

#include <bencode.hpp>

extern char** environ;

namespace global
{
constexpr std::string_view program_name = "daiyousei";
constexpr std::string_view default_unix_socket_name = "daiyousei.sock";
constexpr std::string_view env_name_unix_socket = "DAIYOUSEI_UNIX_SOCKET";
} // namespace global

struct Epoll_client
{
	std::experimental::unique_resource<int, void(*)(int)> epoll_fd_;
	std::experimental::unique_resource<int, void(*)(int)> socket_;
	
	static void checked_close(int fd)
	{
		if (close(fd))
		{
			std::clog << global::program_name << ": failed to close file descriptor " << fd << ": " << std::strerror(errno) << "\n";
		}
	}
	
	Epoll_client()
	{
		auto socket_fd = std::experimental::make_unique_resource_checked(socket(AF_UNIX, SOCK_STREAM, 0), -1, &checked_close);
		if (socket_fd.get() == -1)
		{
			throw std::runtime_error(std::string("failed to create a unix socket: ") + std::strerror(errno));
		}
		
		auto address = sockaddr_un();
		address.sun_family = AF_UNIX;
		auto sun_path = std::string();
		
		if (auto unix_socket = std::getenv(global::env_name_unix_socket.data()))
		{
			sun_path = unix_socket;
		}
		else if (auto runtime_dir = std::getenv("XDG_RUNTIME_DIR"))
		{
			sun_path = (std::filesystem::path(runtime_dir) / global::default_unix_socket_name).string();
		}
		else
		{
			sun_path = (std::filesystem::path("/tmp") / global::default_unix_socket_name).string();
		}
		
		if (sun_path.size() >= std::size(address.sun_path))
		{
			throw std::runtime_error(std::string("value of " + std::string(global::env_name_unix_socket) + " too long, value is: ") + sun_path);
		}
		std::ranges::copy(sun_path, address.sun_path);
		
		auto epoll_fd = std::experimental::make_unique_resource_checked(epoll_create1(0), -1, &checked_close);
		if (epoll_fd.get() == -1)
		{
			throw std::runtime_error(std::string("failed to create epoll file descriptor: ") + std::strerror(errno));
		}
		
		{
			auto event = epoll_event();
			event.events = EPOLLIN;
			
			event.data.fd = 0;
			if (epoll_ctl(epoll_fd.get(), EPOLL_CTL_ADD, 0, &event))
			{
				throw std::runtime_error(std::string("failed to add standard input file descriptor to epoll: ") + std::strerror(errno));
			}
			
			event.data.fd = socket_fd.get();
			if (epoll_ctl(epoll_fd.get(), EPOLL_CTL_ADD, socket_fd.get(), &event))
			{
				throw std::runtime_error(std::string("failed to add unix socket file descriptor to epoll: ") + std::strerror(errno));
			}
		}
		
		if (fcntl(0, F_SETFL, O_NONBLOCK))
		{
			throw std::runtime_error(std::string("failed to set non-blocking mode for the standard input stream: ") + std::strerror(errno));
		}
		
		if (fcntl(socket_fd.get(), F_SETFL, O_NONBLOCK))
		{
			throw std::runtime_error(std::string("failed to set non-blocking mode for unix socket") + sun_path + ": " + std::strerror(errno));
		}
		
		if (connect(socket_fd.get(), reinterpret_cast<sockaddr*>(&address), sizeof(address)))
		{
			throw std::runtime_error(std::string("failed to connect to unix socket ") + sun_path + ": " + std::strerror(errno));
		}
		
		epoll_fd_ = std::move(epoll_fd);
		socket_ =  std::move(socket_fd);
	}
};

struct Client : Epoll_client, bencode::Streaming_deserializer
{
	std::optional<bencode::deserialized::Integer> exitcode_;
	bencode::Serializer::Serializer_buffer serializer_;
	
	static std::size_t read_into(int fd, std::string& output)
	{
		auto total_read_bytes = std::size_t(0);
		auto error = std::optional<std::runtime_error>();
		
		while (true)
		{
			auto prev_size = output.size();
			output.resize(prev_size + 128);
			auto read_bytes = read(fd, output.data() + prev_size, output.size() - prev_size);
			
			if (read_bytes == -1)
			{
				read_bytes = 0;
				
				if (errno == EWOULDBLOCK or errno == EAGAIN)
				{
					// ignore
				}
				else
				{
					error.emplace(std::runtime_error(std::string("read from file descriptor '" + std::to_string(fd) + "' failed: " + std::strerror(errno))));
				}
			}
			
			output.resize(prev_size + read_bytes);
			
			total_read_bytes += read_bytes;
			
			if (read_bytes == 0)
			{
				break;
			}
		}
		
		if (error)
		{
			throw *error;
		}
		
		return total_read_bytes;
	}
	
	Client(int argc, const char* const* argv)
	{
		serializer_.push_raw_data("l");
		serializer_.push_raw_data("4:argv");
		serializer_.push_raw_data("l");
		for (int i = 0; i != argc; ++i)
		{
			serializer_.emplace_byte_string(argv[i]);
		}
		serializer_.push_raw_data("e");
		serializer_.push_raw_data("3:cwd");
		serializer_.emplace_byte_string(std::filesystem::current_path().c_str());
		serializer_.push_raw_data("3:env");
		serializer_.push_raw_data("l");
		
		for (const char* const* env = environ; *env != nullptr; ++env)
		{
			std::size_t mid = 0;
			while ((*env)[mid] != '=')
			{
				++mid;
			}
			std::size_t end = mid + 1;
			while ((*env)[end] != '\0')
			{
				++end;
			}
			
			serializer_.emplace_byte_string(*env, mid);
			serializer_.emplace_byte_string(*env + mid + 1, *env + end);
		}
		serializer_.push_raw_data("e");
		
		send(serializer_.take());
	}
	
	std::ptrdiff_t send(std::string_view data)
	{
		if (auto result = ::send(socket_.get(), data.data(), data.size(), MSG_NOSIGNAL); result != -1)
		{
			return result;
		}
		else
		{
			throw std::runtime_error(std::string("failed to send message: ") + std::strerror(errno));
		}
	}
	
	int run()
	{
		auto events = std::array<epoll_event, 8>();
		bool server_input_available = true;
		auto input = std::string();
		input.reserve(128);
		
		{
			auto communication = std::experimental::unique_resource(socket_.get(), +[](int fd) -> void
			{
				// attempt to tell the server that stdin was closed
				// ignore EPIPE if the server has already closed the connection
				::send(fd, "e", 1, MSG_NOSIGNAL);
				
				if (auto result = shutdown(fd, SHUT_WR); result == -1)
				{
					std::clog << global::program_name << ": socket shutdown failed: " << std::strerror(errno);
				}
			});
			
			while (server_input_available and communication_status_ != Communication_status::terminated)
			{
				auto ready_events = epoll_wait(epoll_fd_.get(), events.data(), events.size(), -1);
				for (int i = 0; i != ready_events; ++i)
				{
					if (events[i].data.fd == 0)
					{
						read_into(events[i].data.fd, input);
						
						if (input.size() == 0)
						{
							communication.reset();
						}
						else
						{
							serializer_.push_raw_data("5:stdin");
							serializer_.emplace_byte_string(input);
							input.clear();
							send(serializer_.take());
						}
					}
					else
					{
						auto read_result = read_into(events[i].data.fd, data_);
						// std::cout << "DATA|" << data_ << "|END OF DATA" << "\n";
						if (read_result == 0)
						{
							server_input_available = false;
						}
						
						receive();
					}
				}
			}
		}
		
		if (not server_input_available and communication_status_ != Communication_status::terminated)
		{
			throw std::runtime_error(std::string("communication terminated by the server without sending end of list"));
		}
		
		if (not data_.empty())
		{
			throw std::runtime_error(std::string("communication terminated with trailing data: ") + data_);
		}
		
		if (exitcode_)
		{
			return exitcode_.value();
		}
		else
		{
			throw std::runtime_error("communication terminated without setting the exit code");
		}
	}
	
	enum struct Communication_status
	{
		not_started,
		ongoing,
		terminated,
	}
	communication_status_;
	
	void visit_list_begin() override
	{
		if (communication_status_ == Communication_status::ongoing)
		{
			throw std::runtime_error(std::string("unexpected start of list"));
		}
		else if (communication_status_ == Communication_status::terminated)
		{
			throw std::runtime_error(std::string("unexpected start of list"));
		}
		else
		{
			communication_status_ = Communication_status::ongoing;
		}
	}
	
	void visit_list_end() override
	{
		if (communication_status_ == Communication_status::not_started)
		{
			throw std::runtime_error(std::string("unexpected end of list"));
		}
		else if (communication_status_ == Communication_status::terminated)
		{
			throw std::runtime_error(std::string("unexpected end of list"));
		}
		else
		{
			communication_status_ = Communication_status::terminated;
		}
	}
	
	void visit_dictionary_begin() override
	{
		throw std::runtime_error(std::string("unexpected start of dictionary"));
	}
	
	void visit_dictionary_end() override
	{
		throw std::runtime_error(std::string("unexpected end of dictionary"));
	}
	
	std::string last_key_;
	
	void visit_byte_string(std::string_view value) override
	{
		if (last_key_.empty())
		{
			bool is_valid = std::ranges::any_of(std::experimental::make_array<std::string_view>("exitcode", "stdout", "stderr"),
			[&value](std::string_view key) -> bool
			{
				return value == key;
			});
			
			if (not is_valid)
			{
				throw std::runtime_error(std::string("key '") + std::string(value) + "' is not valid");
			}
			
			last_key_ = value;
		}
		else
		{
			constexpr auto keys = std::experimental::make_array<std::string_view>("stdout", "stderr");
			
			for (std::size_t i = 0; i != std::size(keys); ++i)
			{
				if (last_key_ == keys[i])
				{
					for (std::ptrdiff_t len = 0; len != std::ssize(value);)
					{
						auto result = write(i + 1, value.data() + len, value.size() - len);
						if (result != -1)
						{
							len += result;
						}
						else
						{
							throw std::runtime_error(std::string("writing to ") + std::string(keys[i]) + " failed: " + std::strerror(errno));
						}
					}
					break;
				}
			}
			
			last_key_.clear();
		}
	}
	
	void visit_integer(bencode::Integer value) override
	{
		if (last_key_.empty())
		{
			throw std::runtime_error(std::string("unexpected integer, value: ") + std::to_string(value));
		}
		else if (last_key_ == "exitcode")
		{
			if (std::exchange(exitcode_, value))
			{
				throw std::runtime_error("multiple exit codes set");
			}
			
			last_key_.clear();
		}
		else
		{
			throw std::runtime_error(std::string("unexpected integer value for key '" + last_key_ + "', value: ") + std::to_string(value));
		}
	}
};
