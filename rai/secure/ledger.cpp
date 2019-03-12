#include <rai/node/common.hpp>
#include <rai/node/stats.hpp>
#include <rai/secure/blockstore.hpp>
#include <rai/secure/ledger.hpp>

namespace
{
/**
 * Roll back the visited block
 */
class rollback_visitor : public rai::block_visitor
{
public:
	rollback_visitor (MDB_txn * transaction_a, rai::ledger & ledger_a) :
	transaction (transaction_a),
	ledger (ledger_a)
	{
	}
	virtual ~rollback_visitor () = default;
	void state_block (rai::state_block const & block_a) override
	{
		auto hash (block_a.hash ());
		rai::block_hash representative (0);
		if (!block_a.hashables.previous.is_zero ())
		{
			representative = block_a.hashables.previous;
		}
		auto previous_balance (ledger.balance (transaction, block_a.hashables.previous));
		auto subtype (ledger.state_subtype (transaction, block_a));
		// Add in amount delta
		ledger.store.representation_add (transaction, hash, 0 - block_a.hashables.balance.number ());
		if (!representative.is_zero ())
		{
			// Move existing representation
			ledger.store.representation_add (transaction, representative, previous_balance);
		}

		rai::account_info info;
		auto error (ledger.store.account_get (transaction, block_a.hashables.account, info));

		if (subtype == rai::state_block_subtype::send)
		{
			rai::pending_key key (block_a.hashables.link, hash);
			while (!ledger.store.pending_exists (transaction, key))
			{
				ledger.rollback (transaction, ledger.latest (transaction, block_a.hashables.link));
			}
			ledger.store.pending_del (transaction, key);
			ledger.stats.inc (rai::stat::type::rollback, rai::stat::detail::send);
		}
		else if (subtype == rai::state_block_subtype::receive || subtype == rai::state_block_subtype::open_receive)
		{
			//auto source_version (ledger.store.block_version (transaction, block_a.hashables.link));
			rai::pending_info pending_info (ledger.account (transaction, block_a.hashables.link), block_a.hashables.balance.number () - previous_balance);
			ledger.store.pending_put (transaction, rai::pending_key (block_a.hashables.account, block_a.hashables.link), pending_info);
			if (subtype == rai::state_block_subtype::receive)
			{
				ledger.stats.inc (rai::stat::type::rollback, rai::stat::detail::receive);
			}
			else if (subtype == rai::state_block_subtype::open_receive)
			{
				ledger.stats.inc (rai::stat::type::rollback, rai::stat::detail::open);
			}
		}

		assert (!error);
		//auto previous_version (ledger.store.block_version (transaction, block_a.hashables.previous));
		auto previous_block (ledger.store.block_get (transaction, block_a.hashables.previous));
		ledger.change_latest (transaction, block_a.hashables.account, block_a.hashables.previous, representative, previous_balance, 
			previous_block == nullptr ? 0 : previous_block->creation_time ().number (), info.block_count - 1, false);

		auto previous (ledger.store.block_get (transaction, block_a.hashables.previous));
		if (previous != nullptr)
		{
			ledger.store.block_successor_clear (transaction, block_a.hashables.previous);
			if (previous->type () < rai::block_type::state)
			{
				ledger.store.frontier_put (transaction, block_a.hashables.previous, block_a.hashables.account);
			}
		}
		ledger.store.block_del (transaction, hash);
	}
	MDB_txn * transaction;
	rai::ledger & ledger;
};

class ledger_processor : public rai::block_visitor
{
public:
	ledger_processor (rai::ledger &, MDB_txn *);
	virtual ~ledger_processor () = default;
	void state_block (rai::state_block const &) override;
	void state_block_impl (rai::state_block const &);
	static bool check_time_sequence (rai::timestamp_t new_time, rai::timestamp_t prev_time, rai::timestamp_t tolerance);
	static bool check_time_sequence (rai::state_block const & new_block, std::unique_ptr<rai::block> & prev_block, rai::timestamp_t tolerance);
	rai::ledger & ledger;
	MDB_txn * transaction;
	rai::process_return result;
};

void ledger_processor::state_block (rai::state_block const & block_a)
{
	result.code = rai::process_result::progress;
	rai::amount prev_balance (0);
	if (!block_a.hashables.previous.is_zero ())
	{
		result.code = ledger.store.block_exists (transaction, block_a.hashables.previous) ? rai::process_result::progress : rai::process_result::gap_previous;
		if (result.code == rai::process_result::progress)
		{
			prev_balance = ledger.balance (transaction, block_a.hashables.previous);
		}
	}
	if (result.code == rai::process_result::progress)
	{
		state_block_impl (block_a);
	}
}


void ledger_processor::state_block_impl (rai::state_block const & block_a)
{
	auto hash (block_a.hash ());
	auto now (block_a.creation_time ().number ());
	auto existing (ledger.store.block_exists (transaction, hash));
	result.code = existing ? rai::process_result::old : rai::process_result::progress; // Have we seen this block before? (Unambiguous)
	if (result.code != rai::process_result::progress) return;
	// creation time should be filled
	result.code = block_a.hashables.creation_time.is_zero () ? rai::process_result::invalid_block_creation_time : rai::process_result::progress;
	if (result.code != rai::process_result::progress) return;
	result.code = validate_message (block_a.hashables.account, hash, block_a.signature) ? rai::process_result::bad_signature : rai::process_result::progress; // Is this block signed correctly (Unambiguous)
	if (result.code != rai::process_result::progress) return;
	result.code = block_a.hashables.account.is_zero () ? rai::process_result::opened_burn_account : rai::process_result::progress; // Is this for the burn account? (Unambiguous)
	if (result.code != rai::process_result::progress) return;
	rai::account_info info;
	result.amount = block_a.hashables.balance;
	auto subtype (rai::state_block_subtype::undefined);
	auto account_error (ledger.store.account_get (transaction, block_a.hashables.account, info));
	if (!account_error)
	{
		// Account already exists
		result.code = block_a.hashables.previous.is_zero () ? rai::process_result::fork : rai::process_result::progress; // Has this account already been opened? (Ambigious)
		if (result.code == rai::process_result::progress)
		{
			result.code = ledger.store.block_exists (transaction, block_a.hashables.previous) ? rai::process_result::progress : rai::process_result::gap_previous; // Does the previous block exist in the ledger? (Unambigious)
			if (result.code == rai::process_result::progress)
			{
				auto prev_block (ledger.store.block_get (transaction, block_a.hashables.previous));
				assert (prev_block != nullptr);
				// creation time should be later, with small tolerance
				result.code = check_time_sequence (block_a, prev_block, rai::ledger::time_tolearance_short) ? rai::process_result::progress : rai::process_result::invalid_block_creation_time;
				if (result.code == rai::process_result::progress)
				{
					subtype = ledger.state_subtype (transaction, block_a);
					result.code = (subtype == rai::state_block_subtype::undefined) ? rai::process_result::invalid_state_block : rai::process_result::progress;
					if (result.code == rai::process_result::progress)
					{
						auto prev_balance_with_manna (info.balance_with_manna (block_a.hashables.account, now).number ());
						result.amount = (rai::state_block_subtype::send == subtype) ? (prev_balance_with_manna - result.amount.number ()) : (result.amount.number () - prev_balance_with_manna);
						result.code = (block_a.hashables.previous == info.head) ? rai::process_result::progress : rai::process_result::fork; // Is the previous block the account's head block? (Ambigious)
						if (result.code == rai::process_result::progress)
						{
							// check for send-to-self, starting from epoch (but allowed for earlier blocks for legacy)
							if (rai::state_block_subtype::send == subtype)
							{
								if (rai::epoch::epoch_of_time (block_a.creation_time ().number ()) >= rai::epoch::epoch_num::epoch2)
								{
									result.code = (block_a.hashables.link == block_a.hashables.account) ? rai::process_result::send_same_account : rai::process_result::progress; // send to self not allowed
								}
							}
						}
					}
				}
			}
		}
	}
	else
	{
		// Account does not yet exists
		result.code = block_a.previous ().is_zero () ? rai::process_result::progress : rai::process_result::gap_previous; // Does the first block in an account yield 0 for previous() ? (Unambigious)
		if (result.code == rai::process_result::progress)
		{
			subtype = block_a.get_subtype (0, 0);
			result.code = (subtype == rai::state_block_subtype::undefined) ? rai::process_result::invalid_state_block : rai::process_result::progress;
			if (result.code == rai::process_result::progress)
			{
				result.code = !block_a.hashables.link.is_zero () ? rai::process_result::progress : rai::process_result::gap_source; // Is the first block receiving from a send ? (Unambigious)
			}
		}
	}
	if (result.code == rai::process_result::progress)
	{
		if (subtype == rai::state_block_subtype::receive || subtype == rai::state_block_subtype::open_receive)
		{
			if (!block_a.hashables.link.is_zero ())
			{
				rai::block_hash source_hash (block_a.hashables.link);
				result.code = ledger.store.block_exists (transaction, source_hash) ? rai::process_result::progress : rai::process_result::gap_source; // Have we seen the source block already? (Harmless)
				if (result.code == rai::process_result::progress)
				{
					auto source_block (ledger.store.block_get (transaction, source_hash));
					assert (source_block != nullptr);
					// creation time should be later, with large tolerance (this is a cross-account-chain check, timezone issues are probable)
					result.code = check_time_sequence (block_a, source_block, rai::ledger::time_tolearance_long) ? rai::process_result::progress : rai::process_result::invalid_block_creation_time;
					if (result.code == rai::process_result::progress)
					{
						// check that received amount matches pending send amount
						rai::pending_key key (block_a.hashables.account, block_a.hashables.link);
						rai::pending_info pending;
						result.code = ledger.store.pending_get (transaction, key, pending) ? rai::process_result::unreceivable : rai::process_result::progress; // Has this source already been received (Malformed)
						if (result.code == rai::process_result::progress)
						{
							result.code = (result.amount == pending.amount) ? rai::process_result::progress : rai::process_result::balance_mismatch;
						}
					}
				}
			}
			else
			{
				// If there's no link, the balance must remain the same, only the representative can change
				result.code = result.amount.is_zero () ? rai::process_result::progress : rai::process_result::balance_mismatch;
			}
		}
	}
	if (result.code == rai::process_result::progress)
	{
		ledger.stats.inc (rai::stat::type::ledger, rai::stat::detail::state_block);
		result.state_subtype = subtype;
		ledger.store.block_put (transaction, hash, block_a, 0);

		if (!info.rep_block.is_zero ())
		{
			// Move existing representation
			ledger.store.representation_add (transaction, info.rep_block, 0 - info.balance.number ());
		}
		// Add in amount delta
		ledger.store.representation_add (transaction, hash, block_a.hashables.balance.number ());

		if (subtype == rai::state_block_subtype::send)
		{
			rai::pending_key key (block_a.hashables.link, hash);
			rai::pending_info info (block_a.hashables.account, result.amount.number ());
			ledger.store.pending_put (transaction, key, info);
			ledger.stats.inc (rai::stat::type::ledger, rai::stat::detail::send);
		}
		else if (subtype == rai::state_block_subtype::receive || subtype == rai::state_block_subtype::open_receive)
		{
			ledger.store.pending_del (transaction, rai::pending_key (block_a.hashables.account, block_a.hashables.link));
			if (subtype == rai::state_block_subtype::receive)
			{
				ledger.stats.inc (rai::stat::type::ledger, rai::stat::detail::receive);
			}
			else if (subtype == rai::state_block_subtype::open_receive)
			{
				ledger.stats.inc (rai::stat::type::ledger, rai::stat::detail::open);
			}
		}

		ledger.change_latest (transaction, block_a.hashables.account, hash, hash, block_a.hashables.balance, block_a.creation_time ().number (), info.block_count + 1, true);
		if (!ledger.store.frontier_get (transaction, info.head).is_zero ())
		{
			ledger.store.frontier_del (transaction, info.head);
		}
		// Frontier table is unnecessary for state blocks and this also prevents old blocks from being inserted on top of state blocks
		result.account = block_a.hashables.account;
	}
}

bool ledger_processor::check_time_sequence (rai::timestamp_t new_time, rai::timestamp_t prev_time, rai::timestamp_t tolerance)
{
	rai::timestamp_t prev_with_tolerance = (prev_time >= tolerance) ? prev_time - tolerance : 0; // avoid underflow
	return new_time >= prev_with_tolerance;
}

bool ledger_processor::check_time_sequence (rai::state_block const & new_block, std::unique_ptr<rai::block> & prev_block, rai::timestamp_t tolerance)
{
	if (prev_block == nullptr) return false;
	return check_time_sequence (new_block.creation_time ().number (), prev_block->creation_time ().number (), tolerance);
}

ledger_processor::ledger_processor (rai::ledger & ledger_a, MDB_txn * transaction_a) :
ledger (ledger_a),
transaction (transaction_a)
{
}
} // namespace

