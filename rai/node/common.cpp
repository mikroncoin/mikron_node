
#include <rai/node/common.hpp>

#include <rai/lib/work.hpp>
#include <rai/node/wallet.hpp>

size_t constexpr rai::protocol_information::query_flag_position;
size_t constexpr rai::protocol_information::response_flag_position;
size_t constexpr rai::protocol_information::full_node_position;
size_t constexpr rai::protocol_information::validating_node_position;
std::array<uint8_t, 2> constexpr rai::message_header::magic_number;

rai::protocol_information::protocol_information (unsigned version_a, unsigned version_min_a, unsigned version_max_a, std::bitset<16> extensions_a) :
version (version_a),
version_min (version_min_a),
version_max (version_max_a),
extensions (extensions_a)
{
}

rai::protocol_information::protocol_information () :
version (rai::protocol_version),
version_min (rai::protocol_version_min),
version_max (rai::protocol_version)
{
}

bool rai::protocol_information::full_node_get () const
{
	return extensions.test (full_node_position);
}

void rai::protocol_information::full_node_set (bool value_a)
{
	extensions.set (full_node_position, value_a);
}

bool rai::protocol_information::validating_node_get () const
{
	return extensions.test (validating_node_position);
}

void rai::protocol_information::validating_node_set (bool value_a)
{
	extensions.set (validating_node_position, value_a);
}

rai::message_header::message_header (rai::message_type message_type_a) :
protocol_info (rai::protocol_version, rai::protocol_version_min, rai::protocol_version, std::bitset<16> ()),
message_type (message_type_a)
{
	protocol_info.full_node_set (true);
	protocol_info.validating_node_set (true);
}

rai::message_header::message_header (bool & error_a, rai::stream & stream_a)
{
	if (!error_a)
	{
		error_a = deserialize (stream_a);
	}
}

void rai::message_header::serialize (rai::stream & stream_a)
{
	rai::write (stream_a, rai::message_header::magic_number);
	rai::write (stream_a, protocol_info.version);
	rai::write (stream_a, protocol_info.version_min);
	rai::write (stream_a, protocol_info.version_max);
	rai::write (stream_a, message_type);
	rai::write (stream_a, static_cast<uint16_t> (protocol_info.extensions.to_ullong ()));
}

bool rai::message_header::deserialize (rai::stream & stream_a)
{
	uint16_t extensions_l;
	std::array<uint8_t, 2> magic_number_l;
	auto result (rai::read (stream_a, magic_number_l));
	result = result || magic_number_l != magic_number;
	result = result || rai::read (stream_a, protocol_info.version);
	result = result || rai::read (stream_a, protocol_info.version_min);
	result = result || rai::read (stream_a, protocol_info.version_max);
	result = result || rai::read (stream_a, message_type);
	result = result || rai::read (stream_a, extensions_l);
	if (!result)
	{
		protocol_info.extensions = extensions_l;
	}
	return result;
}

rai::message::message (rai::message_type message_type_a) :
header (message_type_a)
{
}

rai::message::message (rai::message_header const & header_a) :
header (header_a)
{
}

// MTU - IP header - UDP header
const size_t rai::message_parser::max_safe_udp_message_size = 508;

rai::message_parser::message_parser (rai::message_visitor & visitor_a, rai::work_pool & pool_a) :
visitor (visitor_a),
pool (pool_a),
status (parse_status::success)
{
}

void rai::message_parser::deserialize_buffer (uint8_t const * buffer_a, size_t size_a)
{
	status = parse_status::success;
	auto error (false);
	if (size_a <= max_safe_udp_message_size)
	{
		// Guaranteed to be deliverable
		rai::bufferstream stream (buffer_a, size_a);
		rai::message_header header (error, stream);
		if (!error)
		{
			if (rai::rai_network == rai::rai_networks::rai_beta_network && header.protocol_info.version < rai::protocol_version)
			{
				status = parse_status::outdated_version;
			}
			else
			{
				switch (header.message_type)
				{
					case rai::message_type::keepalive:
					{
						deserialize_keepalive (stream, header);
						break;
					}
					case rai::message_type::publish:
					{
						deserialize_publish (stream, header);
						break;
					}
					case rai::message_type::confirm_req:
					{
						deserialize_confirm_req (stream, header);
						break;
					}
					case rai::message_type::confirm_ack:
					{
						deserialize_confirm_ack (stream, header);
						break;
					}
					case rai::message_type::node_id_handshake:
					{
						deserialize_node_id_handshake (stream, header);
						break;
					}
					default:
					{
						status = parse_status::invalid_message_type;
						break;
					}
				}
			}
		}
		else
		{
			status = parse_status::invalid_header;
		}
	}
}

