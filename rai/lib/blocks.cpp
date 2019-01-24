#include <rai/lib/blocks.hpp>
#include <rai/secure/common.hpp>

#include <ctime>
#include <boost/endian/conversion.hpp>
//#include <boost/date_time/posix_time/posix_time.hpp>

/** Compare blocks, first by type, then content. This is an optimization over dynamic_cast, which is very slow on some platforms. */
namespace
{
template <typename T>
bool blocks_equal (T const & first, rai::block const & second)
{
	static_assert (std::is_base_of<rai::block, T>::value, "Input parameter is not a block type");
	return (first.type () == second.type ()) && (static_cast<T const &> (second)) == first;
}
}

std::string rai::to_string_hex (uint64_t value_a)
{
	std::stringstream stream;
	stream << std::hex << std::noshowbase << std::setw (16) << std::setfill ('0');
	stream << value_a;
	return stream.str ();
}

bool rai::from_string_hex (std::string const & value_a, uint64_t & target_a)
{
	auto error (value_a.empty ());
	if (!error)
	{
		error = value_a.size () > 16;
		if (!error)
		{
			std::stringstream stream (value_a);
			stream << std::hex << std::noshowbase;
			try
			{
				uint64_t number_l;
				stream >> number_l;
				target_a = number_l;
				if (!stream.eof ())
				{
					error = true;
				}
			}
			catch (std::runtime_error &)
			{
				error = true;
			}
		}
	}
	return error;
}

rai::short_timestamp::short_timestamp ()
	: data (0)
{
}

rai::short_timestamp::short_timestamp (rai::timestamp_t creation_time)
	: data (creation_time)
{
}

rai::short_timestamp::short_timestamp (const short_timestamp & time_a)
	: data (time_a.data)
{
}

bool rai::short_timestamp::operator== (rai::short_timestamp const & other_a) const
{
	return data == other_a.data;
}

rai::timestamp_t rai::short_timestamp::number () const
{
	return data.number ();
}

void rai::short_timestamp::set_time_now ()
{
	//uint64_t now = std::chrono::duration_cast<std::chrono::seconds> (std::chrono::system_clock::now ().time_since_epoch ()).count ();
	uint64_t now = (uint64_t)std::time (nullptr);
	return set_from_posix_time (now);
}

rai::timestamp_t rai::short_timestamp::now ()
{
	uint64_t now = (uint64_t)std::time (nullptr);
	return convert_from_posix_time (now);
}

rai::timestamp_t rai::short_timestamp::convert_from_posix_time (uint64_t time_posix)
{
	uint64_t current_time_since_epoch = 0;
	if (time_posix >= (uint64_t)short_timestamp_epoch)
	{
		current_time_since_epoch = time_posix - (uint64_t)short_timestamp_epoch;
	}
	return (rai::timestamp_t)current_time_since_epoch;
}

uint64_t rai::short_timestamp::convert_to_posix_time (rai::timestamp_t time)
{
	return (uint64_t)short_timestamp_epoch + (uint64_t)time;
}

void rai::short_timestamp::set_from_posix_time (uint64_t time_posix)
{
	data = convert_from_posix_time (time_posix);
}

bool rai::short_timestamp::decode_dec (std::string const & text)
{
	return data.decode_dec (text);
}

uint64_t rai::short_timestamp::to_posix_time () const
{
	return (uint64_t)short_timestamp_epoch + (uint64_t)data.number ();
}

bool rai::short_timestamp::is_zero() const
{
	return data.is_zero ();
}

void rai::short_timestamp::clear ()
{
	data.clear();
}

std::string rai::short_timestamp::to_date_string_utc () const
{
	std::time_t timet = (std::time_t)to_posix_time ();
	return std::asctime (std::gmtime (&timet));
	//boost::posix_time::ptime ptime = boost::posix_time::from_time_t (timet);
	//return boost::posix_time::to_simple_string (ptime);
}

std::string rai::short_timestamp::to_date_string_local () const
{
	std::time_t timet = (std::time_t)to_posix_time ();
	std::stringstream ss;
	ss << std::put_time (std::localtime (&timet), "%x %X"); // locale-dependent, 	08/23/01 14:55:02
	return ss.str ();
}

std::string rai::block::to_json ()
{
	std::string result;
	serialize_json (result);
	return result;
}

rai::block_hash rai::block::hash () const
{
	rai::uint256_union result;
	blake2b_state hash_l;
	auto status (blake2b_init (&hash_l, sizeof (result.bytes)));
	assert (status == 0);
	hash (hash_l);
	status = blake2b_final (&hash_l, result.bytes.data (), sizeof (result.bytes));
	assert (status == 0);
	return result;
}

void rai::send_block::visit (rai::block_visitor & visitor_a) const
{
	visitor_a.send_block (*this);
}

void rai::send_block::hash (blake2b_state & hash_a) const
{
	hashables.hash (hash_a);
}

uint64_t rai::send_block::block_work () const
{
	return work;
}

void rai::send_block::block_work_set (uint64_t work_a)
{
	work = work_a;
}

rai::send_hashables::send_hashables (rai::block_hash const & previous_a, rai::account const & destination_a, rai::amount const & balance_a) :
previous (previous_a),
destination (destination_a),
balance (balance_a)
{
}

rai::send_hashables::send_hashables (bool & error_a, rai::stream & stream_a)
{
	error_a = rai::read (stream_a, previous.bytes);
	if (!error_a)
	{
		error_a = rai::read (stream_a, destination.bytes);
		if (!error_a)
		{
			error_a = balance.deserialize(stream_a);
		}
	}
}

