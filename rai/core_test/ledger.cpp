#include <cryptopp/filters.h>
#include <cryptopp/randpool.h>
#include <gtest/gtest.h>
#include <rai/core_test/testutil.hpp>
#include <rai/node/stats.hpp>
#include <rai/node/testing.hpp>

using namespace std::chrono_literals;

// Helper methods introduced when switched from legacy block types to state block, to reduce the effort of test updates
rai::state_block ledger_create_send_state_block_helper (rai::state_block const & prev_block_a, rai::account const & destination_a, rai::amount const & balance_a, rai::keypair const & keys_a)
{
	return rai::state_block (prev_block_a.hashables.account, prev_block_a.hash (), 0, prev_block_a.hashables.representative, balance_a, destination_a, keys_a.prv, keys_a.pub, 0);
}

rai::state_block ledger_create_receive_state_block_helper (rai::state_block const & prev_block_a, rai::state_block const & source_send_a, rai::amount const & balance_a, rai::keypair const & keys_a)
{
	return rai::state_block (prev_block_a.hashables.account, prev_block_a.hash (), 0, prev_block_a.hashables.representative, balance_a, source_send_a.hash (), keys_a.prv, keys_a.pub, 0);
}

rai::state_block ledger_create_open_state_block_helper (rai::state_block const & source_a, rai::account const & representative_a, rai::account const & account_a, rai::amount const & balance_a, rai::keypair const & keys_a)
{
	return rai::state_block (keys_a.pub, 0, 0, representative_a, balance_a, source_a.hash (), keys_a.prv, keys_a.pub, 0);
}

rai::state_block ledger_create_change_state_block_helper (rai::state_block const & prev_block_a, rai::account const & representative_a, rai::keypair const & keys_a)
{
	return rai::state_block (prev_block_a.hashables.account, prev_block_a.hash (), 0, representative_a, prev_block_a.hashables.balance, 0, keys_a.prv, keys_a.pub, 0);
}

// Init returns an error if it can't open files at the path
TEST (ledger, store_error)
{
	bool init (false);
	rai::block_store store (init, boost::filesystem::path ("///"));
	ASSERT_FALSE (!init);
	rai::stat stats;
	rai::ledger ledger (store, stats);
}

// Ledger can be initialized and returns a basic query for an empty account
TEST (ledger, empty)
{
	bool init (false);
	rai::block_store store (init, rai::unique_path ());
	ASSERT_TRUE (!init);
	rai::stat stats;
	rai::ledger ledger (store, stats);
	rai::account account;
	rai::transaction transaction (store.environment, nullptr, false);
	auto balance (ledger.account_balance (transaction, account));
	ASSERT_TRUE (balance == 0);
}

// Genesis account should have the max balance on empty initialization
TEST (ledger, genesis_balance)
{
	bool init (false);
	rai::block_store store (init, rai::unique_path ());
	ASSERT_TRUE (!init);
	rai::stat stats;
	rai::ledger ledger (store, stats);
	rai::genesis genesis;
	rai::transaction transaction (store.environment, nullptr, true);
	genesis.initialize (transaction, store);
	auto balance (ledger.account_balance (transaction, rai::genesis_account));
	ASSERT_EQ (rai::genesis_amount, balance);
	auto amount (ledger.amount (transaction, rai::genesis_account));
	ASSERT_EQ (rai::genesis_amount, amount);
	rai::account_info info;
	ASSERT_FALSE (store.account_get (transaction, rai::genesis_account, info));
	// Frontier time should have been updated when genesis balance was added
	ASSERT_EQ (genesis.block ().creation_time ().number (), info.last_block_time ());
	auto now (rai::short_timestamp::now ());
	ASSERT_GE (now, info.last_block_time ());
}

// Make sure the checksum is the same when ledger reloaded
TEST (ledger, checksum_persistence)
{
	bool init (false);
	rai::block_store store (init, rai::unique_path ());
	ASSERT_TRUE (!init);
	rai::uint256_union checksum1;
	rai::uint256_union max;
	max.qwords[0] = 0;
	max.qwords[0] = ~max.qwords[0];
	max.qwords[1] = 0;
	max.qwords[1] = ~max.qwords[1];
	max.qwords[2] = 0;
	max.qwords[2] = ~max.qwords[2];
	max.qwords[3] = 0;
	max.qwords[3] = ~max.qwords[3];
	rai::stat stats;
	rai::transaction transaction (store.environment, nullptr, true);
	{
		rai::ledger ledger (store, stats);
		rai::genesis genesis;
		genesis.initialize (transaction, store);
		checksum1 = ledger.checksum (transaction, 0, max);
	}
	rai::ledger ledger (store, stats);
	ASSERT_EQ (checksum1, ledger.checksum (transaction, 0, max));
}

// All nodes in the system should agree on the genesis balance
TEST (system, system_genesis)
{
	rai::system system (24000, 2);
	for (auto & i : system.nodes)
	{
		rai::transaction transaction (i->store.environment, nullptr, false);
		ASSERT_EQ (rai::genesis_amount, i->ledger.account_balance (transaction, rai::genesis_account));
	}
}

// Create a send block and publish it.
TEST (ledger, process_send)
{
	bool init (false);
	rai::block_store store (init, rai::unique_path ());
	ASSERT_TRUE (!init);
	rai::stat stats;
	rai::ledger ledger (store, stats);
	rai::transaction transaction (store.environment, nullptr, true);
	rai::genesis genesis;
	genesis.initialize (transaction, store);
	rai::account_info info1;
	ASSERT_FALSE (store.account_get (transaction, rai::test_genesis_key.pub, info1));
	rai::keypair key2;
	rai::state_block send (::ledger_create_send_state_block_helper (genesis.block (), key2.pub, 50, rai::test_genesis_key));
	rai::block_hash hash1 (send.hash ());
	ASSERT_EQ (rai::test_genesis_key.pub, store.frontier_get (transaction, info1.head));
	ASSERT_EQ (1, info1.block_count);
	// This was a valid block, it should progress.
	auto return1 (ledger.process (transaction, send));
	ASSERT_EQ(rai::process_result::progress, return1.code);
	ASSERT_EQ (rai::genesis_amount - 50, ledger.amount (transaction, hash1));
	ASSERT_TRUE (store.frontier_get (transaction, info1.head).is_zero ());
	ASSERT_TRUE (store.frontier_get (transaction, hash1).is_zero ());  // state block does not get into frontier
	ASSERT_EQ (rai::test_genesis_key.pub, return1.account);
	ASSERT_EQ (rai::genesis_amount - 50, return1.amount.number ());
	ASSERT_EQ (50, ledger.account_balance (transaction, rai::test_genesis_key.pub));
	ASSERT_EQ (rai::genesis_amount - 50, ledger.account_pending (transaction, key2.pub));
	rai::account_info info2;
	ASSERT_FALSE (store.account_get (transaction, rai::test_genesis_key.pub, info2));
	ASSERT_EQ (2, info2.block_count);
	auto latest6 (store.block_get (transaction, info2.head));
	ASSERT_NE (nullptr, latest6);
	auto latest7 (dynamic_cast<rai::state_block *> (latest6.get ()));
	ASSERT_NE (nullptr, latest7);
	ASSERT_EQ (send, *latest7);
	// Create an open block opening an account accepting the send we just created
	rai::state_block open (::ledger_create_open_state_block_helper (send, key2.pub, key2.pub, rai::genesis_amount - 50, key2));
	rai::block_hash hash2 (open.hash ());
	// This was a valid block, it should progress.
	auto return2 (ledger.process (transaction, open));
	ASSERT_EQ(rai::process_result::progress, return2.code);
	ASSERT_EQ (rai::genesis_amount - 50, ledger.amount (transaction, hash2));
	ASSERT_EQ (key2.pub, return2.account);
	ASSERT_EQ (rai::genesis_amount - 50, return2.amount.number ());
	ASSERT_TRUE (store.frontier_get (transaction, hash2).is_zero ());  // state block does not get into frontier
	ASSERT_EQ (rai::genesis_amount - 50, ledger.account_balance (transaction, key2.pub));
	ASSERT_EQ (0, ledger.account_pending (transaction, key2.pub));
	ASSERT_EQ (50, ledger.weight (transaction, rai::test_genesis_key.pub));
	ASSERT_EQ (rai::genesis_amount - 50, ledger.weight (transaction, key2.pub));
	rai::account_info info3;
	ASSERT_FALSE (store.account_get (transaction, rai::test_genesis_key.pub, info3));
	auto latest2 (store.block_get (transaction, info3.head));
	ASSERT_NE (nullptr, latest2);
	auto latest3 (dynamic_cast<rai::state_block *> (latest2.get ()));
	ASSERT_NE (nullptr, latest3);
	ASSERT_EQ (send, *latest3);
	rai::account_info info4;
	ASSERT_FALSE (store.account_get (transaction, key2.pub, info4));
	auto latest4 (store.block_get (transaction, info4.head));
	ASSERT_NE (nullptr, latest4);
	auto latest5 (dynamic_cast<rai::state_block *> (latest4.get ()));
	ASSERT_NE (nullptr, latest5);
	ASSERT_EQ (open, *latest5);
	ledger.rollback (transaction, hash2);
	ASSERT_TRUE (store.frontier_get (transaction, hash2).is_zero ());
	rai::account_info info5;
	ASSERT_TRUE (ledger.store.account_get (transaction, key2.pub, info5));
	rai::pending_info pending1;
	ASSERT_FALSE (ledger.store.pending_get (transaction, rai::pending_key (key2.pub, hash1), pending1));
	ASSERT_EQ (rai::test_genesis_key.pub, pending1.source);
	ASSERT_EQ (rai::genesis_amount - 50, pending1.amount.number ());
	ASSERT_EQ (0, ledger.account_balance (transaction, key2.pub));
	ASSERT_EQ (rai::genesis_amount - 50, ledger.account_pending (transaction, key2.pub));
	ASSERT_EQ (50, ledger.account_balance (transaction, rai::test_genesis_key.pub));
	ASSERT_EQ (50, ledger.weight (transaction, rai::test_genesis_key.pub));
	ASSERT_EQ (0, ledger.weight (transaction, key2.pub));
	rai::account_info info6;
	ASSERT_FALSE (ledger.store.account_get (transaction, rai::test_genesis_key.pub, info6));
	ASSERT_EQ (hash1, info6.head);
	ledger.rollback (transaction, info6.head);
	ASSERT_EQ (rai::genesis_amount, ledger.weight (transaction, rai::test_genesis_key.pub));
	ASSERT_TRUE (store.frontier_get (transaction, info1.head).is_zero ());  // state block does not get into frontier
	ASSERT_TRUE (store.frontier_get (transaction, hash1).is_zero ());
	rai::account_info info7;
	ASSERT_FALSE (ledger.store.account_get (transaction, rai::test_genesis_key.pub, info7));
	ASSERT_EQ (1, info7.block_count);
	ASSERT_EQ (info1.head, info7.head);
	rai::pending_info pending2;
	ASSERT_TRUE (ledger.store.pending_get (transaction, rai::pending_key (key2.pub, hash1), pending2));
	ASSERT_EQ (rai::genesis_amount, ledger.account_balance (transaction, rai::test_genesis_key.pub));
	ASSERT_EQ (0, ledger.account_pending (transaction, key2.pub));
}

TEST (ledger, process_receive)
{
	bool init (false);
	rai::block_store store (init, rai::unique_path ());
	ASSERT_TRUE (!init);
	rai::stat stats;
	rai::ledger ledger (store, stats);
	rai::genesis genesis;
	rai::transaction transaction (store.environment, nullptr, true);
	genesis.initialize (transaction, store);
	rai::account_info info1;
	ASSERT_FALSE (store.account_get (transaction, rai::test_genesis_key.pub, info1));
	rai::keypair key2;
	rai::state_block send (::ledger_create_send_state_block_helper (genesis.block (), key2.pub, 50, rai::test_genesis_key));
	rai::block_hash hash1 (send.hash ());
	ASSERT_EQ (rai::process_result::progress, ledger.process (transaction, send).code);
	rai::keypair key3;
	rai::state_block open (::ledger_create_open_state_block_helper (send, key3.pub, key2.pub, rai::genesis_amount - 50, key2));
	rai::block_hash hash2 (open.hash ());
	auto return1 (ledger.process (transaction, open));
	ASSERT_EQ (rai::process_result::progress, return1.code);
	ASSERT_EQ (key2.pub, return1.account);
	ASSERT_EQ (rai::genesis_amount - 50, return1.amount.number ());
	ASSERT_EQ (rai::genesis_amount - 50, ledger.weight (transaction, key3.pub));
	rai::state_block send2 (::ledger_create_send_state_block_helper (send, key2.pub, 25, rai::test_genesis_key));
	rai::block_hash hash3 (send2.hash ());
	ASSERT_EQ (rai::process_result::progress, ledger.process (transaction, send2).code);
	rai::state_block receive (key2.pub, open.hash (), 0, rai::genesis_account, rai::genesis_amount - 25, send2.hash (), key2.prv, key2.pub, 0);
	auto hash4 (receive.hash ());
	ASSERT_TRUE (store.frontier_get (transaction, hash2).is_zero ());
	auto return2 (ledger.process (transaction, receive));
	ASSERT_EQ (25, ledger.amount (transaction, hash4));
	ASSERT_TRUE (store.frontier_get (transaction, hash2).is_zero ());
	ASSERT_TRUE (store.frontier_get (transaction, hash4).is_zero ());
	ASSERT_EQ (rai::process_result::progress, return2.code);
	ASSERT_EQ (key2.pub, return2.account);
	ASSERT_EQ (25, return2.amount.number ());
	ASSERT_EQ (hash4, ledger.latest (transaction, key2.pub));
	ASSERT_EQ (25, ledger.account_balance (transaction, rai::test_genesis_key.pub));
	ASSERT_EQ (0, ledger.account_pending (transaction, key2.pub));
	ASSERT_EQ (rai::genesis_amount - 25, ledger.account_balance (transaction, key2.pub));
	ASSERT_EQ (0, ledger.weight (transaction, key3.pub));
	ledger.rollback (transaction, hash4);
	ASSERT_TRUE (store.block_successor (transaction, hash2).is_zero ());
	ASSERT_TRUE (store.frontier_get (transaction, hash2).is_zero ());
	ASSERT_TRUE (store.frontier_get (transaction, hash4).is_zero ());
	ASSERT_EQ (25, ledger.account_balance (transaction, rai::test_genesis_key.pub));
	ASSERT_EQ (25, ledger.account_pending (transaction, key2.pub));
	ASSERT_EQ (rai::genesis_amount - 50, ledger.account_balance (transaction, key2.pub));
	ASSERT_EQ (rai::genesis_amount - 50, ledger.weight (transaction, key3.pub));
	ASSERT_EQ (hash2, ledger.latest (transaction, key2.pub));
	rai::pending_info pending1;
	ASSERT_FALSE (ledger.store.pending_get (transaction, rai::pending_key (key2.pub, hash3), pending1));
	ASSERT_EQ (rai::test_genesis_key.pub, pending1.source);
	ASSERT_EQ (25, pending1.amount.number ());
}

TEST (ledger, rollback_receiver)
{
	bool init (false);
	rai::block_store store (init, rai::unique_path ());
	ASSERT_TRUE (!init);
	rai::stat stats;
	rai::ledger ledger (store, stats);
	rai::genesis genesis;
	rai::transaction transaction (store.environment, nullptr, true);
	genesis.initialize (transaction, store);
	rai::account_info info1;
	ASSERT_FALSE (store.account_get (transaction, rai::test_genesis_key.pub, info1));
	rai::keypair key2;
	rai::state_block send (::ledger_create_send_state_block_helper (genesis.block (), key2.pub, 50, rai::test_genesis_key));
	rai::block_hash hash1 (send.hash ());
	ASSERT_EQ (rai::process_result::progress, ledger.process (transaction, send).code);
	rai::keypair key3;
	rai::state_block open (::ledger_create_open_state_block_helper (send, key3.pub, key2.pub, rai::genesis_amount - 50, key2));
	rai::block_hash hash2 (open.hash ());
	ASSERT_EQ (rai::process_result::progress, ledger.process (transaction, open).code);
	ASSERT_EQ (hash2, ledger.latest (transaction, key2.pub));
	ASSERT_EQ (50, ledger.account_balance (transaction, rai::test_genesis_key.pub));
	ASSERT_EQ (rai::genesis_amount - 50, ledger.account_balance (transaction, key2.pub));
	ASSERT_EQ (50, ledger.weight (transaction, rai::test_genesis_key.pub));
	ASSERT_EQ (0, ledger.weight (transaction, key2.pub));
	ASSERT_EQ (rai::genesis_amount - 50, ledger.weight (transaction, key3.pub));
	ledger.rollback (transaction, hash1);
	ASSERT_EQ (rai::genesis_amount, ledger.account_balance (transaction, rai::test_genesis_key.pub));
	ASSERT_EQ (0, ledger.account_balance (transaction, key2.pub));
	ASSERT_EQ (rai::genesis_amount, ledger.weight (transaction, rai::test_genesis_key.pub));
	ASSERT_EQ (0, ledger.weight (transaction, key2.pub));
	ASSERT_EQ (0, ledger.weight (transaction, key3.pub));
	rai::account_info info2;
	ASSERT_TRUE (ledger.store.account_get (transaction, key2.pub, info2));
	rai::pending_info pending1;
	ASSERT_TRUE (ledger.store.pending_get (transaction, rai::pending_key (key2.pub, info2.head), pending1));
}