void rai::message_parser::deserialize_keepalive (rai::stream & stream_a, rai::message_header const & header_a)
{
	auto error (false);
	rai::keepalive incoming (error, stream_a, header_a);
	if (!error && at_end (stream_a))
	{
		visitor.keepalive (incoming);
	}
	else
	{
		status = parse_status::invalid_keepalive_message;
	}
}

void rai::message_parser::deserialize_publish (rai::stream & stream_a, rai::message_header const & header_a)
{
	auto error (false);
	rai::publish incoming (error, stream_a, header_a);
	if (!error && at_end (stream_a))
	{
		if (!rai::work_validate (*incoming.block))
		{
			visitor.publish (incoming);
		}
		else
		{
			status = parse_status::insufficient_work;
		}
	}
	else
	{
		status = parse_status::invalid_publish_message;
	}
}

void rai::message_parser::deserialize_confirm_req (rai::stream & stream_a, rai::message_header const & header_a)
{
	auto error (false);
	rai::confirm_req incoming (error, stream_a, header_a);
	if (!error && at_end (stream_a))
	{
		if (!rai::work_validate (*incoming.block))
		{
			visitor.confirm_req (incoming);
		}
		else
		{
			status = parse_status::insufficient_work;
		}
	}
	else
	{
		status = parse_status::invalid_confirm_req_message;
	}
}

void rai::message_parser::deserialize_confirm_ack (rai::stream & stream_a, rai::message_header const & header_a)
{
	auto error (false);
	rai::block_type block_type = rai::message_with_block::deserialize_block_type (stream_a);
	rai::confirm_ack incoming (error, stream_a, header_a, block_type);
	if (!error && at_end (stream_a))
	{
		for (auto & vote_block : incoming.vote->blocks)
		{
			if (!vote_block.which ())
			{
				auto block (boost::get<std::shared_ptr<rai::block>> (vote_block));
				if (rai::work_validate (*block))
				{
					status = parse_status::insufficient_work;
					break;
				}
			}
		}
		if (status == parse_status::success)
		{
			visitor.confirm_ack (incoming);
		}
	}
	else
	{
		status = parse_status::invalid_confirm_ack_message;
	}
}

void rai::message_parser::deserialize_node_id_handshake (rai::stream & stream_a, rai::message_header const & header_a)
{
	bool error_l (false);
	rai::node_id_handshake incoming (error_l, stream_a, header_a);
	if (!error_l && at_end (stream_a))
	{
		visitor.node_id_handshake (incoming);
	}
	else
	{
		status = parse_status::invalid_node_id_handshake_message;
	}
}

bool rai::message_parser::at_end (rai::stream & stream_a)
{
	uint8_t junk;
	auto end (rai::read (stream_a, junk));
	return end;
}

rai::keepalive::keepalive () :
message (rai::message_type::keepalive)
{
	rai::endpoint endpoint (boost::asio::ip::address_v6{}, 0);
	for (auto i (peers.begin ()), n (peers.end ()); i != n; ++i)
	{
		*i = endpoint;
	}
}

rai::keepalive::keepalive (bool & error_a, rai::stream & stream_a, rai::message_header const & header_a) :
message (header_a)
{
	if (!error_a)
	{
		error_a = deserialize (stream_a);
	}
}

void rai::keepalive::visit (rai::message_visitor & visitor_a) const
{
	visitor_a.keepalive (*this);
}

void rai::keepalive::serialize (rai::stream & stream_a)
{
	header.serialize (stream_a);
	for (auto i (peers.begin ()), j (peers.end ()); i != j; ++i)
	{
		assert (i->address ().is_v6 ());
		auto bytes (i->address ().to_v6 ().to_bytes ());
		write (stream_a, bytes);
		write (stream_a, i->port ());
	}
}

