#pragma once

#include <rai/secure/common.hpp>

namespace rai
{
class block_store;
class stat;

class shared_ptr_block_hash
{
public:
	size_t operator() (std::shared_ptr<rai::block> const &) const;
	bool operator() (std::shared_ptr<rai::block> const &, std::shared_ptr<rai::block> const &) const;
};
using tally_t = std::map<rai::amount_t, std::shared_ptr<rai::block>, std::greater<rai::amount_t>>;
class ledger
{
public:
	ledger (rai::block_store &, rai::stat &);
	rai::account account (MDB_txn *, rai::block_hash const &);
	rai::amount_t amount (MDB_txn *, rai::block_hash const &);
	rai::amount_t balance (MDB_txn *, rai::block_hash const &);
	rai::amount_t account_balance (MDB_txn *, rai::account const &);
	rai::amount_t balance_with_manna (MDB_txn *, rai::block_hash const &, rai::timestamp_t); 
	rai::amount_t account_pending (MDB_txn *, rai::account const &);
	rai::amount_t account_balance_with_manna (MDB_txn *, rai::account const &, rai::timestamp_t);
	rai::amount_t weight (MDB_txn *, rai::account const &);
	std::unique_ptr<rai::block> successor (MDB_txn *, rai::block_hash const &);
	std::unique_ptr<rai::block> forked_block (MDB_txn *, rai::block const &);
	rai::block_hash latest (MDB_txn *, rai::account const &);
	rai::block_hash latest_root (MDB_txn *, rai::account const &);
	// return block representative
	rai::account representative_get (MDB_txn *, rai::block_hash const &);
	bool block_exists (rai::block_hash const &);
	std::string block_text (char const *);
	std::string block_text (rai::block_hash const &);
	rai::state_block_subtype state_subtype (MDB_txn *, rai::state_block const &);
	rai::block_hash block_destination (MDB_txn *, rai::block const &);
	rai::block_hash block_source (MDB_txn *, rai::block const &);
	rai::process_return process (MDB_txn *, rai::block const &);
	void rollback (MDB_txn *, rai::block_hash const &);
	void change_latest (MDB_txn *, rai::account const &, rai::block_hash const &, rai::account const &, rai::amount const &, rai::timestamp_t, uint64_t, bool = false);
	void checksum_update (MDB_txn *, rai::block_hash const &);
	rai::checksum checksum (MDB_txn *, rai::account const &, rai::account const &);
	void dump_account_chain (rai::account const &);
	bool could_fit (MDB_txn *, rai::block const &);
	static const rai::timestamp_t time_tolearance_short = 66; // seconds
	static const rai::timestamp_t time_tolearance_long = 33360; // seconds
	static rai::amount_t const unit;
	rai::block_store & store;
	rai::stat & stats;
	std::unordered_map<rai::account, rai::amount_t> bootstrap_weights;
	uint64_t bootstrap_weight_max_blocks;
	std::atomic<bool> check_bootstrap_weights;
	//rai::uint256_union epoch_link;
	//rai::account epoch_signer;
};
};
