#include <boost/property_tree/json_parser.hpp>

#include <fstream>

#include <gtest/gtest.h>

#include <rai/lib/interface.h>
#include <rai/node/common.hpp>
#include <rai/node/node.hpp>

#include <ed25519-donna/ed25519.h>

TEST (ed25519, signing)
{
	rai::uint256_union prv (0);
	rai::uint256_union pub;
	ed25519_publickey (prv.bytes.data (), pub.bytes.data ());
	rai::uint256_union message (0);
	rai::uint512_union signature;
	ed25519_sign (message.bytes.data (), sizeof (message.bytes), prv.bytes.data (), pub.bytes.data (), signature.bytes.data ());
	auto valid1 (ed25519_sign_open (message.bytes.data (), sizeof (message.bytes), pub.bytes.data (), signature.bytes.data ()));
	ASSERT_EQ (0, valid1);
	signature.bytes[32] ^= 0x1;
	auto valid2 (ed25519_sign_open (message.bytes.data (), sizeof (message.bytes), pub.bytes.data (), signature.bytes.data ()));
	ASSERT_NE (0, valid2);
}

TEST (transaction_block, empty)
{
	rai::keypair key1;
	rai::send_block block (0, 1, 13, key1.prv, key1.pub, 2);
	rai::uint256_union hash (block.hash ());
	ASSERT_FALSE (rai::validate_message (key1.pub, hash, block.signature));
	block.signature.bytes[32] ^= 0x1;
	ASSERT_TRUE (rai::validate_message (key1.pub, hash, block.signature));
}

TEST (block, send_serialize)
{
	rai::send_block block1 (0, 1, 2, rai::keypair ().prv, 4, 5);
	std::vector<uint8_t> bytes;
	{
		rai::vectorstream stream1 (bytes);
		block1.serialize (stream1);
	}
	auto data (bytes.data ());
	auto size (bytes.size ());
	ASSERT_NE (nullptr, data);
	ASSERT_NE (0, size);
	rai::bufferstream stream2 (data, size);
	bool error (false);
	rai::send_block block2 (error, stream2);
	ASSERT_FALSE (error);
	ASSERT_EQ (block1, block2);
}

TEST (block, send_serialize_json)
{
	rai::send_block block1 (0, 1, 2, rai::keypair ().prv, 4, 5);
	std::string string1;
	block1.serialize_json (string1);
	ASSERT_NE (0, string1.size ());
	boost::property_tree::ptree tree1;
	std::stringstream istream (string1);
	boost::property_tree::read_json (istream, tree1);
	bool error (false);
	rai::send_block block2 (error, tree1);
	ASSERT_FALSE (error);
	ASSERT_EQ (block1, block2);
}

TEST (block, receive_serialize)
{
	rai::receive_block block1 (0, 1, rai::keypair ().prv, 3, 4);
	rai::keypair key1;
	std::vector<uint8_t> bytes;
	{
		rai::vectorstream stream1 (bytes);
		block1.serialize (stream1);
	}
	rai::bufferstream stream2 (bytes.data (), bytes.size ());
	bool error (false);
	rai::receive_block block2 (error, stream2);
	ASSERT_FALSE (error);
	ASSERT_EQ (block1, block2);
}

TEST (block, receive_serialize_json)
{
	rai::receive_block block1 (0, 1, rai::keypair ().prv, 3, 4);
	std::string string1;
	block1.serialize_json (string1);
	ASSERT_NE (0, string1.size ());
	boost::property_tree::ptree tree1;
	std::stringstream istream (string1);
	boost::property_tree::read_json (istream, tree1);
	bool error (false);
	rai::receive_block block2 (error, tree1);
	ASSERT_FALSE (error);
	ASSERT_EQ (block1, block2);
}

