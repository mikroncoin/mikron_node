#pragma once

#include <rai/lib/blocks.hpp>
#include <rai/secure/utility.hpp>

#include <boost/iterator/transform_iterator.hpp>
#include <boost/property_tree/ptree.hpp>
#include <boost/variant.hpp>

#include <unordered_map>

#include <blake2/blake2.h>

namespace boost
{
template <>
struct hash<rai::uint256_union>
{
	size_t operator() (rai::uint256_union const & value_a) const
	{
		std::hash<rai::uint256_union> hash;
		return hash (value_a);
	}
};
}
namespace rai
{
const uint8_t protocol_version = 54; // Due to comment, to prevent accidental tainting TODO
const uint8_t protocol_version_min = 54;
const uint8_t protocol_version_legacy_min = 1; // Not used as of version 1

class block_store;

/**
 * A key pair. The private key is generated from the random pool, or passed in
 * as a hex string. The public key is derived using ed25519.
 */
class keypair
{
public:
	keypair ();
	keypair (std::string const &);
	keypair (rai::raw_key &&);
	rai::public_key pub;
	rai::raw_key prv;
};

std::unique_ptr<rai::block> deserialize_block (MDB_val const &);

/**
 * Latest information about an account
 */
class account_info
{
public:
	account_info ();
	account_info (rai::mdb_val const &);
	account_info (rai::account_info const &) = default;
	account_info (rai::block_hash const &, rai::block_hash const &, rai::block_hash const &, rai::amount const &, rai::timestamp_t, uint64_t);
	bool operator== (rai::account_info const &) const;
	bool operator!= (rai::account_info const &) const;
	rai::amount balance_with_manna (rai::account const &, rai::timestamp_t) const;
	rai::mdb_val serialize_to_db () const;
	void deserialize_from_db (rai::mdb_val const &);
	size_t size_in_db () const;
	rai::timestamp_t last_block_time () const { return static_cast<rai::timestamp_t> (last_block_time_intern); }
	void last_block_time_set (rai::timestamp_t t) { last_block_time_intern = t; }
	// members, they must be all value types
	rai::block_hash head;
	rai::block_hash rep_block; // to deprecate, all blocks have the representative
	rai::block_hash open_block;
	rai::amount balance;
	::uint64_t last_block_time_intern; // in fact this is a rai::timestamp_t, 4-byte, but for alignment reasons stored on 8 bytes
	::uint64_t block_count;
};

/**
 * Information on an uncollected send
 */
class pending_info
{
public:
	pending_info ();
	pending_info (rai::mdb_val const &);
	pending_info (rai::account const &, rai::amount const &);
	bool operator== (rai::pending_info const &) const;
	rai::mdb_val serialize_to_db () const;
	void deserialize_from_db (rai::mdb_val const &);
	size_t size_in_db () const;
	rai::account source;
	rai::amount amount;
};

class pending_key
{
public:
	pending_key (rai::account const &, rai::block_hash const &);
	pending_key (MDB_val const &);
	void serialize (rai::stream &) const;
	bool deserialize (rai::stream &);
	bool operator== (rai::pending_key const &) const;
	rai::mdb_val val () const;
	rai::account account;
	rai::block_hash hash;
};

class block_info
{
public:
	block_info ();
	block_info (rai::mdb_val const &);
	block_info (rai::account const &, rai::amount const &);
	bool operator== (rai::block_info const &) const;
	rai::mdb_val serialize_to_db () const;
	void deserialize_from_db (rai::mdb_val const &);
	size_t size_in_db () const;
	rai::account account;
	rai::amount balance;
};
class block_counts
{
public:
	block_counts ();
	size_t sum ();
	size_t state;
	size_t comment;
};
typedef std::vector<boost::variant<std::shared_ptr<rai::block>, rai::block_hash>>::const_iterator vote_blocks_vec_iter;
class iterate_vote_blocks_as_hash
{
public:
	iterate_vote_blocks_as_hash () = default;
	rai::block_hash operator() (boost::variant<std::shared_ptr<rai::block>, rai::block_hash> const & item) const;
};
class vote
{
public:
	vote () = default;
	vote (rai::vote const &);
	vote (bool &, rai::stream &);
	vote (bool &, rai::stream &, rai::block_type);
	vote (rai::account const &, rai::raw_key const &, uint64_t, std::shared_ptr<rai::block>);
	vote (rai::account const &, rai::raw_key const &, uint64_t, std::vector<rai::block_hash>);
	vote (MDB_val const &);
	std::string hashes_string () const;
	rai::uint256_union hash () const;
	bool operator== (rai::vote const &) const;
	bool operator!= (rai::vote const &) const;
	void serialize (rai::stream &, rai::block_type);
	void serialize (rai::stream &);
	bool deserialize (rai::stream &);
	bool validate ();
	boost::transform_iterator<rai::iterate_vote_blocks_as_hash, rai::vote_blocks_vec_iter> begin () const;
	boost::transform_iterator<rai::iterate_vote_blocks_as_hash, rai::vote_blocks_vec_iter> end () const;
	std::string to_json () const;
	// Vote round sequence number
	uint64_t sequence;
	// The blocks, or block hashes, that this vote is for
	std::vector<boost::variant<std::shared_ptr<rai::block>, rai::block_hash>> blocks;
	// Account that's voting
	rai::account account;
	// Signature of sequence + block hashes
	rai::signature signature;
	static const std::string hash_prefix;
};
enum class vote_code
{
	invalid, // Vote is not signed correctly
	replay, // Vote does not have the highest sequence number, it's a replay
	vote // Vote has the highest sequence number
};

enum class process_result
{
	progress = 0, // Hasn't been seen before, signed correctly
	bad_signature = 1, // Signature was bad, forged or transmission error
	old = 2, // Already seen and was valid
	negative_spend = 3, // Malicious attempt to spend a negative amount
	fork = 4, // Malicious fork based on previous
	unreceivable = 5, // Source block doesn't exist, has already been received, or requires an account upgrade (epoch blocks)
	gap_previous = 6, // Block marked as previous is unknown
	gap_source = 7, // Block marked as source is unknown
	opened_burn_account = 8, // The impossible happened, someone found the private key associated with the public key '0'.
	balance_mismatch = 9, // Balance and amount delta don't match
	representative_mismatch = 10, // Representative is changed when it is not allowed
	block_position = 11, // This block cannot follow the previous block (e.g. due to epoch)
	invalid_state_block = 12, // a state block with undefined subtype
	invalid_block_creation_time = 13, // Out-of-order block, or invalid block creation time
	send_same_account = 14, // send to self
	invalid_comment_block = 15, // a comment block with invalid parameters
	invalid_comment_block_legacy = 16, // Comment block is not allowed before an epoch time
};
class process_return
{
public:
	rai::process_result code;
	rai::account account;
	rai::amount amount;
	rai::account pending_account;
	rai::state_block_subtype state_subtype;
};
enum class tally_result
{
	vote,
	changed,
	confirm
};
extern rai::keypair const & zero_key;
extern rai::keypair const & test_genesis_key;
extern rai::account const & rai_test_genesis_account;
extern rai::account const & rai_beta_genesis_account;
extern rai::account const & rai_live_genesis_account;
extern std::string const & rai_test_genesis;
extern std::string const & rai_beta_genesis;
extern std::string const & rai_live_genesis;
extern rai::keypair const & test_manna_key;
extern std::string const & genesis_block;
extern rai::account const & genesis_account;
extern rai::account const & burn_account;
extern rai::amount_t const & genesis_amount;
extern rai::timestamp_t const genesis_time;
extern rai::account const & manna_account;
// A block hash that compares inequal to any real block hash
extern rai::block_hash const & not_a_block;
// An account number that compares inequal to any real account number
extern rai::block_hash const & not_an_account;

class genesis
{
public:
	explicit genesis ();
	void initialize (MDB_txn *, rai::block_store &) const;
	rai::block_hash hash () const;
	rai::state_block const & block () const;
	rai::block_hash root () const;
	std::unique_ptr<rai::state_block> genesis_block;
};

class manna_control
{
public:
	static uint32_t manna_start;
	static uint32_t manna_freq;
	static rai::amount_t manna_increment;

	static bool is_manna_account (rai::account const &);
	static rai::amount_t adjust_balance_with_manna (rai::amount_t, rai::timestamp_t, rai::timestamp_t);
	static rai::amount_t compute_manna_increment (rai::timestamp_t, rai::timestamp_t);
};

}
