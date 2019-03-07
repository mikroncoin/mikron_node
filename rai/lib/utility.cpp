#include <rai/lib/utility.hpp>

//#include <boost/filesystem.hpp>
#include <codecvt>
#include <experimental/filesystem>
#include <iostream>
#include <locale>
#include <string>
//#include <algorithm>

// Find an existing file, assuming various character encodings
std::string rai::find_existing_path_with_encodings (std::string path)
{
	// try in original form
	// Use std C++ method for file existence check, to not be influenced by boost filesystem locale (imbue)
	try
	{
		//std::fstream file(path);   if (file.good())
		//if (boost::filesystem::exists(path))
		if (std::experimental::filesystem::exists (path))
		{
			return path;
		}
	}
	catch (...)
	{
		std::cerr << "find_existing_path_with_encodings exception 1" << std::endl;
	}

	// try as UTF
	try
	{
		// UTF-8 to wide
		std::wstring_convert<std::codecvt_utf8<wchar_t>> conv8;
		std::wstring wpath11 = conv8.from_bytes (path);
		// reduce to 8-bit
		std::string path12 = std::string (wpath11.begin (), wpath11.end ());
		if (std::experimental::filesystem::exists (path12))
		{
			return path12;
		}
	}
	catch (...)
	{
		std::cerr << "find_existing_path_with_encodings exception 2" << std::endl;
	}

	// none worked, --> empty string
	return std::string ();
}
