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

void rai::blockstore_tool::debug_receive_times (boost::filesystem::path data_path)
{
	std::cout << "debug_receive_times " << std::endl;
	rai::inactive_node node (data_path);
	rai::transaction transaction (node.node->store.environment, nullptr, false);
	int cnt_rec = 0;
	long sum_time = 0;
	int max_time = 0;
	int min_time = 0;
	int cnt_rec_deviate = 0;
	int range_ok_min = 0;
	int range_ok_max = 60;
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
				if (subtype == rai::state_block_subtype::receive)
				{
					// receive block
					rai::short_timestamp rec_time = state_block->creation_time ();
					rai::block_hash send_hash = state_block->hashables.link;
					auto send_block (node.node->store.block_get (transaction, send_hash));
					rai::short_timestamp send_time = send_block->creation_time ();
					int time_diff = rec_time.number () - send_time.number ();
					cnt_rec += 1;
					sum_time += time_diff;
					if (time_diff > max_time) max_time = time_diff;
					if (time_diff < min_time || min_time == 0) min_time = time_diff;
					if (time_diff < range_ok_min || time_diff > range_ok_max)
					{
						cnt_rec_deviate += 1;
						std::cout << "Receive time: " << time_diff << " ( " << send_time.number() << " " << rec_time.number() << " " << block_hash.to_string () << " )" << std::endl;
					}
				}
			}
			block_hash = block->previous ();
			++cnt_block;
		}
	}
	std::cout << "cnt_block " << cnt_block << " cnt_acc " << cnt_acc << std::endl;
	std::cout << "cnt_rec " << cnt_rec << " cnt_rec_deviate " << cnt_rec_deviate << " sum_time " << sum_time << " min_time " << min_time << " max_time " << max_time << std::endl;
	if (cnt_rec > 0)
	{
		std::cout << "Avg " << (double) sum_time / (double) cnt_rec << std::endl;
	}
}
