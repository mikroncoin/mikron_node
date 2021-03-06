#include <gtest/gtest.h>

#include <rai/node/common.hpp>

TEST (message, keepalive_serialization)
{
	rai::keepalive request1;
	std::vector<uint8_t> bytes;
	{
		rai::vectorstream stream (bytes);
		request1.serialize (stream);
	}
	auto error (false);
	rai::bufferstream stream (bytes.data (), bytes.size ());
	rai::message_header header (error, stream);
	ASSERT_FALSE (error);
	rai::keepalive request2 (error, stream, header);
	ASSERT_FALSE (error);
	ASSERT_EQ (request1, request2);
}

TEST (message, keepalive_deserialize)
{
	rai::keepalive message1;
	message1.peers[0] = rai::endpoint (boost::asio::ip::address_v6::loopback (), 10000);
	std::vector<uint8_t> bytes;
	{
		rai::vectorstream stream (bytes);
		message1.serialize (stream);
	}
	rai::bufferstream stream (bytes.data (), bytes.size ());
	auto error (false);
	rai::message_header header (error, stream);
	ASSERT_FALSE (error);
	ASSERT_EQ (rai::message_type::keepalive, header.message_type);
	rai::keepalive message2 (error, stream, header);
	ASSERT_FALSE (error);
	ASSERT_EQ (message1.peers, message2.peers);
}

TEST (message, publish_serialization)
{
	rai::publish publish (std::unique_ptr<rai::block> (new rai::state_block (4, 0, 0, 4, 2, 1, rai::keypair ().prv, 4, 5)));
	ASSERT_EQ (rai::block_type::state, publish.block->type ());
	publish.header.protocol_info.full_node_set (false);
	ASSERT_FALSE (publish.header.protocol_info.full_node_get ());
	publish.header.protocol_info.full_node_set (true);
	ASSERT_TRUE (publish.header.protocol_info.full_node_get ());
	std::vector<uint8_t> bytes;
	{
		rai::vectorstream stream (bytes);
		publish.serialize (stream);
	}
	ASSERT_TRUE (bytes.size () >= 9);
	ASSERT_EQ ('M', bytes[0]);
	ASSERT_EQ ('T', bytes[1]);
	ASSERT_EQ (rai::protocol_version, bytes[2]);
	ASSERT_EQ (rai::protocol_version_min_get (), bytes[3]);
	ASSERT_EQ (rai::protocol_version, bytes[4]);
	ASSERT_EQ (static_cast<uint8_t> (rai::message_type::publish), bytes[5]);
	ASSERT_EQ (0x0C, bytes[6]);
	ASSERT_EQ (0x00, bytes[7]);
	ASSERT_EQ (static_cast<uint8_t> (rai::block_type::state), bytes[8]);
	rai::bufferstream stream (bytes.data (), bytes.size ());
	auto error (false);
	rai::message_header header (error, stream);
	ASSERT_FALSE (error);
	ASSERT_EQ (rai::protocol_version_min_get (), header.protocol_info.version_min);
	ASSERT_EQ (rai::protocol_version, header.protocol_info.version);
	ASSERT_EQ (rai::protocol_version, header.protocol_info.version_max);
	rai::publish publish2 (error, stream, header);
	ASSERT_FALSE (error);
	ASSERT_EQ (rai::block_type::state, publish2.block->type ());
}

TEST (message, confirm_req_serialization)
{
	//rai::genesis genesis;
	rai::keypair key1;
	rai::confirm_req confirm_req (std::unique_ptr<rai::block> (new rai::state_block (rai::genesis_account, 0, 0, rai::genesis_account, rai::genesis_amount, key1.pub, key1.prv, key1.pub, 0)));
	ASSERT_EQ (rai::block_type::state, confirm_req.block->type ());
	//rai::confirm_req confirm_req (std::shared_ptr<rai::block> (dynamic_cast<rai::block*> (const_cast<rai::state_block *> (&genesis.block ()))));
	std::vector<uint8_t> bytes;
	{
		rai::vectorstream stream1 (bytes);
		confirm_req.serialize (stream1);
	}
	ASSERT_TRUE (bytes.size () >= 9);
	ASSERT_EQ ('M', bytes[0]);
	ASSERT_EQ ('T', bytes[1]);
	ASSERT_EQ (rai::protocol_version, bytes[2]);
	ASSERT_EQ (rai::protocol_version_min_get (), bytes[3]);
	ASSERT_EQ (rai::protocol_version, bytes[4]);
	ASSERT_EQ (static_cast<uint8_t> (rai::message_type::confirm_req), bytes[5]);
	ASSERT_EQ (0x0C, bytes[6]);
	ASSERT_EQ (0x00, bytes[7]);
	ASSERT_EQ (static_cast<uint8_t> (rai::block_type::state), bytes[8]);
	rai::bufferstream stream2 (bytes.data (), bytes.size ());
	bool error (false);
	rai::message_header header (error, stream2);
	ASSERT_FALSE (error);

	rai::confirm_req confirm2 (error, stream2, header);
	ASSERT_FALSE (error);
	ASSERT_EQ (*confirm_req.block.get (), *confirm2.block.get ());
	ASSERT_EQ (confirm_req, confirm2);
}

TEST (message, confirm_ack_serialization)
{
	rai::keypair key1;
	auto vote (std::make_shared<rai::vote> (key1.pub, key1.prv, 0, std::unique_ptr<rai::block> (new rai::state_block (4, 0, 0, 4, 2, 1, key1.prv, 4, 5))));
	rai::confirm_ack con1 (vote);
	std::vector<uint8_t> bytes;
	{
		rai::vectorstream stream1 (bytes);
		con1.serialize (stream1);
	}
	ASSERT_TRUE (bytes.size () >= 9);
	ASSERT_EQ ('M', bytes[0]);
	ASSERT_EQ ('T', bytes[1]);
	ASSERT_EQ (rai::protocol_version, bytes[2]);
	ASSERT_EQ (rai::protocol_version_min_get (), bytes[3]);
	ASSERT_EQ (rai::protocol_version, bytes[4]);
	ASSERT_EQ (static_cast<uint8_t> (rai::message_type::confirm_ack), bytes[5]);
	ASSERT_EQ (0x0C, bytes[6]);
	ASSERT_EQ (0x00, bytes[7]);
	ASSERT_EQ (static_cast<uint8_t> (rai::block_type::state), bytes[8]);
	rai::bufferstream stream2 (bytes.data (), bytes.size ());
	bool error (false);
	rai::message_header header (error, stream2);
	ASSERT_FALSE (error);
	uint8_t block_type_raw (0);
	error = rai::read (stream2, block_type_raw);
	ASSERT_FALSE(error);
	rai::block_type block_type = static_cast<rai::block_type> (block_type_raw);
	rai::confirm_ack con2 (error, stream2, header, block_type);
	ASSERT_FALSE (error);
	ASSERT_EQ (*con1.vote.get (), *con2.vote.get ());
	ASSERT_EQ (con1, con2);
}