bool rai::keepalive::deserialize (rai::stream & stream_a)
{
	assert (header.message_type == rai::message_type::keepalive);
	auto error (false);
	for (auto i (peers.begin ()), j (peers.end ()); i != j && !error; ++i)
	{
		std::array<uint8_t, 16> address;
		uint16_t port;
		if (!read (stream_a, address) && !read (stream_a, port))
		{
			*i = rai::endpoint (boost::asio::ip::address_v6 (address), port);
		}
		else
		{
			error = true;
		}
	}
	return error;
}

bool rai::keepalive::operator== (rai::keepalive const & other_a) const
{
	return peers == other_a.peers;
}

rai::message_with_block::message_with_block (rai::message_type message_type_a) :
	message (message_type_a)
{
}

rai::message_with_block::message_with_block (rai::message_header const & header_a) :
	message (header_a)
{
}

rai::block_type rai::message_with_block::deserialize_block_type (rai::stream & stream_a)
{
	uint8_t block_type_raw (0);
	auto error (rai::read (stream_a, block_type_raw));
	if (error) return rai::block_type::invalid;
	return static_cast<rai::block_type> (block_type_raw);
}

rai::publish::publish (bool & error_a, rai::stream & stream_a, rai::message_header const & header_a) :
message_with_block (header_a)
{
	if (!error_a)
	{
		error_a = deserialize (stream_a);
	}
}

rai::publish::publish (std::shared_ptr<rai::block> block_a) :
message_with_block (rai::message_type::publish),
block (block_a)
{
}

bool rai::publish::deserialize (rai::stream & stream_a)
{
	assert (header.message_type == rai::message_type::publish);
	rai::block_type block_type_l = deserialize_block_type (stream_a);
	block = rai::deserialize_block (stream_a, block_type_l);
	auto result (block == nullptr);
	return result;
}

void rai::publish::serialize (rai::stream & stream_a)
{
	assert (block != nullptr);
	header.serialize (stream_a);
	rai::write (stream_a, static_cast<uint8_t> (block->type ()));
	block->serialize (stream_a);
}

void rai::publish::visit (rai::message_visitor & visitor_a) const
{
	visitor_a.publish (*this);
}

bool rai::publish::operator== (rai::publish const & other_a) const
{
	return *block == *other_a.block;
}

rai::confirm_req::confirm_req (bool & error_a, rai::stream & stream_a, rai::message_header const & header_a) :
message_with_block (header_a)
{
	if (!error_a)
	{
		error_a = deserialize (stream_a);
	}
}

rai::confirm_req::confirm_req (std::shared_ptr<rai::block> block_a) :
message_with_block (rai::message_type::confirm_req),
block (block_a)
{
}

bool rai::confirm_req::deserialize (rai::stream & stream_a)
{
	assert (header.message_type == rai::message_type::confirm_req);
	rai::block_type block_type_l = deserialize_block_type (stream_a);
	block = rai::deserialize_block (stream_a, block_type_l);
	auto result (block == nullptr);
	return result;
}

void rai::confirm_req::visit (rai::message_visitor & visitor_a) const
{
	visitor_a.confirm_req (*this);
}

void rai::confirm_req::serialize (rai::stream & stream_a)
{
	assert (block != nullptr);
	header.serialize (stream_a);
	rai::write (stream_a, static_cast<uint8_t> (block->type ()));
	block->serialize (stream_a);
}

bool rai::confirm_req::operator== (rai::confirm_req const & other_a) const
{
	return *block == *other_a.block;
}

rai::confirm_ack::confirm_ack (bool & error_a, rai::stream & stream_a, rai::message_header const & header_a, rai::block_type & block_type_a) :
message_with_block (header_a),
block_type (block_type_a),
vote (std::make_shared<rai::vote> (error_a, stream_a, block_type_a))
{
}