TEST (block, open_serialize_json)
{
	rai::open_block block1 (0, 1, 0, rai::keypair ().prv, 0, 0);
	std::string string1;
	block1.serialize_json (string1);
	ASSERT_NE (0, string1.size ());
	boost::property_tree::ptree tree1;
	std::stringstream istream (string1);
	boost::property_tree::read_json (istream, tree1);
	bool error (false);
	rai::open_block block2 (error, tree1);
	ASSERT_FALSE (error);
	ASSERT_EQ (block1, block2);
}

TEST (block, change_serialize_json)
{
	rai::change_block block1 (0, 1, rai::keypair ().prv, 3, 4);
	std::string string1;
	block1.serialize_json (string1);
	ASSERT_NE (0, string1.size ());
	boost::property_tree::ptree tree1;
	std::stringstream istream (string1);
	boost::property_tree::read_json (istream, tree1);
	bool error (false);
	rai::change_block block2 (error, tree1);
	ASSERT_FALSE (error);
	ASSERT_EQ (block1, block2);
}

TEST (uint512_union, parse_zero)
{
	rai::uint512_union input (rai::uint512_t (0));
	std::string text;
	input.encode_hex (text);
	rai::uint512_union output;
	auto error (output.decode_hex (text));
	ASSERT_FALSE (error);
	ASSERT_EQ (input, output);
	ASSERT_TRUE (output.number ().is_zero ());
}

TEST (uint512_union, parse_zero_short)
{
	std::string text ("0");
	rai::uint512_union output;
	auto error (output.decode_hex (text));
	ASSERT_FALSE (error);
	ASSERT_TRUE (output.number ().is_zero ());
}

TEST (uint512_union, parse_one)
{
	rai::uint512_union input (rai::uint512_t (1));
	std::string text;
	input.encode_hex (text);
	rai::uint512_union output;
	auto error (output.decode_hex (text));
	ASSERT_FALSE (error);
	ASSERT_EQ (input, output);
	ASSERT_EQ (1, output.number ());
}

TEST (uint512_union, parse_error_symbol)
{
	rai::uint512_union input (rai::uint512_t (1000));
	std::string text;
	input.encode_hex (text);
	text[5] = '!';
	rai::uint512_union output;
	auto error (output.decode_hex (text));
	ASSERT_TRUE (error);
}

TEST (uint512_union, max)
{
	rai::uint512_union input (std::numeric_limits<rai::uint512_t>::max ());
	std::string text;
	input.encode_hex (text);
	rai::uint512_union output;
	auto error (output.decode_hex (text));
	ASSERT_FALSE (error);
	ASSERT_EQ (input, output);
	ASSERT_EQ (rai::uint512_t ("0xffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff"), output.number ());
}

TEST (uint512_union, parse_error_overflow)
{
	rai::uint512_union input (std::numeric_limits<rai::uint512_t>::max ());
	std::string text;
	input.encode_hex (text);
	text.push_back (0);
	rai::uint512_union output;
	auto error (output.decode_hex (text));
	ASSERT_TRUE (error);
}

TEST (send_block, deserialize)
{
	rai::send_block block1 (0, 1, 2, rai::keypair ().prv, 4, 5);
	ASSERT_EQ (block1.hash (), block1.hash ());
	std::vector<uint8_t> bytes;
	{
		rai::vectorstream stream1 (bytes);
		block1.serialize (stream1);
	}
	ASSERT_EQ (rai::send_block::size, bytes.size ());
	rai::bufferstream stream2 (bytes.data (), bytes.size ());
	bool error (false);
	rai::send_block block2 (error, stream2);
	ASSERT_FALSE (error);
	ASSERT_EQ (block1, block2);
}

TEST (receive_block, deserialize)
{
	rai::receive_block block1 (0, 1, rai::keypair ().prv, 3, 4);
	ASSERT_EQ (block1.hash (), block1.hash ());
	block1.hashables.previous = 2;
	block1.hashables.source = 4;
	std::vector<uint8_t> bytes;
	{
		rai::vectorstream stream1 (bytes);
		block1.serialize (stream1);
	}
	ASSERT_EQ (rai::receive_block::size, bytes.size ());
	rai::bufferstream stream2 (bytes.data (), bytes.size ());
	bool error (false);
	rai::receive_block block2 (error, stream2);
	ASSERT_FALSE (error);
	ASSERT_EQ (block1, block2);
}

