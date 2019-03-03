#include <rai/mikron_storetool/block_helper.hpp>


rai::block_helper::block_type rai::block_helper::get_block_type (std::unique_ptr<rai::block> const & block_a, rai::ledger & ledger_a, rai::transaction & transaction_a)
{
	switch (block_a->type ())
	{
		/*
		// Legacy
		case rai::block_type::send: return rai::block_helper::block_type::send;
		case rai::block_type::receive: return rai::block_helper::block_type::receive;
		case rai::block_type::open: return rai::block_helper::block_type::open;
		case rai::block_type::change: return rai::block_helper::block_type::change;
		*/
		case rai::block_type::state:
			{
				auto state_block (dynamic_cast<rai::state_block*> (block_a.get ()));
				return get_state_subtype (*state_block, ledger_a, transaction_a);
			}
			break;
		default:
			assert (false);
			break;
	}
	return rai::block_helper::block_type::undefined;
}

rai::block_hash rai::block_helper::get_receive_source (std::unique_ptr<rai::block> const & block, rai::block_helper::block_type block_type)
{
    switch (block_type)
	{
		/*
		case rai::block_helper::block_type::receive: // legacy
			{
				auto receive_block (dynamic_cast<rai::receive_block*> (block.get ()));
				return receive_block->hashables.source;
			}
			break;
		case rai::block_helper::block_type::open: // legacy
			{
				auto open_block (dynamic_cast<rai::open_block*> (block.get ()));
				return open_block->hashables.source;
			}
			break;
		*/
		case rai::block_helper::block_type::state_receive:
		case rai::block_helper::block_type::state_open_receive:
			{
				auto state_block (dynamic_cast<rai::state_block*> (block.get ()));
				return state_block->hashables.link;
			}
			break;
		/*
		case rai::block_helper::block_type::send: // legacy
		case rai::block_helper::block_type::change: // legacy
		*/
		case rai::block_helper::block_type::state_send:
		case rai::block_helper::block_type::state_open_genesis:
		case rai::block_helper::block_type::state_change:
			// No receive from
			return 0;
		default:
			assert (false);
			break;
	}
    return 0;
}



rai::block_helper::block_type rai::block_helper::get_state_subtype (rai::state_block const & block_a, rai::ledger & ledger_a, rai::transaction & transaction_a)
{
	rai::state_block_subtype subtype = ledger_a.state_subtype (transaction_a, block_a);
	switch (subtype)
	{
		case rai::state_block_subtype::send: return rai::block_helper::block_type::state_send;
		case rai::state_block_subtype::receive: return rai::block_helper::block_type::state_receive;
		case rai::state_block_subtype::open_receive: return rai::block_helper::block_type::state_open_receive;
		case rai::state_block_subtype::open_genesis: return rai::block_helper::block_type::state_open_genesis;
		case rai::state_block_subtype::change: return rai::block_helper::block_type::state_change;
		default:
			assert (false);
			break;
	}
	/*
	// Legacy
	rai::block_hash previous_hash (block_a.hashables.previous);
	if (previous_hash.is_zero ())
	{
		return get_state_subtype_balance (block_a, 0); // no previous block -- in this case time does not matter
	}
	auto previous_block (ledger_a.store.block_get (transaction_a, previous_hash));
	assert (previous_block != nullptr);
	if (previous_block == nullptr) return rai::block_helper::block_type::undefined;
	auto previous_balance = ledger_a.balance (transaction_a, previous_hash); // could be retrieved directly from the block
	return get_state_subtype_balance (block_a, previous_balance);
	*/
}

rai::block_helper::block_type rai::block_helper::get_state_subtype_balance (rai::state_block const & block_a, rai::uint256_t previous_balance_a)
{
	// if there is no previous: open
	if (block_a.hashables.previous.is_zero ())
	{
		if (!block_a.hashables.link.is_zero ())
		{
			// no prev but link: open_receive
			return rai::block_helper::block_type::state_open_receive;
		}
		else
		{
			// no prev, no link: open_genesis
			return rai::block_helper::block_type::state_open_genesis;
		}
	}
	// has previous, has previous balance
	// check balances: if decreasing: send
	auto cur_balance (block_a.hashables.balance.number ());
	if (cur_balance < previous_balance_a)
	{
		return rai::block_helper::block_type::state_send;
	}
	// if balance increasing: receive
	if (cur_balance > previous_balance_a)
	{
		return rai::block_helper::block_type::state_receive;
	}
	// balance does not change, and no link: change
	if (block_a.hashables.link.is_zero ())
	{
		return rai::block_helper::block_type::state_change;
	}
	// otherwise: undefined, which is not a valid block
	return rai::block_helper::block_type::undefined;
}
