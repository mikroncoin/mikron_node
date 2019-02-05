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
		if (type == "state")
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
