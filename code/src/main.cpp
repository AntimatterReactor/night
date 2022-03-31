#include "front-end/front-end.hpp"
#include "error.hpp"
#include "cmakedef.hpp"

#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

int main(int argc, char* argv[])
{
	std::string const more_info = "for more info, run: night --help\n";
	std::vector<std::string_view> const argv_s(argv, argv + argc);

	if (argc == 2 && argv[1][0] == '-')
	{
		if (argv_s[1] == "--help" || argv_s[1] == "-h")
		{
			std::clog << "usage: night <file>|<options>\n"
					  << "options:\n"
					  << "    --help     displays this message and exit\n"
					  << "    --version  displays night's current version\n";
		}
		else if (argv_s[1] == "--version" || argv_s[1] == "-v")
		{
			std::clog << "night v"
					  << night_VERSION_MAJOR << '.'
					  << night_VERSION_MINOR << '.'
					  << night_VERSION_PATCH << '\n';
		}
		else std::clog << "unknown option: " << argv[1] << '\n' << more_info;
	}
	else if (argc == 2 || (argc == 3 && argv[2][0] == '-'))
	{
		if (argv_s[2] == "-debug")
			night::error::debug_flag = true;

		try {
			front_end(argv[1]);
		}
		catch (night::error const& e) {
			std::cerr << e.what();
		}
		catch (std::exception const& e) {
			std::cerr << "Oh no! We've come across an unexpected error:\n\n    " << e.what() <<
				"\n\nPlease submit an issue on the GitHub page:\nhttps://github.com/dynamicsquid/night\n";
		}
	}
	else std::clog << "invalid number of arguments\n" << more_info;

	return 0;
}