TEST (ledger, rollback_representation)
{
	bool init (false);
	rai::block_store store (init, rai::unique_path ());
	ASSERT_TRUE (!init);
	rai::stat stats;
	rai::ledger ledger (store, stats);
	rai::genesis genesis;
	rai::transaction transaction (store.environment, nullptr, true);
	genesis.initialize (transaction, store);
	rai::keypair key5;
	rai::state_block change1 (::ledger_create_change_state_block_helper (genesis.block (), key5.pub, rai::test_genesis_key));
	ASSERT_EQ (rai::process_result::progress, ledger.process (transaction, change1).code);
	rai::keypair key3;
	rai::state_block change2 (::ledger_create_change_state_block_helper (change1, key3.pub, rai::test_genesis_key));
	ASSERT_EQ (rai::process_result::progress, ledger.process (transaction, change2).code);
	rai::keypair key2;
	rai::state_block send1 (::ledger_create_send_state_block_helper (change2, key2.pub, 50, rai::test_genesis_key));
	ASSERT_EQ (rai::process_result::progress, ledger.process (transaction, send1).code);
	rai::keypair key4;
	rai::state_block open (::ledger_create_open_state_block_helper (send1, key4.pub, key2.pub, rai::genesis_amount - 50, key2));
	ASSERT_EQ (rai::process_result::progress, ledger.process (transaction, open).code);
	rai::state_block send2 (::ledger_create_send_state_block_helper (send1, key2.pub, 1, rai::test_genesis_key));
	ASSERT_EQ (rai::process_result::progress, ledger.process (transaction, send2).code);
	rai::state_block receive1 (::ledger_create_receive_state_block_helper (open, send2, rai::genesis_amount - 1, key2));
	ASSERT_EQ (rai::process_result::progress, ledger.process (transaction, receive1).code);
	ASSERT_EQ (0, ledger.weight (transaction, key2.pub));
	ASSERT_EQ (1, ledger.weight (transaction, key3.pub));
	ASSERT_EQ (rai::genesis_amount - 1, ledger.weight (transaction, key4.pub));
	rai::account_info info1;
	ASSERT_FALSE (store.account_get (transaction, key2.pub, info1));
	ASSERT_EQ (receive1.hash (), info1.rep_block);
	ledger.rollback (transaction, receive1.hash ());
	rai::account_info info2;
	ASSERT_FALSE (store.account_get (transaction, key2.pub, info2));
	ASSERT_EQ (open.hash (), info2.rep_block);
	ASSERT_EQ (0, ledger.weight (transaction, key2.pub));
	ASSERT_EQ (rai::genesis_amount - 50, ledger.weight (transaction, key4.pub));
	ledger.rollback (transaction, open.hash ());
	ASSERT_EQ (1, ledger.weight (transaction, key3.pub));
	ASSERT_EQ (0, ledger.weight (transaction, key4.pub));
	ledger.rollback (transaction, send1.hash ());
	ASSERT_EQ (rai::genesis_amount, ledger.weight (transaction, key3.pub));
	rai::account_info info3;
	ASSERT_FALSE (store.account_get (transaction, rai::test_genesis_key.pub, info3));
	ASSERT_EQ (change2.hash (), info3.rep_block);
	ledger.rollback (transaction, change2.hash ());
	rai::account_info info4;
	ASSERT_FALSE (store.account_get (transaction, rai::test_genesis_key.pub, info4));
	ASSERT_EQ (change1.hash (), info4.rep_block);
	ASSERT_EQ (rai::genesis_amount, ledger.weight (transaction, key5.pub));
	ASSERT_EQ (0, ledger.weight (transaction, key3.pub));
}

TEST (ledger, receive_rollback)
{
	bool init (false);
	rai::block_store store (init, rai::unique_path ());
	ASSERT_TRUE (!init);
	rai::stat stats;
	rai::ledger ledger (store, stats);
	rai::genesis genesis;
	rai::transaction transaction (store.environment, nullptr, true);
	genesis.initialize (transaction, store);
	rai::state_block send (::ledger_create_send_state_block_helper (genesis.block (), rai::test_genesis_key.pub, rai::genesis_amount - rai::Gxrb_ratio, rai::test_genesis_key));
	ASSERT_EQ (rai::process_result::progress, ledger.process (transaction, send).code);
	rai::state_block receive (::ledger_create_receive_state_block_helper (send, send, rai::Gxrb_ratio, rai::test_genesis_key));
	ASSERT_EQ (rai::process_result::progress, ledger.process (transaction, receive).code);
	ledger.rollback (transaction, receive.hash ());
}

TEST (ledger, process_duplicate)
{
	bool init (false);
	rai::block_store store (init, rai::unique_path ());
	ASSERT_TRUE (!init);
	rai::stat stats;
	rai::ledger ledger (store, stats);
	rai::genesis genesis;
	rai::transaction transaction (store.environment, nullptr, true);
	genesis.initialize (transaction, store);
	rai::account_info info1;
	ASSERT_FALSE (store.account_get (transaction, rai::test_genesis_key.pub, info1));
	rai::keypair key2;
	rai::state_block send (::ledger_create_send_state_block_helper (genesis.block (), key2.pub, rai::genesis_amount - 50, rai::test_genesis_key));
	rai::block_hash hash1 (send.hash ());
	ASSERT_EQ (rai::process_result::progress, ledger.process (transaction, send).code);
	ASSERT_EQ (rai::process_result::old, ledger.process (transaction, send).code);
	rai::state_block open (::ledger_create_open_state_block_helper (send, 1, key2.pub, 50, key2));
	ASSERT_EQ (rai::process_result::progress, ledger.process (transaction, open).code);
	ASSERT_EQ (rai::process_result::old, ledger.process (transaction, open).code);
}

TEST (ledger, representative_genesis)
{
	bool init (false);
	rai::block_store store (init, rai::unique_path ());
	ASSERT_TRUE (!init);
	rai::stat stats;
	rai::ledger ledger (store, stats);
	rai::genesis genesis;
	rai::transaction transaction (store.environment, nullptr, true);
	genesis.initialize (transaction, store);
	auto latest (ledger.latest (transaction, rai::test_genesis_key.pub));
	ASSERT_FALSE (latest.is_zero ());
	ASSERT_EQ (genesis.hash (), ledger.representative (transaction, latest));
}

TEST (ledger, weight)
{
	bool init (false);
	rai::block_store store (init, rai::unique_path ());
	ASSERT_TRUE (!init);
	rai::stat stats;
	rai::ledger ledger (store, stats);
	rai::genesis genesis;
	rai::transaction transaction (store.environment, nullptr, true);
	genesis.initialize (transaction, store);
	ASSERT_EQ (rai::genesis_amount, ledger.weight (transaction, rai::genesis_account));
}

TEST (ledger, representative_change)
{
	bool init (false);
	rai::block_store store (init, rai::unique_path ());
	ASSERT_TRUE (!init);
	rai::stat stats;
	rai::ledger ledger (store, stats);
	rai::keypair key2;
	rai::genesis genesis;
	rai::transaction transaction (store.environment, nullptr, true);
	genesis.initialize (transaction, store);
	ASSERT_EQ (rai::genesis_amount, ledger.weight (transaction, rai::test_genesis_key.pub));
	ASSERT_EQ (0, ledger.weight (transaction, key2.pub));
	rai::account_info info1;
	ASSERT_FALSE (store.account_get (transaction, rai::test_genesis_key.pub, info1));
	ASSERT_EQ (genesis.hash (), info1.head);
	rai::state_block block (::ledger_create_change_state_block_helper (genesis.block (), key2.pub, rai::test_genesis_key));
	ASSERT_EQ (rai::test_genesis_key.pub, store.frontier_get (transaction, info1.head));
	auto return1 (ledger.process (transaction, block));
	ASSERT_EQ (rai::process_result::progress, return1.code);
	ASSERT_EQ (0, ledger.amount (transaction, block.hash ()));
	ASSERT_TRUE (store.frontier_get (transaction, info1.head).is_zero ());
	ASSERT_TRUE (store.frontier_get (transaction, block.hash ()).is_zero ());
	ASSERT_EQ (rai::test_genesis_key.pub, return1.account);
	ASSERT_EQ (0, ledger.weight (transaction, rai::test_genesis_key.pub));
	ASSERT_EQ (rai::genesis_amount, ledger.weight (transaction, key2.pub));
	rai::account_info info2;
	ASSERT_FALSE (store.account_get (transaction, rai::test_genesis_key.pub, info2));
	ASSERT_EQ (block.hash (), info2.head);
	ledger.rollback (transaction, info2.head);
	ASSERT_TRUE (store.frontier_get (transaction, info1.head).is_zero ());
	ASSERT_TRUE (store.frontier_get (transaction, block.hash ()).is_zero ());
	rai::account_info info3;
	ASSERT_FALSE (store.account_get (transaction, rai::test_genesis_key.pub, info3));
	ASSERT_EQ (info1.head, info3.head);
	ASSERT_EQ (rai::genesis_amount, ledger.weight (transaction, rai::test_genesis_key.pub));
	ASSERT_EQ (0, ledger.weight (transaction, key2.pub));
}

TEST (ledger, send_fork)
{
	bool init (false);
	rai::block_store store (init, rai::unique_path ());
	ASSERT_TRUE (!init);
	rai::stat stats;
	rai::ledger ledger (store, stats);
	rai::keypair key2;
	rai::keypair key3;
	rai::genesis genesis;
	rai::transaction transaction (store.environment, nullptr, true);
	genesis.initialize (transaction, store);
	rai::account_info info1;
	ASSERT_FALSE (store.account_get (transaction, rai::test_genesis_key.pub, info1));
	rai::state_block block (::ledger_create_send_state_block_helper (genesis.block (), key2.pub, 100, rai::test_genesis_key));
	ASSERT_EQ (rai::process_result::progress, ledger.process (transaction, block).code);
	rai::state_block block2 (::ledger_create_send_state_block_helper (genesis.block (), key3.pub, 0, rai::test_genesis_key));
	ASSERT_EQ (rai::process_result::fork, ledger.process (transaction, block2).code);
}

TEST (ledger, receive_fork)
{
	bool init (false);
	rai::block_store store (init, rai::unique_path ());
	ASSERT_TRUE (!init);
	rai::stat stats;
	rai::ledger ledger (store, stats);
	rai::keypair key2;
	rai::keypair key3;
	rai::genesis genesis;
	rai::transaction transaction (store.environment, nullptr, true);
	genesis.initialize (transaction, store);
	rai::account_info info1;
	ASSERT_FALSE (store.account_get (transaction, rai::test_genesis_key.pub, info1));
	rai::state_block block (::ledger_create_send_state_block_helper (genesis.block (), key2.pub, 100, rai::test_genesis_key));
	ASSERT_EQ (rai::process_result::progress, ledger.process (transaction, block).code);
	rai::state_block block2 (::ledger_create_open_state_block_helper (block, key2.pub, key2.pub, rai::genesis_amount - 100, key2));
	ASSERT_EQ (rai::process_result::progress, ledger.process (transaction, block2).code);
	rai::state_block block3 (::ledger_create_change_state_block_helper (block2, key3.pub, key2));
	ASSERT_EQ (rai::process_result::progress, ledger.process (transaction, block3).code);
	rai::state_block block4 (::ledger_create_send_state_block_helper (block, key2.pub, 1, rai::test_genesis_key));
	ASSERT_EQ (rai::process_result::progress, ledger.process (transaction, block4).code);
	rai::state_block block5 (::ledger_create_receive_state_block_helper (block2, block4, 1, key2));
	ASSERT_EQ (rai::process_result::fork, ledger.process (transaction, block5).code);
}

TEST (ledger, open_fork)
{
	bool init (false);
	rai::block_store store (init, rai::unique_path ());
	ASSERT_TRUE (!init);
	rai::stat stats;
	rai::ledger ledger (store, stats);
	rai::keypair key2;
	rai::keypair key3;
	rai::genesis genesis;
	rai::transaction transaction (store.environment, nullptr, true);
	genesis.initialize (transaction, store);
	rai::account_info info1;
	ASSERT_FALSE (store.account_get (transaction, rai::test_genesis_key.pub, info1));
	rai::state_block block (::ledger_create_send_state_block_helper (genesis.block (), key2.pub, 100, rai::test_genesis_key));
	ASSERT_EQ (rai::process_result::progress, ledger.process (transaction, block).code);
	rai::state_block block2 (::ledger_create_open_state_block_helper (block, key2.pub, key2.pub, rai::genesis_amount - 100, key2));
	ASSERT_EQ (rai::process_result::progress, ledger.process (transaction, block2).code);
	rai::state_block block3 (::ledger_create_open_state_block_helper (block, key3.pub, key2.pub, rai::genesis_amount - 100, key2));
	ASSERT_EQ (rai::process_result::fork, ledger.process (transaction, block3).code);
}

TEST (ledger, checksum_single)
{
	bool init (false);
	rai::block_store store (init, rai::unique_path ());
	ASSERT_TRUE (!init);
	rai::genesis genesis;
	rai::transaction transaction (store.environment, nullptr, true);
	genesis.initialize (transaction, store);
	rai::stat stats;
	rai::ledger ledger (store, stats);
	store.checksum_put (transaction, 0, 0, genesis.hash ());
	ASSERT_EQ (genesis.hash (), ledger.checksum (transaction, 0, std::numeric_limits<rai::uint256_t>::max ()));
	ASSERT_EQ (genesis.hash (), ledger.latest (transaction, rai::test_genesis_key.pub));
	rai::state_block block1 (::ledger_create_change_state_block_helper (genesis.block (), rai::account (1), rai::test_genesis_key));
	rai::checksum check1 (ledger.checksum (transaction, 0, std::numeric_limits<rai::uint256_t>::max ()));
	ASSERT_EQ (genesis.hash (), check1);
	ASSERT_EQ (rai::process_result::progress, ledger.process (transaction, block1).code);
	rai::checksum check2 (ledger.checksum (transaction, 0, std::numeric_limits<rai::uint256_t>::max ()));
	ASSERT_EQ (block1.hash (), check2);
}

TEST (ledger, checksum_two)
{
	bool init (false);
	rai::block_store store (init, rai::unique_path ());
	ASSERT_TRUE (!init);
	rai::genesis genesis;
	rai::transaction transaction (store.environment, nullptr, true);
	genesis.initialize (transaction, store);
	rai::stat stats;
	rai::ledger ledger (store, stats);
	store.checksum_put (transaction, 0, 0, genesis.hash ());
	rai::keypair key2;
	ASSERT_EQ (genesis.hash (), ledger.latest (transaction, rai::test_genesis_key.pub));
	rai::state_block block1 (::ledger_create_send_state_block_helper (genesis.block (), key2.pub, 100, rai::test_genesis_key));
	ASSERT_EQ (rai::process_result::progress, ledger.process (transaction, block1).code);
	rai::checksum check1 (ledger.checksum (transaction, 0, std::numeric_limits<rai::uint256_t>::max ()));
	rai::state_block block2 (::ledger_create_open_state_block_helper (block1, 1, key2.pub, rai::genesis_amount - 100, key2));
	ASSERT_EQ (rai::process_result::progress, ledger.process (transaction, block2).code);
	rai::checksum check2 (ledger.checksum (transaction, 0, std::numeric_limits<rai::uint256_t>::max ()));
	ASSERT_EQ (check1, check2 ^ block2.hash ());
}

TEST (ledger, DISABLED_checksum_range)
{
	bool init (false);
	rai::block_store store (init, rai::unique_path ());
	ASSERT_TRUE (!init);
	rai::stat stats;
	rai::ledger ledger (store, stats);
	rai::transaction transaction (store.environment, nullptr, false);
	rai::checksum check1 (ledger.checksum (transaction, 0, std::numeric_limits<rai::uint256_t>::max ()));
	ASSERT_TRUE (check1.is_zero ());
	rai::block_hash hash1 (42);
	rai::checksum check2 (ledger.checksum (transaction, 0, 42));
	ASSERT_TRUE (check2.is_zero ());
	rai::checksum check3 (ledger.checksum (transaction, 42, std::numeric_limits<rai::uint256_t>::max ()));
	ASSERT_EQ (hash1, check3);
}

TEST (system, generate_send_existing)
{
	rai::system system (24000, 1);
	rai::thread_runner runner (system.service, system.nodes[0]->config.io_threads);
	system.wallet (0)->insert_adhoc (rai::test_genesis_key.prv);
	rai::keypair stake_preserver;
	rai::amount_t balance = rai::genesis_amount;
	rai::amount_t amount = balance / 3 * 2;
	balance = balance - amount;
	auto send_block (system.wallet (0)->send_action (rai::genesis_account, stake_preserver.pub, amount, true));
	rai::account_info info1;
	{
		rai::transaction transaction (system.wallet (0)->store.environment, nullptr, false);
		ASSERT_FALSE (system.nodes[0]->store.account_get (transaction, rai::test_genesis_key.pub, info1));
	}
	std::vector<rai::account> accounts;
	accounts.push_back (rai::test_genesis_key.pub);
	system.generate_send_existing (*system.nodes[0], accounts);
	// Have stake_preserver receive funds after generate_send_existing so it isn't chosen as the destination
	{
		rai::transaction transaction (system.nodes[0]->store.environment, nullptr, true);
		auto open_block (std::make_shared<rai::state_block> (::ledger_create_open_state_block_helper (*(dynamic_cast<rai::state_block*>(send_block.get ())), rai::genesis_account, stake_preserver.pub, amount, stake_preserver)));
		system.nodes[0]->work_generate_blocking (*open_block);
		ASSERT_EQ (rai::process_result::progress, system.nodes[0]->ledger.process (transaction, *open_block).code);
	}
	ASSERT_GT (system.nodes[0]->balance (stake_preserver.pub), system.nodes[0]->balance (rai::genesis_account));
	rai::account_info info2;
	{
		rai::transaction transaction (system.wallet (0)->store.environment, nullptr, false);
		ASSERT_FALSE (system.nodes[0]->store.account_get (transaction, rai::test_genesis_key.pub, info2));
	}
	ASSERT_NE (info1.head, info2.head);
	system.deadline_set (15s);
	while (info2.block_count < info1.block_count + 2)
	{
		ASSERT_NO_ERROR (system.poll ());
		rai::transaction transaction (system.wallet (0)->store.environment, nullptr, false);
		ASSERT_FALSE (system.nodes[0]->store.account_get (transaction, rai::test_genesis_key.pub, info2));
	}
	ASSERT_EQ (info1.block_count + 2, info2.block_count);
	ASSERT_EQ (info2.balance, rai::genesis_amount / 3);
	{
		rai::transaction transaction (system.wallet (0)->store.environment, nullptr, false);
		ASSERT_NE (system.nodes[0]->ledger.amount (transaction, info2.head), 0);
	}
	system.stop ();
	runner.join ();
}

