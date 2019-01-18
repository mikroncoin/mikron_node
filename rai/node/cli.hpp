#pragma once

#include <rai/lib/errors.hpp>
#include <boost/program_options.hpp>
#include <boost/filesystem.hpp>

namespace rai
{
/** Command line related error codes */
enum class error_cli
{
	generic = 1,
	parse_error = 2,
	invalid_arguments = 3,
	unknown_command = 4
};

void add_node_options (boost::program_options::options_description &);
boost::filesystem::path data_path_from_options (boost::program_options::variables_map);
std::error_code handle_node_options (boost::program_options::variables_map &);
}

REGISTER_ERROR_CODES (rai, error_cli)
