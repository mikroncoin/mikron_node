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
const char const * test_genesis_private_key_data = "6EBA231F6BDCDA9B67F26CAE66CEF4EE6922F42B37EEDD76D485B5F5A3BC8AA9";
const char const * test_genesis_public_key_data = "96F167643B3DEB3B614003E432F33CCFB0C3E64866DA99ACB484F2B13E3E1980"; // mik_37qjexk5phhd9fin11z68dsmsmxirhm6isptm8pdb39kp6z5w8e1534tigqk
const char const * beta_genesis_public_key_data = "F9D81CD1BBD9439B609E8F2C5D33893E02BE274FAAE899B57A87034DC9542F8C"; // mik_3ygr5mauqpc5mfibx5sednsrkhi4qrmnzcqam8tqo3r5bq6oadwe9prikbt9
const char const * live_genesis_public_key_data = "829DD4F441C7CC5EF6572F11CE00A3F5264EFF25AA6D080507A4D1A8B47E2353"; // Non-final mik_31nxtmt65jyeduu7gdrjsr1c9xb8buzkdcmf314ihb8jo4t9watm86g1fke6
const rai::uint128_t test_genesis_amount = std::numeric_limits<rai::uint128_t>::max ();
const rai::uint128_t beta_genesis_amount = (rai::uint128_t)300000000 * (rai::uint128_t)10000000000 * (rai::uint128_t)10000000000 * (rai::uint128_t)10000000000;
const rai::uint128_t live_genesis_amount = (rai::uint128_t)300000000 * (rai::uint128_t)10000000000 * (rai::uint128_t)10000000000 * (rai::uint128_t)10000000000;
const uint32_t test_genesis_time = 2592000;  // 2018.10.01.  1538352000 - short_timestamp_epoch = 1538352000 - 1535760000 = 2592000
const uint32_t beta_genesis_time = 2592000;  // 2018.10.01.  1538352000 - short_timestamp_epoch = 1538352000 - 1535760000 = 2592000
const uint32_t live_genesis_time = 2592000;  // 2018.10.01.  1538352000 - short_timestamp_epoch = 1538352000 - 1535760000 = 2592000
const char const * test_manna_private_key_data = "AB02030F53BA4527D84859DBFF13DF0A17B74706682D3621D6C8DB0912424D3D";
const char const * test_manna_public_key_data = "A8EC25B743412E09567C3363A11C0D5F5722F26236020D7BF93C9F4E0D161583"; // mik_3c9e6pun8ibg37d9reu5n6g1tqtq6ds86fi43oxzkh6zbr8je7e5eejg5r9a
const char const * beta_manna_public_key_data = beta_genesis_public_key_data;
const char const * live_manna_public_key_data = live_genesis_public_key_data;
const uint32_t test_manna_freq = 4;
const uint32_t beta_manna_freq = 60;
const uint32_t live_manna_freq = 86400;  // 1 day
const rai::uint128_t test_manna_increment = 1000;
const rai::uint128_t beta_manna_increment = (rai::uint128_t)50 * (rai::uint128_t)10000000000 * (rai::uint128_t)10000000000 * (rai::uint128_t)10000000000;
const rai::uint128_t live_manna_increment = (rai::uint128_t)82000 * (rai::uint128_t)10000000000 * (rai::uint128_t)10000000000 * (rai::uint128_t)10000000000;  // Non-final

char const * test_genesis_data = R"%%%({
	"type": "state",
	"account": "mik_37qjexk5phhd9fin11z68dsmsmxirhm6isptm8pdb39kp6z5w8e1534tigqk",
	"creation_time": "{CREATION_TIME_PLACEHOLDER}",
	"previous": "0000000000000000000000000000000000000000000000000000000000000000",
	"representative": "mik_37qjexk5phhd9fin11z68dsmsmxirhm6isptm8pdb39kp6z5w8e1534tigqk",
	"balance": "340282366920938463463374607431768211455",
	"link": "0000000000000000000000000000000000000000000000000000000000000000",
	"work": "d0c54a27352bf9b1",
	"signature": "59D0ED5D0A7243BBB24EDE9FFE76E8F7548E406E66CCD65A88FED1B1E423CE669DF5A8CDCE60A41A70B547C3711CA70A50755478C07EFC890B096932CC4D8F09"
})%%%";