rai::send_hashables::send_hashables (bool & error_a, boost::property_tree::ptree const & tree_a)
{
	try
	{
		auto previous_l (tree_a.get<std::string> ("previous"));
		auto destination_l (tree_a.get<std::string> ("destination"));
		auto balance_l (tree_a.get<std::string> ("balance"));
		error_a = previous.decode_hex (previous_l);
		if (!error_a)
		{
			error_a = destination.decode_account (destination_l);
			if (!error_a)
			{
				error_a = balance.decode_hex (balance_l);
			}
		}
	}
	catch (std::runtime_error const &)
	{
		error_a = true;
	}
}

void rai::send_hashables::hash (blake2b_state & hash_a) const
{
	auto status (blake2b_update (&hash_a, previous.bytes.data (), sizeof (previous.bytes)));
	assert (status == 0);
	status = blake2b_update (&hash_a, destination.bytes.data (), sizeof (destination.bytes));
	assert (status == 0);
	uint64_t balance_big_endian (balance.number_big_endian ());
	status = blake2b_update (&hash_a, &balance_big_endian, sizeof (balance_big_endian));
	assert (status == 0);
}

void rai::send_block::serialize (rai::stream & stream_a) const
{
	write (stream_a, hashables.previous.bytes);
	write (stream_a, hashables.destination.bytes);
	hashables.balance.serialize (stream_a);
	write (stream_a, signature.bytes);
	write (stream_a, work);
}

void rai::send_block::serialize_json (std::string & string_a) const
{
	boost::property_tree::ptree tree;
	tree.put ("type", "send");
	std::string previous;
	hashables.previous.encode_hex (previous);
	tree.put ("previous", previous);
	tree.put ("destination", hashables.destination.to_account ());
	std::string balance;
	hashables.balance.encode_hex (balance);
	tree.put ("balance", balance);
	std::string signature_l;
	signature.encode_hex (signature_l);
	tree.put ("work", rai::to_string_hex (work));
	tree.put ("signature", signature_l);
	std::stringstream ostream;
	boost::property_tree::write_json (ostream, tree);
	string_a = ostream.str ();
}

bool rai::send_block::deserialize (rai::stream & stream_a)
{
	auto error (false);
	error = read (stream_a, hashables.previous.bytes);
	if (!error)
	{
		error = read (stream_a, hashables.destination.bytes);
		if (!error)
		{
			error = hashables.balance.deserialize (stream_a);
			if (!error)
			{
				error = read (stream_a, signature.bytes);
				if (!error)
				{
					error = read (stream_a, work);
				}
			}
		}
	}
	return error;
}

bool rai::send_block::deserialize_json (boost::property_tree::ptree const & tree_a)
{
	auto error (false);
	try
	{
		assert (tree_a.get<std::string> ("type") == "send");
		auto previous_l (tree_a.get<std::string> ("previous"));
		auto destination_l (tree_a.get<std::string> ("destination"));
		auto balance_l (tree_a.get<std::string> ("balance"));
		auto work_l (tree_a.get<std::string> ("work"));
		auto signature_l (tree_a.get<std::string> ("signature"));
		error = hashables.previous.decode_hex (previous_l);
		if (!error)
		{
			error = hashables.destination.decode_account (destination_l);
			if (!error)
			{
				error = hashables.balance.decode_hex (balance_l);
				if (!error)
				{
					error = rai::from_string_hex (work_l, work);
					if (!error)
					{
						error = signature.decode_hex (signature_l);
					}
				}
			}
		}
	}
	catch (std::runtime_error const &)
	{
		error = true;
	}
	return error;
}

rai::send_block::send_block (rai::block_hash const & previous_a, rai::account const & destination_a, rai::amount const & balance_a, rai::raw_key const & prv_a, rai::public_key const & pub_a, uint64_t work_a) :
hashables (previous_a, destination_a, balance_a),
signature (rai::sign_message (prv_a, pub_a, hash ())),
work (work_a)
{
}

rai::send_block::send_block (bool & error_a, rai::stream & stream_a) :
hashables (error_a, stream_a)
{
	if (!error_a)
	{
		error_a = rai::read (stream_a, signature.bytes);
		if (!error_a)
		{
			error_a = rai::read (stream_a, work);
		}
	}
}

rai::send_block::send_block (bool & error_a, boost::property_tree::ptree const & tree_a) :
hashables (error_a, tree_a)
{
	if (!error_a)
	{
		try
		{
			auto signature_l (tree_a.get<std::string> ("signature"));
			auto work_l (tree_a.get<std::string> ("work"));
			error_a = signature.decode_hex (signature_l);
			if (!error_a)
			{
				error_a = rai::from_string_hex (work_l, work);
			}
		}
		catch (std::runtime_error const &)
		{
			error_a = true;
		}
	}
}

bool rai::send_block::operator== (rai::block const & other_a) const
{
	return blocks_equal (*this, other_a);
}

bool rai::send_block::valid_predecessor (rai::block const & block_a) const
{
	bool result;
	switch (block_a.type ())
	{
		case rai::block_type::send:
		case rai::block_type::receive:
		case rai::block_type::open:
			//case rai::block_type::change:
			result = true;
			break;
		default:
			result = false;
			break;
	}
	return result;
}

rai::block_type rai::send_block::type () const
{
	return rai::block_type::send;
}

bool rai::send_block::operator== (rai::send_block const & other_a) const
{
	auto result (hashables.destination == other_a.hashables.destination && hashables.previous == other_a.hashables.previous && hashables.balance == other_a.hashables.balance && work == other_a.work && signature == other_a.signature);
	return result;
}

rai::block_hash rai::send_block::previous () const
{
	return hashables.previous;
}

