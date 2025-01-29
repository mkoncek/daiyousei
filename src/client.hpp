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
#include <experimental/scope>

#include <arguments.hpp>
#include <bencode.hpp>

struct Client
{
	std::experimental::unique_resource<int, void(*)(int)> epoll_fd_;
	std::experimental::unique_resource<int, void(*)(int)> socket_;
	std::optional<bencode::deserialized::Integer> exitcode_;
	bencode::Serializer::Serializer_buffer serializer_;
	
	Client(
		std::experimental::unique_resource<int, void(*)(int)> epoll_fd,
		std::experimental::unique_resource<int, void(*)(int)> socket)
		:
		epoll_fd_(std::move(epoll_fd)),
		socket_(std::move(socket)),
		serializer_(bencode::Serializer::create())
	{
	}
	
	static void checked_close(int fd)
	{
		std::clog << "closing file descriptor " << fd << "\n";
		
		if (close(fd))
		{
			std::clog << "failed to close file descriptor " << fd << ": " << std::strerror(errno) << "\n";
		}
	}
	
	static std::expected<std::size_t, std::error_code> read_into(int fd, std::string& output)
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
	
	static std::expected<Client, std::pair<int, std::string>> create(Arguments args)
	{
		auto socket_fd = std::experimental::make_unique_resource_checked(socket(AF_UNIX, SOCK_STREAM, 0), -1, &checked_close);
		if (socket_fd.get() == -1)
		{
			return std::unexpected(std::pair(255, std::string("failed to create a unix socket: ") + std::strerror(errno)));
		}
		
		auto address = sockaddr_un();
		address.sun_family = AF_UNIX;
		auto sun_path = std::string();
		if (args.unix_socket)
		{
			sun_path = args.unix_socket->string();
		}
		else if (auto unix_socket = std::getenv("DAIYOUSEI_UNIX_SOCKET"))
		{
			sun_path = unix_socket;
		}
		else
		{
			return std::unexpected(std::pair(255, std::string("missing argument: unix socket")));
		}
		
		if (sun_path.size() >= std::size(address.sun_path))
		{
			return std::unexpected(std::pair(255, std::string("argument [unix socket] too long, value is: ") + sun_path));
		}
		std::ranges::copy(sun_path, address.sun_path);
		
		auto epoll_fd = std::experimental::make_unique_resource_checked(epoll_create1(0), -1, &checked_close);
		if (epoll_fd.get() == -1)
		{
			return std::unexpected(std::pair(255, std::string("failed to create epoll file descriptor: ") + std::strerror(errno)));
		}
		
		{
			auto event = epoll_event();
			event.events = EPOLLIN;
			
			event.data.fd = 0;
			if (epoll_ctl(epoll_fd.get(), EPOLL_CTL_ADD, 0, &event))
			{
				return std::unexpected(std::pair(255, std::string("failed to add standard input file descriptor to epoll: ") + std::strerror(errno)));
			}
			
			event.data.fd = socket_fd.get();
			if (epoll_ctl(epoll_fd.get(), EPOLL_CTL_ADD, socket_fd.get(), &event))
			{
				return std::unexpected(std::pair(255, std::string("failed to add unix socket file descriptor to epoll: ") + std::strerror(errno)));
			}
		}
		
		if (fcntl(0, F_SETFL, O_NONBLOCK))
		{
			return std::unexpected(std::pair(255, std::string("failed to set non-blocking mode for the standard input stream: ") + std::strerror(errno)));
		}
		
		if (fcntl(socket_fd.get(), F_SETFL, O_NONBLOCK))
		{
			return std::unexpected(std::pair(255, std::string("failed to set non-blocking mode for unix socket") + sun_path + ": " + std::strerror(errno)));
		}
		
		if (connect(socket_fd.get(), reinterpret_cast<sockaddr*>(&address), sizeof(address)))
		{
			return std::unexpected(std::pair(255, std::string("failed to connect to unix socket ") + sun_path + ": " + std::strerror(errno)));
		}
		
		return Client(std::move(epoll_fd), std::move(socket_fd));
	}
	
