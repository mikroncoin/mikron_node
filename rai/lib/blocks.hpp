#pragma once

#include <rai/lib/numbers.hpp>

#include <assert.h>
#include <blake2/blake2.h>
#include <boost/property_tree/json_parser.hpp>
#include <streambuf>

namespace rai
{
std::string to_string_hex (uint64_t);
bool from_string_hex (std::string const &, uint64_t &);
// We operate on streams of uint8_t by convention
using stream = std::basic_streambuf<uint8_t>;
// Read a a number of bytes
bool read_len (rai::stream & stream_a, size_t len, uint8_t * value);
// Read a raw byte stream the size of `T' and fill value.
template <typename T>
bool read (rai::stream & stream_a, T & value)
{
	static_assert (std::is_pod<T>::value, "Can't stream read non-standard layout types");
	auto amount_read (stream_a.sgetn (reinterpret_cast<uint8_t *> (&value), sizeof (value)));
	return amount_read != sizeof (value);
}
// Write a number of bytes
void write_len (rai::stream & stream_a, size_t len, const uint8_t * value);
template <typename T>
void write (rai::stream & stream_a, T const & value)
{
	static_assert (std::is_pod<T>::value, "Can't stream write non-standard layout types");
	auto amount_written (stream_a.sputn (reinterpret_cast<uint8_t const *> (&value), sizeof (value)));
	assert (amount_written == sizeof (value));
}

class epoch
{
public:
	// The time origin for all block information. Sept 1 2018 00:00 UTC. In Posix time.
	static const rai::uint32_t origin = 1535760000; // Sept 1 2018 00:00 UTC
};

class block_visitor;

enum class block_type : uint8_t
{
	invalid = 0,
	not_a_block = 1,
	//send = 2,
	//receive = 3,
	//open = 4,
	//change = 5,
	state = 6,
	comment = 7
};

using timestamp_t = uint32_t;

// Number of seconds, relative to rai::epoch::origin (Sept 1 2018), 4-byte
class short_timestamp
{
public:
	// The time origin for creation times, in Posix time, Sept 1 2018.
	// default constructor, 0
	short_timestamp ();
	short_timestamp (rai::timestamp_t);
	short_timestamp (const rai::short_timestamp &);
	bool operator== (rai::short_timestamp const &) const;
	rai::timestamp_t number () const;
	void set_time_now ();
	static rai::timestamp_t now ();
	static rai::timestamp_t convert_from_posix_time (uint64_t);
	static uint64_t convert_to_posix_time (rai::timestamp_t);
	void set_from_posix_time (uint64_t);
	bool decode_dec (std::string const &);
	uint64_t to_posix_time () const;
	std::string to_date_string_utc () const;
	std::string to_date_string_local () const;
	bool is_zero () const;
	void clear ();
	uint32_struct data;
};

// Abstract base class (interface) for blocks.
class block
{
public:
	// Return a digest of the hashables in this block.
	virtual rai::block_hash hash () const = 0;
	virtual std::string to_json () const = 0;
	virtual uint64_t work_get () const = 0;
	virtual void work_set (uint64_t) = 0;
	// Block creation time, seconds since rai::epoch::origin, 4-byte
	virtual rai::short_timestamp creation_time () const = 0;
	// Previous block in account's chain, zero for open block
	virtual rai::block_hash previous () const = 0;
	// Source block for open/receive blocks, zero otherwise.
	virtual rai::block_hash source () const = 0;
	// Previous block or account number for open blocks
	virtual rai::block_hash root () const = 0;
	virtual rai::account account () const = 0;
	virtual rai::account representative () const = 0;
	virtual rai::amount balance () const = 0;
	virtual void serialize (rai::stream &) const = 0;
	virtual void serialize_json (std::string &) const = 0;
	virtual void visit (rai::block_visitor &) const = 0;
	virtual bool operator== (rai::block const &) const = 0;
	virtual rai::block_type type () const = 0;
	virtual rai::signature const & signature_get () const = 0;
	virtual void signature_set (rai::uint512_union const &) = 0;
	virtual ~block () = default;
};

// Non-final base class for block hashables, with some common fields
class base_hashables
{
public:
	base_hashables ();
	base_hashables (rai::account const &, rai::block_hash const &, rai::timestamp_t, rai::account const &, rai::amount const &);
	void hash (blake2b_state &) const;
	// Account# / public key that operates this account
	// Uses:
	// Bulk signature validation in advance of further ledger processing
	// Arranging uncomitted transactions by account
	rai::account account;
	// Previous transaction in this chain
	rai::block_hash previous;
	// Block creation time, seconds since rai::epoch::origin (Sept 1 2018), 4-byte
	rai::short_timestamp creation_time;
	// Representative of this account
	rai::account representative;
	// Current balance of this account
	// Allows lookup of account balance simply by looking at the head block
	rai::amount balance;
};

// Non-final base class for blocks, with some common fields and implementations.
class base_block : public block
{
public:
	base_block ();
	base_block (rai::account const &, rai::block_hash const &, rai::timestamp_t, rai::account const &, rai::amount const &, rai::signature const &, uint64_t);
	// Return a digest of the hashables in this block.
	rai::block_hash hash () const override;
	virtual void hash (blake2b_state &) const = 0;
	virtual bool deserialize (rai::stream &) = 0;
	virtual bool deserialize_json (boost::property_tree::ptree const &) = 0;
	virtual std::string to_json () const override;
	rai::short_timestamp creation_time () const override;
	rai::block_hash previous () const override;
	rai::block_hash root () const override;
	rai::account account () const override;
	rai::account representative () const override;
	rai::amount balance () const override;
	rai::block_hash source () const override;
	bool has_previous () const;
	bool has_representative () const;
	void previous_set (rai::block_hash const &);
	void account_set (rai::account const &);
	void representative_set (rai::account const &);
	void balance_set (rai::amount const &);
	virtual rai::signature const & signature_get () const override;
	virtual void signature_set (rai::uint512_union const &) override;
	virtual uint64_t work_get () const override;
	virtual void work_set (uint64_t) override;

protected:
	rai::base_hashables base_hashables;
	rai::signature signature;
	rai::work work;
};

enum class state_block_subtype : uint8_t
{
	undefined = 0,
	send = 2,
	receive = 3,
	open_receive = 4,
	open_genesis = 5,
	change = 6
};

class state_hashables
{
public:
	state_hashables ();
	state_hashables (rai::uint256_union const &);
	void hash (blake2b_state &) const;
	// Link field contains source block_hash if receiving, destination account if sending
	rai::uint256_union link;
};

class state_block : public rai::base_block
{
public:
	state_block (rai::account const &, rai::block_hash const &, rai::timestamp_t, rai::account const &, rai::amount const &, rai::uint256_union const &, rai::raw_key const &, rai::public_key const &, uint64_t);
	state_block (bool &, rai::stream &);
	state_block (bool &, boost::property_tree::ptree const &);
	virtual ~state_block () = default;
	using rai::block::hash;
	void hash (blake2b_state &) const override;
	rai::uint256_union link () const;
	virtual bool deserialize (rai::stream &) override;
	virtual bool deserialize_json (boost::property_tree::ptree const &) override;
	void serialize (rai::stream &) const override;
	void serialize_json (std::string &) const override;
	void visit (rai::block_visitor &) const override;
	rai::block_type type () const override;
	bool operator== (rai::block const &) const override;
	bool operator== (rai::state_block const &) const;
	// Determining whether it is a send or receive block requires the previous balance too.  A more convenient version is through ledger, use that if possible.
	rai::state_block_subtype get_subtype (rai::amount_t, rai::timestamp_t) const;
	bool is_valid_open_subtype () const;
	bool is_valid_send_or_receive_subtype () const;
	bool is_valid_change_subtype () const;
	bool has_link () const;
	static size_t constexpr size = sizeof (rai::account) + sizeof (rai::block_hash) + sizeof (rai::short_timestamp) + sizeof (rai::account) + sizeof (rai::amount) + sizeof (rai::uint256_union) + sizeof (rai::signature) + sizeof (uint64_t);
	rai::state_hashables hashables;
};

enum class comment_block_subtype : uint8_t
{
	undefined = 0,
	// Comment valid for the account from now on
	account = 1,
	// Comment valid for the next send block only
	//send = 2
};

class comment_hashables
{
public:
	comment_hashables ();
	comment_hashables (rai::uint32_t, rai::var_len_string const &);
	void hash (blake2b_state &) const;
	rai::uint32_t subtype;
	// Comment string, var len, UTF-8
	rai::var_len_string comment;
};

class comment_block : public rai::base_block
{
public:
	comment_block (rai::account const &, rai::block_hash const &, rai::timestamp_t, rai::account const &, rai::amount const &, rai::comment_block_subtype, std::string const &, rai::raw_key const &, rai::public_key const &, uint64_t);
	comment_block (bool &, rai::stream &);
	comment_block (bool &, boost::property_tree::ptree const &);
	virtual ~comment_block () = default;
	using rai::block::hash;
	void hash (blake2b_state &) const override;
	rai::comment_block_subtype subtype () const;
	static rai::var_len_string comment_string_to_raw (std::string const &);
	static std::string comment_raw_to_string (rai::var_len_string const &);
	rai::var_len_string const & comment_raw () const;
	std::string comment () const;
	virtual bool deserialize (rai::stream &) override;
	virtual bool deserialize_json (boost::property_tree::ptree const &) override;
	void serialize (rai::stream &) const override;
	void serialize_json (std::string &) const override;
	void visit (rai::block_visitor &) const override;
	rai::block_type type () const override;
	bool operator== (rai::block const &) const override;
	bool operator== (rai::comment_block const &) const;
	static size_t const max_comment_length = 160;
	static size_t constexpr size_base = sizeof (rai::account) + sizeof (rai::block_hash) + sizeof (rai::short_timestamp) + sizeof (rai::account) + sizeof (rai::amount) + sizeof (rai::uint32_t) + sizeof (uint16_t) + sizeof (rai::signature) + sizeof (uint64_t);
	rai::comment_hashables hashables;
};

class block_visitor
{
public:
	virtual void state_block (rai::state_block const &) = 0;
	virtual void comment_block (rai::comment_block const &) = 0;
	virtual ~block_visitor () = default;
};
std::unique_ptr<rai::block> deserialize_block (rai::stream &);
std::unique_ptr<rai::block> deserialize_block (rai::stream &, rai::block_type);
std::unique_ptr<rai::block> deserialize_block_json (boost::property_tree::ptree const &);
void serialize_block (rai::stream &, rai::block const &);
}
