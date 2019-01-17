#include <boost/filesystem.hpp>

namespace rai
{
	class blockstore_tool
	{
	public:
		static void debug_pending (boost::filesystem::path);
		static void debug_send_to_self (boost::filesystem::path);
	};
}