rai::block_hash rai::send_block::source () const
{
	return 0;
}

rai::block_hash rai::send_block::root () const
{
	return hashables.previous;
}

rai::account rai::send_block::representative () const
{
	return 0;
}

rai::signature rai::send_block::block_signature () const
{
	return signature;
}

void rai::send_block::signature_set (rai::uint512_union const & signature_a)
{
	signature = signature_a;
}

rai::open_hashables::open_hashables (rai::block_hash const & source_a, rai::account const & representative_a, rai::account const & account_a) :
source (source_a),
representative (representative_a),
account (account_a)
{
}

rai::open_hashables::open_hashables (bool & error_a, rai::stream & stream_a)
{
	error_a = rai::read (stream_a, source.bytes);
	if (!error_a)
	{
		error_a = rai::read (stream_a, representative.bytes);
		if (!error_a)
		{
			error_a = rai::read (stream_a, account.bytes);
		}
	}
}

rai::open_hashables::open_hashables (bool & error_a, boost::property_tree::ptree const & tree_a)
{
	try
	{
		auto source_l (tree_a.get<std::string> ("source"));
		auto representative_l (tree_a.get<std::string> ("representative"));
		auto account_l (tree_a.get<std::string> ("account"));
		error_a = source.decode_hex (source_l);
		if (!error_a)
		{
			error_a = representative.decode_account (representative_l);
			if (!error_a)
			{
				error_a = account.decode_account (account_l);
			}
		}
	}
	catch (std::runtime_error const &)
	{
		error_a = true;
	}
}

void rai::open_hashables::hash (blake2b_state & hash_a) const
{
	blake2b_update (&hash_a, source.bytes.data (), sizeof (source.bytes));
	blake2b_update (&hash_a, representative.bytes.data (), sizeof (representative.bytes));
	blake2b_update (&hash_a, account.bytes.data (), sizeof (account.bytes));
}

rai::open_block::open_block (rai::block_hash const & source_a, rai::account const & representative_a, rai::account const & account_a, rai::raw_key const & prv_a, rai::public_key const & pub_a, uint64_t work_a) :
hashables (source_a, representative_a, account_a),
signature (rai::sign_message (prv_a, pub_a, hash ())),
work (work_a)
{
	assert (!representative_a.is_zero ());
}

rai::open_block::open_block (rai::block_hash const & source_a, rai::account const & representative_a, rai::account const & account_a, std::nullptr_t) :
hashables (source_a, representative_a, account_a),
work (0)
{
	signature.clear ();
}

rai::open_block::open_block (bool & error_a, rai::stream & stream_a) :
hashables (error_a, stream_a)
{
	if (!error_a)
	{
		error_a = rai::read (stream_a, signature);
		if (!error_a)
		{
			error_a = rai::read (stream_a, work);
		}
	}
}

rai::open_block::open_block (bool & error_a, boost::property_tree::ptree const & tree_a) :
hashables (error_a, tree_a)
{
	if (!error_a)
	{
		try
		{
			auto work_l (tree_a.get<std::string> ("work"));
			auto signature_l (tree_a.get<std::string> ("signature"));
			error_a = rai::from_string_hex (work_l, work);
			if (!error_a)
			{
				error_a = signature.decode_hex (signature_l);
			}
		}
		catch (std::runtime_error const &)
		{
			error_a = true;
		}
	}
}

void rai::open_block::hash (blake2b_state & hash_a) const
{
	hashables.hash (hash_a);
}

uint64_t rai::open_block::block_work () const
{
	return work;
}

void rai::open_block::block_work_set (uint64_t work_a)
{
	work = work_a;
}

rai::block_hash rai::open_block::previous () const
{
	rai::block_hash result (0);
	return result;
}

void rai::open_block::serialize (rai::stream & stream_a) const
{
	write (stream_a, hashables.source);
	write (stream_a, hashables.representative);
	write (stream_a, hashables.account);
	write (stream_a, signature);
	write (stream_a, work);
}

void rai::open_block::serialize_json (std::string & string_a) const
{
	boost::property_tree::ptree tree;
	tree.put ("type", "open");
	tree.put ("source", hashables.source.to_string ());
	tree.put ("representative", representative ().to_account ());
	tree.put ("account", hashables.account.to_account ());
	std::string signature_l;
	signature.encode_hex (signature_l);
	tree.put ("work", rai::to_string_hex (work));
	tree.put ("signature", signature_l);
	std::stringstream ostream;
	boost::property_tree::write_json (ostream, tree);
	string_a = ostream.str ();
}

bool rai::open_block::deserialize (rai::stream & stream_a)
{
	auto error (read (stream_a, hashables.source));
	if (!error)
	{
		error = read (stream_a, hashables.representative);
		if (!error)
		{
			error = read (stream_a, hashables.account);
			if (!error)
			{
				error = read (stream_a, signature);
				if (!error)
				{
					error = read (stream_a, work);
				}
			}
		}
	}
	return error;
}

bool rai::open_block::deserialize_json (boost::property_tree::ptree const & tree_a)
{
	auto error (false);
	try
	{
		assert (tree_a.get<std::string> ("type") == "open");
		auto source_l (tree_a.get<std::string> ("source"));
		auto representative_l (tree_a.get<std::string> ("representative"));
		auto account_l (tree_a.get<std::string> ("account"));
		auto work_l (tree_a.get<std::string> ("work"));
		auto signature_l (tree_a.get<std::string> ("signature"));
		error = hashables.source.decode_hex (source_l);
		if (!error)
		{
			error = hashables.representative.decode_hex (representative_l);
			if (!error)
			{
				error = hashables.account.decode_hex (account_l);
				if (!error)
				{
					error = rai::from_string_hex (work_l, work);
					if (!error)
					{
						error = signature.decode_hex (signature_l);
					}
				}
			}
		}
	}
	catch (std::runtime_error const &)
	{
		error = true;
	}
	return error;
}