size_t rai::shared_ptr_block_hash::operator() (std::shared_ptr<rai::block> const & block_a) const
{
	auto hash (block_a->hash ());
	auto result (static_cast<size_t> (hash.qwords[0]));
	return result;
}

bool rai::shared_ptr_block_hash::operator() (std::shared_ptr<rai::block> const & lhs, std::shared_ptr<rai::block> const & rhs) const
{
	return lhs->hash () == rhs->hash ();
}

rai::ledger::ledger (rai::block_store & store_a, rai::stat & stat_a) :
store (store_a),
stats (stat_a),
check_bootstrap_weights (true)
{
}

// Balance for account containing hash
rai::amount_t rai::ledger::balance (MDB_txn * transaction_a, rai::block_hash const & hash_a)
{
	return store.block_balance (transaction_a, hash_a);
}

// Balance for account containing hash.  Balance adjusted with current manna increment.
rai::amount_t rai::ledger::balance_with_manna (MDB_txn * transaction_a, rai::block_hash const & hash_a, rai::timestamp_t now)
{
	if (hash_a.is_zero ())
	{
		return 0;
	}
	auto block (store.block_get (transaction_a, hash_a));
	assert (block != nullptr);
	if (block == nullptr)
	{
		return 0;
	}
	auto balance (block->balance ().number ());
	if (!rai::manna_control::is_manna_account (block->account ()))
	{
		return balance;
	}
	// manna adjustment
	return rai::manna_control::adjust_balance_with_manna (balance, block->creation_time ().number (), now);
}

