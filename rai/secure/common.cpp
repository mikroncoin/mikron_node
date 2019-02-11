#include <rai/secure/common.hpp>

#include <rai/lib/interface.h>
#include <rai/node/common.hpp>
#include <rai/secure/blockstore.hpp>
#include <rai/secure/versioning.hpp>

#include <boost/property_tree/json_parser.hpp>
#include <boost/algorithm/string/replace.hpp>

#include <queue>

#include <ed25519-donna/ed25519.h>

// Genesis keys for network variants
namespace
{
const char * test_genesis_private_key_data = "6EBA231F6BDCDA9B67F26CAE66CEF4EE6922F42B37EEDD76D485B5F5A3BC8AA9";
const char * test_genesis_public_key_data = "96F167643B3DEB3B614003E432F33CCFB0C3E64866DA99ACB484F2B13E3E1980"; // mik_37qjexk5phhd9fin11z68dsmsmxirhm6isptm8pdb39kp6z5w8e1534tigqk
const char * beta_genesis_public_key_data = "7F6FA3E3C6D40583B02057D9AE1B5DA04D1647FE7F3F1BC11252BEA684729E64"; // mik_1zuhnhjwfo17igr41oysorfoua4f4s5zwzsz5h1j6noynt4979m6z5nnt5q4
const char * live_genesis_public_key_data = "7B73946E4EE555E4E7F2654829F3E5577606AD74268A0FD777594B01464EBAA0"; // mik_1yumkjq6xscowmmz6sca79sycoup1tpqabnc3zdqgpcd1756xgo1k53z7yeg
const rai::amount_t test_genesis_amount = std::numeric_limits<rai::amount_t>::max (); // 2^64-1 18446744073709551615
const rai::amount_t beta_genesis_amount = (rai::amount_t)300000000 * (rai::amount_t)10000000000;
const rai::amount_t live_genesis_amount = (rai::amount_t)300000000 * (rai::amount_t)10000000000;
const uint32_t test_genesis_time = 2592000; // 2018.10.01.  1538352000 - short_timestamp_epoch = 1538352000 - 1535760000 = 2592000
const uint32_t beta_genesis_time = 2592000; // 2018.10.01.  1538352000 - short_timestamp_epoch = 1538352000 - 1535760000 = 2592000
const uint32_t live_genesis_time = 2592000; // 2018.10.01.  1538352000 - short_timestamp_epoch = 1538352000 - 1535760000 = 2592000
const char * test_manna_private_key_data = "AB02030F53BA4527D84859DBFF13DF0A17B74706682D3621D6C8DB0912424D3D";
const char * test_manna_public_key_data = "A8EC25B743412E09567C3363A11C0D5F5722F26236020D7BF93C9F4E0D161583"; // mik_3c9e6pun8ibg37d9reu5n6g1tqtq6ds86fi43oxzkh6zbr8je7e5eejg5r9a
const char * beta_manna_public_key_data = beta_genesis_public_key_data; // manna account is the genesis account
const char * live_manna_public_key_data = live_genesis_public_key_data; // manna account is the genesis account
const uint32_t test_manna_freq = 4;
const uint32_t beta_manna_freq = 60;
const uint32_t live_manna_freq = 86400; // 1 day
const rai::amount_t test_manna_increment = 1000;
const rai::amount_t beta_manna_increment = (rai::amount_t)50 * (rai::amount_t)10000000000;
const rai::amount_t live_manna_increment = (rai::amount_t)82000 * (rai::amount_t)10000000000;

char const * test_genesis_data = R"%%%({
	"type": "state",
	"account": "mik_37qjexk5phhd9fin11z68dsmsmxirhm6isptm8pdb39kp6z5w8e1534tigqk",
	"creation_time": "2592000",
	"previous": "0000000000000000000000000000000000000000000000000000000000000000",
	"representative": "mik_37qjexk5phhd9fin11z68dsmsmxirhm6isptm8pdb39kp6z5w8e1534tigqk",
	"balance": "18446744073709551615",
	"link": "0000000000000000000000000000000000000000000000000000000000000000",
	"work": "d0c54a27352bf9b1",
	"signature": "59D0ED5D0A7243BBB24EDE9FFE76E8F7548E406E66CCD65A88FED1B1E423CE669DF5A8CDCE60A41A70B547C3711CA70A50755478C07EFC890B096932CC4D8F09"
})%%%";