TEST (system, generate_send_new)
{
	rai::system system (24000, 1);
	rai::thread_runner runner (system.service, system.nodes[0]->config.io_threads);
	system.wallet (0)->insert_adhoc (rai::test_genesis_key.prv);
	{
		rai::transaction transaction (system.nodes[0]->store.environment, nullptr, false);
		auto iterator1 (system.nodes[0]->store.latest_begin (transaction));
		ASSERT_NE (system.nodes[0]->store.latest_end (), iterator1);
		++iterator1;
		ASSERT_EQ (system.nodes[0]->store.latest_end (), iterator1);
	}
	rai::keypair stake_preserver;
	rai::amount_t balance = rai::genesis_amount;
	rai::amount_t amount = (balance / 3) * 2;
	balance = balance - amount;
	auto send_block (system.wallet (0)->send_action (rai::genesis_account, stake_preserver.pub, amount, true));
	{
		rai::transaction transaction (system.nodes[0]->store.environment, nullptr, true);
		auto open_block (std::make_shared<rai::state_block> (::ledger_create_open_state_block_helper (*(dynamic_cast<rai::state_block*>(send_block.get ())), rai::genesis_account, stake_preserver.pub, amount, stake_preserver)));
		system.nodes[0]->work_generate_blocking (*open_block);
		ASSERT_EQ (rai::process_result::progress, system.nodes[0]->ledger.process (transaction, *open_block).code);
	}
	ASSERT_GT (system.nodes[0]->balance (stake_preserver.pub), system.nodes[0]->balance (rai::genesis_account));
	std::vector<rai::account> accounts;
	accounts.push_back (rai::test_genesis_key.pub);
	system.generate_send_new (*system.nodes[0], accounts);
	rai::account new_account (0);
	{
		rai::transaction transaction (system.nodes[0]->store.environment, nullptr, false);
		auto iterator2 (system.wallet (0)->store.begin (transaction));
		if (iterator2->first.uint256 () != rai::test_genesis_key.pub)
		{
			new_account = iterator2->first.uint256 ();
		}
		++iterator2;
		ASSERT_NE (system.wallet (0)->store.end (), iterator2);
		if (iterator2->first.uint256 () != rai::test_genesis_key.pub)
		{
			new_account = iterator2->first.uint256 ();
		}
		++iterator2;
		ASSERT_EQ (system.wallet (0)->store.end (), iterator2);
		ASSERT_FALSE (new_account.is_zero ());
	}
	system.deadline_set (10s);
	while (system.nodes[0]->balance (new_account) == 0)
	{
		ASSERT_NO_ERROR (system.poll ());
	}
	system.stop ();
	runner.join ();
}

TEST (ledger, representation)
{
	bool init (false);
	rai::block_store store (init, rai::unique_path ());
	ASSERT_TRUE (!init);
	rai::stat stats;
	rai::ledger ledger (store, stats);
	rai::genesis genesis;
	rai::transaction transaction (store.environment, nullptr, true);
	genesis.initialize (transaction, store);
	ASSERT_EQ (rai::genesis_amount, store.representation_get (transaction, rai::test_genesis_key.pub));
	rai::keypair key2;
	rai::state_block block1 (::ledger_create_send_state_block_helper (genesis.block (), key2.pub, rai::genesis_amount - 100, rai::test_genesis_key));
	ASSERT_EQ (rai::process_result::progress, ledger.process (transaction, block1).code);
	ASSERT_EQ (rai::genesis_amount - 100, store.representation_get (transaction, rai::test_genesis_key.pub));
	rai::keypair key3;
	rai::state_block block2 (::ledger_create_open_state_block_helper (block1, key3.pub, key2.pub, 100, key2));
	ASSERT_EQ (rai::process_result::progress, ledger.process (transaction, block2).code);
	ASSERT_EQ (rai::genesis_amount - 100, store.representation_get (transaction, rai::test_genesis_key.pub));
	ASSERT_EQ (0, store.representation_get (transaction, key2.pub));
	ASSERT_EQ (100, store.representation_get (transaction, key3.pub));
	rai::state_block block3 (::ledger_create_send_state_block_helper (block1, key2.pub, rai::genesis_amount - 200, rai::test_genesis_key));
	ASSERT_EQ (rai::process_result::progress, ledger.process (transaction, block3).code);
	ASSERT_EQ (rai::genesis_amount - 200, store.representation_get (transaction, rai::test_genesis_key.pub));
	ASSERT_EQ (0, store.representation_get (transaction, key2.pub));
	ASSERT_EQ (100, store.representation_get (transaction, key3.pub));
	rai::state_block block4 (::ledger_create_receive_state_block_helper (block2, block3, 200, key2));
	ASSERT_EQ (rai::process_result::progress, ledger.process (transaction, block4).code);
	ASSERT_EQ (rai::genesis_amount - 200, store.representation_get (transaction, rai::test_genesis_key.pub));
	ASSERT_EQ (0, store.representation_get (transaction, key2.pub));
	ASSERT_EQ (200, store.representation_get (transaction, key3.pub));
	rai::keypair key4;
	rai::state_block block5 (::ledger_create_change_state_block_helper (block4, key4.pub, key2));
	ASSERT_EQ (rai::process_result::progress, ledger.process (transaction, block5).code);
	ASSERT_EQ (rai::genesis_amount - 200, store.representation_get (transaction, rai::test_genesis_key.pub));
	ASSERT_EQ (0, store.representation_get (transaction, key2.pub));
	ASSERT_EQ (0, store.representation_get (transaction, key3.pub));
	ASSERT_EQ (200, store.representation_get (transaction, key4.pub));
	rai::keypair key5;
	rai::state_block block6 (::ledger_create_send_state_block_helper (block5, key5.pub, 100, key2));
	ASSERT_EQ (rai::process_result::progress, ledger.process (transaction, block6).code);
	ASSERT_EQ (rai::genesis_amount - 200, store.representation_get (transaction, rai::test_genesis_key.pub));
	ASSERT_EQ (0, store.representation_get (transaction, key2.pub));
	ASSERT_EQ (0, store.representation_get (transaction, key3.pub));
	ASSERT_EQ (100, store.representation_get (transaction, key4.pub));
	ASSERT_EQ (0, store.representation_get (transaction, key5.pub));
	rai::keypair key6;
	rai::state_block block7 (::ledger_create_open_state_block_helper (block6, key6.pub, key5.pub, 100, key5));
	ASSERT_EQ (rai::process_result::progress, ledger.process (transaction, block7).code);
	ASSERT_EQ (rai::genesis_amount - 200, store.representation_get (transaction, rai::test_genesis_key.pub));
	ASSERT_EQ (0, store.representation_get (transaction, key2.pub));
	ASSERT_EQ (0, store.representation_get (transaction, key3.pub));
	ASSERT_EQ (100, store.representation_get (transaction, key4.pub));
	ASSERT_EQ (0, store.representation_get (transaction, key5.pub));
	ASSERT_EQ (100, store.representation_get (transaction, key6.pub));
	rai::state_block block8 (::ledger_create_send_state_block_helper (block6, key5.pub, 0, key2));
	ASSERT_EQ (rai::process_result::progress, ledger.process (transaction, block8).code);
	ASSERT_EQ (rai::genesis_amount - 200, store.representation_get (transaction, rai::test_genesis_key.pub));
	ASSERT_EQ (0, store.representation_get (transaction, key2.pub));
	ASSERT_EQ (0, store.representation_get (transaction, key3.pub));
	ASSERT_EQ (0, store.representation_get (transaction, key4.pub));
	ASSERT_EQ (0, store.representation_get (transaction, key5.pub));
	ASSERT_EQ (100, store.representation_get (transaction, key6.pub));
	rai::state_block block9 (::ledger_create_receive_state_block_helper (block7, block8, 200, key5));
	ASSERT_EQ (rai::process_result::progress, ledger.process (transaction, block9).code);
	ASSERT_EQ (rai::genesis_amount - 200, store.representation_get (transaction, rai::test_genesis_key.pub));
	ASSERT_EQ (0, store.representation_get (transaction, key2.pub));
	ASSERT_EQ (0, store.representation_get (transaction, key3.pub));
	ASSERT_EQ (0, store.representation_get (transaction, key4.pub));
	ASSERT_EQ (0, store.representation_get (transaction, key5.pub));
	ASSERT_EQ (200, store.representation_get (transaction, key6.pub));
}

TEST (ledger, double_open)
{
	bool init (false);
	rai::block_store store (init, rai::unique_path ());
	ASSERT_TRUE (!init);
	rai::stat stats;
	rai::ledger ledger (store, stats);
	rai::genesis genesis;
	rai::transaction transaction (store.environment, nullptr, true);
	genesis.initialize (transaction, store);
	rai::keypair key2;
	rai::state_block send1 (::ledger_create_send_state_block_helper (genesis.block (), key2.pub, 1, rai::test_genesis_key));
	ASSERT_EQ (rai::process_result::progress, ledger.process (transaction, send1).code);
	rai::state_block open1 (::ledger_create_open_state_block_helper (send1, key2.pub, key2.pub, rai::genesis_amount - 1, key2));
	ASSERT_EQ (rai::process_result::progress, ledger.process (transaction, open1).code);
	rai::state_block open2 (::ledger_create_open_state_block_helper ( send1, rai::test_genesis_key.pub, key2.pub, rai::genesis_amount - 1, key2));
	ASSERT_EQ (rai::process_result::fork, ledger.process (transaction, open2).code);
}

TEST (ledger, double_receive)
{
	bool init (false);
	rai::block_store store (init, rai::unique_path ());
	ASSERT_TRUE (!init);
	rai::stat stats;
	rai::ledger ledger (store, stats);
	rai::genesis genesis;
	rai::transaction transaction (store.environment, nullptr, true);
	genesis.initialize (transaction, store);
	rai::keypair key2;
	rai::state_block send1 (::ledger_create_send_state_block_helper (genesis.block (), key2.pub, 10, rai::test_genesis_key));
	ASSERT_EQ (rai::process_result::progress, ledger.process (transaction, send1).code);
	rai::state_block open1 (::ledger_create_open_state_block_helper (send1, key2.pub, key2.pub, rai::genesis_amount - 10, key2));
	ASSERT_EQ (rai::process_result::progress, ledger.process (transaction, open1).code);
	rai::state_block receive1 (::ledger_create_receive_state_block_helper (open1, send1, rai::genesis_amount, key2));
	ASSERT_EQ (rai::process_result::unreceivable, ledger.process (transaction, receive1).code);
}

TEST (votes, check_signature)
{
	rai::system system (24000, 1);
	auto & node1 (*system.nodes[0]);
	rai::genesis genesis;
	rai::keypair key1;
	auto send1 (std::make_shared<rai::state_block> (::ledger_create_send_state_block_helper (genesis.block (), key1.pub, rai::genesis_amount - 100, rai::test_genesis_key)));
	rai::transaction transaction (node1.store.environment, nullptr, true);
	ASSERT_EQ (rai::process_result::progress, node1.ledger.process (transaction, *send1).code);
	auto node_l (system.nodes[0]);
	node1.active.start (send1);
	auto votes1 (node1.active.roots.find (send1->root ())->election);
	ASSERT_EQ (1, votes1->last_votes.size ());
	auto vote1 (std::make_shared<rai::vote> (rai::test_genesis_key.pub, rai::test_genesis_key.prv, 1, send1));
	vote1->signature.bytes[0] ^= 1;
	ASSERT_EQ (rai::vote_code::invalid, node1.vote_processor.vote_blocking (transaction, vote1, rai::endpoint (boost::asio::ip::address_v6 (), 0)));
	vote1->signature.bytes[0] ^= 1;
	ASSERT_EQ (rai::vote_code::vote, node1.vote_processor.vote_blocking (transaction, vote1, rai::endpoint (boost::asio::ip::address_v6 (), 0)));
	ASSERT_EQ (rai::vote_code::replay, node1.vote_processor.vote_blocking (transaction, vote1, rai::endpoint (boost::asio::ip::address_v6 (), 0)));
}

TEST (votes, add_one)
{
	rai::system system (24000, 1);
	auto & node1 (*system.nodes[0]);
	rai::genesis genesis;
	rai::keypair key1;
	auto send1 (std::make_shared<rai::state_block> (::ledger_create_send_state_block_helper (genesis.block (), key1.pub, rai::genesis_amount - 100, rai::test_genesis_key)));
	{
		rai::transaction transaction (node1.store.environment, nullptr, true);
		ASSERT_EQ (rai::process_result::progress, node1.ledger.process (transaction, *send1).code);
	}
	auto node_l (system.nodes[0]);
	node1.active.start (send1);
	auto votes1 (node1.active.roots.find (send1->root ())->election);
	ASSERT_EQ (1, votes1->last_votes.size ());
	auto vote1 (std::make_shared<rai::vote> (rai::test_genesis_key.pub, rai::test_genesis_key.prv, 1, send1));
	ASSERT_FALSE (node1.active.vote (vote1));
	auto vote2 (std::make_shared<rai::vote> (rai::test_genesis_key.pub, rai::test_genesis_key.prv, 2, send1));
	ASSERT_FALSE (node1.active.vote (vote2));
	ASSERT_EQ (2, votes1->last_votes.size ());
	auto existing1 (votes1->last_votes.find (rai::test_genesis_key.pub));
	ASSERT_NE (votes1->last_votes.end (), existing1);
	ASSERT_EQ (send1->hash (), existing1->second.hash);
	rai::transaction transaction (system.nodes[0]->store.environment, nullptr, false);
	auto winner (*votes1->tally (transaction).begin ());
	ASSERT_EQ (*send1, *winner.second);
	ASSERT_EQ (rai::genesis_amount - 100, winner.first);
}

TEST (votes, add_two)
{
	rai::system system (24000, 1);
	auto & node1 (*system.nodes[0]);
	rai::genesis genesis;
	rai::keypair key1;
	auto send1 (std::make_shared<rai::state_block> (::ledger_create_send_state_block_helper (genesis.block (), key1.pub, rai::genesis_amount - 100, rai::test_genesis_key)));
	{
		rai::transaction transaction (node1.store.environment, nullptr, true);
		ASSERT_EQ (rai::process_result::progress, node1.ledger.process (transaction, *send1).code);
	}
	auto node_l (system.nodes[0]);
	node1.active.start (send1);
	auto votes1 (node1.active.roots.find (send1->root ())->election);
	auto vote1 (std::make_shared<rai::vote> (rai::test_genesis_key.pub, rai::test_genesis_key.prv, 1, send1));
	ASSERT_FALSE (node1.active.vote (vote1));
	rai::keypair key2;
	auto send2 (std::make_shared<rai::state_block> (::ledger_create_send_state_block_helper (genesis.block (), key2.pub, 0, rai::test_genesis_key)));
	auto vote2 (std::make_shared<rai::vote> (key2.pub, key2.prv, 1, send2));
	ASSERT_FALSE (node1.active.vote (vote2));
	ASSERT_EQ (3, votes1->last_votes.size ());
	ASSERT_NE (votes1->last_votes.end (), votes1->last_votes.find (rai::test_genesis_key.pub));
	ASSERT_EQ (send1->hash (), votes1->last_votes[rai::test_genesis_key.pub].hash);
	ASSERT_NE (votes1->last_votes.end (), votes1->last_votes.find (key2.pub));
	ASSERT_EQ (send2->hash (), votes1->last_votes[key2.pub].hash);
	rai::transaction transaction (system.nodes[0]->store.environment, nullptr, false);
	auto winner (*votes1->tally (transaction).begin ());
	ASSERT_EQ (*send1, *winner.second);
}

// Higher sequence numbers change the vote
TEST (votes, add_existing)
{
	rai::system system (24000, 1);
	auto & node1 (*system.nodes[0]);
	rai::genesis genesis;
	rai::keypair key1;
	auto send1 (std::make_shared<rai::state_block> (::ledger_create_send_state_block_helper (genesis.block (), key1.pub, rai::genesis_amount - rai::Gxrb_ratio, rai::test_genesis_key)));
	{
		rai::transaction transaction (node1.store.environment, nullptr, true);
		ASSERT_EQ (rai::process_result::progress, node1.ledger.process (transaction, *send1).code);
	}
	auto node_l (system.nodes[0]);
	node1.active.start (send1);
	auto votes1 (node1.active.roots.find (send1->root ())->election);
	auto vote1 (std::make_shared<rai::vote> (rai::test_genesis_key.pub, rai::test_genesis_key.prv, 1, send1));
	ASSERT_FALSE (node1.active.vote (vote1));
	ASSERT_FALSE (node1.active.publish (send1));
	ASSERT_EQ (1, votes1->last_votes[rai::test_genesis_key.pub].sequence);
	rai::keypair key2;
	auto send2 (std::make_shared<rai::state_block> (::ledger_create_send_state_block_helper (genesis.block (), key2.pub, rai::genesis_amount - rai::Gxrb_ratio, rai::test_genesis_key)));
	auto vote2 (std::make_shared<rai::vote> (rai::test_genesis_key.pub, rai::test_genesis_key.prv, 2, send2));
	// Pretend we've waited the timeout
	votes1->last_votes[rai::test_genesis_key.pub].time = std::chrono::steady_clock::now () - std::chrono::seconds (20);
	ASSERT_FALSE (node1.active.vote (vote2));
	ASSERT_FALSE (node1.active.publish (send2));
	ASSERT_EQ (2, votes1->last_votes[rai::test_genesis_key.pub].sequence);
	// Also resend the old vote, and see if we respect the sequence number
	votes1->last_votes[rai::test_genesis_key.pub].time = std::chrono::steady_clock::now () - std::chrono::seconds (20);
	ASSERT_TRUE (node1.active.vote (vote1));
	ASSERT_EQ (2, votes1->last_votes[rai::test_genesis_key.pub].sequence);
	ASSERT_EQ (2, votes1->last_votes.size ());
	ASSERT_NE (votes1->last_votes.end (), votes1->last_votes.find (rai::test_genesis_key.pub));
	ASSERT_EQ (send2->hash (), votes1->last_votes[rai::test_genesis_key.pub].hash);
	rai::transaction transaction (system.nodes[0]->store.environment, nullptr, false);
	auto winner (*votes1->tally (transaction).begin ());
	ASSERT_EQ (*send2, *winner.second);
}

