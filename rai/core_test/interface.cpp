#include <gtest/gtest.h>

#include <memory>

#include <rai/lib/blocks.hpp>
#include <rai/lib/interface.h>
#include <rai/lib/numbers.hpp>
#include <rai/lib/work.hpp>

TEST (interface, xrb_uint64_to_dec)
{
	rai::uint64_struct zero (0);
	char text[20+1] = { 0 };
	xrb_uint64_to_dec ((uint8_t *)&zero.data, text);
	ASSERT_STREQ ("0", text);
}

TEST (interface, xrb_uint256_to_string)
{
	rai::uint256_union zero (0);
	char text[65] = { 0 };
	xrb_uint256_to_string (zero.bytes.data (), text);
	ASSERT_STREQ ("0000000000000000000000000000000000000000000000000000000000000000", text);
}

TEST (interface, xrb_uint256_to_address)
{
	rai::uint256_union zero (0);
	char text[65] = { 0 };
	xrb_uint256_to_address (zero.bytes.data (), text);
	ASSERT_STREQ ("mik_1111111111111111111111111111111111111111111111111111hifc8npp", text);
}

TEST (interface, xrb_uint512_to_string)
{
	rai::uint512_union zero (0);
	char text[129] = { 0 };
	xrb_uint512_to_string (zero.bytes.data (), text);
	ASSERT_STREQ ("00000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000", text);
}

TEST (interface, xrb_uint64_from_dec)
{
	rai::uint128_struct zero (0);
	ASSERT_EQ (0, xrb_uint64_from_dec ("0", zero.bytes.data ()));
	ASSERT_EQ (0, xrb_uint64_from_dec ("18446744073709551615", zero.bytes.data ()));  // 2^64-1
	ASSERT_EQ (1, xrb_uint64_from_dec ("18446744073709551616", zero.bytes.data ()));
	ASSERT_EQ (1, xrb_uint64_from_dec ("18446744073%09551615", zero.bytes.data ()));
}

TEST (interface, xrb_uint256_from_string)
{
	rai::uint256_union zero (0);
	ASSERT_EQ (0, xrb_uint256_from_string ("0000000000000000000000000000000000000000000000000000000000000000", zero.bytes.data ()));
	ASSERT_EQ (1, xrb_uint256_from_string ("00000000000000000000000000000000000000000000000000000000000000000", zero.bytes.data ()));
	ASSERT_EQ (1, xrb_uint256_from_string ("000000000000000000000000000%000000000000000000000000000000000000", zero.bytes.data ()));
}

TEST (interface, xrb_uint512_from_string)
{
	rai::uint512_union zero (0);
	ASSERT_EQ (0, xrb_uint512_from_string ("00000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000", zero.bytes.data ()));
	ASSERT_EQ (1, xrb_uint512_from_string ("000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000", zero.bytes.data ()));
	ASSERT_EQ (1, xrb_uint512_from_string ("0000000000000000000000000000000000000000000000000000000000%000000000000000000000000000000000000000000000000000000000000000000000", zero.bytes.data ()));
}

TEST (interface, xrb_valid_address)
{
	ASSERT_EQ (0, xrb_valid_address ("mik_1111111111111111111111111111111111111111111111111111hifc8npp"));
	ASSERT_EQ (1, xrb_valid_address ("mik_1111111111111111111111111111111111111111111111111111hifc8nppp"));
	ASSERT_EQ (1, xrb_valid_address ("mik_1111111211111111111111111111111111111111111111111111hifc8npp"));
}

TEST (interface, xrb_seed_create)
{
	rai::uint256_union seed;
	xrb_generate_random (seed.bytes.data ());
	ASSERT_FALSE (seed.is_zero ());
}

TEST (interface, xrb_seed_key)
{
	rai::uint256_union seed (0);
	rai::uint256_union prv;
	xrb_seed_key (seed.bytes.data (), 0, prv.bytes.data ());
	ASSERT_FALSE (prv.is_zero ());
}

TEST (interface, xrb_key_account)
{
	rai::uint256_union prv (0);
	rai::uint256_union pub;
	xrb_key_account (prv.bytes.data (), pub.bytes.data ());
	ASSERT_FALSE (pub.is_zero ());
}

TEST (interface, sign_transaction)
{
	rai::raw_key key;
	xrb_generate_random (key.data.bytes.data ());
	rai::uint256_union pub;
	xrb_key_account (key.data.bytes.data (), pub.bytes.data ());
	rai::state_block send (pub, 0, 0, pub, 0, 0, key, pub, 0);
	ASSERT_FALSE (rai::validate_message (pub, send.hash (), send.signature_get ()));
	auto signature (send.signature_get ());
	signature.bytes[0] ^= 1;
	send.signature_set (signature);
	ASSERT_TRUE (rai::validate_message (pub, send.hash (), send.signature_get ()));
	auto transaction (xrb_sign_transaction (send.to_json ().c_str (), key.data.bytes.data ()));
	boost::property_tree::ptree block_l;
	std::string transaction_l (transaction);
	std::stringstream block_stream (transaction_l);
	boost::property_tree::read_json (block_stream, block_l);
	auto block (rai::deserialize_block_json (block_l));
	ASSERT_NE (nullptr, block);
	auto send1 (dynamic_cast<rai::state_block *> (block.get ()));
	ASSERT_NE (nullptr, send1);
	ASSERT_FALSE (rai::validate_message (pub, send.hash (), send1->signature_get ()));
	free (transaction);
}

TEST (interface, fail_sign_transaction)
{
	rai::uint256_union data (0);
	xrb_sign_transaction ("", data.bytes.data ());
}

TEST (interface, work_transaction)
{
	rai::raw_key key;
	xrb_generate_random (key.data.bytes.data ());
	rai::uint256_union pub;
	xrb_key_account (key.data.bytes.data (), pub.bytes.data ());
	rai::state_block send (pub, 1, 0, pub, 0, 0, key, pub, 0);
	auto transaction (xrb_work_transaction (send.to_json ().c_str ()));
	boost::property_tree::ptree block_l;
	std::string transaction_l (transaction);
	std::stringstream block_stream (transaction_l);
	boost::property_tree::read_json (block_stream, block_l);
	auto block (rai::deserialize_block_json (block_l));
	ASSERT_NE (nullptr, block);
	ASSERT_FALSE (rai::work_validate (*block));
	free (transaction);
}

TEST (interface, fail_work_transaction)
{
	rai::uint256_union data (0);
	xrb_work_transaction ("");
}
