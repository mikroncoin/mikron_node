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
	rai::state_block block (key1.pub, 0, 0, rai::genesis_account, 13, 1, key1.prv, key1.pub, 2);
	rai::uint256_union hash (block.hash ());
	ASSERT_FALSE (rai::validate_message (key1.pub, hash, block.signature_get ()));
	auto signature (block.signature_get ());
	signature.bytes[32] ^= 0x1;
	block.signature_set (signature);
	ASSERT_TRUE (rai::validate_message (key1.pub, hash, block.signature_get ()));
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
	auto block (std::unique_ptr<rai::state_block> (new rai::state_block (2, 0, 0, rai::genesis_account, 200, key2.pub, rai::keypair ().prv, 2, 3)));
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
	auto block (std::unique_ptr<rai::state_block> (new rai::state_block (2, 0, 0, rai::genesis_account, 200, key2.pub, rai::keypair ().prv, 2, 3)));
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
	ASSERT_EQ (212, rai::state_block::size);
	rai::keypair key1;
	rai::keypair key2;
	rai::state_block block1 (key1.pub, 1, 12345, key2.pub, 2, 4, key1.prv, key1.pub, 5);
	ASSERT_EQ (key1.pub, block1.account ());
	ASSERT_EQ (rai::block_hash (1), block1.previous ());
	ASSERT_EQ (rai::short_timestamp (12345).data.number (), block1.creation_time ().data.number ());
	ASSERT_EQ (key2.pub, block1.representative ());
	ASSERT_EQ (rai::amount (2), block1.balance ());
	ASSERT_EQ (rai::uint256_union (4), block1.link ());
	std::vector<uint8_t> bytes;
	{
		rai::vectorstream stream (bytes);
		block1.serialize (stream);
	}
	ASSERT_EQ (rai::state_block::size, bytes.size ());
	ASSERT_EQ (212, bytes.size ());
	ASSERT_EQ (32 + 4 + 32 + 32 + 8 + 32 + 64 + 8, bytes.size ());
	ASSERT_EQ (0x39, bytes[32 + 3]); // Ensure creation_time is serialized big-endian
	ASSERT_EQ (0x00, bytes[32 + 0]);
	ASSERT_EQ (0x02, bytes[32 + 4 + 32 + 32 + 7]); // Ensure amount is serialized big-endian
	ASSERT_EQ (0x00, bytes[32 + 4 + 32 + 32 + 0]);
	ASSERT_EQ (0x05, bytes[32 + 4 + 32 + 32 + 8 + 32 + 64 + 7]); // Ensure work is serialized big-endian
	ASSERT_EQ (0x00, bytes[32 + 4 + 32 + 32 + 8 + 32 + 64 + 0]);
	bool error1 (false);
	rai::bufferstream stream (bytes.data (), bytes.size ());
	rai::state_block block2 (error1, stream);
	ASSERT_FALSE (error1);
	ASSERT_EQ (key1.pub, block2.account ());
	ASSERT_EQ (rai::block_hash (1), block2.previous ());
	ASSERT_EQ (rai::short_timestamp (12345).data.number (), block2.creation_time ().data.number ());
	ASSERT_EQ (key2.pub, block2.representative ());
	ASSERT_EQ (rai::amount (2), block2.balance ());
	ASSERT_EQ (rai::uint256_union (4), block2.link ());
	ASSERT_EQ (block1, block2);
	block2.account_set (rai::account ());
	block2.previous_set (rai::block_hash ());
	block2.creation_time ().data.decode_dec ("0");
	block2.representative_set (rai::account ());
	block2.balance_set (rai::amount ());
	block2.hashables.link.clear ();
	block2.signature_set (rai::uint512_union (0));
	block2.work_set (0);
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
	block3.account_set (rai::account ());
	block3.previous_set (rai::account ());
	block2.creation_time ().data.decode_dec ("0");
	block3.representative_set (rai::account ());
	block3.balance_set (rai::amount ());
	block3.hashables.link.clear ();
	block3.signature_set (rai::uint512_union (0));
	block3.work_set (0);
	ASSERT_FALSE (block3.deserialize_json (tree));
	ASSERT_EQ (block1, block3);
}