char const * beta_genesis_data = R"%%%({
	"type": "state",
	"account": "mik_1zuhnhjwfo17igr41oysorfoua4f4s5zwzsz5h1j6noynt4979m6z5nnt5q4",
	"creation_time": "2592000",
	"previous": "0000000000000000000000000000000000000000000000000000000000000000",
	"representative": "mik_1zuhnhjwfo17igr41oysorfoua4f4s5zwzsz5h1j6noynt4979m6z5nnt5q4",
	"balance": "3000000000000000000",
	"link": "0000000000000000000000000000000000000000000000000000000000000000",
	"work": "d2fe25345d6d63a5",
	"signature": "7A5C03F5927E9ADFB13EEFECA268348F10CCFD93FE68B23FB488D597D062A166CCC0DE02EF87708836BD2BC70D0125F9B83ADE30325912DA66B288EAD017F906"
})%%%";

// Non-final
char const * live_genesis_data = R"%%%({
	"type": "state",
	"account": "mik_1yumkjq6xscowmmz6sca79sycoup1tpqabnc3zdqgpcd1756xgo1k53z7yeg",
	"creation_time": "2592000",
	"previous": "0000000000000000000000000000000000000000000000000000000000000000",
	"representative": "mik_1yumkjq6xscowmmz6sca79sycoup1tpqabnc3zdqgpcd1756xgo1k53z7yeg",
	"balance": "3000000000000000000",
	"link": "0000000000000000000000000000000000000000000000000000000000000000",
	"work": "0371dbd772ad4aa9",
	"signature": "1B478F03412E02AF69FCA72269CB613FCA06E7AD6CA86FF089876E9178E6D0319A22921B4238FFB94B749844D9C965B1D965BA1A3C6B55E959A5418122E85A09"
})%%%";

class ledger_constants
{
public:
	ledger_constants () :
	zero_key ("0"),
	test_genesis_key (test_genesis_private_key_data),
	rai_test_genesis_account (test_genesis_public_key_data),
	rai_beta_genesis_account (beta_genesis_public_key_data),
	rai_live_genesis_account (live_genesis_public_key_data),
	rai_test_genesis (test_genesis_data),
	rai_beta_genesis (beta_genesis_data),
	rai_live_genesis (live_genesis_data),
	rai_test_genesis_amount (test_genesis_amount),
	rai_beta_genesis_amount (beta_genesis_amount),
	rai_live_genesis_amount (live_genesis_amount),
	test_manna_key (test_manna_private_key_data),
	genesis_account (
		rai::rai_network == rai::rai_networks::rai_test_network ? rai_test_genesis_account :
		rai::rai_network == rai::rai_networks::rai_beta_network ? rai_beta_genesis_account :
		rai_live_genesis_account),
	genesis_block (
		rai::rai_network == rai::rai_networks::rai_test_network ? rai_test_genesis :
		rai::rai_network == rai::rai_networks::rai_beta_network ? rai_beta_genesis :
		rai_live_genesis),
	genesis_amount (
		rai::rai_network == rai::rai_networks::rai_test_network ? rai_test_genesis_amount :
		rai::rai_network == rai::rai_networks::rai_beta_network ? rai_beta_genesis_amount :
		rai_live_genesis_amount),
	manna_account (
		rai::rai_network == rai::rai_networks::rai_test_network ? test_manna_public_key_data :
		rai::rai_network == rai::rai_networks::rai_beta_network ? beta_manna_public_key_data :
		live_manna_public_key_data),
	burn_account (0)
	{
		CryptoPP::AutoSeededRandomPool random_pool;
		// Randomly generating these mean no two nodes will ever have the same sentinel values which protects against some insecure algorithms
		random_pool.GenerateBlock (not_a_block.bytes.data (), not_a_block.bytes.size ());
		random_pool.GenerateBlock (not_an_account.bytes.data (), not_an_account.bytes.size ());
	}
	rai::keypair zero_key;
	rai::keypair test_genesis_key;
	rai::account rai_test_genesis_account;
	rai::account rai_beta_genesis_account;
	rai::account rai_live_genesis_account;
	std::string rai_test_genesis;
	std::string rai_beta_genesis;
	std::string rai_live_genesis;
	rai::amount_t rai_test_genesis_amount;
	rai::amount_t rai_beta_genesis_amount;
	rai::amount_t rai_live_genesis_amount;
	rai::keypair test_manna_key;
	rai::account genesis_account;
	std::string genesis_block;
	rai::amount_t genesis_amount;
	static const rai::timestamp_t genesis_time =
		rai::rai_network == rai::rai_networks::rai_test_network ? test_genesis_time :
		rai::rai_network == rai::rai_networks::rai_beta_network ? beta_genesis_time :
		live_genesis_time;
	rai::account manna_account;
	rai::block_hash not_a_block;
	rai::account not_an_account;
	rai::account burn_account;
};
ledger_constants globals;
}

