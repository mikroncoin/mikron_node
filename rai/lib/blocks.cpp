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

rai::base_hashables::base_hashables () :
account (),
creation_time (),
previous (),
representative (),
balance ()
{
}

// if time is 0, current time is taken
rai::base_hashables::base_hashables (rai::account const & account_a, rai::block_hash const & previous_a, rai::timestamp_t creation_time_a, rai::account const & representative_a, rai::amount const & balance_a) :
account (account_a),
creation_time (creation_time_a),
previous (previous_a),
representative (representative_a),
balance (balance_a)
{
	if (creation_time_a == 0)
	{
		creation_time.set_time_now ();
	}
}

void rai::base_hashables::hash (blake2b_state & hash_a) const
{
	blake2b_update (&hash_a, account.bytes.data (), sizeof (account.bytes));
	uint32_t creation_time_big_endian (creation_time.data.number_big_endian ());
	blake2b_update (&hash_a, &creation_time_big_endian, sizeof (creation_time_big_endian));
	blake2b_update (&hash_a, previous.bytes.data (), sizeof (previous.bytes));
	blake2b_update (&hash_a, representative.bytes.data (), sizeof (representative.bytes));
	uint64_t balance_big_endian (balance.number_big_endian ());
	blake2b_update (&hash_a, &balance_big_endian, sizeof (balance_big_endian));
}

rai::base_block::base_block () :
base_hashables (),
signature (),
work ()
{
}

rai::base_block::base_block (rai::account const & account_a, rai::block_hash const & previous_a, rai::timestamp_t creation_time_a, rai::account const & representative_a, rai::amount const & balance_a, rai::signature const & signature_a, uint64_t work_a) :
base_hashables (account_a, previous_a, creation_time_a, representative_a, balance_a),
signature (signature_a),
work (work_a)
{
}

rai::block_hash rai::base_block::hash () const
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

rai::short_timestamp rai::base_block::creation_time () const
{
	return base_hashables.creation_time;
}

rai::block_hash rai::base_block::previous () const
{
	return base_hashables.previous;
}

rai::block_hash rai::base_block::root () const
{
	return !base_hashables.previous.is_zero () ? base_hashables.previous : base_hashables.account;
}

rai::account rai::base_block::account () const
{
	return base_hashables.account;
}

rai::account rai::base_block::representative () const
{
	return base_hashables.representative;
}

rai::amount rai::base_block::balance () const
{
	return base_hashables.balance;
}

rai::block_hash rai::base_block::source () const
{
	return 0;
}

bool rai::base_block::has_previous () const
{
	return !base_hashables.previous.is_zero ();
}

bool rai::base_block::has_representative () const
{
	return !base_hashables.representative.is_zero ();
}

void rai::base_block::previous_set (rai::block_hash const & previous_a)
{
	base_hashables.previous = previous_a;
}

void rai::base_block::account_set (rai::account const & account_a)
{
	base_hashables.account = account_a;
}

void rai::base_block::representative_set (rai::account const & representative_a)
{
	base_hashables.representative = representative_a;
}

void rai::base_block::balance_set (rai::amount const & balance_a)
{
	base_hashables.balance = balance_a;
}

std::string rai::base_block::to_json () const
{
	std::string result;
	serialize_json (result);
	return result;
}

rai::signature const & rai::base_block::signature_get () const
{
	return signature;
}

void rai::base_block::signature_set (rai::uint512_union const & signature_a)
{
	signature = signature_a;
}

uint64_t rai::base_block::work_get () const
{
	return work.number ();
}

void rai::base_block::work_set (uint64_t work_a)
{
	work = work_a;
}

rai::state_hashables::state_hashables () :
link ()
{
}

rai::state_hashables::state_hashables (rai::uint256_union const & link_a) :
link (link_a)
{
}

void rai::state_hashables::hash (blake2b_state & hash_a) const
{
	blake2b_update (&hash_a, link.bytes.data (), sizeof (link.bytes));
}

size_t constexpr rai::state_block::size;

