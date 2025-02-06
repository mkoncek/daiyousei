#include <client.hpp>

#include <iostream>

int main(int argc, const char* const* argv)
{
	if (argc == 0)
	{
		std::clog << global::program_name << ": " << "argc is 0" << "\n";
	}
	else try
	{
		auto client = Client(argc, argv);
		return client.run();
	}
	catch (bencode::Deserialization_exception& ex)
	{
		std::clog << global::program_name << ": deserialization error: " << ex.what() << "\n";
	}
	catch (std::exception& ex)
	{
		std::clog << global::program_name << ": " << ex.what() << "\n";
	}
	
	return 255;
}