char const * test_genesis_legacy_data = R"%%%({
	"type": "open",
	"source": "96F167643B3DEB3B614003E432F33CCFB0C3E64866DA99ACB484F2B13E3E1980",
	"representative": "mik_37qjexk5phhd9fin11z68dsmsmxirhm6isptm8pdb39kp6z5w8e1534tigqk",
	"account": "mik_37qjexk5phhd9fin11z68dsmsmxirhm6isptm8pdb39kp6z5w8e1534tigqk",
	"work": "2fa557bd2cec318e",
	"signature": "863BBC1C4686C8A1F4C17997ABF6C18A655A82A5272D692CA0889140B3F8250666A6161D2900756F206414DA7820C591B8124833F785E6AA7FE1258A840F7D00"
})%%%";

char const * beta_genesis_data = R"%%%({
	"type": "state",
	"account": "mik_3ygr5mauqpc5mfibx5sednsrkhi4qrmnzcqam8tqo3r5bq6oadwe9prikbt9",
	"creation_time": "{CREATION_TIME_PLACEHOLDER}",
	"previous": "0000000000000000000000000000000000000000000000000000000000000000",
	"representative": "mik_3ygr5mauqpc5mfibx5sednsrkhi4qrmnzcqam8tqo3r5bq6oadwe9prikbt9",
	"balance": "300000000000000000000000000000000000000",
	"link": "0000000000000000000000000000000000000000000000000000000000000000",
	"work": "8374208633b49332",
	"signature": "FE6AE6DEA56D2DE5706101BBDA6CF6261657632E4BE0FAD5537BF32DDCF0D1AD189613876CCDD817DA62731E748E6BB3D1B630673EC0697DB7AE9745BEDF1F08"
})%%%";

// Non-final
char const * live_genesis_data = R"%%%({
	"type": "state",
	"account": "mik_31nxtmt65jyeduu7gdrjsr1c9xb8buzkdcmf314ihb8jo4t9watm86g1fke6",
	"creation_time": "{CREATION_TIME_PLACEHOLDER}",
	"previous": "0000000000000000000000000000000000000000000000000000000000000000",
	"representative": "mik_31nxtmt65jyeduu7gdrjsr1c9xb8buzkdcmf314ihb8jo4t9watm86g1fke6",
	"balance": "300000000000000000000000000000000000000",
	"link": "0000000000000000000000000000000000000000000000000000000000000000",
	"work": "744f96cdce4c96a8",
	"signature": "9EDBC55F4EEA81D5748B9C31FA7269DC1C2C133BC603B85D51DE5EC25B1C56DA137E22FA6722C17658C02C67E9E8C5DC7F5F8DC591428416A8032CD99F0F2603"
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
	rai_test_genesis_legacy (test_genesis_legacy_data),
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
	std::string rai_test_genesis_legacy;
	rai::uint128_t rai_test_genesis_amount;
	rai::uint128_t rai_beta_genesis_amount;
	rai::uint128_t rai_live_genesis_amount;
	rai::keypair test_manna_key;
	rai::account genesis_account;
	std::string genesis_block;
	rai::uint128_t genesis_amount;
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

size_t constexpr rai::send_block::size;
size_t constexpr rai::receive_block::size;
size_t constexpr rai::open_block::size;
size_t constexpr rai::change_block::size;
size_t constexpr rai::state_block::size;

rai::keypair const & rai::zero_key (globals.zero_key);
rai::keypair const & rai::test_genesis_key (globals.test_genesis_key);
rai::account const & rai::rai_test_genesis_account (globals.rai_test_genesis_account);
rai::account const & rai::rai_beta_genesis_account (globals.rai_beta_genesis_account);
rai::account const & rai::rai_live_genesis_account (globals.rai_live_genesis_account);
std::string const & rai::rai_test_genesis (globals.rai_test_genesis);
std::string const & rai::rai_beta_genesis (globals.rai_beta_genesis);
std::string const & rai::rai_live_genesis (globals.rai_live_genesis);
std::string const & rai::rai_test_genesis_legacy (globals.rai_test_genesis_legacy);
rai::keypair const & rai::test_manna_key (globals.test_manna_key);

rai::account const & rai::genesis_account (globals.genesis_account);
std::string const & rai::genesis_block (globals.genesis_block);
rai::uint128_t const & rai::genesis_amount (globals.genesis_amount);
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
open_block (0),
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