// if time is 0, current time is taken
rai::state_block::state_block (rai::account const & account_a, rai::block_hash const & previous_a, rai::timestamp_t creation_time_a, rai::account const & representative_a, rai::amount const & balance_a, rai::uint256_union const & link_a, rai::raw_key const & prv_a, rai::public_key const & pub_a, uint64_t work_a) :
base_block (account_a, previous_a, creation_time_a, representative_a, balance_a, uint512_union (), work_a),
hashables (link_a)
{
	signature_set (rai::sign_message (prv_a, pub_a, rai::base_block::hash ()));
}

rai::state_block::state_block (bool & error_a, rai::stream & stream_a) :
base_block (),
hashables ()
{
	if (error_a)
		return;
	error_a = deserialize (stream_a);
}

rai::state_block::state_block (bool & error_a, boost::property_tree::ptree const & tree_a) :
base_block (),
hashables ()
{
	if (error_a)
		return;
	error_a = deserialize_json (tree_a);
}

void rai::state_block::hash (blake2b_state & hash_a) const
{
	rai::uint256_union preamble (static_cast<uint64_t> (rai::block_type::state));
	blake2b_update (&hash_a, preamble.bytes.data (), preamble.bytes.size ());
	base_hashables.hash (hash_a);
	hashables.hash (hash_a);
}

rai::uint256_union rai::state_block::link () const
{
	return hashables.link;
}

void rai::state_block::serialize (rai::stream & stream_a) const
{
	write (stream_a, base_hashables.account);
	base_hashables.creation_time.data.serialize (stream_a);
	write (stream_a, base_hashables.previous);
	write (stream_a, base_hashables.representative);
	base_hashables.balance.serialize (stream_a);
	write (stream_a, hashables.link);
	write (stream_a, signature);
	work.serialize (stream_a);
}

void rai::state_block::serialize_json (std::string & string_a) const
{
	boost::property_tree::ptree tree;
	tree.put ("type", "state");
	tree.put ("account", base_hashables.account.to_account ());
	tree.put ("creation_time", base_hashables.creation_time.data.to_string_dec ());
	tree.put ("creation_time_as_date", base_hashables.creation_time.to_date_string_utc ());
	tree.put ("previous", base_hashables.previous.to_string ());
	tree.put ("representative", base_hashables.representative.to_account ());
	tree.put ("balance", base_hashables.balance.to_string_dec ());
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
	auto error (read (stream_a, base_hashables.account));
	if (error) return error;
	error = base_hashables.creation_time.data.deserialize (stream_a);
	if (error) return error;
	error = read (stream_a, base_hashables.previous);
	if (error) return error;
	error = read (stream_a, base_hashables.representative);
	if (error) return error;
	error = base_hashables.balance.deserialize (stream_a);
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
		error = base_hashables.account.decode_account (account_l);
		if (error) return error;
		error = base_hashables.creation_time.data.decode_dec (creation_time_l);
		if (error) return error;
		error = base_hashables.previous.decode_hex (previous_l);
		if (error) return error;
		error = base_hashables.representative.decode_account (representative_l);
		if (error) return error;
		error = base_hashables.balance.decode_dec (balance_l);
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
	return base_hashables.account == other_a.base_hashables.account && base_hashables.previous == other_a.base_hashables.previous && base_hashables.representative == other_a.base_hashables.representative && base_hashables.balance == other_a.base_hashables.balance && hashables.link == other_a.hashables.link && signature == other_a.signature && work == other_a.work;
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
	auto cur_balance (base_hashables.balance.number ());
	if (rai::manna_control::is_manna_account (base_hashables.account))
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
	if (base_hashables.account.is_zero ())
		return false;
	if (has_previous ()) return false;
	if (base_hashables.account != rai::genesis_account)
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
	if (base_hashables.account.is_zero ())
		return false;
	if (!has_previous ()) return false;
	if (!has_link ()) return false;
	return true;
}

bool rai::state_block::is_valid_change_subtype () const
{
	if (base_hashables.account.is_zero ())
		return false;
	if (!has_representative ()) return false;
	if (!has_previous ()) return false;
	if (has_link ()) return false;
	return true;
}