// Lower sequence numbers are ignored
TEST (votes, add_old)
{
	rai::system system (24000, 1);
	auto & node1 (*system.nodes[0]);
	rai::genesis genesis;
	rai::keypair key1;
	auto send1 (std::make_shared<rai::state_block> (ledger_create_send_state_block_helper (genesis.block (), key1.pub, 0, rai::test_genesis_key)));
	rai::transaction transaction (node1.store.environment, nullptr, true);
	ASSERT_EQ (rai::process_result::progress, node1.ledger.process (transaction, *send1).code);
	auto node_l (system.nodes[0]);
	node1.active.start (send1);
	auto votes1 (node1.active.roots.find (send1->root ())->election);
	auto vote1 (std::make_shared<rai::vote> (rai::test_genesis_key.pub, rai::test_genesis_key.prv, 2, send1));
	node1.vote_processor.vote_blocking (transaction, vote1, node1.network.endpoint ());
	rai::keypair key2;
	auto send2 (std::make_shared<rai::state_block> (::ledger_create_send_state_block_helper (genesis.block (), key2.pub, 0, rai::test_genesis_key)));
	auto vote2 (std::make_shared<rai::vote> (rai::test_genesis_key.pub, rai::test_genesis_key.prv, 1, send2));
	votes1->last_votes[rai::test_genesis_key.pub].time = std::chrono::steady_clock::now () - std::chrono::seconds (20);
	node1.vote_processor.vote_blocking (transaction, vote2, node1.network.endpoint ());
	ASSERT_EQ (2, votes1->last_votes.size ());
	ASSERT_NE (votes1->last_votes.end (), votes1->last_votes.find (rai::test_genesis_key.pub));
	ASSERT_EQ (send1->hash (), votes1->last_votes[rai::test_genesis_key.pub].hash);
	auto winner (*votes1->tally (transaction).begin ());
	ASSERT_EQ (*send1, *winner.second);
}

// Lower sequence numbers are accepted for different accounts
TEST (votes, add_old_different_account)
{
	rai::system system (24000, 1);
	auto & node1 (*system.nodes[0]);
	rai::genesis genesis;
	rai::keypair key1;
	auto send1 (std::make_shared<rai::state_block> (::ledger_create_send_state_block_helper (genesis.block (), key1.pub, 1, rai::test_genesis_key)));
	auto send2 (std::make_shared<rai::state_block> (::ledger_create_send_state_block_helper (*send1, key1.pub, 0, rai::test_genesis_key)));
	{
		rai::transaction transaction (node1.store.environment, nullptr, true);
		ASSERT_EQ (rai::process_result::progress, node1.ledger.process (transaction, *send1).code);
		ASSERT_EQ (rai::process_result::progress, node1.ledger.process (transaction, *send2).code);
	}
	auto node_l (system.nodes[0]);
	node1.active.start (send1);
	node1.active.start (send2);
	auto votes1 (node1.active.roots.find (send1->root ())->election);
	auto votes2 (node1.active.roots.find (send2->root ())->election);
	ASSERT_EQ (1, votes1->last_votes.size ());
	ASSERT_EQ (1, votes2->last_votes.size ());
	auto vote1 (std::make_shared<rai::vote> (rai::test_genesis_key.pub, rai::test_genesis_key.prv, 2, send1));
	rai::transaction transaction (system.nodes[0]->store.environment, nullptr, false);
	auto vote_result1 (node1.vote_processor.vote_blocking (transaction, vote1, node1.network.endpoint ()));
	ASSERT_EQ (rai::vote_code::vote, vote_result1);
	ASSERT_EQ (2, votes1->last_votes.size ());
	ASSERT_EQ (1, votes2->last_votes.size ());
	auto vote2 (std::make_shared<rai::vote> (rai::test_genesis_key.pub, rai::test_genesis_key.prv, 1, send2));
	auto vote_result2 (node1.vote_processor.vote_blocking (transaction, vote2, node1.network.endpoint ()));
	ASSERT_EQ (rai::vote_code::vote, vote_result2);
	ASSERT_EQ (2, votes1->last_votes.size ());
	ASSERT_EQ (2, votes2->last_votes.size ());
	ASSERT_NE (votes1->last_votes.end (), votes1->last_votes.find (rai::test_genesis_key.pub));
	ASSERT_NE (votes2->last_votes.end (), votes2->last_votes.find (rai::test_genesis_key.pub));
	ASSERT_EQ (send1->hash (), votes1->last_votes[rai::test_genesis_key.pub].hash);
	ASSERT_EQ (send2->hash (), votes2->last_votes[rai::test_genesis_key.pub].hash);
	auto winner1 (*votes1->tally (transaction).begin ());
	ASSERT_EQ (*send1, *winner1.second);
	auto winner2 (*votes2->tally (transaction).begin ());
	ASSERT_EQ (*send2, *winner2.second);
}

// The voting cooldown is respected
TEST (votes, add_cooldown)
{
	rai::system system (24000, 1);
	auto & node1 (*system.nodes[0]);
	rai::genesis genesis;
	rai::keypair key1;
	auto send1 (std::make_shared<rai::state_block> (::ledger_create_send_state_block_helper (genesis.block (), key1.pub, 0, rai::test_genesis_key)));
	rai::transaction transaction (node1.store.environment, nullptr, true);
	ASSERT_EQ (rai::process_result::progress, node1.ledger.process (transaction, *send1).code);
	auto node_l (system.nodes[0]);
	node1.active.start (send1);
	auto votes1 (node1.active.roots.find (send1->root ())->election);
	auto vote1 (std::make_shared<rai::vote> (rai::test_genesis_key.pub, rai::test_genesis_key.prv, 1, send1));
	node1.vote_processor.vote_blocking (transaction, vote1, node1.network.endpoint ());
	rai::keypair key2;
	auto send2 (std::make_shared<rai::state_block> (::ledger_create_send_state_block_helper (genesis.block (), key2.pub, 0, rai::test_genesis_key)));
	auto vote2 (std::make_shared<rai::vote> (rai::test_genesis_key.pub, rai::test_genesis_key.prv, 2, send2));
	node1.vote_processor.vote_blocking (transaction, vote2, node1.network.endpoint ());
	ASSERT_EQ (2, votes1->last_votes.size ());
	ASSERT_NE (votes1->last_votes.end (), votes1->last_votes.find (rai::test_genesis_key.pub));
	ASSERT_EQ (send1->hash (), votes1->last_votes[rai::test_genesis_key.pub].hash);
	auto winner (*votes1->tally (transaction).begin ());
	ASSERT_EQ (*send1, *winner.second);
}

// Query for block successor
TEST (ledger, successor)
{
	rai::system system (24000, 1);
	rai::keypair key1;
	rai::genesis genesis;
	rai::state_block send1 = ::ledger_create_send_state_block_helper (genesis.block (), key1.pub, 0, rai::test_genesis_key);
	rai::transaction transaction (system.nodes[0]->store.environment, nullptr, true);
	ASSERT_EQ (rai::process_result::progress, system.nodes[0]->ledger.process (transaction, send1).code);
	ASSERT_EQ (send1, *system.nodes[0]->ledger.successor (transaction, genesis.hash ()));
	ASSERT_EQ (*genesis.genesis_block, *system.nodes[0]->ledger.successor (transaction, genesis.root ()));
	ASSERT_EQ (nullptr, system.nodes[0]->ledger.successor (transaction, 0));
}

TEST (ledger, fail_change_old)
{
	bool init (false);
	rai::block_store store (init, rai::unique_path ());
	ASSERT_FALSE (init);
	rai::stat stats;
	rai::ledger ledger (store, stats);
	rai::genesis genesis;
	rai::transaction transaction (store.environment, nullptr, true);
	genesis.initialize (transaction, store);
	rai::keypair key1;
	rai::state_block block (::ledger_create_change_state_block_helper (genesis.block (), key1.pub, rai::test_genesis_key));
	auto result1 (ledger.process (transaction, block));
	ASSERT_EQ (rai::process_result::progress, result1.code);
	auto result2 (ledger.process (transaction, block));
	ASSERT_EQ (rai::process_result::old, result2.code);
}

TEST (ledger, fail_change_gap_previous)
{
	bool init (false);
	rai::block_store store (init, rai::unique_path ());
	ASSERT_FALSE (init);
	rai::stat stats;
	rai::ledger ledger (store, stats);
	rai::genesis genesis;
	rai::transaction transaction (store.environment, nullptr, true);
	genesis.initialize (transaction, store);
	rai::keypair key1;
	// open
	rai::state_block block (rai::test_genesis_key.pub, 1, 0, key1.pub, rai::genesis_amount, 0, rai::test_genesis_key.prv, rai::test_genesis_key.pub, 0);
	auto result1 (ledger.process (transaction, block));
	ASSERT_EQ (rai::process_result::gap_previous, result1.code);
}

TEST (ledger, fail_change_bad_signature)
{
	bool init (false);
	rai::block_store store (init, rai::unique_path ());
	ASSERT_FALSE (init);
	rai::stat stats;
	rai::ledger ledger (store, stats);
	rai::genesis genesis;
	rai::transaction transaction (store.environment, nullptr, true);
	genesis.initialize (transaction, store);
	rai::keypair key1;
	// change
	rai::state_block block (0, genesis.hash (), 0, key1.pub, rai::genesis_amount, 0, rai::keypair ().prv, 0, 0);
	auto result1 (ledger.process (transaction, block));
	ASSERT_EQ (rai::process_result::bad_signature, result1.code);
}

TEST (ledger, fail_change_fork)
{
	bool init (false);
	rai::block_store store (init, rai::unique_path ());
	ASSERT_FALSE (init);
	rai::stat stats;
	rai::ledger ledger (store, stats);
	rai::genesis genesis;
	rai::transaction transaction (store.environment, nullptr, true);
	genesis.initialize (transaction, store);
	rai::keypair key1;
	rai::state_block block1 (::ledger_create_change_state_block_helper (genesis.block (), key1.pub, rai::test_genesis_key));
	auto result1 (ledger.process (transaction, block1));
	ASSERT_EQ (rai::process_result::progress, result1.code);
	rai::keypair key2;
	rai::state_block block2 (::ledger_create_change_state_block_helper (genesis.block (), key2.pub, rai::test_genesis_key));
	auto result2 (ledger.process (transaction, block2));
	ASSERT_EQ (rai::process_result::fork, result2.code);
}

TEST (ledger, fail_send_old)
{
	bool init (false);
	rai::block_store store (init, rai::unique_path ());
	ASSERT_FALSE (init);
	rai::stat stats;
	rai::ledger ledger (store, stats);
	rai::genesis genesis;
	rai::transaction transaction (store.environment, nullptr, true);
	genesis.initialize (transaction, store);
	rai::keypair key1;
	rai::state_block block (::ledger_create_send_state_block_helper (genesis.block (), key1.pub, 1, rai::test_genesis_key));
	auto result1 (ledger.process (transaction, block));
	ASSERT_EQ (rai::process_result::progress, result1.code);
	auto result2 (ledger.process (transaction, block));
	ASSERT_EQ (rai::process_result::old, result2.code);
}

TEST (ledger, fail_send_gap_previous)
{
	bool init (false);
	rai::block_store store (init, rai::unique_path ());
	ASSERT_FALSE (init);
	rai::stat stats;
	rai::ledger ledger (store, stats);
	rai::genesis genesis;
	rai::transaction transaction (store.environment, nullptr, true);
	genesis.initialize (transaction, store);
	rai::keypair key1;
	rai::state_block block (rai::genesis_account, 1, 0, rai::genesis_account, rai::genesis_amount - 1, key1.pub, rai::test_genesis_key.prv, rai::test_genesis_key.pub, 0);
	auto result1 (ledger.process (transaction, block));
	ASSERT_EQ (rai::process_result::gap_previous, result1.code);
}

TEST (ledger, fail_send_bad_signature)
{
	bool init (false);
	rai::block_store store (init, rai::unique_path ());
	ASSERT_FALSE (init);
	rai::stat stats;
	rai::ledger ledger (store, stats);
	rai::genesis genesis;
	rai::transaction transaction (store.environment, nullptr, true);
	genesis.initialize (transaction, store);
	rai::keypair key1;
	rai::state_block block (rai::genesis_account, genesis.hash (), 0, rai::genesis_account, rai::genesis_amount - 1, key1.pub, rai::keypair ().prv, 0, 0);
	auto result1 (ledger.process (transaction, block));
	ASSERT_EQ (rai::process_result::bad_signature, result1.code);
}

TEST (ledger, fail_send_negative_spend)
{
	bool init (false);
	rai::block_store store (init, rai::unique_path ());
	ASSERT_FALSE (init);
	rai::stat stats;
	rai::ledger ledger (store, stats);
	rai::genesis genesis;
	rai::transaction transaction (store.environment, nullptr, true);
	genesis.initialize (transaction, store);
	rai::keypair key1;
	rai::state_block block1 (::ledger_create_send_state_block_helper (genesis.block (), key1.pub, 1, rai::test_genesis_key));
	ASSERT_EQ (rai::process_result::progress, ledger.process (transaction, block1).code);
	rai::keypair key2;
	// intended as a send with negative amount, but it is in fact a receive, with an invalid link (destination account interpreted as send hash)
	rai::state_block block2 (::ledger_create_send_state_block_helper (block1, key2.pub, 2, rai::test_genesis_key));
	ASSERT_EQ (rai::process_result::gap_source, ledger.process (transaction, block2).code);
}

TEST (ledger, fail_send_fork)
{
	bool init (false);
	rai::block_store store (init, rai::unique_path ());
	ASSERT_FALSE (init);
	rai::stat stats;
	rai::ledger ledger (store, stats);
	rai::genesis genesis;
	rai::transaction transaction (store.environment, nullptr, true);
	genesis.initialize (transaction, store);
	rai::keypair key1;
	rai::state_block block1 (::ledger_create_send_state_block_helper (genesis.block (), key1.pub, 2, rai::test_genesis_key));
	ASSERT_EQ (rai::process_result::progress, ledger.process (transaction, block1).code);
	rai::keypair key2;
	rai::state_block block2 (::ledger_create_send_state_block_helper (genesis.block (), key2.pub, 1, rai::test_genesis_key));
	ASSERT_EQ (rai::process_result::fork, ledger.process (transaction, block2).code);
}

TEST (ledger, fail_open_old)
{
	bool init (false);
	rai::block_store store (init, rai::unique_path ());
	ASSERT_FALSE (init);
	rai::stat stats;
	rai::ledger ledger (store, stats);
	rai::genesis genesis;
	rai::transaction transaction (store.environment, nullptr, true);
	genesis.initialize (transaction, store);
	rai::keypair key1;
	rai::state_block block1 (::ledger_create_send_state_block_helper (genesis.block (), key1.pub, 1, rai::test_genesis_key));
	ASSERT_EQ (rai::process_result::progress, ledger.process (transaction, block1).code);
	rai::state_block block2 (::ledger_create_open_state_block_helper (block1, 1, key1.pub, rai::genesis_amount - 1, key1));
	ASSERT_EQ (rai::process_result::progress, ledger.process (transaction, block2).code);
	ASSERT_EQ (rai::process_result::old, ledger.process (transaction, block2).code);
}

TEST (ledger, fail_open_gap_source)
{
	bool init (false);
	rai::block_store store (init, rai::unique_path ());
	ASSERT_FALSE (init);
	rai::stat stats;
	rai::ledger ledger (store, stats);
	rai::genesis genesis;
	rai::transaction transaction (store.environment, nullptr, true);
	genesis.initialize (transaction, store);
	rai::keypair key1;
	rai::state_block block2 (key1.pub, 0, 0, 2, 3, 1, key1.prv, key1.pub, 0);
	auto result2 (ledger.process (transaction, block2));
	ASSERT_EQ (rai::process_result::gap_source, result2.code);
}

TEST (ledger, fail_open_bad_signature)
{
	bool init (false);
	rai::block_store store (init, rai::unique_path ());
	ASSERT_FALSE (init);
	rai::stat stats;
	rai::ledger ledger (store, stats);
	rai::genesis genesis;
	rai::transaction transaction (store.environment, nullptr, true);
	genesis.initialize (transaction, store);
	rai::keypair key1;
	rai::state_block block1 (::ledger_create_send_state_block_helper (genesis.block (), key1.pub, 1, rai::test_genesis_key));
	ASSERT_EQ (rai::process_result::progress, ledger.process (transaction, block1).code);
	rai::state_block block2 (::ledger_create_open_state_block_helper (block1, 1, key1.pub, rai::genesis_amount - 1, key1));
	block2.signature.clear ();
	ASSERT_EQ (rai::process_result::bad_signature, ledger.process (transaction, block2).code);
}

TEST (ledger, fail_open_fork_previous)
{
	bool init (false);
	rai::block_store store (init, rai::unique_path ());
	ASSERT_FALSE (init);
	rai::stat stats;
	rai::ledger ledger (store, stats);
	rai::genesis genesis;
	rai::transaction transaction (store.environment, nullptr, true);
	genesis.initialize (transaction, store);
	rai::keypair key1;
	rai::state_block block1 (::ledger_create_send_state_block_helper (genesis.block (), key1.pub, 1, rai::test_genesis_key));
	ASSERT_EQ (rai::process_result::progress, ledger.process (transaction, block1).code);
	rai::state_block block2 (::ledger_create_send_state_block_helper (block1, key1.pub, 0, rai::test_genesis_key));
	ASSERT_EQ (rai::process_result::progress, ledger.process (transaction, block2).code);
	rai::state_block block3 (::ledger_create_open_state_block_helper (block1, 1, key1.pub, rai::genesis_amount - 1, key1));
	ASSERT_EQ (rai::process_result::progress, ledger.process (transaction, block3).code);
	rai::state_block block4 (::ledger_create_open_state_block_helper (block2, 1, key1.pub, 0, key1));
	ASSERT_EQ (rai::process_result::fork, ledger.process (transaction, block4).code);
}

