#include <rai/mikron_storetool/blockstore_tool.hpp>
#include <rai/node/cli.hpp>
#include <rai/node/node.hpp>
#include <rai/node/testing.hpp>
#include <rai/rai_node/daemon.hpp>

#include <argon2.h>

#include <boost/lexical_cast.hpp>
#include <boost/program_options.hpp>

int main (int argc, char * const * argv)
{
	boost::program_options::options_description description ("Command line options");
	rai::add_node_options (description);
	
	const char* option_help = "help";
	const char* option_version = "version";
	const char* option_debug_pending = "debug_pending";
	const char* option_debug_send_to_self = "debug_send_to_self";
	const char* option_debug_receive_times = "debug_receive_times";

	// clang-format off
	description.add_options ()
		(option_help, "Print out options")
		(option_version, "Prints out version")
		(option_debug_pending, "List pending transactions")
		(option_debug_send_to_self, "Search for sends to self")
		(option_debug_receive_times, "Analyze send-to-receive time differences");
	// clang-format on

	boost::program_options::variables_map vm;
	try
	{
		boost::program_options::store (boost::program_options::parse_command_line (argc, argv, description), vm);
	}
	catch (boost::program_options::error const & err)
	{
		std::cerr << err.what () << std::endl;
		return 1;
	}
	boost::program_options::notify (vm);
	int result (0);
	boost::filesystem::path data_path = rai::data_path_from_options (vm);
	auto ec = rai::handle_node_options (vm);

	if (ec == rai::error_cli::unknown_command)
	{
		if (vm.count (option_help))
		{
			std::cout << description << std::endl;
		}
		else if (vm.count (option_version))
		{
			std::cout << "Version " << RAIBLOCKS_VERSION_MAJOR << "." << RAIBLOCKS_VERSION_MINOR << "." << RAIBLOCKS_VERSION_PATCH << std::endl;
		}
		else if (vm.count (option_debug_pending))
		{
			rai::blockstore_tool::debug_pending (data_path);
		}
		else if (vm.count (option_debug_send_to_self))
		{
			rai::blockstore_tool::debug_send_to_self (data_path);
		}
		else if (vm.count (option_debug_receive_times))
		{
			rai::blockstore_tool::debug_receive_times (data_path);
		}
		else
		{
			std::cout << description << std::endl;
			result = -1;
		}
	}
	return result;
}