size_t constexpr rai::state_block::size;

rai::keypair const & rai::zero_key (globals.zero_key);
rai::keypair const & rai::test_genesis_key (globals.test_genesis_key);
rai::account const & rai::rai_test_genesis_account (globals.rai_test_genesis_account);
rai::account const & rai::rai_beta_genesis_account (globals.rai_beta_genesis_account);
rai::account const & rai::rai_live_genesis_account (globals.rai_live_genesis_account);
std::string const & rai::rai_test_genesis (globals.rai_test_genesis);
std::string const & rai::rai_beta_genesis (globals.rai_beta_genesis);
std::string const & rai::rai_live_genesis (globals.rai_live_genesis);
rai::keypair const & rai::test_manna_key (globals.test_manna_key);

rai::account const & rai::genesis_account (globals.genesis_account);
std::string const & rai::genesis_block (globals.genesis_block);
rai::amount_t const & rai::genesis_amount (globals.genesis_amount);
rai::timestamp_t const rai::genesis_time (globals.genesis_time);
rai::account const & rai::manna_account (globals.manna_account);
rai::block_hash const & rai::not_a_block (globals.not_a_block);
rai::block_hash const & rai::not_an_account (globals.not_an_account);
rai::account const & rai::burn_account (globals.burn_account);

// Create a new random keypair
rai::keypair::keypair ()
{
	random_pool.GenerateBlock (prv.data.bytes.data (), prv.data.bytes.size ());
	ed25519_publickey (prv.data.bytes.data (), pub.bytes.data ());
}

// Create a keypair given a private key
rai::keypair::keypair (rai::raw_key && prv_a) :
prv (std::move (prv_a))
{
	ed25519_publickey (prv.data.bytes.data (), pub.bytes.data ());
}

// Create a keypair given a hex string of the private key
rai::keypair::keypair (std::string const & prv_a)
{
	auto error (prv.data.decode_hex (prv_a));
	assert (!error);
	ed25519_publickey (prv.data.bytes.data (), pub.bytes.data ());
}

// Serialize a block prefixed with an 8-bit typecode
void rai::serialize_block (rai::stream & stream_a, rai::block const & block_a)
{
	write (stream_a, block_a.type ());
	block_a.serialize (stream_a);
}

std::unique_ptr<rai::block> rai::deserialize_block (MDB_val const & val_a)
{
	rai::bufferstream stream (reinterpret_cast<uint8_t const *> (val_a.mv_data), val_a.mv_size);
	return deserialize_block (stream);
}

rai::account_info::account_info () :
head (0),
rep_block (0),
balance (0),
last_block_time_intern (0),
block_count (0)
{
}

rai::account_info::account_info (rai::mdb_val const & val_a)
{
	deserialize_from_db (val_a);
}

rai::account_info::account_info (rai::block_hash const & head_a, rai::block_hash const & rep_block_a, rai::block_hash const & open_block_a, rai::amount const & balance_a, rai::timestamp_t last_block_time_a, uint64_t block_count_a) :
head (head_a),
rep_block (rep_block_a),
open_block (open_block_a),
balance (balance_a),
last_block_time_intern (last_block_time_a),
block_count (block_count_a)
{
}

bool rai::account_info::operator== (rai::account_info const & other_a) const
{
	return head == other_a.head && rep_block == other_a.rep_block && open_block == other_a.open_block && balance == other_a.balance &&
		last_block_time_intern == other_a.last_block_time_intern && block_count == other_a.block_count;
}

bool rai::account_info::operator!= (rai::account_info const & other_a) const
{
	return !(*this == other_a);
}