void rai::open_block::visit (rai::block_visitor & visitor_a) const
{
	visitor_a.open_block (*this);
}

rai::block_type rai::open_block::type () const
{
	return rai::block_type::open;
}

bool rai::open_block::operator== (rai::block const & other_a) const
{
	return blocks_equal (*this, other_a);
}

bool rai::open_block::operator== (rai::open_block const & other_a) const
{
	return hashables.source == other_a.hashables.source && hashables.representative == other_a.hashables.representative && hashables.account == other_a.hashables.account && work == other_a.work && signature == other_a.signature;
}

bool rai::open_block::valid_predecessor (rai::block const & block_a) const
{
	return false;
}

rai::block_hash rai::open_block::source () const
{
	return hashables.source;
}

rai::block_hash rai::open_block::root () const
{
	return hashables.account;
}

rai::account rai::open_block::representative () const
{
	return hashables.representative;
}

rai::signature rai::open_block::block_signature () const
{
	return signature;
}

void rai::open_block::signature_set (rai::uint512_union const & signature_a)
{
	signature = signature_a;
}

/*
rai::change_hashables::change_hashables (rai::block_hash const & previous_a, rai::account const & representative_a) :
previous (previous_a),
representative (representative_a)
{
}

rai::change_hashables::change_hashables (bool & error_a, rai::stream & stream_a)
{
	error_a = rai::read (stream_a, previous);
	if (!error_a)
	{
		error_a = rai::read (stream_a, representative);
	}
}

rai::change_hashables::change_hashables (bool & error_a, boost::property_tree::ptree const & tree_a)
{
	try
	{
		auto previous_l (tree_a.get<std::string> ("previous"));
		auto representative_l (tree_a.get<std::string> ("representative"));
		error_a = previous.decode_hex (previous_l);
		if (!error_a)
		{
			error_a = representative.decode_account (representative_l);
		}
	}
	catch (std::runtime_error const &)
	{
		error_a = true;
	}
}

void rai::change_hashables::hash (blake2b_state & hash_a) const
{
	blake2b_update (&hash_a, previous.bytes.data (), sizeof (previous.bytes));
	blake2b_update (&hash_a, representative.bytes.data (), sizeof (representative.bytes));
}

rai::change_block::change_block (rai::block_hash const & previous_a, rai::account const & representative_a, rai::raw_key const & prv_a, rai::public_key const & pub_a, uint64_t work_a) :
hashables (previous_a, representative_a),
signature (rai::sign_message (prv_a, pub_a, hash ())),
work (work_a)
{
}

rai::change_block::change_block (bool & error_a, rai::stream & stream_a) :
hashables (error_a, stream_a)
{
	if (!error_a)
	{
		error_a = rai::read (stream_a, signature);
		if (!error_a)
		{
			error_a = rai::read (stream_a, work);
		}
	}
}

rai::change_block::change_block (bool & error_a, boost::property_tree::ptree const & tree_a) :
hashables (error_a, tree_a)
{
	if (!error_a)
	{
		try
		{
			auto work_l (tree_a.get<std::string> ("work"));
			auto signature_l (tree_a.get<std::string> ("signature"));
			error_a = rai::from_string_hex (work_l, work);
			if (!error_a)
			{
				error_a = signature.decode_hex (signature_l);
			}
		}
		catch (std::runtime_error const &)
		{
			error_a = true;
		}
	}
}

void rai::change_block::hash (blake2b_state & hash_a) const
{
	hashables.hash (hash_a);
}

uint64_t rai::change_block::block_work () const
{
	return work;
}

void rai::change_block::block_work_set (uint64_t work_a)
{
	work = work_a;
}

rai::block_hash rai::change_block::previous () const
{
	return hashables.previous;
}

void rai::change_block::serialize (rai::stream & stream_a) const
{
	write (stream_a, hashables.previous);
	write (stream_a, hashables.representative);
	write (stream_a, signature);
	write (stream_a, work);
}

void rai::change_block::serialize_json (std::string & string_a) const
{
	boost::property_tree::ptree tree;
	tree.put ("type", "change");
	tree.put ("previous", hashables.previous.to_string ());
	tree.put ("representative", representative ().to_account ());
	tree.put ("work", rai::to_string_hex (work));
	std::string signature_l;
	signature.encode_hex (signature_l);
	tree.put ("signature", signature_l);
	std::stringstream ostream;
	boost::property_tree::write_json (ostream, tree);
	string_a = ostream.str ();
}

bool rai::change_block::deserialize (rai::stream & stream_a)
{
	auto error (read (stream_a, hashables.previous));
	if (!error)
	{
		error = read (stream_a, hashables.representative);
		if (!error)
		{
			error = read (stream_a, signature);
			if (!error)
			{
				error = read (stream_a, work);
			}
		}
	}
	return error;
}

bool rai::change_block::deserialize_json (boost::property_tree::ptree const & tree_a)
{
	auto error (false);
	try
	{
		assert (tree_a.get<std::string> ("type") == "change");
		auto previous_l (tree_a.get<std::string> ("previous"));
		auto representative_l (tree_a.get<std::string> ("representative"));
		auto work_l (tree_a.get<std::string> ("work"));
		auto signature_l (tree_a.get<std::string> ("signature"));
		error = hashables.previous.decode_hex (previous_l);
		if (!error)
		{
			error = hashables.representative.decode_hex (representative_l);
			if (!error)
			{
				error = rai::from_string_hex (work_l, work);
				if (!error)
				{
					error = signature.decode_hex (signature_l);
				}
			}
		}
	}
	catch (std::runtime_error const &)
	{
		error = true;
	}
	return error;
}

void rai::change_block::visit (rai::block_visitor & visitor_a) const
{
	visitor_a.change_block (*this);
}

rai::block_type rai::change_block::type () const
{
	return rai::block_type::change;
}

bool rai::change_block::operator== (rai::block const & other_a) const
{
	return blocks_equal (*this, other_a);
}

bool rai::change_block::operator== (rai::change_block const & other_a) const
{
	return hashables.previous == other_a.hashables.previous && hashables.representative == other_a.hashables.representative && work == other_a.work && signature == other_a.signature;
}

bool rai::change_block::valid_predecessor (rai::block const & block_a) const
{
	bool result;
	switch (block_a.type ())
	{
		case rai::block_type::send:
		case rai::block_type::receive:
		case rai::block_type::open:
		case rai::block_type::change:
			result = true;
			break;
		default:
			result = false;
			break;
	}
	return result;
}

rai::block_hash rai::change_block::source () const
{
	return 0;
}

rai::block_hash rai::change_block::root () const
{
	return hashables.previous;
}

rai::account rai::change_block::representative () const
{
	return hashables.representative;
}

rai::signature rai::change_block::block_signature () const
{
	return signature;
}

void rai::change_block::signature_set (rai::uint512_union const & signature_a)
{
	signature = signature_a;
}
*/

