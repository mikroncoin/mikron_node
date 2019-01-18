#include <rai/mikron_storetool/blockstore_tool.hpp>
#include <rai/node/node.hpp>

void rai::blockstore_tool::debug_pending (boost::filesystem::path data_path)
{
	std::cout << "debug_pending " << data_path << std::endl;
	rai::inactive_node node (data_path);
	rai::transaction transaction (node.node->store.environment, nullptr, false);
	rai::amount total_amount = 0;
	int cnt = 0;
	rai::timestamp_t now = rai::short_timestamp::now ();
	for (auto i (node.node->store.pending_begin (transaction)), n (node.node->store.pending_end ()); i != n; ++i, ++cnt)
	{
		rai::pending_key key (i->first);
		rai::pending_info info (i->second);
		rai::amount amount = info.amount;
		total_amount = total_amount.number () + amount.number ();
		auto block (node.node->store.block_get (transaction, key.hash));
		rai::short_timestamp send_time = block->creation_time ();
		long age = now - send_time.number ();
		std::cout << std::endl << "Pending " << cnt << " " << amount.format_balance (rai::Mxrb_ratio, 10, true) << " since " << age << " " << send_time.to_date_string_utc () << " from " << info.source.to_account () << " to " << key.account.to_account () << " hash " << key.hash.to_string () << std::endl;
	}
	std::cout << "Count: " << cnt << " Total amount: " << total_amount.format_balance (rai::Mxrb_ratio, 10, true) << std::endl;
}

void rai::blockstore_tool::debug_send_to_self (boost::filesystem::path data_path)
{
	std::cout << "debug_send_to_self " << std::endl;
	rai::inactive_node node (data_path);
	rai::transaction transaction (node.node->store.environment, nullptr, false);
	int cnt_acc = 0;
	int cnt_block = 0;
	for (auto i (node.node->store.latest_begin (transaction)), n (node.node->store.latest_end ()); i != n; ++i, ++cnt_acc)
	{
		rai::account account = rai::account (i->first.uint256 ());
		rai::account_info account_info = rai::account_info (i->second);
		rai::block_hash block_hash = account_info.head;
		//std::cout << cnt_acc << " " << cnt_block << " " << account.to_account () << " " << account_info.balance.to_string_dec () << " " << account_info.open_block.to_string () << std::endl;
		while (block_hash.number () != 0)
		{
			//std::cout << "  " << cnt_block << " " << int (subtype) << " " << block_hash.to_string () << std::endl;
			auto block (node.node->store.block_get (transaction, block_hash));
			if (block->type () == rai::block_type::state)
			{
				// state block
				auto state_block (dynamic_cast<rai::state_block *> (block.get ()));
				rai::state_block_subtype subtype = node.node->ledger.state_subtype (transaction, *state_block);
				if (subtype == rai::state_block_subtype::send)
				{
					// send block
					if (state_block->hashables.account.number () == state_block->hashables.link.number ())
					{
						// send to self
						std::cout << "Send to self: " << state_block->creation_time ().to_date_string_utc () << " " << state_block->hashables.account.to_account () << " " << block_hash.to_string () << " " << std::endl;
					}
				}
			}
			block_hash = block->previous ();
			++cnt_block;
		}
	}
	std::cout << std::endl << cnt_acc << " " << cnt_block << std::endl;
}