TEST (state_block, hashing)
{
	rai::keypair key;
	rai::state_block block (key.pub, 0, 12345, key.pub, 0, 0, key.prv, key.pub, 0);
	auto hash (block.hash ());
	auto account (block.account ());
	account.bytes[0] ^= 0x1;
	block.account_set (account);
	ASSERT_NE (hash, block.hash ());
	account.bytes[0] ^= 0x1;
	block.account_set (account);
	ASSERT_EQ (hash, block.hash ());
	auto previous (block.previous ());
	previous.bytes[0] ^= 0x1;
	block.previous_set (previous);
	ASSERT_NE (hash, block.hash ());
	previous.bytes[0] ^= 0x1;
	block.previous_set (previous);
	ASSERT_EQ (hash, block.hash ());
	auto representative (block.representative ());
	representative.bytes[0] ^= 0x1;
	block.representative_set (representative);
	ASSERT_NE (hash, block.hash ());
	representative.bytes[0] ^= 0x1;
	block.representative_set (representative);
	ASSERT_EQ (hash, block.hash ());
	auto balance (block.balance ());
	((uint8_t *)&balance.data)[0] ^= 0x1;
	block.balance_set (balance);
	ASSERT_NE (hash, block.hash ());
	((uint8_t *)&balance.data)[0] ^= 0x1;
	block.balance_set (balance);
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
	ASSERT_EQ (rai::state_block_subtype::send, send.get_subtype (rai::genesis_amount, 0));
	ASSERT_FALSE (send.is_valid_open_subtype ());
	ASSERT_TRUE (send.is_valid_send_or_receive_subtype ());
	ASSERT_FALSE (send.is_valid_change_subtype ());
	ASSERT_EQ (rai::process_result::progress, ledger.process (transaction, send).code);
	rai::state_block open (key.pub, 0, 0, key.pub, 100, send.hash (), key.prv, key.pub, 0);
	ASSERT_EQ (rai::state_block_subtype::open_receive, open.get_subtype (0, 0));
	ASSERT_TRUE (open.is_valid_open_subtype ());
	ASSERT_FALSE (open.is_valid_send_or_receive_subtype ());
	ASSERT_FALSE (open.is_valid_change_subtype ());
	ASSERT_EQ (rai::process_result::progress, ledger.process (transaction, open).code);
	rai::state_block send2 (rai::genesis_account, send.hash (), 0, rai::genesis_account, rai::genesis_amount - 210, key.pub, rai::test_genesis_key.prv, rai::test_genesis_key.pub, 0);
	ASSERT_EQ (rai::state_block_subtype::send, send2.get_subtype (rai::genesis_amount - 100, 0));
	ASSERT_FALSE (send2.is_valid_open_subtype ());
	ASSERT_TRUE (send2.is_valid_send_or_receive_subtype ());
	ASSERT_FALSE (send2.is_valid_change_subtype ());
	ASSERT_EQ (rai::process_result::progress, ledger.process (transaction, send2).code);
	rai::state_block receive (key.pub, open.hash (), 0, key.pub, 210, send2.hash(), key.prv, key.pub, 0);
	ASSERT_EQ (rai::state_block_subtype::receive, receive.get_subtype (100, 0));
	ASSERT_FALSE (receive.is_valid_open_subtype ());
	ASSERT_TRUE (receive.is_valid_send_or_receive_subtype ());
	ASSERT_FALSE (receive.is_valid_change_subtype ());
	ASSERT_EQ (rai::process_result::progress, ledger.process (transaction, receive).code);
	rai::state_block change (key.pub, receive.hash (), 0, rai::genesis_account, 210, 0, key.prv, key.pub, 0);
	ASSERT_EQ (rai::state_block_subtype::change, change.get_subtype (210, 0));
	ASSERT_FALSE (change.is_valid_open_subtype ());
	ASSERT_FALSE (change.is_valid_send_or_receive_subtype ());
	ASSERT_TRUE (change.is_valid_change_subtype ());
	ASSERT_EQ (rai::process_result::progress, ledger.process (transaction, change).code);
	// invalid block
	rai::state_block invalid (key.pub, receive.hash (), 0, rai::genesis_account, 210, send2.hash (), key.prv, key.pub, 0);
	ASSERT_EQ (rai::state_block_subtype::undefined, invalid.get_subtype (210, 0));
	ASSERT_FALSE (invalid.is_valid_open_subtype ());
	ASSERT_TRUE (invalid.is_valid_send_or_receive_subtype ());
	ASSERT_FALSE (invalid.is_valid_change_subtype ());
	ASSERT_EQ (rai::process_result::invalid_state_block, ledger.process (transaction, invalid).code);
}