// if time is 0, current time is taken
rai::state_hashables::state_hashables (rai::account const & account_a, rai::block_hash const & previous_a, rai::timestamp_t creation_time_a, rai::account const & representative_a, rai::amount const & balance_a, rai::uint256_union const & link_a) :
account (account_a),
creation_time (creation_time_a),
previous (previous_a),
representative (representative_a),
balance (balance_a),
link (link_a)
{
	if (creation_time_a == 0)
	{
		creation_time.set_time_now ();
	}
}

rai::state_hashables::state_hashables (bool & error_a, rai::stream & stream_a)
{
	error_a = rai::read (stream_a, account);
	if (error_a) return;
	error_a = creation_time.data.deserialize (stream_a);
	if (error_a) return;
	error_a = rai::read (stream_a, previous);
	if (error_a) return;
	error_a = rai::read (stream_a, representative);
	if (error_a) return;
	error_a = balance.deserialize (stream_a);
	if (error_a) return;
	error_a = rai::read (stream_a, link);
}

rai::state_hashables::state_hashables (bool & error_a, boost::property_tree::ptree const & tree_a)
{
	try
	{
		auto account_l (tree_a.get<std::string> ("account"));
		auto creation_time_l (tree_a.get<std::string> ("creation_time"));
		auto previous_l (tree_a.get<std::string> ("previous"));
		auto representative_l (tree_a.get<std::string> ("representative"));
		auto balance_l (tree_a.get<std::string> ("balance"));
		auto link_l (tree_a.get<std::string> ("link"));
		error_a = account.decode_account (account_l);
		if (error_a) return;
		error_a = creation_time.data.decode_dec (creation_time_l);
		if (error_a) return;
		error_a = previous.decode_hex (previous_l);
		if (error_a) return;
		error_a = representative.decode_account (representative_l);
		if (error_a) return;
		error_a = balance.decode_dec (balance_l);
		if (error_a) return;
		error_a = link.decode_account (link_l) && link.decode_hex (link_l);
	}
	catch (std::runtime_error const &)
	{
		error_a = true;
	}
}

void rai::state_hashables::hash (blake2b_state & hash_a) const
{
	blake2b_update (&hash_a, account.bytes.data (), sizeof (account.bytes));
	uint32_t creation_time_big_endian (creation_time.data.number_big_endian ());
	blake2b_update (&hash_a, &creation_time_big_endian, sizeof (creation_time_big_endian));
	blake2b_update (&hash_a, previous.bytes.data (), sizeof (previous.bytes));
	blake2b_update (&hash_a, representative.bytes.data (), sizeof (representative.bytes));
	uint64_t balance_big_endian (balance.number_big_endian ());
	blake2b_update (&hash_a, &balance_big_endian, sizeof (balance_big_endian));
	blake2b_update (&hash_a, link.bytes.data (), sizeof (link.bytes));
}

// if time is 0, current time is taken
rai::state_block::state_block (rai::account const & account_a, rai::block_hash const & previous_a, rai::timestamp_t creation_time_a, rai::account const & representative_a, rai::amount const & balance_a, rai::uint256_union const & link_a, rai::raw_key const & prv_a, rai::public_key const & pub_a, uint64_t work_a) :
hashables (account_a, previous_a, creation_time_a, representative_a, balance_a, link_a),
signature (rai::sign_message (prv_a, pub_a, hash ())),
work (work_a)
{
}

rai::state_block::state_block (bool & error_a, rai::stream & stream_a) :
hashables (error_a, stream_a)
{
	if (!error_a)
	{
		error_a = rai::read (stream_a, signature);
		if (!error_a)
		{
			error_a = work.deserialize (stream_a);
		}
	}
}