rai::amount rai::account_info::balance_with_manna (rai::account const & account_a, rai::timestamp_t now_a) const
{
	if (!rai::manna_control::is_manna_account (account_a))
	{
		return balance;
	}
	rai::amount_t balance1 = balance.number ();
	rai::timestamp_t now = now_a;
	if (now == 0)
	{
		now = rai::short_timestamp::now ();
	}
	rai::amount_t balance2 = rai::manna_control::adjust_balance_with_manna (balance1, last_block_time (), now);
	return balance2;
}

size_t rai::account_info::size_in_db () const
{
	// make sure class is well packed
	assert (sizeof (rai::account_info) == sizeof (head) + sizeof (rep_block) + sizeof (open_block) + sizeof (balance) + sizeof (last_block_time_intern) + sizeof (block_count));
	return sizeof (rai::account_info);
}

rai::mdb_val rai::account_info::serialize_to_db () const
{
	auto size (size_in_db ());
	assert (size == sizeof (*this));
	return rai::mdb_val (size, const_cast<rai::account_info *> (this));
}

void rai::account_info::deserialize_from_db (rai::mdb_val const & val_a)
{
	auto size (size_in_db ());
	assert (val_a.value.mv_size == size);
	std::copy (reinterpret_cast<uint8_t const *> (val_a.value.mv_data), reinterpret_cast<uint8_t const *> (val_a.value.mv_data) + size, reinterpret_cast<uint8_t *> (this));
}

rai::block_counts::block_counts () :
state (0)
{
}

size_t rai::block_counts::sum ()
{
	return state;
}

rai::pending_info::pending_info () :
source (0),
amount (0)
{
}

rai::pending_info::pending_info (rai::mdb_val const & val_a)
{
	deserialize_from_db (val_a);
}

rai::pending_info::pending_info (rai::account const & source_a, rai::amount const & amount_a) :
source (source_a),
amount (amount_a)
{
}

size_t rai::pending_info::size_in_db () const
{
	// make sure class is well packed
	assert (sizeof (rai::pending_info) == sizeof (source) + sizeof (amount));
	return sizeof (rai::pending_info);
}

rai::mdb_val rai::pending_info::serialize_to_db () const
{
	auto size (size_in_db ());
	assert (size == sizeof (*this));
	return rai::mdb_val (size, const_cast<rai::pending_info *> (this));
}

void rai::pending_info::deserialize_from_db (rai::mdb_val const & val_a)
{
	auto size (size_in_db ());
	assert (val_a.value.mv_size == size);
	std::copy (reinterpret_cast<uint8_t const *> (val_a.value.mv_data), reinterpret_cast<uint8_t const *> (val_a.value.mv_data) + size, reinterpret_cast<uint8_t *> (this));
}

bool rai::pending_info::operator== (rai::pending_info const & other_a) const
{
	return source == other_a.source && amount == other_a.amount;
}

rai::pending_key::pending_key (rai::account const & account_a, rai::block_hash const & hash_a) :
account (account_a),
hash (hash_a)
{
}

rai::pending_key::pending_key (MDB_val const & val_a)
{
	assert (val_a.mv_size == sizeof (*this));
	static_assert (sizeof (account) + sizeof (hash) == sizeof (*this), "Packed class");
	std::copy (reinterpret_cast<uint8_t const *> (val_a.mv_data), reinterpret_cast<uint8_t const *> (val_a.mv_data) + sizeof (*this), reinterpret_cast<uint8_t *> (this));
}

void rai::pending_key::serialize (rai::stream & stream_a) const
{
	rai::write (stream_a, account.bytes);
	rai::write (stream_a, hash.bytes);
}

bool rai::pending_key::deserialize (rai::stream & stream_a)
{
	auto error (rai::read (stream_a, account.bytes));
	if (!error)
	{
		error = rai::read (stream_a, hash.bytes);
	}
	return error;
}

bool rai::pending_key::operator== (rai::pending_key const & other_a) const
{
	return account == other_a.account && hash == other_a.hash;
}

rai::mdb_val rai::pending_key::val () const
{
	return rai::mdb_val (sizeof (*this), const_cast<rai::pending_key *> (this));
}

rai::block_info::block_info () :
account (0),
balance (0)
{
}

rai::block_info::block_info (rai::mdb_val const & val_a)
{
	deserialize_from_db (val_a);
}