// Balance for an account by account number
rai::amount_t rai::ledger::account_balance (MDB_txn * transaction_a, rai::account const & account_a)
{
	rai::amount_t result (0);
	rai::account_info info;
	auto none (store.account_get (transaction_a, account_a, info));
	if (!none)
	{
		result = info.balance.number ();
	}
	return result;
}

rai::amount_t rai::ledger::account_balance_with_manna (MDB_txn * transaction_a, rai::account const & account_a, rai::timestamp_t now)
{
	rai::amount_t result (0);
	rai::account_info info;
	auto none (store.account_get (transaction_a, account_a, info));
	if (!none)
	{
		result = info.balance_with_manna (account_a, now).number ();
	}
	return result;
}

rai::amount_t rai::ledger::account_pending (MDB_txn * transaction_a, rai::account const & account_a)
{
	rai::amount_t result (0);
	rai::account end (account_a.number () + 1);
	for (auto i (store.pending_begin (transaction_a, rai::pending_key (account_a, 0))), n (store.pending_begin (transaction_a, rai::pending_key (end, 0))); i != n; ++i)
	{
		rai::pending_info info (i->second);
		result += info.amount.number ();
	}
	return result;
}

rai::process_return rai::ledger::process (MDB_txn * transaction_a, rai::block const & block_a)
{
	ledger_processor processor (*this, transaction_a);
	block_a.visit (processor);
	return processor.result;
}