TEST (open_block, deserialize)
{
	rai::open_block block1 (0, 1, 0, rai::keypair ().prv, 0, 0);
	ASSERT_EQ (block1.hash (), block1.hash ());
	std::vector<uint8_t> bytes;
	{
		rai::vectorstream stream (bytes);
		block1.serialize (stream);
	}
	ASSERT_EQ (rai::open_block::size, bytes.size ());
	rai::bufferstream stream (bytes.data (), bytes.size ());
	bool error (false);
	rai::open_block block2 (error, stream);
	ASSERT_FALSE (error);
	ASSERT_EQ (block1, block2);
}

TEST (change_block, deserialize)
{
	rai::change_block block1 (1, 2, rai::keypair ().prv, 4, 5);
	ASSERT_EQ (block1.hash (), block1.hash ());
	std::vector<uint8_t> bytes;
	{
		rai::vectorstream stream1 (bytes);
		block1.serialize (stream1);
	}
	ASSERT_EQ (rai::change_block::size, bytes.size ());
	auto data (bytes.data ());
	auto size (bytes.size ());
	ASSERT_NE (nullptr, data);
	ASSERT_NE (0, size);
	rai::bufferstream stream2 (data, size);
	bool error (false);
	rai::change_block block2 (error, stream2);
	ASSERT_FALSE (error);
	ASSERT_EQ (block1, block2);
}

TEST (frontier_req, serialization)
{
	rai::frontier_req request1;
	request1.start = 1;
	request1.age = 2;
	request1.count = 3;
	std::vector<uint8_t> bytes;
	{
		rai::vectorstream stream (bytes);
		request1.serialize (stream);
	}
	auto error (false);
	rai::bufferstream stream (bytes.data (), bytes.size ());
	rai::message_header header (error, stream);
	ASSERT_FALSE (error);
	rai::frontier_req request2 (error, stream, header);
	ASSERT_FALSE (error);
	ASSERT_EQ (request1, request2);
}

TEST (block, publish_req_serialization)
{
	rai::keypair key1;
	rai::keypair key2;
	auto block (std::unique_ptr<rai::send_block> (new rai::send_block (0, key2.pub, 200, rai::keypair ().prv, 2, 3)));
	rai::publish req (std::move (block));
	std::vector<uint8_t> bytes;
	{
		rai::vectorstream stream (bytes);
		req.serialize (stream);
	}
	auto error (false);
	rai::bufferstream stream2 (bytes.data (), bytes.size ());
	rai::message_header header (error, stream2);
	ASSERT_FALSE (error);
	rai::publish req2 (error, stream2, header);
	ASSERT_FALSE (error);
	ASSERT_EQ (req, req2);
	ASSERT_EQ (*req.block, *req2.block);
}

TEST (block, confirm_req_serialization)
{
	rai::keypair key1;
	rai::keypair key2;
	auto block (std::unique_ptr<rai::send_block> (new rai::send_block (0, key2.pub, 200, rai::keypair ().prv, 2, 3)));
	rai::confirm_req req (std::move (block));
	std::vector<uint8_t> bytes;
	{
		rai::vectorstream stream (bytes);
		req.serialize (stream);
	}
	auto error (false);
	rai::bufferstream stream2 (bytes.data (), bytes.size ());
	rai::message_header header (error, stream2);
	rai::confirm_req req2 (error, stream2, header);
	ASSERT_FALSE (error);
	ASSERT_EQ (req, req2);
	ASSERT_EQ (*req.block, *req2.block);
}