rai::block_info::block_info (rai::account const & account_a, rai::amount const & balance_a) :
account (account_a),
balance (balance_a)
{
}

size_t rai::block_info::size_in_db () const
{
	// make sure class is well packed
	assert (sizeof (rai::block_info) == sizeof (account) + sizeof (balance));
	return sizeof (rai::block_info);
}

rai::mdb_val rai::block_info::serialize_to_db () const
{
	auto size (size_in_db ());
	assert (size == sizeof (*this));
	return rai::mdb_val (size, const_cast<rai::block_info *> (this));
}

void rai::block_info::deserialize_from_db (rai::mdb_val const & val_a)
{
	auto size (size_in_db ());
	assert (val_a.value.mv_size == size);
	std::copy (reinterpret_cast<uint8_t const *> (val_a.value.mv_data), reinterpret_cast<uint8_t const *> (val_a.value.mv_data) + size, reinterpret_cast<uint8_t *> (this));
}

bool rai::block_info::operator== (rai::block_info const & other_a) const
{
	return account == other_a.account && balance == other_a.balance;
}

bool rai::vote::operator== (rai::vote const & other_a) const
{
	auto blocks_equal (true);
	if (blocks.size () != other_a.blocks.size ())
	{
		blocks_equal = false;
	}
	else
	{
		for (auto i (0); blocks_equal && i < blocks.size (); ++i)
		{
			auto block (blocks[i]);
			auto other_block (other_a.blocks[i]);
			if (block.which () != other_block.which ())
			{
				blocks_equal = false;
			}
			else if (block.which ())
			{
				if (boost::get<rai::block_hash> (block) != boost::get<rai::block_hash> (other_block))
				{
					blocks_equal = false;
				}
			}
			else
			{
				if (!(*boost::get<std::shared_ptr<rai::block>> (block) == *boost::get<std::shared_ptr<rai::block>> (other_block)))
				{
					blocks_equal = false;
				}
			}
		}
	}
	return sequence == other_a.sequence && blocks_equal && account == other_a.account && signature == other_a.signature;
}

bool rai::vote::operator!= (rai::vote const & other_a) const
{
	return !(*this == other_a);
}

std::string rai::vote::to_json () const
{
	std::stringstream stream;
	boost::property_tree::ptree tree;
	tree.put ("account", account.to_account ());
	tree.put ("signature", signature.number ());
	tree.put ("sequence", std::to_string (sequence));
	boost::property_tree::ptree blocks_tree;
	for (auto block : blocks)
	{
		if (block.which ())
		{
			blocks_tree.put ("", boost::get<std::shared_ptr<rai::block>> (block)->to_json ());
		}
		else
		{
			blocks_tree.put ("", boost::get<std::shared_ptr<rai::block>> (block)->hash ().to_string ());
		}
	}
	tree.add_child ("blocks", blocks_tree);
	boost::property_tree::write_json (stream, tree);
	return stream.str ();
}

rai::vote::vote (rai::vote const & other_a) :
sequence (other_a.sequence),
blocks (other_a.blocks),
account (other_a.account),
signature (other_a.signature)
{
}

rai::vote::vote (bool & error_a, rai::stream & stream_a)
{
	error_a = deserialize (stream_a);
}

rai::vote::vote (bool & error_a, rai::stream & stream_a, rai::block_type type_a)
{
	if (!error_a)
	{
		error_a = rai::read (stream_a, account.bytes);
		if (!error_a)
		{
			error_a = rai::read (stream_a, signature.bytes);
			if (!error_a)
			{
				error_a = rai::read (stream_a, sequence);
				if (!error_a)
				{
					while (!error_a && stream_a.in_avail () > 0)
					{
						if (type_a == rai::block_type::not_a_block)
						{
							rai::block_hash block_hash;
							error_a = rai::read (stream_a, block_hash);
							if (!error_a)
							{
								blocks.push_back (block_hash);
							}
						}
						else
						{
							std::shared_ptr<rai::block> block (rai::deserialize_block (stream_a, type_a));
							error_a = block == nullptr;
							if (!error_a)
							{
								blocks.push_back (block);
							}
						}
					}
					if (blocks.empty ())
					{
						error_a = true;
					}
				}
			}
		}
	}
}