rai::account rai::ledger::representative_get (MDB_txn * transaction_a, rai::block_hash const & hash_a)
{
	if (hash_a.is_zero ())
	{
		return 0;
	}
	auto block (store.block_get (transaction_a, hash_a));
	assert (block != nullptr);
	if (block == nullptr)
	{
		return 0;
	}
	return block->representative ();
}

bool rai::ledger::block_exists (rai::block_hash const & hash_a)
{
	rai::transaction transaction (store.environment, nullptr, false);
	auto result (store.block_exists (transaction, hash_a));
	return result;
}

std::string rai::ledger::block_text (char const * hash_a)
{
	return block_text (rai::block_hash (hash_a));
}

std::string rai::ledger::block_text (rai::block_hash const & hash_a)
{
	std::string result;
	rai::transaction transaction (store.environment, nullptr, false);
	auto block (store.block_get (transaction, hash_a));
	if (block != nullptr)
	{
		block->serialize_json (result);
	}
	return result;
}

rai::state_block_subtype rai::ledger::state_subtype (MDB_txn * transaction_a, rai::state_block const & block_a)
{
	rai::block_hash previous_hash (block_a.hashables.previous);
	if (previous_hash.is_zero ())
	{
		return block_a.get_subtype (0, 0); // no previous block -- in this case time does not matter
	}
	auto previous_block (store.block_get (transaction_a, previous_hash));
	assert (previous_block != nullptr);
	if (previous_block == nullptr) return rai::state_block_subtype::undefined;
	auto previous_balance = balance (transaction_a, previous_hash); // could be retrieved directly from the block
	return block_a.get_subtype (previous_balance, previous_block->creation_time ().number ());
}

