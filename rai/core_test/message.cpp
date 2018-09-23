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
	rai::publish publish (std::unique_ptr<rai::block> (new rai::send_block (0, 1, 2, rai::keypair ().prv, 4, 5)));
	ASSERT_EQ (rai::block_type::send, publish.header.block_type ());
	publish.header.protocol_info.set_full_node (false);
	ASSERT_FALSE (publish.header.protocol_info.is_full_node ());
	publish.header.protocol_info.set_full_node (true);
	ASSERT_TRUE (publish.header.protocol_info.is_full_node ());
	std::vector<uint8_t> bytes;
	{
		rai::vectorstream stream (bytes);
		publish.header.serialize (stream);
	}
	ASSERT_EQ (8, bytes.size ());
	ASSERT_EQ ('M', bytes[0]);
	ASSERT_EQ ('T', bytes[1]);
	ASSERT_EQ (rai::protocol_version, bytes[2]);
	ASSERT_EQ (rai::protocol_version, bytes[3]);
	ASSERT_EQ (rai::protocol_version_min, bytes[4]);
	ASSERT_EQ (static_cast<uint8_t> (rai::message_type::publish), bytes[5]);
	ASSERT_EQ (0x02, bytes[6]);
	ASSERT_EQ (static_cast<uint8_t> (rai::block_type::send), bytes[7]);
	rai::bufferstream stream (bytes.data (), bytes.size ());
	auto error (false);
	rai::message_header header (error, stream);
	ASSERT_FALSE (error);
	ASSERT_EQ (rai::protocol_version_min, header.protocol_info.version_min);
	ASSERT_EQ (rai::protocol_version, header.protocol_info.version);
	ASSERT_EQ (rai::protocol_version, header.protocol_info.version_max);
	ASSERT_EQ (rai::message_type::publish, header.message_type);
}

TEST (message, confirm_ack_serialization)
{
	rai::keypair key1;
	auto vote (std::make_shared<rai::vote> (key1.pub, key1.prv, 0, std::unique_ptr<rai::block> (new rai::send_block (0, 1, 2, key1.prv, 4, 5))));
	rai::confirm_ack con1 (vote);
	std::vector<uint8_t> bytes;
	{
		rai::vectorstream stream1 (bytes);
		con1.serialize (stream1);
	}
	rai::bufferstream stream2 (bytes.data (), bytes.size ());
	bool error (false);
	rai::message_header header (error, stream2);
	rai::confirm_ack con2 (error, stream2, header);
	ASSERT_FALSE (error);
	ASSERT_EQ (con1, con2);
}