rai::vote::vote (rai::account const & account_a, rai::raw_key const & prv_a, uint64_t sequence_a, std::shared_ptr<rai::block> block_a) :
sequence (sequence_a),
blocks (1, block_a),
account (account_a),
signature (rai::sign_message (prv_a, account_a, hash ()))
{
}

rai::vote::vote (rai::account const & account_a, rai::raw_key const & prv_a, uint64_t sequence_a, std::vector<rai::block_hash> blocks_a) :
sequence (sequence_a),
account (account_a)
{
	assert (blocks_a.size () > 0);
	for (auto hash : blocks_a)
	{
		blocks.push_back (hash);
	}
	signature = rai::sign_message (prv_a, account_a, hash ());
}

rai::vote::vote (MDB_val const & value_a)
{
	rai::bufferstream stream (reinterpret_cast<uint8_t const *> (value_a.mv_data), value_a.mv_size);
	assert (!deserialize (stream));
}

std::string rai::vote::hashes_string () const
{
	std::string result;
	for (auto hash : *this)
	{
		result += hash.to_string ();
		result += ", ";
	}
	return result;
}

const std::string rai::vote::hash_prefix = "vote ";

rai::uint256_union rai::vote::hash () const
{
	rai::uint256_union result;
	blake2b_state hash;
	blake2b_init (&hash, sizeof (result.bytes));
	if (blocks.size () > 1 || (blocks.size () > 0 && blocks[0].which ()))
	{
		blake2b_update (&hash, hash_prefix.data (), hash_prefix.size ());
	}
	for (auto block_hash : *this)
	{
		blake2b_update (&hash, block_hash.bytes.data (), sizeof (block_hash.bytes));
	}
	union
	{
		uint64_t qword;
		std::array<uint8_t, 8> bytes;
	};
	qword = sequence;
	blake2b_update (&hash, bytes.data (), sizeof (bytes));
	blake2b_final (&hash, result.bytes.data (), sizeof (result.bytes));
	return result;
}

void rai::vote::serialize (rai::stream & stream_a, rai::block_type type)
{
	write (stream_a, account);
	write (stream_a, signature);
	write (stream_a, sequence);
	for (auto block : blocks)
	{
		if (block.which ())
		{
			assert (type == rai::block_type::not_a_block);
			write (stream_a, boost::get<rai::block_hash> (block));
		}
		else
		{
			if (type == rai::block_type::not_a_block)
			{
				write (stream_a, boost::get<std::shared_ptr<rai::block>> (block)->hash ());
			}
			else
			{
				boost::get<std::shared_ptr<rai::block>> (block)->serialize (stream_a);
			}
		}
	}
}

void rai::vote::serialize (rai::stream & stream_a)
{
	write (stream_a, account);
	write (stream_a, signature);
	write (stream_a, sequence);
	for (auto block : blocks)
	{
		if (block.which ())
		{
			write (stream_a, rai::block_type::not_a_block);
			write (stream_a, boost::get<rai::block_hash> (block));
		}
		else
		{
			rai::serialize_block (stream_a, *boost::get<std::shared_ptr<rai::block>> (block));
		}
	}
}

bool rai::vote::deserialize (rai::stream & stream_a)
{
	auto result (read (stream_a, account));
	if (!result)
	{
		result = read (stream_a, signature);
		if (!result)
		{
			result = read (stream_a, sequence);
			if (!result)
			{
				rai::block_type type;
				while (!result)
				{
					if (rai::read (stream_a, type))
					{
						if (blocks.empty ())
						{
							result = true;
						}
						break;
					}
					if (!result)
					{
						if (type == rai::block_type::not_a_block)
						{
							rai::block_hash block_hash;
							result = rai::read (stream_a, block_hash);
							if (!result)
							{
								blocks.push_back (block_hash);
							}
						}
						else
						{
							std::shared_ptr<rai::block> block (rai::deserialize_block (stream_a, type));
							result = block == nullptr;
							if (!result)
							{
								blocks.push_back (block);
							}
						}
					}
				}
			}
		}
	}
	return result;
}

bool rai::vote::validate ()
{
	auto result (rai::validate_message (account, hash (), signature));
	return result;
}