rai::confirm_ack::confirm_ack (std::shared_ptr<rai::vote> vote_a) :
message_with_block (rai::message_type::confirm_ack),
block_type (rai::block_type::invalid), // set later
vote (vote_a)
{
	auto & first_vote_block (vote_a->blocks[0]);
	if (first_vote_block.which ())
	{
		block_type = rai::block_type::not_a_block;
	}
	else
	{
		block_type = boost::get<std::shared_ptr<rai::block>> (first_vote_block)->type ();
	}
}

/*  Not used, see deserialize_config_ack ()
bool rai::confirm_ack::deserialize (rai::stream & stream_a)
{
	assert (header.message_type == rai::message_type::confirm_ack);
	// block_type is deserialized with the header
	auto result (vote->deserialize (stream_a));
	return result;
}
*/

void rai::confirm_ack::serialize (rai::stream & stream_a)
{
	assert (block_type == rai::block_type::not_a_block || block_type == rai::block_type::send || block_type == rai::block_type::receive || block_type == rai::block_type::open || block_type == rai::block_type::change || block_type == rai::block_type::state);
	header.serialize (stream_a);
	rai::write (stream_a, static_cast<uint8_t> (block_type));
	vote->serialize (stream_a, block_type);
}

bool rai::confirm_ack::operator== (rai::confirm_ack const & other_a) const
{
	auto result (*vote == *other_a.vote);
	return result;
}

void rai::confirm_ack::visit (rai::message_visitor & visitor_a) const
{
	visitor_a.confirm_ack (*this);
}

rai::frontier_req::frontier_req () :
message (rai::message_type::frontier_req)
{
}

rai::frontier_req::frontier_req (bool & error_a, rai::stream & stream_a, rai::message_header const & header_a) :
message (header_a)
{
	if (!error_a)
	{
		error_a = deserialize (stream_a);
	}
}

bool rai::frontier_req::deserialize (rai::stream & stream_a)
{
	assert (header.message_type == rai::message_type::frontier_req);
	auto result (read (stream_a, start.bytes));
	if (!result)
	{
		result = read (stream_a, age);
		if (!result)
		{
			result = read (stream_a, count);
		}
	}
	return result;
}

void rai::frontier_req::serialize (rai::stream & stream_a)
{
	header.serialize (stream_a);
	write (stream_a, start.bytes);
	write (stream_a, age);
	write (stream_a, count);
}

void rai::frontier_req::visit (rai::message_visitor & visitor_a) const
{
	visitor_a.frontier_req (*this);
}

bool rai::frontier_req::operator== (rai::frontier_req const & other_a) const
{
	return start == other_a.start && age == other_a.age && count == other_a.count;
}

rai::bulk_pull::bulk_pull () :
message (rai::message_type::bulk_pull)
{
}

rai::bulk_pull::bulk_pull (bool & error_a, rai::stream & stream_a, rai::message_header const & header_a) :
message (header_a)
{
	if (!error_a)
	{
		error_a = deserialize (stream_a);
	}
}

void rai::bulk_pull::visit (rai::message_visitor & visitor_a) const
{
	visitor_a.bulk_pull (*this);
}

bool rai::bulk_pull::deserialize (rai::stream & stream_a)
{
	assert (header.message_type == rai::message_type::bulk_pull);
	auto result (read (stream_a, start));
	if (!result)
	{
		result = read (stream_a, end);
	}
	return result;
}

void rai::bulk_pull::serialize (rai::stream & stream_a)
{
	header.serialize (stream_a);
	write (stream_a, start);
	write (stream_a, end);
}

rai::bulk_pull_account::bulk_pull_account () :
message (rai::message_type::bulk_pull_account)
{
}

rai::bulk_pull_account::bulk_pull_account (bool & error_a, rai::stream & stream_a, rai::message_header const & header_a) :
message (header_a)
{
	if (!error_a)
	{
		error_a = deserialize (stream_a);
	}
}

void rai::bulk_pull_account::visit (rai::message_visitor & visitor_a) const
{
	visitor_a.bulk_pull_account (*this);
}

bool rai::bulk_pull_account::deserialize (rai::stream & stream_a)
{
	assert (header.message_type == rai::message_type::bulk_pull_account);
	auto result (read (stream_a, account));
	if (!result)
	{
		result = read (stream_a, minimum_amount);
		if (!result)
		{
			result = read (stream_a, flags);
		}
	}
	return result;
}