rai::state_block::state_block (bool & error_a, boost::property_tree::ptree const & tree_a) :
hashables (error_a, tree_a)
{
	if (!error_a)
	{
		try
		{
			auto type_l (tree_a.get<std::string> ("type"));
			auto signature_l (tree_a.get<std::string> ("signature"));
			auto work_l (tree_a.get<std::string> ("work"));
			error_a = type_l != "state";
			if (!error_a)
			{
				error_a = work.decode_hex (work_l);
				if (!error_a)
				{
					error_a = signature.decode_hex (signature_l);
				}
			}
		}
		catch (std::runtime_error const &)
		{
			error_a = true;
		}
	}
}

void rai::state_block::hash (blake2b_state & hash_a) const
{
	rai::uint256_union preamble (static_cast<uint64_t> (rai::block_type::state));
	blake2b_update (&hash_a, preamble.bytes.data (), preamble.bytes.size ());
	hashables.hash (hash_a);
}

uint64_t rai::state_block::block_work () const
{
	return work.number ();
}

void rai::state_block::block_work_set (uint64_t work_a)
{
	work = work_a;
}

rai::short_timestamp rai::state_block::creation_time () const
{
	return hashables.creation_time;
}

rai::block_hash rai::state_block::previous () const
{
	return hashables.previous;
}

void rai::state_block::serialize (rai::stream & stream_a) const
{
	write (stream_a, hashables.account);
	hashables.creation_time.data.serialize (stream_a);
	write (stream_a, hashables.previous);
	write (stream_a, hashables.representative);
	hashables.balance.serialize (stream_a);
	write (stream_a, hashables.link);
	write (stream_a, signature);
	work.serialize (stream_a);
}

void rai::state_block::serialize_json (std::string & string_a) const
{
	boost::property_tree::ptree tree;
	tree.put ("type", "state");
	tree.put ("account", hashables.account.to_account ());
	tree.put ("creation_time", hashables.creation_time.data.to_string_dec ());
	tree.put ("creation_time_as_date", hashables.creation_time.to_date_string_utc ());
	tree.put ("previous", hashables.previous.to_string ());
	tree.put ("representative", representative ().to_account ());
	tree.put ("balance", hashables.balance.to_string_dec ());
	tree.put ("link", hashables.link.to_string ());
	tree.put ("link_as_account", hashables.link.to_account ());
	std::string signature_l;
	signature.encode_hex (signature_l);
	tree.put ("signature", signature_l);
	tree.put ("work", work.to_string ());
	std::stringstream ostream;
	boost::property_tree::write_json (ostream, tree);
	string_a = ostream.str ();
}

bool rai::state_block::deserialize (rai::stream & stream_a)
{
	auto error (read (stream_a, hashables.account));
	if (error) return error;
	error = hashables.creation_time.data.deserialize (stream_a);
	if (error) return error;
	error = read (stream_a, hashables.previous);
	if (error) return error;
	error = read (stream_a, hashables.representative);
	if (error) return error;
	error = hashables.balance.deserialize (stream_a);
	if (error) return error;
	error = read (stream_a, hashables.link);
	if (error) return error;
	error = read (stream_a, signature);
	if (error) return error;
	error = work.deserialize (stream_a);
	return error;
}

bool rai::state_block::deserialize_json (boost::property_tree::ptree const & tree_a)
{
	auto error (false);
	try
	{
		assert (tree_a.get<std::string> ("type") == "state");
		auto account_l (tree_a.get<std::string> ("account"));
		auto creation_time_l (tree_a.get<std::string> ("creation_time"));
		auto previous_l (tree_a.get<std::string> ("previous"));
		auto representative_l (tree_a.get<std::string> ("representative"));
		auto balance_l (tree_a.get<std::string> ("balance"));
		auto link_l (tree_a.get<std::string> ("link"));
		auto work_l (tree_a.get<std::string> ("work"));
		auto signature_l (tree_a.get<std::string> ("signature"));
		error = hashables.account.decode_account (account_l);
		if (error) return error;
		error = hashables.creation_time.data.decode_dec (creation_time_l);
		if (error) return error;
		error = hashables.previous.decode_hex (previous_l);
		if (error) return error;
		error = hashables.representative.decode_account (representative_l);
		if (error) return error;
		error = hashables.balance.decode_dec (balance_l);
		if (error) return error;
		error = hashables.link.decode_account (link_l) && hashables.link.decode_hex (link_l);
		if (error) return error;
		error = work.decode_hex (work_l);
		if (error) return error;
		error = signature.decode_hex (signature_l);
	}
	catch (std::runtime_error const &)
	{
		error = true;
	}
	return error;
}

void rai::state_block::visit (rai::block_visitor & visitor_a) const
{
	visitor_a.state_block (*this);
}

rai::block_type rai::state_block::type () const
{
	return rai::block_type::state;
}

bool rai::state_block::operator== (rai::block const & other_a) const
{
	return blocks_equal (*this, other_a);
}

bool rai::state_block::operator== (rai::state_block const & other_a) const
{
	return hashables.account == other_a.hashables.account && hashables.previous == other_a.hashables.previous && hashables.representative == other_a.hashables.representative && hashables.balance == other_a.hashables.balance && hashables.link == other_a.hashables.link && signature == other_a.signature && work == other_a.work;
}

bool rai::state_block::valid_predecessor (rai::block const & block_a) const
{
	return true;
}

rai::block_hash rai::state_block::source () const
{
	return 0;
}

rai::block_hash rai::state_block::root () const
{
	return !hashables.previous.is_zero () ? hashables.previous : hashables.account;
}

rai::account rai::state_block::representative () const
{
	return hashables.representative;
}

rai::signature rai::state_block::block_signature () const
{
	return signature;
}

void rai::state_block::signature_set (rai::uint512_union const & signature_a)
{
	signature = signature_a;
}