TEST (comment_block, create)
{
	rai::keypair key;
	auto comment (rai::comment_block (1, 2, 0, 5, 6, rai::comment_block_subtype::account, "COMMENT1", key.prv, key.pub, 0));
	ASSERT_EQ (rai::comment_block_subtype::account, comment.subtype ());
	ASSERT_EQ ("COMMENT1", comment.comment ());
}

TEST (comment_block, raw_comment)
{
	auto comment1 ("COMMENT1");
	auto comment_raw1 (rai::comment_block::comment_string_to_raw (comment1));
	ASSERT_EQ ('C', comment_raw1.bytes[0]);
	ASSERT_EQ ('O', comment_raw1.bytes[1]);
	ASSERT_EQ ('M', comment_raw1.bytes[2]);
	ASSERT_EQ ('1', comment_raw1.bytes[7]);
	ASSERT_EQ (0, comment_raw1.bytes[8]);
	auto comment2 (rai::comment_block::comment_raw_to_string (comment_raw1));
	ASSERT_EQ ("COMMENT1", comment2);

	rai::keypair key;
	auto comment5 (rai::comment_block (1, 2, 0, 5, 6, rai::comment_block_subtype::account, "COMMENT1", key.prv, key.pub, 0));
	ASSERT_EQ ("COMMENT1", comment5.comment ());
	ASSERT_EQ ('O', comment5.comment_raw ().bytes[1]);
}

TEST (comment_block, long_comment)
{
	int maxlen = 64;
	std::string max_comment;
	for (auto i (0); i < maxlen; ++i)
	{
		max_comment.append(std::to_string (i % 10));
	}
	std::string toolong_comment (max_comment);
	toolong_comment.append ("EXTRA");

	rai::keypair key;
	// comment truncated
	auto comment5 (rai::comment_block (1, 2, 0, 5, 6, rai::comment_block_subtype::account, toolong_comment, key.prv, key.pub, 0));
	ASSERT_EQ (max_comment, comment5.comment ());
	ASSERT_EQ (maxlen, comment5.comment ().length ());
}

TEST (comment_block, utf_comment)
{
	// TODO UTF-8 conversion
	std::string comment1 ("MÍKRÓ+ÁÉÍÓŐÚŰX");
	ASSERT_EQ (23, comment1.length ());  // longer, UTF
	auto comment_raw1 (rai::comment_block::comment_string_to_raw (comment1));
	ASSERT_EQ ('M', comment_raw1.bytes[0]);
	ASSERT_EQ (195, comment_raw1.bytes[1]);
	ASSERT_EQ ('X', comment_raw1.bytes[22]);
	ASSERT_EQ (0, comment_raw1.bytes[23]);
	auto comment2 (rai::comment_block::comment_raw_to_string (comment_raw1));
	ASSERT_EQ (comment1, comment2);
	ASSERT_EQ (23, comment2.length ());
}

TEST (comment_block, serialization)
{
	ASSERT_EQ (245, rai::comment_block::size);
	// TODO
}