	std::expected<void, std::pair<int, std::string>> handle_list(bencode::deserialized::List& list)
	{
		std::cout << "handle list" << "\n";
		std::cout << list.size() << "\n";
		
		for (std::size_t i = 0; i + 1 < list.size(); i += 2)
		{
			auto key = std::string_view();
			try
			{
				key = list[i].get_byte_string();
			}
			catch (std::bad_variant_access& ex)
			{
				return std::unexpected(std::pair(255, "expected key to be a byte string"));
			}
			
			auto& value = list[i + 1];
			
			if (key == "exitcode")
			{
				try
				{
					exitcode_.emplace(value.get_integer());
				}
				catch (std::bad_variant_access& ex)
				{
					return std::unexpected(std::pair(255, "expected exitcode value to be an integer"));
				}
			}
			else if (key == "stdout")
			{
				try
				{
					std::cout << value.get_byte_string();
				}
				catch (std::bad_variant_access& ex)
				{
					return std::unexpected(std::pair(255, "expected stdout value to be a byte string"));
				}
			}
			else if (key == "stderr")
			{
				try
				{
					std::clog << value.get_byte_string();
				}
				catch (std::bad_variant_access& ex)
				{
					return std::unexpected(std::pair(255, "expected stderr value to be a byte string"));
				}
			}
			else
			{
				return std::unexpected(std::pair(255, std::string("unrecognized key: ") + std::string(key)));
			}
		}
		
		if (list.size() % 2 == 1)
		{
			list[0] = std::move(list[list.size() - 1]);
		}
		
		list.resize(list.size() % 2);
		
		return {};
	}
	
	std::expected<std::ptrdiff_t, std::pair<int, std::string>> send(std::string_view data)
	{
		if (auto result = ::send(socket_.get(), data.data(), data.size(), 0); result != -1)
		{
			return result;
		}
		else
		{
			return std::unexpected(std::pair(255, std::string("failed to send message: ") + std::strerror(errno)));
		}
	}
	
	std::expected<int, std::pair<int, std::string>> run()
	{
		auto events = std::array<epoll_event, 8>();
		auto deserializer = bencode::Deserializer();
		bool input_available = true;
		auto input = std::string();
		input.reserve(128);
		
		while (input_available)
		{
			auto ready_events = epoll_wait(epoll_fd_.get(), events.data(), events.size(), -1);
			for (int i = 0; i != ready_events; ++i)
			{
				std::cout << "## " << events[i].events << "\n";
				std::cout << i << "\n";
				if (events[i].data.fd == 0)
				{
					if (auto rresult = read_into(events[i].data.fd, input); not rresult)
					{
						return std::unexpected(std::pair(255, rresult.error().message()));
					}
					
					if (input.size() != 0)
					{
						serializer_.push_byte_string(input);
						input.clear();
						
						if (auto sresult = send(serializer_.take()); not sresult)
						{
							return std::unexpected(sresult.error());
						}
					}
				}
				else if (auto rresult = read_into(events[i].data.fd, deserializer.data_))
				{
					std::cout << "|| " << deserializer.data_.size() << "\n";
					if (rresult.value() == 0)
					{
						std::clog << "input stream " << events[i].data.fd << " closed"  << "\n";
						input_available = false;
					}
					
					if (not deserializer.empty())
					{
						auto dresult = deserializer.deserialize();
						bencode::deserialized::List* list = nullptr;
						if (dresult)
						{
							list = &dresult.value().get_list();
						}
						else if (dresult.error() == bencode::Deserialization_error::incomplete_message)
						{
							list = &deserializer.in_progress_.get_list();
						}
						else
						{
							return std::unexpected(std::pair(255, std::to_string(int(dresult.error()))));
						}
						
						if (auto result = handle_list(*list); not result)
						{
							return std::unexpected(result.error());
						}
						std::cout << list->size() << "\n";
					}
				}
				else
				{
					// Read error
					return std::unexpected(std::pair(255, rresult.error().message()));
				}
			}
		}
		
		if (exitcode_)
		{
			return exitcode_.value();
		}
		else
		{
			return std::unexpected(std::pair(255, "communication terminated without setting the exit code"));
		}
	}
};