rai::state_block_subtype rai::state_block::get_subtype (rai::amount_t previous_balance_a, rai::timestamp_t previous_block_time) const
{
	// if there is no previous: open
	if (!has_previous ())
	{
		if (has_link ())
		{
			// no prev but link: open_receive
			return rai::state_block_subtype::open_receive;
		}
		else
		{
			// no prev, no link: open_genesis
			return rai::state_block_subtype::open_genesis;
		}
	}
	// has previous, has previous balance
	// check balances: if decreasing: send
	auto cur_balance (hashables.balance.number ());
	if (rai::manna_control::is_manna_account (hashables.account))
	{
		// manna adjustment.  Note that here we want to reverse-adjust, that's why the times are in reverse order
		cur_balance = rai::manna_control::adjust_balance_with_manna (cur_balance, creation_time ().number (), previous_block_time);
	}
	if (cur_balance < previous_balance_a)
	{
		return rai::state_block_subtype::send;
	}
	// if balance increasing: receive
	if (cur_balance > previous_balance_a)
	{
		return rai::state_block_subtype::receive;
	}
	// balance does not change, and no link: change
	if (!has_link ())
	{
		return rai::state_block_subtype::change;
	}
	// if (balance == previous_balance && !handler.node.ledger.epoch_link.is_zero() && block_a.hashables.link == handler.node.ledger.epoch_link)
	// epoch
	// otherwise: undefined, which is not a valid block
	return rai::state_block_subtype::undefined;
}

bool rai::state_block::is_valid_open_subtype () const
{
	if (hashables.account.is_zero ()) return false;
	if (has_previous ()) return false;
	if (hashables.account != rai::genesis_account)
	{
		// normal accounts have link (to a send)
		if (!has_link ()) return false;
	}
	else
	{
		// genesis block has no link
		if (has_link ()) return false;
	}
	return true;
}

bool rai::state_block::is_valid_send_or_receive_subtype () const
{
	// balance change is not known
	if (hashables.account.is_zero ()) return false;
	if (!has_previous ()) return false;
	if (!has_link ()) return false;
	return true;
}

bool rai::state_block::is_valid_change_subtype () const
{
	if (hashables.account.is_zero ()) return false;
	if (!has_representative ()) return false;
	if (!has_previous ()) return false;
	if (has_link ()) return false;
	return true;
}

bool rai::state_block::has_previous () const
{
	return !hashables.previous.is_zero ();
}

bool rai::state_block::has_link () const
{
	return !hashables.link.is_zero ();
}

bool rai::state_block::has_representative () const
{
	return !hashables.representative.is_zero ();
}

std::unique_ptr<rai::block> rai::deserialize_block_json (boost::property_tree::ptree const & tree_a)
{
	std::unique_ptr<rai::block> result;
	try
	{
		auto type (tree_a.get<std::string> ("type"));
		if (type == "receive")
		{
			bool error (false);
			std::unique_ptr<rai::receive_block> obj (new rai::receive_block (error, tree_a));
			if (!error)
			{
				result = std::move (obj);
			}
		}
		else if (type == "send")
		{
			bool error (false);
			std::unique_ptr<rai::send_block> obj (new rai::send_block (error, tree_a));
			if (!error)
			{
				result = std::move (obj);
			}
		}
		else if (type == "open")
		{
			bool error (false);
			std::unique_ptr<rai::open_block> obj (new rai::open_block (error, tree_a));
			if (!error)
			{
				result = std::move (obj);
			}
		}
		else if (type == "state")
		{
			bool error (false);
			std::unique_ptr<rai::state_block> obj (new rai::state_block (error, tree_a));
			if (!error)
			{
				result = std::move (obj);
			}
		}
	}
	catch (std::runtime_error const &)
	{
	}
	return result;
}

std::unique_ptr<rai::block> rai::deserialize_block (rai::stream & stream_a)
{
	rai::block_type type;
	auto error (read (stream_a, type));
	std::unique_ptr<rai::block> result;
	if (!error)
	{
		result = rai::deserialize_block (stream_a, type);
	}
	return result;
}

std::unique_ptr<rai::block> rai::deserialize_block (rai::stream & stream_a, rai::block_type type_a)
{
	std::unique_ptr<rai::block> result;
	switch (type_a)
	{
		case rai::block_type::receive:
		{
			bool error (false);
			std::unique_ptr<rai::receive_block> obj (new rai::receive_block (error, stream_a));
			if (!error)
			{
				result = std::move (obj);
			}
			break;
		}
		case rai::block_type::send:
		{
			bool error (false);
			std::unique_ptr<rai::send_block> obj (new rai::send_block (error, stream_a));
			if (!error)
			{
				result = std::move (obj);
			}
			break;
		}
		case rai::block_type::open:
		{
			bool error (false);
			std::unique_ptr<rai::open_block> obj (new rai::open_block (error, stream_a));
			if (!error)
			{
				result = std::move (obj);
			}
			break;
		}
		case rai::block_type::state:
		{
			bool error (false);
			std::unique_ptr<rai::state_block> obj (new rai::state_block (error, stream_a));
			if (!error)
			{
				result = std::move (obj);
			}
			break;
		}
		default:
			assert (false);
			break;
	}
	return result;
}

void rai::receive_block::visit (rai::block_visitor & visitor_a) const
{
	visitor_a.receive_block (*this);
}

bool rai::receive_block::operator== (rai::receive_block const & other_a) const
{
	auto result (hashables.previous == other_a.hashables.previous && hashables.source == other_a.hashables.source && work == other_a.work && signature == other_a.signature);
	return result;
}