bool rai::state_block::has_link () const
{
	return !hashables.link.is_zero ();
}

rai::comment_hashables::comment_hashables () :
subtype (),
comment ()
{
}

rai::comment_hashables::comment_hashables (rai::uint32_t subtype_a, rai::uint512_union const & comment_a) :
subtype (subtype_a),
comment (comment_a)
{
}

void rai::comment_hashables::hash (blake2b_state & hash_a) const
{
	blake2b_update (&hash_a, &subtype, sizeof (subtype));
	blake2b_update (&hash_a, comment.bytes.data (), sizeof (comment.bytes));
}

size_t constexpr rai::comment_block::size;

// if time is 0, current time is taken
rai::comment_block::comment_block (rai::account const & account_a, rai::block_hash const & previous_a, rai::timestamp_t creation_time_a, rai::account const & representative_a, rai::amount const & balance_a, rai::comment_block_subtype subtype_a, std::string const & comment_a, rai::raw_key const & prv_a, rai::public_key const & pub_a, uint64_t work_a) :
base_block (account_a, previous_a, creation_time_a, representative_a, balance_a, uint512_union (), work_a),
hashables ((uint32_t)subtype_a, comment_string_to_raw (comment_a))
{
	signature_set (rai::sign_message (prv_a, pub_a, rai::base_block::hash ()));
}

rai::comment_block::comment_block (bool & error_a, rai::stream & stream_a) :
base_block (),
hashables ()
{
	if (error_a)
		return;
	error_a = deserialize (stream_a);
}

rai::comment_block::comment_block (bool & error_a, boost::property_tree::ptree const & tree_a) :
base_block (),
hashables ()
{
	if (error_a)
		return;
	error_a = deserialize_json (tree_a);
}

void rai::comment_block::hash (blake2b_state & hash_a) const
{
	rai::uint256_union preamble (static_cast<uint64_t> (rai::block_type::comment));
	blake2b_update (&hash_a, preamble.bytes.data (), preamble.bytes.size ());
	base_hashables.hash (hash_a);
	hashables.hash (hash_a);
}

rai::comment_block_subtype rai::comment_block::subtype () const
{
	return (rai::comment_block_subtype)hashables.subtype;
}

rai::uint512_union rai::comment_block::comment_string_to_raw (std::string const & comment_a)
{
	// TODO UTF-8 conversion
	auto len (comment_a.length ());
	rai::uint512_union raw;
	raw.clear ();
	auto maxlen (sizeof (raw.bytes));
	if (len > maxlen)
	{
		len = maxlen;
	}
	for (auto i (0); i < len; ++i)
	{
		raw.bytes[i] = (uint8_t)comment_a[i];
	}
	return raw;
}

std::string rai::comment_block::comment_raw_to_string (rai::uint512_union const & comment_raw_a)
{
	// TODO UTF-8 conversion
	std::array<uint8_t, 64> const & data = comment_raw_a.bytes;
	auto nullidx (std::find (data.begin (), data.end (), 0));
	std::string comment (data.begin (), nullidx);
	return comment;
}

rai::uint512_union const & rai::comment_block::comment_raw () const
{
	return hashables.comment;
}

std::string rai::comment_block::comment () const
{
	return comment_raw_to_string (hashables.comment);
}

bool rai::comment_block::deserialize (rai::stream & stream_a)
{
	auto error (read (stream_a, base_hashables.account));
	if (error)
		return error;
	error = base_hashables.creation_time.data.deserialize (stream_a);
	if (error)
		return error;
	error = read (stream_a, base_hashables.previous);
	if (error)
		return error;
	error = read (stream_a, base_hashables.representative);
	if (error)
		return error;
	error = base_hashables.balance.deserialize (stream_a);
	if (error)
		return error;
	error = read (stream_a, hashables.subtype);
	if (error)
		return error;
	error = read (stream_a, hashables.comment);
	if (error)
		return error;
	error = read (stream_a, signature);
	if (error)
		return error;
	error = work.deserialize (stream_a);
	return error;
}