rai::block_hash rai::ledger::block_destination (MDB_txn * transaction_a, rai::block const & block_a)
{
	rai::block_hash result (0);
	rai::state_block const * state_block (dynamic_cast<rai::state_block const *> (&block_a));
	if (state_block != nullptr && state_subtype (transaction_a, *state_block) == rai::state_block_subtype::send)
	{
		result = state_block->hashables.link;
	}
	return result;
}

rai::block_hash rai::ledger::block_source (MDB_txn * transaction_a, rai::block const & block_a)
{
	// If block_a.source () is nonzero, then we have our source.
	// However, universal blocks will always return zero.
	rai::block_hash result (block_a.source ());
	rai::state_block const * state_block (dynamic_cast<rai::state_block const *> (&block_a));
	if (state_block != nullptr && state_subtype (transaction_a, *state_block) != rai::state_block_subtype::send)
	{
		result = state_block->hashables.link;
	}
	return result;
}

// Vote weight of an account
rai::amount_t rai::ledger::weight (MDB_txn * transaction_a, rai::account const & account_a)
{
	if (check_bootstrap_weights.load ())
	{
		auto blocks = store.block_count (transaction_a);
		if (blocks.sum () < bootstrap_weight_max_blocks)
		{
			auto weight = bootstrap_weights.find (account_a);
			if (weight != bootstrap_weights.end ())
			{
				return weight->second;
			}
		}
		else
		{
			check_bootstrap_weights = false;
		}
	}
	return store.representation_get (transaction_a, account_a);
}

// Rollback blocks until `block_a' doesn't exist
void rai::ledger::rollback (MDB_txn * transaction_a, rai::block_hash const & block_a)
{
	assert (store.block_exists (transaction_a, block_a));
	auto account_l (account (transaction_a, block_a));
	rollback_visitor rollback (transaction_a, *this);
	rai::account_info info;
	while (store.block_exists (transaction_a, block_a))
	{
		auto latest_error (store.account_get (transaction_a, account_l, info));
		assert (!latest_error);
		auto block (store.block_get (transaction_a, info.head));
		block->visit (rollback);
	}
}