/*
void rai::account_info::serialize (rai::stream & stream_a) const
{
	write (stream_a, head.bytes);
	write (stream_a, rep_block.bytes);
	write (stream_a, open_block.bytes);
	write (stream_a, balance.bytes);
	write (stream_a, last_block_time_intern);
	write (stream_a, block_count);
}

bool rai::account_info::deserialize (rai::stream & stream_a)
{
	auto error (read (stream_a, head.bytes));
	if (!error)
	{
		error = read (stream_a, rep_block.bytes);
		if (!error)
		{
			error = read (stream_a, open_block.bytes);
			if (!error)
			{
				error = read (stream_a, balance.bytes);
				if (!error)
				{
					error = read (stream_a, modified);
					if (!error)
					{
						error = read (stream_a, block_count);
					}
				}
			}
		}
	}
	return error;
}
*/

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
	rai::uint128_t balance1 = balance.number ();
	rai::timestamp_t now = now_a;
	if (now == 0)
	{
		now = rai::short_timestamp::now ();
	}
	rai::uint128_t balance2 = rai::manna_control::adjust_balance_with_manna (balance1, last_block_time (), now);
	return balance2;
}

size_t rai::account_info::db_size () const
{
	// make sure class is well packed
	assert (sizeof (rai::account_info) == sizeof (head) + sizeof (rep_block) + sizeof (open_block) + sizeof (balance) + sizeof (last_block_time_intern) + sizeof (block_count));
	return sizeof (rai::account_info);
}

rai::mdb_val rai::account_info::serialize_to_db () const
{
	auto size (db_size ());
	assert (size == sizeof (*this));
	return rai::mdb_val (size, const_cast<rai::account_info *> (this));
}

void rai::account_info::deserialize_from_db (rai::mdb_val const & val_a)
{
	auto size (db_size());
	assert (val_a.value.mv_size == size);
	std::copy (reinterpret_cast<uint8_t const *> (val_a.value.mv_data), reinterpret_cast<uint8_t const *> (val_a.value.mv_data) + size, reinterpret_cast<uint8_t *> (this));
}

rai::block_counts::block_counts () :
send (0),
receive (0),
open (0),
change (0),
state (0)
{
}

size_t rai::block_counts::sum ()
{
	return send + receive + open + change + state;
}

rai::pending_info::pending_info () :
source (0),
amount (0)
{
}

rai::pending_info::pending_info (rai::mdb_val const & val_a)
{
	auto db_size (sizeof (source) + sizeof (amount));
	assert (val_a.value.mv_size == db_size);
	assert (reinterpret_cast<const uint8_t *> (this) == reinterpret_cast<const uint8_t *> (&source));
	assert (reinterpret_cast<const uint8_t *> (&source) + sizeof (source) == reinterpret_cast<const uint8_t *> (&amount));
	std::copy (reinterpret_cast<uint8_t const *> (val_a.value.mv_data), reinterpret_cast<uint8_t const *> (val_a.value.mv_data) + db_size, reinterpret_cast<uint8_t *> (this));
}

rai::pending_info::pending_info (rai::account const & source_a, rai::amount const & amount_a) :
source (source_a),
amount (amount_a)
{
}

void rai::pending_info::serialize (rai::stream & stream_a) const
{
	rai::write (stream_a, source.bytes);
	rai::write (stream_a, amount.bytes);
}

bool rai::pending_info::deserialize (rai::stream & stream_a)
{
	auto result (rai::read (stream_a, source.bytes));
	if (!result)
	{
		result = rai::read (stream_a, amount.bytes);
	}
	return result;
}

bool rai::pending_info::operator== (rai::pending_info const & other_a) const
{
	return source == other_a.source && amount == other_a.amount;
}