TEST (ledger, fail_open_account_mismatch)
{
	bool init (false);
	rai::block_store store (init, rai::unique_path ());
	ASSERT_FALSE (init);
	rai::stat stats;
	rai::ledger ledger (store, stats);
	rai::genesis genesis;
	rai::transaction transaction (store.environment, nullptr, true);
	genesis.initialize (transaction, store);
	rai::keypair key1;
	rai::state_block block1 (::ledger_create_send_state_block_helper (genesis.block (), key1.pub, 1, rai::test_genesis_key));
	ASSERT_EQ (rai::process_result::progress, ledger.process (transaction, block1).code);
	rai::keypair badkey;
	rai::state_block block2 (::ledger_create_open_state_block_helper (block1, 1, badkey.pub, rai::genesis_amount - 1, badkey));
	ASSERT_NE (rai::process_result::progress, ledger.process (transaction, block2).code);
}

TEST (ledger, fail_receive_old)
{
	bool init (false);
	rai::block_store store (init, rai::unique_path ());
	ASSERT_FALSE (init);
	rai::stat stats;
	rai::ledger ledger (store, stats);
	rai::genesis genesis;
	rai::transaction transaction (store.environment, nullptr, true);
	genesis.initialize (transaction, store);
	rai::keypair key1;
	rai::state_block block1 (::ledger_create_send_state_block_helper (genesis.block (), key1.pub, 1, rai::test_genesis_key));
	ASSERT_EQ (rai::process_result::progress, ledger.process (transaction, block1).code);
	rai::state_block block2 (::ledger_create_send_state_block_helper (block1, key1.pub, 0, rai::test_genesis_key));
	ASSERT_EQ (rai::process_result::progress, ledger.process (transaction, block2).code);
	rai::state_block block3 (::ledger_create_open_state_block_helper (block1, 1, key1.pub, rai::genesis_amount - 1, key1));
	ASSERT_EQ (rai::process_result::progress, ledger.process (transaction, block3).code);
	rai::state_block block4 (::ledger_create_receive_state_block_helper (block3, block2, 0, key1));
	ASSERT_EQ (rai::process_result::progress, ledger.process (transaction, block4).code);
	ASSERT_EQ (rai::process_result::old, ledger.process (transaction, block4).code);
}

TEST (ledger, fail_receive_gap_source)
{
	bool init (false);
	rai::block_store store (init, rai::unique_path ());
	ASSERT_FALSE (init);
	rai::stat stats;
	rai::ledger ledger (store, stats);
	rai::genesis genesis;
	rai::transaction transaction (store.environment, nullptr, true);
	genesis.initialize (transaction, store);
	rai::keypair key1;
	rai::state_block block1 (::ledger_create_send_state_block_helper (genesis.block (), key1.pub, 1, rai::test_genesis_key));
	auto result1 (ledger.process (transaction, block1));
	ASSERT_EQ (rai::process_result::progress, result1.code);
	rai::state_block block2 (::ledger_create_send_state_block_helper (block1, key1.pub, 0, rai::test_genesis_key));
	auto result2 (ledger.process (transaction, block2));
	ASSERT_EQ (rai::process_result::progress, result2.code);
	rai::state_block block3 (::ledger_create_open_state_block_helper (block1, 1, key1.pub, rai::genesis_amount - 1, key1));
	auto result3 (ledger.process (transaction, block3));
	ASSERT_EQ (rai::process_result::progress, result3.code);
	rai::state_block block4 (key1.pub, block3.hash (), 0, 1, rai::genesis_amount, 3, key1.prv, key1.pub, 0);  // receive
	auto result4 (ledger.process (transaction, block4));
	ASSERT_EQ (rai::process_result::gap_source, result4.code);
}

TEST (ledger, fail_receive_overreceive)
{
	bool init (false);
	rai::block_store store (init, rai::unique_path ());
	ASSERT_FALSE (init);
	rai::stat stats;
	rai::ledger ledger (store, stats);
	rai::genesis genesis;
	rai::transaction transaction (store.environment, nullptr, true);
	genesis.initialize (transaction, store);
	rai::keypair key1;
	rai::state_block block1 (::ledger_create_send_state_block_helper (genesis.block (), key1.pub, 10, rai::test_genesis_key));
	auto result1 (ledger.process (transaction, block1));
	ASSERT_EQ (rai::process_result::progress, result1.code);
	rai::state_block block2 (::ledger_create_open_state_block_helper (block1, 1, key1.pub, rai::genesis_amount - 10, key1));
	auto result3 (ledger.process (transaction, block2));
	ASSERT_EQ (rai::process_result::progress, result3.code);
	rai::state_block block3 (::ledger_create_receive_state_block_helper (block2, block1, rai::genesis_amount, key1));
	auto result4 (ledger.process (transaction, block3));
	ASSERT_EQ (rai::process_result::unreceivable, result4.code);
}

TEST (ledger, fail_receive_bad_signature)
{
	bool init (false);
	rai::block_store store (init, rai::unique_path ());
	ASSERT_FALSE (init);
	rai::stat stats;
	rai::ledger ledger (store, stats);
	rai::genesis genesis;
	rai::transaction transaction (store.environment, nullptr, true);
	genesis.initialize (transaction, store);
	rai::keypair key1;
	rai::state_block block1 (::ledger_create_send_state_block_helper (genesis.block (), key1.pub, 1, rai::test_genesis_key));
	auto result1 (ledger.process (transaction, block1));
	ASSERT_EQ (rai::process_result::progress, result1.code);
	rai::state_block block2 (::ledger_create_send_state_block_helper (block1, key1.pub, 0, rai::test_genesis_key));
	auto result2 (ledger.process (transaction, block2));
	ASSERT_EQ (rai::process_result::progress, result2.code);
	rai::state_block block3 (::ledger_create_open_state_block_helper (block1, 1, key1.pub, rai::genesis_amount - 1, key1));
	auto result3 (ledger.process (transaction, block3));
	ASSERT_EQ (rai::process_result::progress, result3.code);
	rai::state_block block4 (key1.pub, block3.hash (), 0, 1, 1, block2.hash (), rai::keypair ().prv, 0, 0);
	auto result4 (ledger.process (transaction, block4));
	ASSERT_EQ (rai::process_result::bad_signature, result4.code);
}

TEST (ledger, fail_receive_gap_previous_opened)
{
	bool init (false);
	rai::block_store store (init, rai::unique_path ());
	ASSERT_FALSE (init);
	rai::stat stats;
	rai::ledger ledger (store, stats);
	rai::genesis genesis;
	rai::transaction transaction (store.environment, nullptr, true);
	genesis.initialize (transaction, store);
	rai::keypair key1;
	rai::state_block block1 (::ledger_create_send_state_block_helper (genesis.block (), key1.pub, 1, rai::test_genesis_key));
	auto result1 (ledger.process (transaction, block1));
	ASSERT_EQ (rai::process_result::progress, result1.code);
	rai::state_block block2 (::ledger_create_send_state_block_helper (block1, key1.pub, 0, rai::test_genesis_key));
	auto result2 (ledger.process (transaction, block2));
	ASSERT_EQ (rai::process_result::progress, result2.code);
	rai::state_block block3 (::ledger_create_open_state_block_helper (block1, 1, key1.pub, rai::genesis_amount - 1, key1));
	auto result3 (ledger.process (transaction, block3));
	ASSERT_EQ (rai::process_result::progress, result3.code);
	rai::state_block block4 (key1.pub, 1, 0, rai::genesis_account, 0, block2.hash (), key1.prv, key1.pub, 0);
	auto result4 (ledger.process (transaction, block4));
	ASSERT_EQ (rai::process_result::gap_previous, result4.code);
}

TEST (ledger, fail_receive_gap_previous_unopened)
{
	bool init (false);
	rai::block_store store (init, rai::unique_path ());
	ASSERT_FALSE (init);
	rai::stat stats;
	rai::ledger ledger (store, stats);
	rai::genesis genesis;
	rai::transaction transaction (store.environment, nullptr, true);
	genesis.initialize (transaction, store);
	rai::keypair key1;
	rai::state_block block1 (::ledger_create_send_state_block_helper (genesis.block (), key1.pub, 1, rai::test_genesis_key));
	auto result1 (ledger.process (transaction, block1));
	ASSERT_EQ (rai::process_result::progress, result1.code);
	rai::state_block block2 (::ledger_create_send_state_block_helper (block1, key1.pub, 0, rai::test_genesis_key));
	auto result2 (ledger.process (transaction, block2));
	ASSERT_EQ (rai::process_result::progress, result2.code);
	rai::state_block block3 (key1.pub, 1, 0, rai::genesis_account, 0, block2.hash (), key1.prv, key1.pub, 0);
	auto result3 (ledger.process (transaction, block3));
	ASSERT_EQ (rai::process_result::gap_previous, result3.code);
}

TEST (ledger, fail_receive_fork_previous)
{
	bool init (false);
	rai::block_store store (init, rai::unique_path ());
	ASSERT_FALSE (init);
	rai::stat stats;
	rai::ledger ledger (store, stats);
	rai::genesis genesis;
	rai::transaction transaction (store.environment, nullptr, true);
	genesis.initialize (transaction, store);
	rai::keypair key1;
	rai::state_block block1 (::ledger_create_send_state_block_helper (genesis.block (), key1.pub, 1, rai::test_genesis_key));
	auto result1 (ledger.process (transaction, block1));
	ASSERT_EQ (rai::process_result::progress, result1.code);
	rai::state_block block2 (::ledger_create_send_state_block_helper (block1, key1.pub, 0, rai::test_genesis_key));
	auto result2 (ledger.process (transaction, block2));
	ASSERT_EQ (rai::process_result::progress, result2.code);
	rai::state_block block3 (::ledger_create_open_state_block_helper (block1, 1, key1.pub, rai::genesis_amount - 1, key1));
	auto result3 (ledger.process (transaction, block3));
	ASSERT_EQ (rai::process_result::progress, result3.code);
	rai::keypair key2;
	rai::state_block block4 (::ledger_create_send_state_block_helper (block3, key1.pub, 1, key1));
	auto result4 (ledger.process (transaction, block4));
	ASSERT_EQ (rai::process_result::progress, result4.code);
	rai::state_block block5 (::ledger_create_receive_state_block_helper (block3, block2, 0, key1));
	auto result5 (ledger.process (transaction, block5));
	ASSERT_EQ (rai::process_result::fork, result5.code);
}

TEST (ledger, fail_receive_received_source)
{
	bool init (false);
	rai::block_store store (init, rai::unique_path ());
	ASSERT_FALSE (init);
	rai::stat stats;
	rai::ledger ledger (store, stats);
	rai::genesis genesis;
	rai::transaction transaction (store.environment, nullptr, true);
	genesis.initialize (transaction, store);
	rai::keypair key1;
	rai::state_block block1 (::ledger_create_send_state_block_helper (genesis.block (), key1.pub, 2, rai::test_genesis_key));
	auto result1 (ledger.process (transaction, block1));
	ASSERT_EQ (rai::process_result::progress, result1.code);
	rai::state_block block2 (::ledger_create_send_state_block_helper (block1, key1.pub, 1, rai::test_genesis_key));
	auto result2 (ledger.process (transaction, block2));
	ASSERT_EQ (rai::process_result::progress, result2.code);
	rai::state_block block6 (::ledger_create_send_state_block_helper (block2, key1.pub, 0, rai::test_genesis_key));
	auto result6 (ledger.process (transaction, block6));
	ASSERT_EQ (rai::process_result::progress, result6.code);
	rai::state_block block3 (::ledger_create_open_state_block_helper (block1, 1, key1.pub, rai::genesis_amount - 2, key1));
	auto result3 (ledger.process (transaction, block3));
	ASSERT_EQ (rai::process_result::progress, result3.code);
	rai::keypair key2;
	rai::state_block block4 (::ledger_create_send_state_block_helper (block3, key1.pub, 1, key1));
	auto result4 (ledger.process (transaction, block4));
	ASSERT_EQ (rai::process_result::progress, result4.code);
	rai::state_block block5 (::ledger_create_receive_state_block_helper (block4, block2, 2, key1));
	auto result5 (ledger.process (transaction, block5));
	ASSERT_EQ (rai::process_result::progress, result5.code);
	rai::state_block block7 (::ledger_create_receive_state_block_helper (block3, block2, rai::genesis_amount - 1, key1));
	auto result7 (ledger.process (transaction, block7));
	ASSERT_EQ (rai::process_result::fork, result7.code);
}

TEST (ledger, latest_empty)
{
	bool init (false);
	rai::block_store store (init, rai::unique_path ());
	ASSERT_FALSE (init);
	rai::stat stats;
	rai::ledger ledger (store, stats);
	rai::keypair key;
	rai::transaction transaction (store.environment, nullptr, false);
	auto latest (ledger.latest (transaction, key.pub));
	ASSERT_TRUE (latest.is_zero ());
}

TEST (ledger, latest_root)
{
	bool init (false);
	rai::block_store store (init, rai::unique_path ());
	ASSERT_FALSE (init);
	rai::stat stats;
	rai::ledger ledger (store, stats);
	rai::genesis genesis;
	rai::transaction transaction (store.environment, nullptr, true);
	genesis.initialize (transaction, store);
	rai::keypair key;
	ASSERT_EQ (key.pub, ledger.latest_root (transaction, key.pub));
	ASSERT_EQ (genesis.hash (), ledger.latest (transaction, rai::test_genesis_key.pub));
	rai::state_block send (::ledger_create_send_state_block_helper (genesis.block (), rai::genesis_account, 1, rai::test_genesis_key));
	ASSERT_EQ (rai::process_result::progress, ledger.process (transaction, send).code);
	ASSERT_EQ (send.hash (), ledger.latest_root (transaction, rai::test_genesis_key.pub));
}

TEST (ledger, change_representative_move_representation)
{
	bool init (false);
	rai::block_store store (init, rai::unique_path ());
	ASSERT_TRUE (!init);
	rai::stat stats;
	rai::ledger ledger (store, stats);
	rai::keypair key1;
	rai::transaction transaction (store.environment, nullptr, true);
	rai::genesis genesis;
	genesis.initialize (transaction, store);
	ASSERT_EQ (rai::genesis_amount, ledger.weight (transaction, rai::test_genesis_key.pub));
	rai::state_block send (::ledger_create_send_state_block_helper (genesis.block (), key1.pub, 0, rai::test_genesis_key));
	ASSERT_EQ (rai::process_result::progress, ledger.process (transaction, send).code);
	ASSERT_EQ (0, ledger.weight (transaction, rai::test_genesis_key.pub));
	rai::keypair key2;
	rai::state_block change (::ledger_create_change_state_block_helper (send, key2.pub, rai::test_genesis_key));
	ASSERT_EQ (rai::process_result::progress, ledger.process (transaction, change).code);
	rai::keypair key3;
	ASSERT_EQ (0, ledger.weight (transaction, key3.pub));
	rai::state_block open (::ledger_create_open_state_block_helper (send, key3.pub, key1.pub, rai::genesis_amount, key1));
	ASSERT_EQ (rai::process_result::progress, ledger.process (transaction, open).code);
	ASSERT_EQ (rai::genesis_amount, ledger.weight (transaction, key3.pub));
}

TEST (ledger, send_open_receive_rollback)
{
	bool init (false);
	rai::block_store store (init, rai::unique_path ());
	ASSERT_TRUE (!init);
	rai::stat stats;
	rai::ledger ledger (store, stats);
	rai::transaction transaction (store.environment, nullptr, true);
	rai::genesis genesis;
	genesis.initialize (transaction, store);
	rai::account_info info1;
	ASSERT_FALSE (store.account_get (transaction, rai::test_genesis_key.pub, info1));
	ASSERT_EQ (genesis.hash (), info1.head);
	rai::keypair key1;
	rai::state_block send1 (::ledger_create_send_state_block_helper (genesis.block (), key1.pub, rai::genesis_amount - 50, rai::test_genesis_key));
	auto return1 (ledger.process (transaction, send1));
	ASSERT_EQ (rai::process_result::progress, return1.code);
	rai::state_block send2 (::ledger_create_send_state_block_helper (send1, key1.pub, rai::genesis_amount - 110, rai::test_genesis_key));
	auto return2 (ledger.process (transaction, send2));
	ASSERT_EQ (rai::process_result::progress, return2.code);
	rai::keypair key2;
	rai::state_block open (::ledger_create_open_state_block_helper (send1, key2.pub, key1.pub, 50, key1));
	auto return4 (ledger.process (transaction, open));
	ASSERT_EQ (rai::process_result::progress, return4.code);
	rai::state_block receive (::ledger_create_receive_state_block_helper (open, send2, 110, key1));
	auto return5 (ledger.process (transaction, receive));
	ASSERT_EQ (rai::process_result::progress, return5.code);
	rai::keypair key3;
	ASSERT_EQ (110, ledger.weight (transaction, key2.pub));
	ASSERT_EQ (rai::genesis_amount - 110, ledger.weight (transaction, rai::test_genesis_key.pub));
	ASSERT_EQ (0, ledger.weight (transaction, key3.pub));
	rai::state_block change1 (::ledger_create_change_state_block_helper (send2, key3.pub, rai::test_genesis_key));
	auto return6 (ledger.process (transaction, change1));
	ASSERT_EQ (rai::process_result::progress, return6.code);
	ASSERT_EQ (110, ledger.weight (transaction, key2.pub));
	ASSERT_EQ (0, ledger.weight (transaction, rai::test_genesis_key.pub));
	ASSERT_EQ (rai::genesis_amount - 110, ledger.weight (transaction, key3.pub));
	ledger.rollback (transaction, receive.hash ());
	ASSERT_EQ (50, ledger.weight (transaction, key2.pub));
	ASSERT_EQ (0, ledger.weight (transaction, rai::test_genesis_key.pub));
	ASSERT_EQ (rai::genesis_amount - 110, ledger.weight (transaction, key3.pub));
	ledger.rollback (transaction, open.hash ());
	ASSERT_EQ (0, ledger.weight (transaction, key2.pub));
	ASSERT_EQ (0, ledger.weight (transaction, rai::test_genesis_key.pub));
	ASSERT_EQ (rai::genesis_amount - 110, ledger.weight (transaction, key3.pub));
	ledger.rollback (transaction, change1.hash ());
	ASSERT_EQ (0, ledger.weight (transaction, key2.pub));
	ASSERT_EQ (0, ledger.weight (transaction, key3.pub));
	ASSERT_EQ (rai::genesis_amount - 110, ledger.weight (transaction, rai::test_genesis_key.pub));
	ledger.rollback (transaction, send2.hash ());
	ASSERT_EQ (0, ledger.weight (transaction, key2.pub));
	ASSERT_EQ (0, ledger.weight (transaction, key3.pub));
	ASSERT_EQ (rai::genesis_amount - 50, ledger.weight (transaction, rai::test_genesis_key.pub));
	ledger.rollback (transaction, send1.hash ());
	ASSERT_EQ (0, ledger.weight (transaction, key2.pub));
	ASSERT_EQ (0, ledger.weight (transaction, key3.pub));
	ASSERT_EQ (rai::genesis_amount - 0, ledger.weight (transaction, rai::test_genesis_key.pub));
}