bool rai::comment_block::deserialize_json (boost::property_tree::ptree const & tree_a)
{
	auto error (false);
	try
	{
		assert (tree_a.get<std::string> ("type") == "comment");
		auto account_l (tree_a.get<std::string> ("account"));
		error = base_hashables.account.decode_account (account_l);
		if (error)
			return error;
		auto creation_time_l (tree_a.get<std::string> ("creation_time"));
		error = base_hashables.creation_time.data.decode_dec (creation_time_l);
		if (error)
			return error;
		auto previous_l (tree_a.get<std::string> ("previous"));
		error = base_hashables.previous.decode_hex (previous_l);
		if (error)
			return error;
		auto representative_l (tree_a.get<std::string> ("representative"));
		error = base_hashables.representative.decode_account (representative_l);
		if (error)
			return error;
		auto balance_l (tree_a.get<std::string> ("balance"));
		error = base_hashables.balance.decode_dec (balance_l);
		if (error)
			return error;
		hashables.subtype = (uint32_t)rai::comment_block_subtype::account; // TODO
		auto comment_l (tree_a.get<std::string> ("comment"));
		hashables.comment = comment_string_to_raw (comment_l);
		auto work_l (tree_a.get<std::string> ("work"));
		error = work.decode_hex (work_l);
		if (error)
			return error;
		auto signature_l (tree_a.get<std::string> ("signature"));
		error = signature.decode_hex (signature_l);
	}
	catch (std::runtime_error const &)
	{
		error = true;
	}
	return error;
}

void rai::comment_block::serialize (rai::stream & stream_a) const
{
	write (stream_a, base_hashables.account);
	base_hashables.creation_time.data.serialize (stream_a);
	write (stream_a, base_hashables.previous);
	write (stream_a, base_hashables.representative);
	base_hashables.balance.serialize (stream_a);
	write (stream_a, hashables.subtype);
	write (stream_a, hashables.comment);
	write (stream_a, signature);
	work.serialize (stream_a);
}

void rai::comment_block::serialize_json (std::string & string_a) const
{
	boost::property_tree::ptree tree;
	tree.put ("type", "comment");
	tree.put ("account", base_hashables.account.to_account ());
	tree.put ("creation_time", base_hashables.creation_time.data.to_string_dec ());
	tree.put ("creation_time_as_date", base_hashables.creation_time.to_date_string_utc ());
	tree.put ("previous", base_hashables.previous.to_string ());
	tree.put ("representative", base_hashables.representative.to_account ());
	tree.put ("balance", base_hashables.balance.to_string_dec ());
	tree.put ("subtype", std::to_string (hashables.subtype));
	tree.put ("comment", comment ());
	tree.put ("comment_as_hex", comment_raw ().number ());
	std::string signature_l;
	signature.encode_hex (signature_l);
	tree.put ("signature", signature_l);
	tree.put ("work", work.to_string ());
	std::stringstream ostream;
	boost::property_tree::write_json (ostream, tree);
	string_a = ostream.str ();
}

void rai::comment_block::visit (rai::block_visitor & visitor_a) const
{
	visitor_a.comment_block (*this);
}

rai::block_type rai::comment_block::type () const
{
	return rai::block_type::comment;
}

bool rai::comment_block::operator== (rai::block const & other_a) const
{
	return blocks_equal (*this, other_a);
}

bool rai::comment_block::operator== (rai::comment_block const & other_a) const
{
	return base_hashables.account == other_a.base_hashables.account && base_hashables.previous == other_a.base_hashables.previous && base_hashables.representative == other_a.base_hashables.representative && base_hashables.balance == other_a.base_hashables.balance && hashables.subtype == other_a.hashables.subtype && hashables.comment == other_a.hashables.comment && signature == other_a.signature && work == other_a.work;
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
		else if (type == "comment")
		{
			bool error (false);
			std::unique_ptr<rai::comment_block> obj (new rai::comment_block (error, tree_a));
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
		case rai::block_type::comment:
		{
			bool error (false);
			std::unique_ptr<rai::comment_block> obj (new rai::comment_block (error, stream_a));
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