rai::mdb_val rai::pending_info::val () const
{
	assert (reinterpret_cast<const uint8_t *> (this) == reinterpret_cast<const uint8_t *> (&source));
	assert (reinterpret_cast<const uint8_t *> (this) + sizeof (source) == reinterpret_cast<const uint8_t *> (&amount));
	return rai::mdb_val (sizeof (source) + sizeof (amount), const_cast<rai::pending_info *> (this));
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

rai::block_info::block_info (MDB_val const & val_a)
{
	assert (val_a.mv_size == sizeof (*this));
	static_assert (sizeof (account) + sizeof (balance) == sizeof (*this), "Packed class");
	std::copy (reinterpret_cast<uint8_t const *> (val_a.mv_data), reinterpret_cast<uint8_t const *> (val_a.mv_data) + sizeof (*this), reinterpret_cast<uint8_t *> (this));
}

rai::block_info::block_info (rai::account const & account_a, rai::amount const & balance_a) :
account (account_a),
balance (balance_a)
{
}

void rai::block_info::serialize (rai::stream & stream_a) const
{
	rai::write (stream_a, account.bytes);
	rai::write (stream_a, balance.bytes);
}

bool rai::block_info::deserialize (rai::stream & stream_a)
{
	auto error (rai::read (stream_a, account.bytes));
	if (!error)
	{
		error = rai::read (stream_a, balance.bytes);
	}
	return error;
}

bool rai::block_info::operator== (rai::block_info const & other_a) const
{
	return account == other_a.account && balance == other_a.balance;
}

rai::mdb_val rai::block_info::val () const
{
	return rai::mdb_val (sizeof (*this), const_cast<rai::block_info *> (this));
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

rai::amount_visitor::amount_visitor (MDB_txn * transaction_a, rai::block_store & store_a) :
transaction (transaction_a),
store (store_a),
current_amount (0),
current_balance (0),
amount (0)
{
}

void rai::amount_visitor::send_block (rai::send_block const & block_a)
{
	current_balance = block_a.hashables.previous;
	amount = block_a.hashables.balance.number ();
	current_amount = 0;
}

void rai::amount_visitor::receive_block (rai::receive_block const & block_a)
{
	current_amount = block_a.hashables.source;
}

void rai::amount_visitor::open_block (rai::open_block const & block_a)
{
	if (block_a.hashables.source != rai::genesis_account)
	{
		current_amount = block_a.hashables.source;
	}
	else
	{
		amount = rai::genesis_amount;
		current_amount = 0;
	}
}

void rai::amount_visitor::state_block (rai::state_block const & block_a)
{
	current_balance = block_a.hashables.previous;
	amount = block_a.hashables.balance.number ();
	current_amount = 0;
}

void rai::amount_visitor::change_block (rai::change_block const & block_a)
{
	amount = 0;
	current_amount = 0;
}

void rai::amount_visitor::compute (rai::block_hash const & block_hash)
{
	current_amount = block_hash;
	while (!current_amount.is_zero () || !current_balance.is_zero ())
	{
		if (!current_amount.is_zero ())
		{
			auto block (store.block_get (transaction, current_amount));
			if (block != nullptr)
			{
				block->visit (*this);
			}
			else
			{
				if (block_hash == rai::genesis_account)
				{
					amount = rai::genesis_amount;
					current_amount = 0;
				}
				else
				{
					assert (false);
					amount = 0;
					current_amount = 0;
				}
			}
		}
		else
		{
			balance_visitor prev (transaction, store);
			prev.compute (current_balance);
			rai::uint128_t previous_balance = prev.balance;
			assert (prev.balance_block != nullptr);
			if (prev.balance_block != nullptr && rai::manna_control::is_manna_account (prev.balance_block->hashables.account))
			{ 
				// manna adjustment
				auto block (store.block_get (transaction, block_hash));
				assert (block != nullptr);
				if (block != nullptr)
				{
					previous_balance = rai::manna_control::adjust_balance_with_manna (previous_balance, prev.balance_block->creation_time().number (), block->creation_time ().number ());
				}
			}
			amount = amount < previous_balance ? previous_balance - amount : amount - previous_balance;
			current_balance = 0;
		}
	}
}

rai::balance_visitor::balance_visitor (MDB_txn * transaction_a, rai::block_store & store_a) :
transaction (transaction_a),
store (store_a),
current_balance (0),
current_amount (0),
balance (0),
balance_block (nullptr)
{
}

void rai::balance_visitor::send_block (rai::send_block const & block_a)
{
	balance += block_a.hashables.balance.number ();
	current_balance = 0;
}

void rai::balance_visitor::receive_block (rai::receive_block const & block_a)
{
	rai::block_info block_info;
	if (!store.block_info_get (transaction, block_a.hash (), block_info))
	{
		balance += block_info.balance.number ();
		current_balance = 0;
	}
	else
	{
		current_amount = block_a.hashables.source;
		current_balance = block_a.hashables.previous;
	}
}

void rai::balance_visitor::open_block (rai::open_block const & block_a)
{
	current_amount = block_a.hashables.source;
	current_balance = 0;
}

void rai::balance_visitor::change_block (rai::change_block const & block_a)
{
	rai::block_info block_info;
	if (!store.block_info_get (transaction, block_a.hash (), block_info))
	{
		balance += block_info.balance.number ();
		current_balance = 0;
	}
	else
	{
		current_balance = block_a.hashables.previous;
	}
}

void rai::balance_visitor::state_block (rai::state_block const & block_a)
{
	balance = block_a.hashables.balance.number ();
	balance_block = std::make_shared<rai::state_block> (block_a);
	current_balance = 0;
}

void rai::balance_visitor::compute (rai::block_hash const & block_hash)
{
	current_balance = block_hash;
	while (!current_balance.is_zero () || !current_amount.is_zero ())
	{
		if (!current_amount.is_zero ())
		{
			assert (false);
			balance = 0;
			current_balance = 0;
			current_amount = 0;
			//amount_visitor source (transaction, store);
			//source.compute (current_amount);
			//balance += source.amount;
			//current_amount = 0;
		}
		else
		{
			auto block (store.block_get (transaction, current_balance));
			assert (block != nullptr);
			block->visit (*this);
		}
	}
}

rai::representative_visitor::representative_visitor (MDB_txn * transaction_a, rai::block_store & store_a) :
transaction (transaction_a),
store (store_a),
result (0)
{
}

void rai::representative_visitor::compute (rai::block_hash const & hash_a)
{
	current = hash_a;
	while (result.is_zero ())
	{
		auto block (store.block_get (transaction, current));
		assert (block != nullptr);
		block->visit (*this);
	}
}

void rai::representative_visitor::send_block (rai::send_block const & block_a)
{
	current = block_a.previous ();
}

void rai::representative_visitor::receive_block (rai::receive_block const & block_a)
{
	current = block_a.previous ();
}

void rai::representative_visitor::open_block (rai::open_block const & block_a)
{
	result = block_a.hash ();
}

void rai::representative_visitor::change_block (rai::change_block const & block_a)
{
	result = block_a.hash ();
}

void rai::representative_visitor::state_block (rai::state_block const & block_a)
{
	result = block_a.hash ();
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
	std::string block_json2 = boost::replace_all_copy (block_json, "{CREATION_TIME_PLACEHOLDER}", std::to_string (rai::genesis_time));
	std::stringstream istream (block_json2);
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
	store_a.account_put (transaction_a, genesis_account, { hash_l, genesis_block->hash (), genesis_block->hash (), genesis_block->hashables.balance, genesis_block->creation_time ().number (), 1 });
	store_a.representation_put (transaction_a, genesis_account, genesis_block->hashables.balance.number ());
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

rai::genesis_legacy_with_open::genesis_legacy_with_open()
{
	boost::property_tree::ptree tree;
	std::stringstream istream(rai::rai_test_genesis_legacy);
	boost::property_tree::read_json(istream, tree);
	auto block(rai::deserialize_block_json(tree));
	assert (dynamic_cast<rai::open_block *> (block.get ()) != nullptr);
	genesis_block.reset (static_cast<rai::open_block *> (block.release()));
}

void rai::genesis_legacy_with_open::initialize(MDB_txn * transaction_a, rai::block_store & store_a) const
{
	auto hash_l (hash ());
	assert (store_a.latest_begin(transaction_a) == store_a.latest_end());
	store_a.block_put (transaction_a, hash_l, *genesis_block);
	store_a.account_put (transaction_a, genesis_account, { hash_l, genesis_block->hash (), genesis_block->hash (), rai::genesis_amount, genesis_block->creation_time ().number (), 1 });
	store_a.representation_put (transaction_a, genesis_account, rai::genesis_amount);
	store_a.checksum_put (transaction_a, 0, 0, hash_l);
	store_a.frontier_put (transaction_a, hash_l, genesis_account);
}

rai::block_hash rai::genesis_legacy_with_open::hash () const
{
	return genesis_block->hash ();
}

uint32_t rai::manna_control::manna_start = rai::genesis_time;
uint32_t rai::manna_control::manna_freq = 
	rai::rai_network == rai::rai_networks::rai_test_network ? test_manna_freq :
	rai::rai_network == rai::rai_networks::rai_beta_network ? beta_manna_freq :
	live_manna_freq;
rai::uint128_t rai::manna_control::manna_increment =
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

rai::uint128_t rai::manna_control::adjust_balance_with_manna (rai::uint128_t orig_balance, rai::timestamp_t from, rai::timestamp_t to)
{
	if (from <= to)
	{
		rai::uint128_t manna_increment = compute_manna_increment (from, to);
		// possible overflow in the far future
		return orig_balance + manna_increment;
	}
	// reverse order (e.g. due to out-of-sync clocks)
	rai::uint128_t manna_decrement = compute_manna_increment (to, from);
	if (manna_decrement < orig_balance)
	{
		return orig_balance - manna_decrement;
	}
	return 0;  // prevent underflow
}

rai::uint128_t rai::manna_control::compute_manna_increment (rai::timestamp_t from, rai::timestamp_t to)
{
	assert (from <= to);
	if (from >= to) return 0;
	if (from < manna_start) from = manna_start;  // no change before manna_start
	uint32_t t1 = (uint32_t) (from / manna_freq);
	uint32_t t2 = (uint32_t) (to / manna_freq);
	return (rai::uint128_t) (t2 - t1) * (rai::uint128_t) manna_increment;
}