// Return account containing hash
rai::account rai::ledger::account (MDB_txn * transaction_a, rai::block_hash const & hash_a)
{
	rai::account result;
	auto hash (hash_a);
	rai::block_hash successor (1);
	rai::block_info block_info;
	std::unique_ptr<rai::block> block (store.block_get (transaction_a, hash));
	if (block == nullptr)
	{
		// block not found
		assert (!result.is_zero());
		return result;
	}
	while (!successor.is_zero () && block->type () != rai::block_type::state && store.block_info_get (transaction_a, successor, block_info))
	{
		successor = store.block_successor (transaction_a, hash);
		if (!successor.is_zero ())
		{
			hash = successor;
			block = store.block_get (transaction_a, hash);
		}
	}
	if (block->type () == rai::block_type::state)
	{
		auto state_block (dynamic_cast<rai::state_block *> (block.get ()));
		result = state_block->hashables.account;
	}
	else if (successor.is_zero ())
	{
		result = store.frontier_get (transaction_a, hash);
	}
	else
	{
		result = block_info.account;
	}
	assert (!result.is_zero ());
	return result;
}

// Return amount decrease or increase for block (absolute value)
rai::amount_t rai::ledger::amount (MDB_txn * transaction_a, rai::block_hash const & hash_a)
{
	int amount_sign_l = 0;
	// ignore sign
	return amount_with_sign (transaction_a, hash_a, amount_sign_l);
}

// Return amount decrease or increase for block; absolute value and sign (-1/0/+1)
rai::amount_t rai::ledger::amount_with_sign (MDB_txn * transaction_a, rai::block_hash const & hash_a, int & amount_sign)
{
	amount_sign = 0;
	if (hash_a.is_zero ())
	{
		return 0;
	}
	auto block (store.block_get (transaction_a, hash_a));
	if (block == nullptr)
	{
		if (hash_a == rai::genesis_account)
		{
			return rai::genesis_amount;
		}
		assert (block != nullptr);
		return 0;
	}
	// block != nullptr
	auto prev_hash (block->previous ());
	auto balance (block->balance ().number ());

	if (prev_hash.is_zero ())
	{
		amount_sign = 1;
		return balance;
	}
	// prev_hash != 0
	auto prev_block (store.block_get (transaction_a, prev_hash));
	assert (prev_block != nullptr);
	if (prev_block == nullptr)
	{
		amount_sign = 1;
		return balance;
	}
	auto prev_balance (prev_block->balance ().number ());
	if (rai::manna_control::is_manna_account (prev_block->account ()))
	{
		// manna adjustment
		prev_balance = rai::manna_control::adjust_balance_with_manna (prev_balance, prev_block->creation_time ().number (), block->creation_time ().number ());
	}
	rai::amount_t amount = 0;
	if (prev_balance < balance)
	{
		// increase
		amount_sign = 1;
		amount = balance - prev_balance;
	}
	else if (prev_balance > balance)
	{
		// decrease
		amount_sign = -1;
		amount = prev_balance - balance;
	}
	else
	{
		// same
		amount_sign = 0;
		amount = 0;
	}
	return amount;
}

// Return latest block for account
rai::block_hash rai::ledger::latest (MDB_txn * transaction_a, rai::account const & account_a)
{
	rai::account_info info;
	auto latest_error (store.account_get (transaction_a, account_a, info));
	return latest_error ? 0 : info.head;
}

// Return latest root for account, account number of there are no blocks for this account.
rai::block_hash rai::ledger::latest_root (MDB_txn * transaction_a, rai::account const & account_a)
{
	rai::account_info info;
	auto latest_error (store.account_get (transaction_a, account_a, info));
	rai::block_hash result;
	if (latest_error)
	{
		result = account_a;
	}
	else
	{
		result = info.head;
	}
	return result;
}

rai::checksum rai::ledger::checksum (MDB_txn * transaction_a, rai::account const & begin_a, rai::account const & end_a)
{
	rai::checksum result;
	auto error (store.checksum_get (transaction_a, 0, 0, result));
	assert (!error);
	return result;
}

void rai::ledger::dump_account_chain (rai::account const & account_a)
{
	rai::transaction transaction (store.environment, nullptr, false);
	auto hash (latest (transaction, account_a));
	while (!hash.is_zero ())
	{
		auto block (store.block_get (transaction, hash));
		assert (block != nullptr);
		std::cerr << hash.to_string () << std::endl;
		hash = block->previous ();
	}
}