TEST (ledger, bootstrap_rep_weight)
{
	bool init (false);
	rai::block_store store (init, rai::unique_path ());
	ASSERT_TRUE (!init);
	rai::stat stats;
	rai::ledger ledger (store, stats);
	rai::account_info info1;
	rai::keypair key2;
	rai::genesis genesis;
	rai::block_hash send1_hash;
	{
		rai::transaction transaction (store.environment, nullptr, true);
		genesis.initialize (transaction, store);
		ASSERT_FALSE (store.account_get (transaction, rai::test_genesis_key.pub, info1));
		ASSERT_EQ (genesis.hash (), info1.head);
		rai::state_block send (::ledger_create_send_state_block_helper (genesis.block (), key2.pub, std::numeric_limits<rai::amount_t>::max () - 50, rai::test_genesis_key));
		send1_hash = send.hash ();
		ledger.process (transaction, send);
		ASSERT_FALSE (store.account_get (transaction, rai::test_genesis_key.pub, info1));
		ASSERT_EQ (send1_hash, info1.head);
	}
	{
		rai::transaction transaction (store.environment, nullptr, false);
		ledger.bootstrap_weight_max_blocks = 3;
		ledger.bootstrap_weights[key2.pub] = 1000;
		ASSERT_EQ (1000, ledger.weight (transaction, key2.pub));
	}
	{
		rai::transaction transaction (store.environment, nullptr, true);
		rai::state_block send (rai::genesis_account, send1_hash, 0, rai::genesis_account, std::numeric_limits<rai::amount_t>::max() - 100, key2.pub, rai::test_genesis_key.prv, rai::test_genesis_key.pub, 0);
		ledger.process (transaction, send);
	}
	{
		rai::transaction transaction (store.environment, nullptr, false);
		ASSERT_EQ (0, ledger.weight (transaction, key2.pub));
	}
}

TEST (ledger, block_destination_source)
{
	bool init (false);
	rai::block_store store (init, rai::unique_path ());
	ASSERT_TRUE (!init);
	rai::stat stats;
	rai::ledger ledger (store, stats);
	rai::genesis genesis;
	rai::transaction transaction (store.environment, nullptr, true);
	genesis.initialize (transaction, store);
	rai::keypair dest;
	rai::amount_t balance (rai::genesis_amount);
	balance -= rai::Gxrb_ratio;
	rai::state_block block1 (::ledger_create_send_state_block_helper (genesis.block (), dest.pub, balance, rai::test_genesis_key));
	balance -= rai::Gxrb_ratio;
	rai::state_block block2 (::ledger_create_send_state_block_helper (block1, rai::genesis_account, balance, rai::test_genesis_key));
	balance += rai::Gxrb_ratio;
	rai::state_block block3 (::ledger_create_receive_state_block_helper (block2, block2, balance, rai::test_genesis_key));
	balance -= rai::Gxrb_ratio;
	rai::state_block block4 (rai::genesis_account, block3.hash (), 0, rai::genesis_account, balance, dest.pub, rai::test_genesis_key.prv, rai::test_genesis_key.pub, 0);
	balance -= rai::Gxrb_ratio;
	rai::state_block block5 (rai::genesis_account, block4.hash (), 0, rai::genesis_account, balance, rai::genesis_account, rai::test_genesis_key.prv, rai::test_genesis_key.pub, 0);
	balance += rai::Gxrb_ratio;
	rai::state_block block6 (rai::genesis_account, block5.hash (), 0, rai::genesis_account, balance, block5.hash (), rai::test_genesis_key.prv, rai::test_genesis_key.pub, 0);
	ASSERT_EQ (rai::process_result::progress, ledger.process (transaction, block1).code);
	ASSERT_EQ (rai::process_result::progress, ledger.process (transaction, block2).code);
	ASSERT_EQ (rai::process_result::progress, ledger.process (transaction, block3).code);
	ASSERT_EQ (rai::process_result::progress, ledger.process (transaction, block4).code);
	ASSERT_EQ (rai::process_result::progress, ledger.process (transaction, block5).code);
	ASSERT_EQ (rai::process_result::progress, ledger.process (transaction, block6).code);
	ASSERT_EQ (balance, ledger.balance (transaction, block6.hash ()));
	ASSERT_EQ (dest.pub, ledger.block_destination (transaction, block1));
	ASSERT_TRUE (ledger.block_source (transaction, block1).is_zero ());
	ASSERT_EQ (rai::genesis_account, ledger.block_destination (transaction, block2));
	ASSERT_TRUE (ledger.block_source (transaction, block2).is_zero ());
	ASSERT_TRUE (ledger.block_destination (transaction, block3).is_zero ());
	ASSERT_EQ (block2.hash (), ledger.block_source (transaction, block3));
	ASSERT_EQ (dest.pub, ledger.block_destination (transaction, block4));
	ASSERT_TRUE (ledger.block_source (transaction, block4).is_zero ());
	ASSERT_EQ (rai::genesis_account, ledger.block_destination (transaction, block5));
	ASSERT_TRUE (ledger.block_source (transaction, block5).is_zero ());
	ASSERT_TRUE (ledger.block_destination (transaction, block6).is_zero ());
	ASSERT_EQ (block5.hash (), ledger.block_source (transaction, block6));
}

TEST (ledger, state_account)
{
	bool init (false);
	rai::block_store store (init, rai::unique_path ());
	ASSERT_TRUE (!init);
	rai::stat stats;
	rai::ledger ledger (store, stats);
	rai::genesis genesis;
	rai::transaction transaction (store.environment, nullptr, true);
	genesis.initialize (transaction, store);
	rai::state_block send1 (rai::genesis_account, genesis.hash (), 0, rai::genesis_account, rai::genesis_amount - rai::Gxrb_ratio, rai::genesis_account, rai::test_genesis_key.prv, rai::test_genesis_key.pub, 0);
	ASSERT_EQ (rai::process_result::progress, ledger.process (transaction, send1).code);
	ASSERT_EQ (rai::genesis_account, ledger.account (transaction, send1.hash ()));
}

TEST (ledger, state_send_receive)
{
	bool init (false);
	rai::block_store store (init, rai::unique_path ());
	ASSERT_TRUE (!init);
	rai::stat stats;
	rai::ledger ledger (store, stats);
	rai::genesis genesis;
	rai::transaction transaction (store.environment, nullptr, true);
	genesis.initialize (transaction, store);
	rai::state_block send1 (rai::genesis_account, genesis.hash (), 0, rai::genesis_account, rai::genesis_amount - rai::Gxrb_ratio, rai::genesis_account, rai::test_genesis_key.prv, rai::test_genesis_key.pub, 0);
	ASSERT_EQ (rai::process_result::progress, ledger.process (transaction, send1).code);
	ASSERT_TRUE (store.block_exists (transaction, send1.hash ()));
	auto send2 (store.block_get (transaction, send1.hash ()));
	ASSERT_NE (nullptr, send2);
	ASSERT_EQ (send1, *send2);
	ASSERT_EQ (rai::genesis_amount - rai::Gxrb_ratio, ledger.balance (transaction, send1.hash ()));
	ASSERT_EQ (rai::Gxrb_ratio, ledger.amount (transaction, send1.hash ()));
	ASSERT_EQ (rai::genesis_amount - rai::Gxrb_ratio, ledger.weight (transaction, rai::genesis_account));
	ASSERT_TRUE (store.pending_exists (transaction, rai::pending_key (rai::genesis_account, send1.hash ())));
	rai::state_block receive1 (rai::genesis_account, send1.hash (), 0, rai::genesis_account, rai::genesis_amount, send1.hash (), rai::test_genesis_key.prv, rai::test_genesis_key.pub, 0);
	ASSERT_EQ (rai::process_result::progress, ledger.process (transaction, receive1).code);
	ASSERT_TRUE (store.block_exists (transaction, receive1.hash ()));
	auto receive2 (store.block_get (transaction, receive1.hash ()));
	ASSERT_NE (nullptr, receive2);
	ASSERT_EQ (receive1, *receive2);
	ASSERT_EQ (rai::genesis_amount, ledger.balance (transaction, receive1.hash ()));
	ASSERT_EQ (rai::Gxrb_ratio, ledger.amount (transaction, receive1.hash ()));
	ASSERT_EQ (rai::genesis_amount, ledger.weight (transaction, rai::genesis_account));
	ASSERT_FALSE (store.pending_exists (transaction, rai::pending_key (rai::genesis_account, send1.hash ())));
}

TEST (ledger, state_receive)
{
	bool init (false);
	rai::block_store store (init, rai::unique_path ());
	ASSERT_TRUE (!init);
	rai::stat stats;
	rai::ledger ledger (store, stats);
	rai::genesis genesis;
	rai::transaction transaction (store.environment, nullptr, true);
	genesis.initialize (transaction, store);
	rai::state_block send1 (::ledger_create_send_state_block_helper (genesis.block (), rai::genesis_account, rai::genesis_amount - rai::Gxrb_ratio, rai::test_genesis_key));
	ASSERT_EQ (rai::process_result::progress, ledger.process (transaction, send1).code);
	ASSERT_TRUE (store.block_exists (transaction, send1.hash ()));
	auto send2 (store.block_get (transaction, send1.hash ()));
	ASSERT_NE (nullptr, send2);
	ASSERT_EQ (send1, *send2);
	ASSERT_EQ (rai::genesis_amount - rai::Gxrb_ratio, ledger.balance (transaction, send1.hash ()));
	ASSERT_EQ (rai::Gxrb_ratio, ledger.amount (transaction, send1.hash ()));
	ASSERT_EQ (rai::genesis_amount - rai::Gxrb_ratio, ledger.weight (transaction, rai::genesis_account));
	rai::state_block receive1 (rai::genesis_account, send1.hash (), 0, rai::genesis_account, rai::genesis_amount, send1.hash (), rai::test_genesis_key.prv, rai::test_genesis_key.pub, 0);
	ASSERT_EQ (rai::process_result::progress, ledger.process (transaction, receive1).code);
	ASSERT_TRUE (store.block_exists (transaction, receive1.hash ()));
	auto receive2 (store.block_get (transaction, receive1.hash ()));
	ASSERT_NE (nullptr, receive2);
	ASSERT_EQ (receive1, *receive2);
	ASSERT_EQ (rai::genesis_amount, ledger.balance (transaction, receive1.hash ()));
	ASSERT_EQ (rai::Gxrb_ratio, ledger.amount (transaction, receive1.hash ()));
	ASSERT_EQ (rai::genesis_amount, ledger.weight (transaction, rai::genesis_account));
}

TEST (ledger, state_rep_change)
{
	bool init (false);
	rai::block_store store (init, rai::unique_path ());
	ASSERT_TRUE (!init);
	rai::stat stats;
	rai::ledger ledger (store, stats);
	rai::genesis genesis;
	rai::transaction transaction (store.environment, nullptr, true);
	genesis.initialize (transaction, store);
	rai::keypair rep;
	rai::state_block change1 (rai::genesis_account, genesis.hash (), 0, rep.pub, rai::genesis_amount, 0, rai::test_genesis_key.prv, rai::test_genesis_key.pub, 0);
	ASSERT_EQ (rai::process_result::progress, ledger.process (transaction, change1).code);
	ASSERT_TRUE (store.block_exists (transaction, change1.hash ()));
	auto change2 (store.block_get (transaction, change1.hash ()));
	ASSERT_NE (nullptr, change2);
	ASSERT_EQ (change1, *change2);
	ASSERT_EQ (rai::genesis_amount, ledger.balance (transaction, change1.hash ()));
	ASSERT_EQ (0, ledger.amount (transaction, change1.hash ()));
	ASSERT_EQ (0, ledger.weight (transaction, rai::genesis_account));
	ASSERT_EQ (rai::genesis_amount, ledger.weight (transaction, rep.pub));
}

TEST (ledger, state_open)
{
	bool init (false);
	rai::block_store store (init, rai::unique_path ());
	ASSERT_TRUE (!init);
	rai::stat stats;
	rai::ledger ledger (store, stats);
	rai::genesis genesis;
	rai::transaction transaction (store.environment, nullptr, true);
	genesis.initialize (transaction, store);
	rai::keypair destination;
	rai::state_block send1 (rai::genesis_account, genesis.hash (), 0, rai::genesis_account, rai::genesis_amount - rai::Gxrb_ratio, destination.pub, rai::test_genesis_key.prv, rai::test_genesis_key.pub, 0);
	ASSERT_EQ (rai::process_result::progress, ledger.process (transaction, send1).code);
	ASSERT_TRUE (store.block_exists (transaction, send1.hash ()));
	auto send2 (store.block_get (transaction, send1.hash ()));
	ASSERT_NE (nullptr, send2);
	ASSERT_EQ (send1, *send2);
	ASSERT_EQ (rai::genesis_amount - rai::Gxrb_ratio, ledger.balance (transaction, send1.hash ()));
	ASSERT_EQ (rai::Gxrb_ratio, ledger.amount (transaction, send1.hash ()));
	ASSERT_EQ (rai::genesis_amount - rai::Gxrb_ratio, ledger.weight (transaction, rai::genesis_account));
	ASSERT_TRUE (store.pending_exists (transaction, rai::pending_key (destination.pub, send1.hash ())));
	rai::state_block open1 (destination.pub, 0, 0, rai::genesis_account, rai::Gxrb_ratio, send1.hash (), destination.prv, destination.pub, 0);
	ASSERT_EQ (rai::process_result::progress, ledger.process (transaction, open1).code);
	ASSERT_FALSE (store.pending_exists (transaction, rai::pending_key (destination.pub, send1.hash ())));
	ASSERT_TRUE (store.block_exists (transaction, open1.hash ()));
	auto open2 (store.block_get (transaction, open1.hash ()));
	ASSERT_NE (nullptr, open2);
	ASSERT_EQ (open1, *open2);
	ASSERT_EQ (rai::Gxrb_ratio, ledger.balance (transaction, open1.hash ()));
	ASSERT_EQ (rai::Gxrb_ratio, ledger.amount (transaction, open1.hash ()));
	ASSERT_EQ (rai::genesis_amount, ledger.weight (transaction, rai::genesis_account));
}

// Not relevant any more  Make sure old block types can't be inserted after a state block.
//TEST (ledger, send_after_state_fail)

// Not relevant any more  Make sure old block types can't be inserted after a state block.
//TEST (ledger, receive_after_state_fail)

// Not relevant any more  Make sure old block types can't be inserted after a state block.
//TEST (ledger, change_after_state_fail)

TEST (ledger, state_unreceivable_fail)
{
	bool init (false);
	rai::block_store store (init, rai::unique_path ());
	ASSERT_TRUE (!init);
	rai::stat stats;
	rai::ledger ledger (store, stats);
	rai::genesis genesis;
	rai::transaction transaction (store.environment, nullptr, true);
	genesis.initialize (transaction, store);
	rai::state_block send1 (::ledger_create_send_state_block_helper (genesis.block (), rai::genesis_account, rai::genesis_amount - rai::Gxrb_ratio, rai::test_genesis_key));
	ASSERT_EQ (rai::process_result::progress, ledger.process (transaction, send1).code);
	ASSERT_TRUE (store.block_exists (transaction, send1.hash ()));
	auto send2 (store.block_get (transaction, send1.hash ()));
	ASSERT_NE (nullptr, send2);
	ASSERT_EQ (send1, *send2);
	ASSERT_EQ (rai::genesis_amount - rai::Gxrb_ratio, ledger.balance (transaction, send1.hash ()));
	ASSERT_EQ (rai::Gxrb_ratio, ledger.amount (transaction, send1.hash ()));
	ASSERT_EQ (rai::genesis_amount - rai::Gxrb_ratio, ledger.weight (transaction, rai::genesis_account));
	rai::state_block receive1 (rai::genesis_account, send1.hash (), 0, rai::genesis_account, rai::genesis_amount, 1, rai::test_genesis_key.prv, rai::test_genesis_key.pub, 0);
	ASSERT_EQ (rai::process_result::gap_source, ledger.process (transaction, receive1).code);
}