TEST (state_block, serialization)
{
	rai::keypair key1;
	rai::keypair key2;
	rai::state_block block1 (key1.pub, 1, 12345, key2.pub, 2, 4, key1.prv, key1.pub, 5);
	ASSERT_EQ (key1.pub, block1.hashables.account);
	ASSERT_EQ (rai::block_hash (1), block1.previous ());
	ASSERT_EQ (rai::short_timestamp (12345).data.number (), block1.creation_time ().data.number ());
	ASSERT_EQ (key2.pub, block1.hashables.representative);
	ASSERT_EQ (rai::amount (2), block1.hashables.balance);
	ASSERT_EQ (rai::uint256_union (4), block1.hashables.link);
	std::vector<uint8_t> bytes;
	{
		rai::vectorstream stream (bytes);
		block1.serialize (stream);
	}
	ASSERT_EQ (rai::state_block::size, bytes.size ());
	ASSERT_EQ (0x5, bytes[219]); // Ensure work is serialized big-endian
	bool error1 (false);
	rai::bufferstream stream (bytes.data (), bytes.size ());
	rai::state_block block2 (error1, stream);
	ASSERT_FALSE (error1);
	ASSERT_EQ (key1.pub, block2.hashables.account);
	ASSERT_EQ (rai::block_hash (1), block2.previous ());
	ASSERT_EQ (rai::short_timestamp (12345).data.number (), block2.creation_time ().data.number ());
	ASSERT_EQ (key2.pub, block2.hashables.representative);
	ASSERT_EQ (rai::amount (2), block2.hashables.balance);
	ASSERT_EQ (rai::uint256_union (4), block2.hashables.link);
	ASSERT_EQ (block1, block2);
	block2.hashables.account.clear ();
	block2.hashables.previous.clear ();
	block2.hashables.creation_time.data.decode_dec ("0");
	block2.hashables.representative.clear ();
	block2.hashables.balance.clear ();
	block2.hashables.link.clear ();
	block2.signature.clear ();
	block2.work = 0;
	rai::bufferstream stream2 (bytes.data (), bytes.size ());
	ASSERT_FALSE (block2.deserialize (stream2));
	ASSERT_EQ (block1, block2);
	std::string json;
	block1.serialize_json (json);
	std::stringstream body (json);
	boost::property_tree::ptree tree;
	boost::property_tree::read_json (body, tree);
	bool error2 (false);
	rai::state_block block3 (error2, tree);
	ASSERT_FALSE (error2);
	ASSERT_EQ (block1, block3);
	block3.hashables.account.clear ();
	block3.hashables.previous.clear ();
	block2.hashables.creation_time.data.decode_dec ("0");
	block3.hashables.representative.clear ();
	block3.hashables.balance.clear ();
	block3.hashables.link.clear ();
	block3.signature.clear ();
	block3.work = 0;
	ASSERT_FALSE (block3.deserialize_json (tree));
	ASSERT_EQ (block1, block3);
}

TEST (state_block, hashing)
{
	rai::keypair key;
	rai::state_block block (key.pub, 0, 12345, key.pub, 0, 0, key.prv, key.pub, 0);
	auto hash (block.hash ());
	block.hashables.account.bytes[0] ^= 0x1;
	ASSERT_NE (hash, block.hash ());
	block.hashables.account.bytes[0] ^= 0x1;
	ASSERT_EQ (hash, block.hash ());
	block.hashables.previous.bytes[0] ^= 0x1;
	ASSERT_NE (hash, block.hash ());
	block.hashables.previous.bytes[0] ^= 0x1;
	ASSERT_EQ (hash, block.hash ());
	block.hashables.representative.bytes[0] ^= 0x1;
	ASSERT_NE (hash, block.hash ());
	block.hashables.representative.bytes[0] ^= 0x1;
	ASSERT_EQ (hash, block.hash ());
	block.hashables.balance.bytes[0] ^= 0x1;
	ASSERT_NE (hash, block.hash ());
	block.hashables.balance.bytes[0] ^= 0x1;
	ASSERT_EQ (hash, block.hash ());
	block.hashables.link.bytes[0] ^= 0x1;
	ASSERT_NE (hash, block.hash ());
	block.hashables.link.bytes[0] ^= 0x1;
	ASSERT_EQ (hash, block.hash ());
}