class block_fit_visitor : public rai::block_visitor
{
public:
	block_fit_visitor (rai::ledger & ledger_a, MDB_txn * transaction_a) :
	ledger (ledger_a),
	transaction (transaction_a),
	result (false)
	{
	}
	void state_block (rai::state_block const & block_a) override
	{
		result = block_a.previous ().is_zero () || ledger.store.block_exists (transaction, block_a.previous ());
		if (result && ledger.state_subtype (transaction, block_a) != rai::state_block_subtype::send)
		{
			result &= ledger.store.block_exists (transaction, block_a.hashables.link);
		}
	}
	rai::ledger & ledger;
	MDB_txn * transaction;
	bool result;
};

bool rai::ledger::could_fit (MDB_txn * transaction_a, rai::block const & block_a)
{
	block_fit_visitor visitor (*this, transaction_a);
	block_a.visit (visitor);
	return visitor.result;
}

void rai::ledger::checksum_update (MDB_txn * transaction_a, rai::block_hash const & hash_a)
{
	rai::checksum value;
	auto error (store.checksum_get (transaction_a, 0, 0, value));
	assert (!error);
	value ^= hash_a;
	store.checksum_put (transaction_a, 0, 0, value);
}

void rai::ledger::change_latest (MDB_txn * transaction_a, rai::account const & account_a, rai::block_hash const & hash_a, rai::block_hash const & rep_block_a, rai::amount const & balance_a, rai::timestamp_t last_block_time_a, uint64_t block_count_a, bool is_state)
{
	rai::account_info info;
	auto exists (!store.account_get (transaction_a, account_a, info));
	if (exists)
	{
		checksum_update (transaction_a, info.head);
	}
	else
	{
		assert (store.block_get (transaction_a, hash_a)->previous ().is_zero ());
		info.open_block = hash_a;
	}
	if (!hash_a.is_zero ())
	{
		info.head = hash_a;
		info.rep_block = rep_block_a;
		info.balance = balance_a;
		info.last_block_time_set (last_block_time_a);
		info.block_count = block_count_a;
		if (exists)
		{
			// otherwise we'd end up with a duplicate
			store.account_del (transaction_a, account_a);
		}
		store.account_put (transaction_a, account_a, info);
		if (!(block_count_a % store.block_info_max) && !is_state)
		{
			rai::block_info block_info;
			block_info.account = account_a;
			block_info.balance = balance_a;
			store.block_info_put (transaction_a, hash_a, block_info);
		}
		checksum_update (transaction_a, hash_a);
	}
	else
	{
		store.account_del (transaction_a, account_a);
	}
}

std::unique_ptr<rai::block> rai::ledger::successor (MDB_txn * transaction_a, rai::uint256_union const & root_a)
{
	rai::block_hash successor (0);
	if (store.account_exists (transaction_a, root_a))
	{
		rai::account_info info;
		auto error (store.account_get (transaction_a, root_a, info));
		assert (!error);
		successor = info.open_block;
	}
	else
	{
		successor = store.block_successor (transaction_a, root_a);
	}
	std::unique_ptr<rai::block> result;
	if (!successor.is_zero ())
	{
		result = store.block_get (transaction_a, successor);
	}
	assert (successor.is_zero () || result != nullptr);
	return result;
}

std::unique_ptr<rai::block> rai::ledger::forked_block (MDB_txn * transaction_a, rai::block const & block_a)
{
	assert (!store.block_exists (transaction_a, block_a.hash ()));
	auto root (block_a.root ());
	assert (store.block_exists (transaction_a, root) || store.account_exists (transaction_a, root));
	std::unique_ptr<rai::block> result (store.block_get (transaction_a, store.block_successor (transaction_a, root)));
	if (result == nullptr)
	{
		rai::account_info info;
		auto error (store.account_get (transaction_a, root, info));
		assert (!error);
		result = store.block_get (transaction_a, info.open_block);
		assert (result != nullptr);
	}
	return result;
}