rai::block_hash rai::iterate_vote_blocks_as_hash::operator() (boost::variant<std::shared_ptr<rai::block>, rai::block_hash> const & item) const
{
	rai::block_hash result;
	if (item.which ())
	{
		result = boost::get<rai::block_hash> (item);
	}
	else
	{
		result = boost::get<std::shared_ptr<rai::block>> (item)->hash ();
	}
	return result;
}

boost::transform_iterator<rai::iterate_vote_blocks_as_hash, rai::vote_blocks_vec_iter> rai::vote::begin () const
{
	return boost::transform_iterator<rai::iterate_vote_blocks_as_hash, rai::vote_blocks_vec_iter> (blocks.begin (), rai::iterate_vote_blocks_as_hash ());
}

boost::transform_iterator<rai::iterate_vote_blocks_as_hash, rai::vote_blocks_vec_iter> rai::vote::end () const
{
	return boost::transform_iterator<rai::iterate_vote_blocks_as_hash, rai::vote_blocks_vec_iter> (blocks.end (), rai::iterate_vote_blocks_as_hash ());
}

rai::genesis::genesis ()
{
	boost::property_tree::ptree tree;
	std::string block_json (rai::genesis_block);
	std::stringstream istream (block_json);
	boost::property_tree::read_json (istream, tree);
	auto block (rai::deserialize_block_json (tree));
	assert (dynamic_cast<rai::state_block *> (block.get ()) != nullptr);
	genesis_block.reset (static_cast<rai::state_block *> (block.release ()));
	assert (genesis_block->is_valid_open_subtype ());
}

void rai::genesis::initialize (MDB_txn * transaction_a, rai::block_store & store_a) const
{
	auto hash_l (hash ());
	assert (store_a.latest_begin (transaction_a) == store_a.latest_end ());
	store_a.block_put (transaction_a, hash_l, *genesis_block, rai::block_hash (0));
	store_a.account_put (transaction_a, genesis_account, { hash_l, genesis_block->hash (), genesis_block->hash (), genesis_block->balance (), genesis_block->creation_time ().number (), 1 });
	store_a.representation_put (transaction_a, genesis_account, genesis_block->balance ().number ());
	store_a.checksum_put (transaction_a, 0, 0, hash_l);
	store_a.frontier_put (transaction_a, hash_l, genesis_account);
}

rai::block_hash rai::genesis::hash () const
{
	return genesis_block->hash ();
}

rai::state_block const & rai::genesis::block () const
{
	return *genesis_block;
}

rai::block_hash rai::genesis::root () const
{
	return genesis_block->root ();
}

uint32_t rai::manna_control::manna_start = rai::genesis_time;
uint32_t rai::manna_control::manna_freq = 
	rai::rai_network == rai::rai_networks::rai_test_network ? test_manna_freq :
	rai::rai_network == rai::rai_networks::rai_beta_network ? beta_manna_freq :
	live_manna_freq;
rai::amount_t rai::manna_control::manna_increment =
	rai::rai_network == rai::rai_networks::rai_test_network ? test_manna_increment :
	rai::rai_network == rai::rai_networks::rai_beta_network ? beta_manna_increment :
	live_manna_increment;

bool rai::manna_control::is_manna_account (rai::account const & account_a)
{
	if (account_a == rai::manna_account)
	{
		return true;
	}
	return false;
}

rai::amount_t rai::manna_control::adjust_balance_with_manna (rai::amount_t orig_balance, rai::timestamp_t from, rai::timestamp_t to)
{
	if (from <= to)
	{
		rai::amount_t manna_increment = compute_manna_increment (from, to);
		// possible overflow in the far future
		return orig_balance + manna_increment;
	}
	// reverse order (e.g. due to out-of-sync clocks)
	rai::amount_t manna_decrement = compute_manna_increment (to, from);
	if (manna_decrement < orig_balance)
	{
		return orig_balance - manna_decrement;
	}
	return 0; // prevent underflow
}

rai::amount_t rai::manna_control::compute_manna_increment (rai::timestamp_t from, rai::timestamp_t to)
{
	assert (from <= to);
	if (from >= to) return 0;
	if (from < manna_start)
	{
		from = manna_start; // no change before manna_start
	}
	uint32_t t1 = (uint32_t) (from / manna_freq);
	uint32_t t2 = (uint32_t) (to / manna_freq);
	return (rai::amount_t) (t2 - t1) * (rai::amount_t) manna_increment;
}