TEST (state_block, subtype)
{
	bool init (false);
	rai::block_store store (init, rai::unique_path ());
	ASSERT_TRUE (!init);
	rai::stat stats;
	rai::ledger ledger (store, stats);
	rai::transaction transaction (store.environment, nullptr, true);
	rai::genesis genesis;
	genesis.initialize (transaction, store);
	rai::keypair key;
	rai::account_info info1;
	ASSERT_FALSE (store.account_get (transaction, rai::genesis_account, info1));
	rai::state_block send (rai::genesis_account, info1.head, 0, rai::genesis_account, rai::genesis_amount - 100, key.pub, rai::test_genesis_key.prv, rai::test_genesis_key.pub, 0);
	ASSERT_EQ (rai::state_block_subtype::send, send.get_subtype (rai::genesis_amount));
	ASSERT_FALSE (send.is_valid_open_subtype ());
	ASSERT_TRUE (send.is_valid_send_or_receive_subtype ());
	ASSERT_FALSE (send.is_valid_change_subtype ());
	ASSERT_EQ (rai::process_result::progress, ledger.process (transaction, send).code);
	rai::state_block open (key.pub, 0, 0, key.pub, 100, send.hash (), key.prv, key.pub, 0);
	ASSERT_EQ (rai::state_block_subtype::open_receive, open.get_subtype (0));
	ASSERT_TRUE (open.is_valid_open_subtype ());
	ASSERT_FALSE (open.is_valid_send_or_receive_subtype ());
	ASSERT_FALSE (open.is_valid_change_subtype ());
	ASSERT_EQ (rai::process_result::progress, ledger.process(transaction, open).code);
	rai::state_block send2 (rai::genesis_account, send.hash (), 0, rai::genesis_account, rai::genesis_amount - 210, key.pub, rai::test_genesis_key.prv, rai::test_genesis_key.pub, 0);
	ASSERT_EQ (rai::state_block_subtype::send, send2.get_subtype (rai::genesis_amount - 100));
	ASSERT_FALSE (send2.is_valid_open_subtype ());
	ASSERT_TRUE (send2.is_valid_send_or_receive_subtype ());
	ASSERT_FALSE (send2.is_valid_change_subtype ());
	ASSERT_EQ (rai::process_result::progress, ledger.process(transaction, send2).code);
	rai::state_block receive (key.pub, open.hash (), 0, key.pub, 210, send2.hash(), key.prv, key.pub, 0);
	ASSERT_EQ (rai::state_block_subtype::receive, receive.get_subtype (100));
	ASSERT_FALSE (receive.is_valid_open_subtype ());
	ASSERT_TRUE (receive.is_valid_send_or_receive_subtype ());
	ASSERT_FALSE (receive.is_valid_change_subtype ());
	ASSERT_EQ (rai::process_result::progress, ledger.process(transaction, receive).code);
	rai::state_block change (key.pub, receive.hash (), 0, rai::genesis_account, 210, 0, key.prv, key.pub, 0);
	ASSERT_EQ (rai::state_block_subtype::change, change.get_subtype (210));
	ASSERT_FALSE (change.is_valid_open_subtype ());
	ASSERT_FALSE (change.is_valid_send_or_receive_subtype ());
	ASSERT_TRUE (change.is_valid_change_subtype ());
	ASSERT_EQ (rai::process_result::progress, ledger.process(transaction, change).code);
	// invalid block
	rai::state_block invalid (key.pub, receive.hash (), 0, rai::genesis_account, 210, send2.hash (), key.prv, key.pub, 0);
	ASSERT_EQ (rai::state_block_subtype::undefined, invalid.get_subtype (210));
	ASSERT_FALSE (invalid.is_valid_open_subtype ());
	ASSERT_TRUE (invalid.is_valid_send_or_receive_subtype ());
	ASSERT_FALSE (invalid.is_valid_change_subtype ());
	ASSERT_EQ(rai::process_result::invalid_state_block, ledger.process(transaction, invalid).code);
}