TEST (ledger, state_receive_bad_amount_fail)
{
	bool init (false);
	rai::block_store store (init, rai::unique_path ());
	ASSERT_TRUE (!init);
	rai::stat stats;
	rai::ledger ledger (store, stats);
	rai::genesis genesis;
	rai::transaction transaction (store.environment, nullptr, true);
	genesis.initialize (transaction, store);
	rai::keypair key;
	rai::state_block send1 (::ledger_create_send_state_block_helper (genesis.block (), key.pub, rai::genesis_amount - 100, rai::test_genesis_key));
	ASSERT_EQ (rai::process_result::progress, ledger.process (transaction, send1).code);
	rai::state_block open (key.pub, 0, 0, key.pub, 100, send1.hash (), key.prv, key.pub, 0);
	ASSERT_EQ (rai::process_result::progress, ledger.process (transaction, open).code);
	rai::state_block send2 (::ledger_create_send_state_block_helper (send1, key.pub, rai::genesis_amount - 1100, rai::test_genesis_key));
	ASSERT_EQ (rai::process_result::progress, ledger.process (transaction, send2).code);
	ASSERT_TRUE (store.block_exists (transaction, send2.hash ()));
	auto send22 (store.block_get (transaction, send2.hash ()));
	ASSERT_NE (nullptr, send22);
	ASSERT_EQ (send2, *send22);
	ASSERT_EQ (rai::genesis_amount - 1100, ledger.balance (transaction, send2.hash ()));
	ASSERT_EQ (1000, ledger.amount (transaction, send2.hash ()));
	ASSERT_EQ (rai::genesis_amount - 1100, ledger.weight (transaction, rai::genesis_account));
	rai::state_block receive_more (key.pub, open.hash (), 0, key.pub, 1100 + 1, send2.hash (), key.prv, key.pub, 0);
	ASSERT_EQ (rai::process_result::balance_mismatch, ledger.process (transaction, receive_more).code);
	rai::state_block receive_less (key.pub, open.hash (), 0, key.pub, 1100 - 1, send2.hash (), key.prv, key.pub, 0);
	ASSERT_EQ (rai::process_result::balance_mismatch, ledger.process (transaction, receive_less).code);
	rai::state_block receive_correct (key.pub, open.hash (), 0, key.pub, 1100, send2.hash (), key.prv, key.pub, 0);
	ASSERT_EQ (rai::process_result::progress, ledger.process (transaction, receive_correct).code);
}

TEST (ledger, state_no_link_amount_fail)
{
	bool init (false);
	rai::block_store store (init, rai::unique_path ());
	ASSERT_TRUE (!init);
	rai::stat stats;
	rai::ledger ledger (store, stats);
	rai::genesis genesis;
	rai::transaction transaction (store.environment, nullptr, true);
	genesis.initialize (transaction, store);
	rai::state_block send1 (rai::genesis_account, genesis.hash (), 0, rai::genesis_account, rai::genesis_amount - rai::Gxrb_ratio, rai::genesis_account, rai::test_genesis_key.prv, rai::test_genesis_key.pub, 0);
	ASSERT_EQ (rai::process_result::progress, ledger.process (transaction, send1).code);
	rai::keypair rep;
	rai::state_block change1 (rai::genesis_account, send1.hash (), 0, rep.pub, rai::genesis_amount, 0, rai::test_genesis_key.prv, rai::test_genesis_key.pub, 0);
	ASSERT_EQ (rai::process_result::balance_mismatch, ledger.process (transaction, change1).code);
}

TEST (ledger, state_receive_wrong_account_fail)
{
	bool init (false);
	rai::block_store store (init, rai::unique_path ());
	ASSERT_TRUE (!init);
	rai::stat stats;
	rai::ledger ledger (store, stats);
	rai::genesis genesis;
	rai::transaction transaction (store.environment, nullptr, true);
	genesis.initialize (transaction, store);
	rai::state_block send1 (rai::genesis_account, genesis.hash (), 0, rai::genesis_account, rai::genesis_amount - rai::Gxrb_ratio, rai::genesis_account, rai::test_genesis_key.prv, rai::test_genesis_key.pub, 0);
	ASSERT_EQ (rai::process_result::progress, ledger.process (transaction, send1).code);
	ASSERT_TRUE (store.block_exists (transaction, send1.hash ()));
	auto send2 (store.block_get (transaction, send1.hash ()));
	ASSERT_NE (nullptr, send2);
	ASSERT_EQ (send1, *send2);
	ASSERT_EQ (rai::genesis_amount - rai::Gxrb_ratio, ledger.balance (transaction, send1.hash ()));
	ASSERT_EQ (rai::Gxrb_ratio, ledger.amount (transaction, send1.hash ()));
	ASSERT_EQ (rai::genesis_amount - rai::Gxrb_ratio, ledger.weight (transaction, rai::genesis_account));
	rai::keypair key;
	rai::state_block receive1 (key.pub, 0, 0, rai::genesis_account, rai::Gxrb_ratio, send1.hash (), key.prv, key.pub, 0);
	ASSERT_EQ (rai::process_result::unreceivable, ledger.process (transaction, receive1).code);
}

// Mixing old open and state not relevant any more
//TEST (ledger, state_open_state_fork)

// Mixing old open and state not relevant any more
//TEST (ledger, state_state_open_fork)

TEST (ledger, state_open_previous_fail)
{
	bool init (false);
	rai::block_store store (init, rai::unique_path ());
	ASSERT_TRUE (!init);
	rai::stat stats;
	rai::ledger ledger (store, stats);
	rai::genesis genesis;
	rai::transaction transaction (store.environment, nullptr, true);
	genesis.initialize (transaction, store);
	rai::keypair destination;
	rai::state_block send1 (rai::genesis_account, genesis.hash (), 0, rai::genesis_account, rai::genesis_amount - rai::Gxrb_ratio, destination.pub, rai::test_genesis_key.prv, rai::test_genesis_key.pub, 0);
	ASSERT_EQ (rai::process_result::progress, ledger.process (transaction, send1).code);
	rai::state_block open1 (destination.pub, destination.pub, 0, rai::genesis_account, rai::Gxrb_ratio, send1.hash (), destination.prv, destination.pub, 0);
	ASSERT_EQ (rai::process_result::gap_previous, ledger.process (transaction, open1).code);
}

TEST (ledger, state_open_source_fail)
{
	bool init (false);
	rai::block_store store (init, rai::unique_path ());
	ASSERT_TRUE (!init);
	rai::stat stats;
	rai::ledger ledger (store, stats);
	rai::genesis genesis;
	rai::transaction transaction (store.environment, nullptr, true);
	genesis.initialize (transaction, store);
	rai::keypair destination;
	rai::state_block send1 (rai::genesis_account, genesis.hash (), 0, rai::genesis_account, rai::genesis_amount - rai::Gxrb_ratio, destination.pub, rai::test_genesis_key.prv, rai::test_genesis_key.pub, 0);
	ASSERT_EQ (rai::process_result::progress, ledger.process (transaction, send1).code);
	rai::state_block open1 (destination.pub, 0, 0, rai::genesis_account, 0, 0, destination.prv, destination.pub, 0);
	ASSERT_EQ (rai::process_result::gap_source, ledger.process (transaction, open1).code);
}

TEST (ledger, state_send_change)
{
	bool init (false);
	rai::block_store store (init, rai::unique_path ());
	ASSERT_TRUE (!init);
	rai::stat stats;
	rai::ledger ledger (store, stats);
	rai::genesis genesis;
	rai::transaction transaction (store.environment, nullptr, true);
	genesis.initialize (transaction, store);
	rai::keypair rep;
	rai::state_block send1 (rai::genesis_account, genesis.hash (), 0, rep.pub, rai::genesis_amount - rai::Gxrb_ratio, rai::genesis_account, rai::test_genesis_key.prv, rai::test_genesis_key.pub, 0);
	ASSERT_EQ (rai::process_result::progress, ledger.process (transaction, send1).code);
	ASSERT_TRUE (store.block_exists (transaction, send1.hash ()));
	auto send2 (store.block_get (transaction, send1.hash ()));
	ASSERT_NE (nullptr, send2);
	ASSERT_EQ (send1, *send2);
	ASSERT_EQ (rai::genesis_amount - rai::Gxrb_ratio, ledger.balance (transaction, send1.hash ()));
	ASSERT_EQ (rai::Gxrb_ratio, ledger.amount (transaction, send1.hash ()));
	ASSERT_EQ (0, ledger.weight (transaction, rai::genesis_account));
	ASSERT_EQ (rai::genesis_amount - rai::Gxrb_ratio, ledger.weight (transaction, rep.pub));
}

TEST (ledger, state_receive_change)
{
	bool init (false);
	rai::block_store store (init, rai::unique_path ());
	ASSERT_TRUE (!init);
	rai::stat stats;
	rai::ledger ledger (store, stats);
	rai::genesis genesis;
	rai::transaction transaction (store.environment, nullptr, true);
	genesis.initialize (transaction, store);
	rai::state_block send1 (rai::genesis_account, genesis.hash (), 0, rai::genesis_account, rai::genesis_amount - rai::Gxrb_ratio, rai::genesis_account, rai::test_genesis_key.prv, rai::test_genesis_key.pub, 0);
	ASSERT_EQ (rai::process_result::progress, ledger.process (transaction, send1).code);
	ASSERT_TRUE (store.block_exists (transaction, send1.hash ()));
	auto send2 (store.block_get (transaction, send1.hash ()));
	ASSERT_NE (nullptr, send2);
	ASSERT_EQ (send1, *send2);
	ASSERT_EQ (rai::genesis_amount - rai::Gxrb_ratio, ledger.balance (transaction, send1.hash ()));
	ASSERT_EQ (rai::Gxrb_ratio, ledger.amount (transaction, send1.hash ()));
	ASSERT_EQ (rai::genesis_amount - rai::Gxrb_ratio, ledger.weight (transaction, rai::genesis_account));
	rai::keypair rep;
	rai::state_block receive1 (rai::genesis_account, send1.hash (), 0, rep.pub, rai::genesis_amount, send1.hash (), rai::test_genesis_key.prv, rai::test_genesis_key.pub, 0);
	ASSERT_EQ (rai::process_result::progress, ledger.process (transaction, receive1).code);
	ASSERT_TRUE (store.block_exists (transaction, receive1.hash ()));
	auto receive2 (store.block_get (transaction, receive1.hash ()));
	ASSERT_NE (nullptr, receive2);
	ASSERT_EQ (receive1, *receive2);
	ASSERT_EQ (rai::genesis_amount, ledger.balance (transaction, receive1.hash ()));
	ASSERT_EQ (rai::Gxrb_ratio, ledger.amount (transaction, receive1.hash ()));
	ASSERT_EQ (0, ledger.weight (transaction, rai::genesis_account));
	ASSERT_EQ (rai::genesis_amount, ledger.weight (transaction, rep.pub));
}

TEST (ledger, state_open_old)
{
	bool init (false);
	rai::block_store store (init, rai::unique_path ());
	ASSERT_TRUE (!init);
	rai::stat stats;
	rai::ledger ledger (store, stats);
	rai::genesis genesis;
	rai::transaction transaction (store.environment, nullptr, true);
	genesis.initialize (transaction, store);
	rai::keypair destination;
	rai::state_block send1 (rai::genesis_account, genesis.hash (), 0, rai::genesis_account, rai::genesis_amount - rai::Gxrb_ratio, destination.pub, rai::test_genesis_key.prv, rai::test_genesis_key.pub, 0);
	ASSERT_EQ (rai::process_result::progress, ledger.process (transaction, send1).code);
	rai::state_block open1 (::ledger_create_open_state_block_helper (send1, rai::genesis_account, destination.pub, rai::Gxrb_ratio, destination));
	ASSERT_EQ (rai::process_result::progress, ledger.process (transaction, open1).code);
	ASSERT_EQ (rai::Gxrb_ratio, ledger.balance (transaction, open1.hash ()));
	ASSERT_EQ (rai::Gxrb_ratio, ledger.amount (transaction, open1.hash ()));
	ASSERT_EQ (rai::genesis_amount, ledger.weight (transaction, rai::genesis_account));
}

TEST (ledger, state_receive_old)
{
	bool init (false);
	rai::block_store store (init, rai::unique_path ());
	ASSERT_TRUE (!init);
	rai::stat stats;
	rai::ledger ledger (store, stats);
	rai::genesis genesis;
	rai::transaction transaction (store.environment, nullptr, true);
	genesis.initialize (transaction, store);
	rai::keypair destination;
	rai::state_block send1 (rai::genesis_account, genesis.hash (), 0, rai::genesis_account, rai::genesis_amount - rai::Gxrb_ratio, destination.pub, rai::test_genesis_key.prv, rai::test_genesis_key.pub, 0);
	ASSERT_EQ (rai::process_result::progress, ledger.process (transaction, send1).code);
	rai::state_block send2 (rai::genesis_account, send1.hash (), 0, rai::genesis_account, rai::genesis_amount - (2 * rai::Gxrb_ratio), destination.pub, rai::test_genesis_key.prv, rai::test_genesis_key.pub, 0);
	ASSERT_EQ (rai::process_result::progress, ledger.process (transaction, send2).code);
	rai::state_block open1 (::ledger_create_open_state_block_helper (send1, rai::genesis_account, destination.pub, rai::Gxrb_ratio, destination));
	ASSERT_EQ (rai::process_result::progress, ledger.process (transaction, open1).code);
	rai::state_block receive1 (::ledger_create_receive_state_block_helper (open1, send2, 2 * rai::Gxrb_ratio, destination));
	ASSERT_EQ (rai::process_result::progress, ledger.process (transaction, receive1).code);
	ASSERT_EQ (2 * rai::Gxrb_ratio, ledger.balance (transaction, receive1.hash ()));
	ASSERT_EQ (rai::Gxrb_ratio, ledger.amount (transaction, receive1.hash ()));
	ASSERT_EQ (rai::genesis_amount, ledger.weight (transaction, rai::genesis_account));
}

TEST (ledger, state_rollback_send)
{
	bool init (false);
	rai::block_store store (init, rai::unique_path ());
	ASSERT_TRUE (!init);
	rai::stat stats;
	rai::ledger ledger (store, stats);
	rai::genesis genesis;
	rai::transaction transaction (store.environment, nullptr, true);
	genesis.initialize (transaction, store);
	rai::state_block send1 (rai::genesis_account, genesis.hash (), 0, rai::genesis_account, rai::genesis_amount - rai::Gxrb_ratio, rai::genesis_account, rai::test_genesis_key.prv, rai::test_genesis_key.pub, 0);
	ASSERT_EQ (rai::process_result::progress, ledger.process (transaction, send1).code);
	ASSERT_TRUE (store.block_exists (transaction, send1.hash ()));
	auto send2 (store.block_get (transaction, send1.hash ()));
	ASSERT_NE (nullptr, send2);
	ASSERT_EQ (send1, *send2);
	ASSERT_EQ (rai::genesis_amount - rai::Gxrb_ratio, ledger.account_balance (transaction, rai::genesis_account));
	ASSERT_EQ (rai::genesis_amount - rai::Gxrb_ratio, ledger.weight (transaction, rai::genesis_account));
	rai::pending_info info;
	ASSERT_FALSE (store.pending_get (transaction, rai::pending_key (rai::genesis_account, send1.hash ()), info));
	ASSERT_EQ (rai::genesis_account, info.source);
	ASSERT_EQ (rai::Gxrb_ratio, info.amount.number ());
	ledger.rollback (transaction, send1.hash ());
	ASSERT_FALSE (store.block_exists (transaction, send1.hash ()));
	ASSERT_EQ (rai::genesis_amount, ledger.account_balance (transaction, rai::genesis_account));
	ASSERT_EQ (rai::genesis_amount, ledger.weight (transaction, rai::genesis_account));
	ASSERT_FALSE (store.pending_exists (transaction, rai::pending_key (rai::genesis_account, send1.hash ())));
	ASSERT_TRUE (store.block_successor (transaction, genesis.hash ()).is_zero ());
}

TEST (ledger, state_rollback_receive)
{
	bool init (false);
	rai::block_store store (init, rai::unique_path ());
	ASSERT_TRUE (!init);
	rai::stat stats;
	rai::ledger ledger (store, stats);
	rai::genesis genesis;
	rai::transaction transaction (store.environment, nullptr, true);
	genesis.initialize (transaction, store);
	rai::state_block send1 (rai::genesis_account, genesis.hash (), 0, rai::genesis_account, rai::genesis_amount - rai::Gxrb_ratio, rai::genesis_account, rai::test_genesis_key.prv, rai::test_genesis_key.pub, 0);
	ASSERT_EQ (rai::process_result::progress, ledger.process (transaction, send1).code);
	rai::state_block receive1 (rai::genesis_account, send1.hash (), 0, rai::genesis_account, rai::genesis_amount, send1.hash (), rai::test_genesis_key.prv, rai::test_genesis_key.pub, 0);
	ASSERT_EQ (rai::process_result::progress, ledger.process (transaction, receive1).code);
	ASSERT_FALSE (store.pending_exists (transaction, rai::pending_key (rai::genesis_account, receive1.hash ())));
	ledger.rollback (transaction, receive1.hash ());
	rai::pending_info info;
	ASSERT_FALSE (store.pending_get (transaction, rai::pending_key (rai::genesis_account, send1.hash ()), info));
	ASSERT_EQ (rai::genesis_account, info.source);
	ASSERT_EQ (rai::Gxrb_ratio, info.amount.number ());
	ASSERT_FALSE (store.block_exists (transaction, receive1.hash ()));
	ASSERT_EQ (rai::genesis_amount - rai::Gxrb_ratio, ledger.account_balance (transaction, rai::genesis_account));
	ASSERT_EQ (rai::genesis_amount - rai::Gxrb_ratio, ledger.weight (transaction, rai::genesis_account));
}

TEST (ledger, state_rollback_received_send)
{
	bool init (false);
	rai::block_store store (init, rai::unique_path ());
	ASSERT_TRUE (!init);
	rai::stat stats;
	rai::ledger ledger (store, stats);
	rai::genesis genesis;
	rai::transaction transaction (store.environment, nullptr, true);
	genesis.initialize (transaction, store);
	rai::keypair key;
	rai::state_block send1 (rai::genesis_account, genesis.hash (), 0, rai::genesis_account, rai::genesis_amount - rai::Gxrb_ratio, key.pub, rai::test_genesis_key.prv, rai::test_genesis_key.pub, 0);
	ASSERT_EQ (rai::process_result::progress, ledger.process (transaction, send1).code);
	rai::state_block receive1 (key.pub, 0, 0, key.pub, rai::Gxrb_ratio, send1.hash (), key.prv, key.pub, 0);
	ASSERT_EQ (rai::process_result::progress, ledger.process (transaction, receive1).code);
	ASSERT_FALSE (store.pending_exists (transaction, rai::pending_key (rai::genesis_account, receive1.hash ())));
	ledger.rollback (transaction, send1.hash ());
	ASSERT_FALSE (store.pending_exists (transaction, rai::pending_key (rai::genesis_account, send1.hash ())));
	ASSERT_FALSE (store.block_exists (transaction, send1.hash ()));
	ASSERT_FALSE (store.block_exists (transaction, receive1.hash ()));
	ASSERT_EQ (rai::genesis_amount, ledger.account_balance (transaction, rai::genesis_account));
	ASSERT_EQ (rai::genesis_amount, ledger.weight (transaction, rai::genesis_account));
	ASSERT_EQ (0, ledger.account_balance (transaction, key.pub));
	ASSERT_EQ (0, ledger.weight (transaction, key.pub));
}