bool rai::receive_block::deserialize (rai::stream & stream_a)
{
	auto error (false);
	error = read (stream_a, hashables.previous.bytes);
	if (!error)
	{
		error = read (stream_a, hashables.source.bytes);
		if (!error)
		{
			error = read (stream_a, signature.bytes);
			if (!error)
			{
				error = read (stream_a, work);
			}
		}
	}
	return error;
}

bool rai::receive_block::deserialize_json (boost::property_tree::ptree const & tree_a)
{
	auto error (false);
	try
	{
		assert (tree_a.get<std::string> ("type") == "receive");
		auto previous_l (tree_a.get<std::string> ("previous"));
		auto source_l (tree_a.get<std::string> ("source"));
		auto work_l (tree_a.get<std::string> ("work"));
		auto signature_l (tree_a.get<std::string> ("signature"));
		error = hashables.previous.decode_hex (previous_l);
		if (!error)
		{
			error = hashables.source.decode_hex (source_l);
			if (!error)
			{
				error = rai::from_string_hex (work_l, work);
				if (!error)
				{
					error = signature.decode_hex (signature_l);
				}
			}
		}
	}
	catch (std::runtime_error const &)
	{
		error = true;
	}
	return error;
}

void rai::receive_block::serialize (rai::stream & stream_a) const
{
	write (stream_a, hashables.previous.bytes);
	write (stream_a, hashables.source.bytes);
	write (stream_a, signature.bytes);
	write (stream_a, work);
}

void rai::receive_block::serialize_json (std::string & string_a) const
{
	boost::property_tree::ptree tree;
	tree.put ("type", "receive");
	std::string previous;
	hashables.previous.encode_hex (previous);
	tree.put ("previous", previous);
	std::string source;
	hashables.source.encode_hex (source);
	tree.put ("source", source);
	std::string signature_l;
	signature.encode_hex (signature_l);
	tree.put ("work", rai::to_string_hex (work));
	tree.put ("signature", signature_l);
	std::stringstream ostream;
	boost::property_tree::write_json (ostream, tree);
	string_a = ostream.str ();
}

rai::receive_block::receive_block (rai::block_hash const & previous_a, rai::block_hash const & source_a, rai::raw_key const & prv_a, rai::public_key const & pub_a, uint64_t work_a) :
hashables (previous_a, source_a),
signature (rai::sign_message (prv_a, pub_a, hash ())),
work (work_a)
{
}

rai::receive_block::receive_block (bool & error_a, rai::stream & stream_a) :
hashables (error_a, stream_a)
{
	if (!error_a)
	{
		error_a = rai::read (stream_a, signature);
		if (!error_a)
		{
			error_a = rai::read (stream_a, work);
		}
	}
}

rai::receive_block::receive_block (bool & error_a, boost::property_tree::ptree const & tree_a) :
hashables (error_a, tree_a)
{
	if (!error_a)
	{
		try
		{
			auto signature_l (tree_a.get<std::string> ("signature"));
			auto work_l (tree_a.get<std::string> ("work"));
			error_a = signature.decode_hex (signature_l);
			if (!error_a)
			{
				error_a = rai::from_string_hex (work_l, work);
			}
		}
		catch (std::runtime_error const &)
		{
			error_a = true;
		}
	}
}

void rai::receive_block::hash (blake2b_state & hash_a) const
{
	hashables.hash (hash_a);
}

uint64_t rai::receive_block::block_work () const
{
	return work;
}

void rai::receive_block::block_work_set (uint64_t work_a)
{
	work = work_a;
}

bool rai::receive_block::operator== (rai::block const & other_a) const
{
	return blocks_equal (*this, other_a);
}

bool rai::receive_block::valid_predecessor (rai::block const & block_a) const
{
	bool result;
	switch (block_a.type ())
	{
		case rai::block_type::send:
		case rai::block_type::receive:
		case rai::block_type::open:
			//case rai::block_type::change:
			result = true;
			break;
		default:
			result = false;
			break;
	}
	return result;
}

rai::block_hash rai::receive_block::previous () const
{
	return hashables.previous;
}

rai::block_hash rai::receive_block::source () const
{
	return hashables.source;
}

rai::block_hash rai::receive_block::root () const
{
	return hashables.previous;
}

rai::account rai::receive_block::representative () const
{
	return 0;
}

rai::signature rai::receive_block::block_signature () const
{
	return signature;
}

void rai::receive_block::signature_set (rai::uint512_union const & signature_a)
{
	signature = signature_a;
}

rai::block_type rai::receive_block::type () const
{
	return rai::block_type::receive;
}

rai::receive_hashables::receive_hashables (rai::block_hash const & previous_a, rai::block_hash const & source_a) :
previous (previous_a),
source (source_a)
{
}

rai::receive_hashables::receive_hashables (bool & error_a, rai::stream & stream_a)
{
	error_a = rai::read (stream_a, previous.bytes);
	if (!error_a)
	{
		error_a = rai::read (stream_a, source.bytes);
	}
}

rai::receive_hashables::receive_hashables (bool & error_a, boost::property_tree::ptree const & tree_a)
{
	try
	{
		auto previous_l (tree_a.get<std::string> ("previous"));
		auto source_l (tree_a.get<std::string> ("source"));
		error_a = previous.decode_hex (previous_l);
		if (!error_a)
		{
			error_a = source.decode_hex (source_l);
		}
	}
	catch (std::runtime_error const &)
	{
		error_a = true;
	}
}

void rai::receive_hashables::hash (blake2b_state & hash_a) const
{
	blake2b_update (&hash_a, previous.bytes.data (), sizeof (previous.bytes));
	blake2b_update (&hash_a, source.bytes.data (), sizeof (source.bytes));
}
