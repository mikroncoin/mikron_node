#include <rai/lib/blocks.hpp>
#include <rai/node/node.hpp>
#include <rai/lib/numbers.hpp>

namespace rai
{
class block_helper
{
public:
	// state block subtype, or legacy block type (duplicated here for BW compatibility)
	enum class block_type : uint8_t
	{
		undefined = 0,
		/*
		send = 2, // legacy
		receive = 3, // legacy
		open = 4, // legacy
		change = 5, // legacy
		*/
		state_send = 6,
		state_receive = 7,
		state_open_receive = 8,
		state_open_genesis = 9,
		state_change = 10
	};

	static block_type get_block_type (std::unique_ptr<rai::block> const &, rai::ledger &, rai::transaction &);
	static rai::block_hash get_receive_source (std::unique_ptr<rai::block> const &, block_type);
	static block_type get_state_subtype (rai::state_block const &, rai::ledger &, rai::transaction &);
	// legacy
	static block_type get_state_subtype_balance (rai::state_block const &, rai::uint256_t);
};
}