TEST (ledger, state_rep_change_rollback)
{
	bool init (false);
	rai::block_store store (init, rai::unique_path ());
	ASSERT_TRUE (!init);
	rai::stat stats;
	rai::ledger ledger (store, stats);
	rai::genesis genesis;
	rai::transaction transaction (store.environment, nullptr, true);
	genesis.initialize (transaction, store);
	rai::keypair rep;
	rai::state_block change1 (rai::genesis_account, genesis.hash (), 0, rep.pub, rai::genesis_amount, 0, rai::test_genesis_key.prv, rai::test_genesis_key.pub, 0);
	ASSERT_EQ (rai::process_result::progress, ledger.process (transaction, change1).code);
	ledger.rollback (transaction, change1.hash ());
	ASSERT_FALSE (store.block_exists (transaction, change1.hash ()));
	ASSERT_EQ (rai::genesis_amount, ledger.account_balance (transaction, rai::genesis_account));
	ASSERT_EQ (rai::genesis_amount, ledger.weight (transaction, rai::genesis_account));
	ASSERT_EQ (0, ledger.weight (transaction, rep.pub));
}

TEST (ledger, state_open_rollback)
{
	bool init (false);
	rai::block_store store (init, rai::unique_path ());
	ASSERT_TRUE (!init);
	rai::stat stats;
	rai::ledger ledger (store, stats);
	rai::genesis genesis;
	rai::transaction transaction (store.environment, nullptr, true);
	genesis.initialize (transaction, store);
	rai::keypair destination;
	rai::state_block send1 (rai::genesis_account, genesis.hash (), 0, rai::genesis_account, rai::genesis_amount - rai::Gxrb_ratio, destination.pub, rai::test_genesis_key.prv, rai::test_genesis_key.pub, 0);
	ASSERT_EQ (rai::process_result::progress, ledger.process (transaction, send1).code);
	rai::state_block open1 (destination.pub, 0, 0, rai::genesis_account, rai::Gxrb_ratio, send1.hash (), destination.prv, destination.pub, 0);
	ASSERT_EQ (rai::process_result::progress, ledger.process (transaction, open1).code);
	ledger.rollback (transaction, open1.hash ());
	ASSERT_FALSE (store.block_exists (transaction, open1.hash ()));
	ASSERT_EQ (0, ledger.account_balance (transaction, destination.pub));
	ASSERT_EQ (rai::genesis_amount - rai::Gxrb_ratio, ledger.weight (transaction, rai::genesis_account));
	rai::pending_info info;
	ASSERT_FALSE (store.pending_get (transaction, rai::pending_key (destination.pub, send1.hash ()), info));
	ASSERT_EQ (rai::genesis_account, info.source);
	ASSERT_EQ (rai::Gxrb_ratio, info.amount.number ());
}

TEST (ledger, state_send_change_rollback)
{
	bool init (false);
	rai::block_store store (init, rai::unique_path ());
	ASSERT_TRUE (!init);
	rai::stat stats;
	rai::ledger ledger (store, stats);
	rai::genesis genesis;
	rai::transaction transaction (store.environment, nullptr, true);
	genesis.initialize (transaction, store);
	rai::keypair rep;
	rai::state_block send1 (rai::genesis_account, genesis.hash (), 0, rep.pub, rai::genesis_amount - rai::Gxrb_ratio, rai::genesis_account, rai::test_genesis_key.prv, rai::test_genesis_key.pub, 0);
	ASSERT_EQ (rai::process_result::progress, ledger.process (transaction, send1).code);
	ledger.rollback (transaction, send1.hash ());
	ASSERT_FALSE (store.block_exists (transaction, send1.hash ()));
	ASSERT_EQ (rai::genesis_amount, ledger.account_balance (transaction, rai::genesis_account));
	ASSERT_EQ (rai::genesis_amount, ledger.weight (transaction, rai::genesis_account));
	ASSERT_EQ (0, ledger.weight (transaction, rep.pub));
}

TEST (ledger, state_receive_change_rollback)
{
	bool init (false);
	rai::block_store store (init, rai::unique_path ());
	ASSERT_TRUE (!init);
	rai::stat stats;
	rai::ledger ledger (store, stats);
	rai::genesis genesis;
	rai::transaction transaction (store.environment, nullptr, true);
	genesis.initialize (transaction, store);
	rai::state_block send1 (rai::genesis_account, genesis.hash (), 0, rai::genesis_account, rai::genesis_amount - rai::Gxrb_ratio, rai::genesis_account, rai::test_genesis_key.prv, rai::test_genesis_key.pub, 0);
	ASSERT_EQ (rai::process_result::progress, ledger.process (transaction, send1).code);
	rai::keypair rep;
	rai::state_block receive1 (rai::genesis_account, send1.hash (), 0, rep.pub, rai::genesis_amount, send1.hash (), rai::test_genesis_key.prv, rai::test_genesis_key.pub, 0);
	ASSERT_EQ (rai::process_result::progress, ledger.process (transaction, receive1).code);
	ledger.rollback (transaction, receive1.hash ());
	ASSERT_FALSE (store.block_exists (transaction, receive1.hash ()));
	ASSERT_EQ (rai::genesis_amount - rai::Gxrb_ratio, ledger.account_balance (transaction, rai::genesis_account));
	ASSERT_EQ (rai::genesis_amount - rai::Gxrb_ratio, ledger.weight (transaction, rai::genesis_account));
	ASSERT_EQ (0, ledger.weight (transaction, rep.pub));
}

/* Epoch and upgrades are no longer relevant
TEST (ledger, epoch_blocks_general)
TEST (ledger, epoch_blocks_receive_upgrade)
TEST (ledger, epoch_blocks_fork)
*/

rai::amount_t reference_manna_increment (rai::timestamp_t time1, rai::timestamp_t time2)
{
	uint32_t t1 = (uint32_t)(time1 / rai::manna_control::manna_freq);
	uint32_t t2 = (uint32_t)(time2 / rai::manna_control::manna_freq);
	return (rai::amount_t) (t2 - t1) * (rai::amount_t)rai::manna_control::manna_increment;
}

TEST (ledger_manna, balance_later)
{
	bool init (false);
	rai::block_store store (init, rai::unique_path ());
	ASSERT_TRUE (!init);
	rai::stat stats;
	rai::ledger ledger (store, stats);
	rai::genesis genesis;
	rai::transaction transaction (store.environment, nullptr, true);
	genesis.initialize (transaction, store);
	ASSERT_EQ (rai::genesis_amount, ledger.account_balance (transaction, rai::genesis_account));
	// send to manna account
	rai::timestamp_t time1 = rai::genesis_time + 12345;
	rai::state_block send (rai::genesis_account, genesis.hash (), time1, rai::genesis_account, rai::genesis_amount - 100000000, rai::manna_account, rai::test_genesis_key.prv, rai::test_genesis_key.pub, 0);
	ASSERT_EQ (rai::process_result::progress, ledger.process (transaction, send).code);
	ASSERT_EQ (rai::genesis_amount - 100000000, ledger.account_balance (transaction, rai::genesis_account));
	ASSERT_EQ (rai::genesis_amount - 100000000, ledger.account_balance_with_manna (transaction, rai::genesis_account, time1));
	ASSERT_EQ (100000000, ledger.amount (transaction, send.hash ()));
	// initial receive on manna account
	rai::timestamp_t time2 = time1 + 30;
	rai::state_block receive (rai::manna_account, 0, time2, rai::manna_account, 100000000, send.hash (), rai::test_manna_key.prv, rai::test_manna_key.pub, 0);
	ASSERT_EQ (rai::process_result::progress, ledger.process (transaction, receive).code);
	ASSERT_EQ (100000000, ledger.account_balance (transaction, rai::manna_account));
	ASSERT_EQ (100000000, ledger.account_balance_with_manna (transaction, rai::manna_account, time2));
	ASSERT_EQ (100000000, ledger.amount (transaction, receive.hash ()));
	// at later time balance increases
	rai::timestamp_t time3 = time2 + 600;
	ASSERT_EQ (100000000, ledger.account_balance (transaction, rai::manna_account));
	ASSERT_EQ (100000000 + reference_manna_increment (time2, time3), ledger.account_balance_with_manna (transaction, rai::manna_account, time3));
	rai::timestamp_t time4 = time3 + 5000;
	ASSERT_EQ (100000000 + reference_manna_increment (time2, time4), ledger.account_balance_with_manna (transaction, rai::manna_account, time4));
	// no manna genesis in the past
	rai::timestamp_t time0_before = rai::genesis_time - 1234567;
	ASSERT_EQ (100000000 - reference_manna_increment (rai::genesis_time, time2), ledger.account_balance_with_manna (transaction, rai::manna_account, time0_before));
}

TEST (ledger_manna, send)
{
	bool init (false);
	rai::block_store store (init, rai::unique_path ());
	ASSERT_TRUE (!init);
	rai::stat stats;
	rai::ledger ledger (store, stats);
	rai::genesis genesis;
	rai::transaction transaction (store.environment, nullptr, true);
	genesis.initialize (transaction, store);
	ASSERT_EQ (rai::genesis_amount, ledger.account_balance (transaction, rai::genesis_account));
	// send to manna account
	rai::timestamp_t time1 = rai::genesis_time + 12345;
	rai::state_block send0 (rai::genesis_account, genesis.hash (), time1, rai::genesis_account, rai::genesis_amount - 100000000, rai::manna_account, rai::test_genesis_key.prv, rai::test_genesis_key.pub, 0);
	ASSERT_EQ (rai::process_result::progress, ledger.process (transaction, send0).code);
	ASSERT_EQ (rai::genesis_amount - 100000000, ledger.account_balance_with_manna (transaction, rai::genesis_account, time1));
	// initial receive on manna account
	rai::timestamp_t time2 = time1 + 30;
	rai::state_block receive0 (rai::manna_account, 0, time2, rai::manna_account, 100000000, send0.hash (), rai::test_manna_key.prv, rai::test_manna_key.pub, 0);
	ASSERT_EQ (rai::process_result::progress, ledger.process (transaction, receive0).code);
	ASSERT_EQ (100000000, ledger.account_balance (transaction, rai::manna_account));
	ASSERT_EQ (100000000, ledger.account_balance_with_manna (transaction, rai::manna_account, time2));

	// send from manna account
	rai::keypair key3;
	rai::timestamp_t time3 = time2 + 600;
	ASSERT_FALSE (rai::manna_control::is_manna_account (rai::genesis_account));
	ASSERT_TRUE (rai::manna_control::is_manna_account (rai::manna_account));
	ASSERT_FALSE (rai::manna_control::is_manna_account (key3.pub));
	ASSERT_GT (reference_manna_increment (time2, time3), 0);
	rai::state_block send (rai::manna_account, receive0.hash (), time3, rai::manna_account, 100000000 + reference_manna_increment (time2, time3) - 100, key3.pub, rai::test_manna_key.prv, rai::test_manna_key.pub, 0);
	ASSERT_EQ (rai::state_block_subtype::send, ledger.state_subtype (transaction, send));
	ASSERT_EQ (rai::process_result::progress, ledger.process (transaction, send).code);
	ASSERT_EQ (100, ledger.amount (transaction, send.hash ()));
	ASSERT_EQ (100000000 + reference_manna_increment (time2, time3) - 100, ledger.account_balance (transaction, rai::manna_account));
	ASSERT_EQ (100000000 + reference_manna_increment (time2, time3) - 100, ledger.account_balance_with_manna (transaction, rai::manna_account, time3));
	ASSERT_EQ (100, ledger.account_pending (transaction, key3.pub));
	// receive from manna account
	rai::timestamp_t time4 = time3 + 30;
	rai::state_block receive (key3.pub, 0, time4, key3.pub, 100, send.hash (), key3.prv, key3.pub, 0);
	ASSERT_EQ (rai::state_block_subtype::open_receive, ledger.state_subtype (transaction, receive));
	ASSERT_EQ (rai::process_result::progress, ledger.process (transaction, receive).code);
	ASSERT_EQ (100, ledger.amount (transaction, receive.hash ()));
	ASSERT_EQ (100, ledger.account_balance_with_manna (transaction, key3.pub, time3));
	ASSERT_EQ (0, ledger.account_pending (transaction, key3.pub));

	// receive to manna account
	rai::timestamp_t time7 = time4 + 1200;
	rai::state_block send2 (key3.pub, receive.hash(), time7, key3.pub, 100 - 10, rai::manna_account, key3.prv, key3.pub, 0);
	ASSERT_EQ (rai::state_block_subtype::send, ledger.state_subtype (transaction, send2));
	ASSERT_EQ (rai::process_result::progress, ledger.process (transaction, send2).code);
	ASSERT_EQ (10, ledger.amount (transaction, send2.hash ()));
	ASSERT_EQ (100 - 10, ledger.account_balance_with_manna (transaction, key3.pub, time7));
	ASSERT_EQ (10, ledger.account_pending (transaction, rai::manna_account));
	rai::timestamp_t time8 = time7 + 30;
	rai::state_block receive2 (rai::manna_account, send.hash (), time8, rai::manna_account, 100000000 + reference_manna_increment (time2, time8) - 100 + 10, send2.hash (), rai::test_manna_key.prv, rai::test_manna_key.pub, 0);
	ASSERT_EQ (rai::state_block_subtype::receive, ledger.state_subtype (transaction, receive2));
	ASSERT_EQ (rai::process_result::progress, ledger.process (transaction, receive2).code);
	ASSERT_EQ (10, ledger.amount (transaction, receive2.hash ()));
	ASSERT_EQ (100000000 + reference_manna_increment (time2, time8) - 100 + 10, ledger.account_balance_with_manna (transaction, rai::manna_account, time8));
	ASSERT_EQ (0, ledger.account_pending (transaction, rai::manna_account));

	// check balances at later time
	rai::timestamp_t time10 = time1 + 2000000;
	ASSERT_EQ (rai::genesis_amount - 100000000, ledger.account_balance_with_manna (transaction, rai::genesis_account, time10));
	ASSERT_EQ (100000000 + reference_manna_increment (time2, time10) - 100 + 10, ledger.account_balance_with_manna (transaction, rai::manna_account, time10));  // increases
	ASSERT_EQ (100 - 10, ledger.account_balance_with_manna (transaction, key3.pub, time10));

	// check balances at end-of-the-world (sometime in year 2150)
	rai::timestamp_t time_eotw = std::numeric_limits<rai::timestamp_t>::max () - 11;
	std::string time_eotw_string = rai::short_timestamp (time_eotw).to_date_string_utc ();
	auto bal_at_eotw (100000000 + reference_manna_increment (time2, time_eotw) - 100 + 10);
	ASSERT_EQ (bal_at_eotw, ledger.account_balance_with_manna (transaction, rai::manna_account, time_eotw));  // increases
}

TEST (ledger_manna, receive_into_manna_wrong_amount)
{
	bool init (false);
	rai::block_store store (init, rai::unique_path ());
	ASSERT_TRUE (!init);
	rai::stat stats;
	rai::ledger ledger (store, stats);
	rai::genesis genesis;
	rai::transaction transaction (store.environment, nullptr, true);
	genesis.initialize (transaction, store);
	ASSERT_EQ (rai::genesis_amount, ledger.account_balance (transaction, rai::genesis_account));
	// send to manna account
	rai::timestamp_t time1 = rai::genesis_time + 12345;
	rai::state_block send0 (rai::genesis_account, genesis.hash (), time1, rai::genesis_account, rai::genesis_amount - 100000000, rai::manna_account, rai::test_genesis_key.prv, rai::test_genesis_key.pub, 0);
	ASSERT_EQ (rai::process_result::progress, ledger.process (transaction, send0).code);
	ASSERT_EQ (rai::genesis_amount - 100000000, ledger.account_balance_with_manna (transaction, rai::genesis_account, time1));
	// initial receive on manna account
	rai::timestamp_t time2 = time1 + 30;
	rai::state_block receive0 (rai::manna_account, 0, time2, rai::manna_account, 100000000, send0.hash (), rai::test_manna_key.prv, rai::test_manna_key.pub, 0);
	ASSERT_EQ (rai::process_result::progress, ledger.process (transaction, receive0).code);
	ASSERT_EQ (100000000, ledger.account_balance (transaction, rai::manna_account));
	ASSERT_EQ (100000000, ledger.account_balance_with_manna (transaction, rai::manna_account, time2));

	// send to manna
	rai::timestamp_t time3 = time2 + 600;
	rai::state_block send (rai::genesis_account, send0.hash (), time3, rai::genesis_account, rai::genesis_amount - 100000000 - 100, rai::manna_account, rai::test_genesis_key.prv, rai::test_genesis_key.pub, 0);
	ASSERT_EQ (rai::state_block_subtype::send, ledger.state_subtype (transaction, send));
	ASSERT_EQ (rai::process_result::progress, ledger.process (transaction, send).code);
	ASSERT_EQ (100, ledger.amount (transaction, send.hash ()));
	ASSERT_EQ (100000000 + reference_manna_increment (time2, time3), ledger.account_balance_with_manna (transaction, rai::manna_account, time3));
	ASSERT_EQ (100, ledger.account_pending (transaction, rai::manna_account));
	// receive into manna account with wrong amount
	rai::timestamp_t time4 = time3 + 30;
	rai::state_block receive (rai::manna_account, receive0.hash (), time4, rai::manna_account, 100000000 + reference_manna_increment (time2, time4) + 100 + 1, send.hash (), rai::test_manna_key.prv, rai::test_manna_key.pub, 0);
	ASSERT_EQ (rai::state_block_subtype::receive, ledger.state_subtype (transaction, receive));
	ASSERT_EQ (rai::process_result::balance_mismatch, ledger.process (transaction, receive).code);
}