void rai::bulk_pull_account::serialize (rai::stream & stream_a)
{
	header.serialize (stream_a);
	write (stream_a, account);
	write (stream_a, minimum_amount);
	write (stream_a, flags);
}

rai::bulk_pull_blocks::bulk_pull_blocks () :
message (rai::message_type::bulk_pull_blocks)
{
}

rai::bulk_pull_blocks::bulk_pull_blocks (bool & error_a, rai::stream & stream_a, rai::message_header const & header_a) :
message (header_a)
{
	if (!error_a)
	{
		error_a = deserialize (stream_a);
	}
}

void rai::bulk_pull_blocks::visit (rai::message_visitor & visitor_a) const
{
	visitor_a.bulk_pull_blocks (*this);
}

bool rai::bulk_pull_blocks::deserialize (rai::stream & stream_a)
{
	assert (header.message_type == rai::message_type::bulk_pull_blocks);
	auto result (read (stream_a, min_hash));
	if (!result)
	{
		result = read (stream_a, max_hash);
		if (!result)
		{
			result = read (stream_a, mode);
			if (!result)
			{
				result = read (stream_a, max_count);
			}
		}
	}
	return result;
}

void rai::bulk_pull_blocks::serialize (rai::stream & stream_a)
{
	header.serialize (stream_a);
	write (stream_a, min_hash);
	write (stream_a, max_hash);
	write (stream_a, mode);
	write (stream_a, max_count);
}

rai::bulk_push::bulk_push () :
message (rai::message_type::bulk_push)
{
}

rai::bulk_push::bulk_push (rai::message_header const & header_a) :
message (header_a)
{
}

bool rai::bulk_push::deserialize (rai::stream & stream_a)
{
	assert (header.message_type == rai::message_type::bulk_push);
	return false;
}

void rai::bulk_push::serialize (rai::stream & stream_a)
{
	header.serialize (stream_a);
}

void rai::bulk_push::visit (rai::message_visitor & visitor_a) const
{
	visitor_a.bulk_push (*this);
}

rai::node_id_handshake::node_id_handshake (bool & error_a, rai::stream & stream_a, rai::message_header const & header_a) :
message (header_a),
query (boost::none),
response (boost::none)
{
	error_a = deserialize (stream_a);
}

rai::node_id_handshake::node_id_handshake (boost::optional<rai::uint256_union> query, boost::optional<std::pair<rai::account, rai::signature>> response) :
message (rai::message_type::node_id_handshake),
query (query),
response (response)
{
	if (query)
	{
		header.protocol_info.extensions.set (rai::protocol_information::query_flag_position);
	}
	if (response)
	{
		header.protocol_info.extensions.set (rai::protocol_information::response_flag_position);
	}
}

bool rai::node_id_handshake::deserialize (rai::stream & stream_a)
{
	auto result (false);
	assert (header.message_type == rai::message_type::node_id_handshake);
	if (!result && header.protocol_info.extensions.test (rai::protocol_information::query_flag_position))
	{
		rai::uint256_union query_hash;
		result = read (stream_a, query_hash);
		if (!result)
		{
			query = query_hash;
		}
	}
	if (!result && header.protocol_info.extensions.test (rai::protocol_information::response_flag_position))
	{
		rai::account response_account;
		result = read (stream_a, response_account);
		if (!result)
		{
			rai::signature response_signature;
			result = read (stream_a, response_signature);
			if (!result)
			{
				response = std::make_pair (response_account, response_signature);
			}
		}
	}
	return result;
}

void rai::node_id_handshake::serialize (rai::stream & stream_a)
{
	header.serialize (stream_a);
	if (query)
	{
		write (stream_a, *query);
	}
	if (response)
	{
		write (stream_a, response->first);
		write (stream_a, response->second);
	}
}

bool rai::node_id_handshake::operator== (rai::node_id_handshake const & other_a) const
{
	auto result (*query == *other_a.query && *response == *other_a.response);
	return result;
}

void rai::node_id_handshake::visit (rai::message_visitor & visitor_a) const
{
	visitor_a.node_id_handshake (*this);
}

rai::message_visitor::~message_visitor ()
{
}
