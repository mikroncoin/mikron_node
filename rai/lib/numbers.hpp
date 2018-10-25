#pragma once

#include <boost/multiprecision/cpp_int.hpp>

#include <cryptopp/osrng.h>

namespace rai
{
// Random pool used by RaiBlocks.
// This must be thread_local as long as the AutoSeededRandomPool implementation requires it
extern thread_local CryptoPP::AutoSeededRandomPool random_pool;

using uint32_t = ::uint32_t;
using uint64_t = ::uint64_t;
using uint128_t = boost::multiprecision::uint128_t;
using uint256_t = boost::multiprecision::uint256_t;
using uint512_t = boost::multiprecision::uint512_t;

// Used for timestamp
struct uint32_struct
{
public:
	uint32_struct () = default;
	uint32_struct (uint32_t);
	uint32_struct (std::string const &);
	bool operator== (rai::uint32_struct const &) const;
	void encode_dec (std::string &) const;
	bool decode_dec (std::string const &);
	std::string to_string_dec () const;
	bool is_zero () const;
	void clear ();
	uint32_t number () const;
	// wrapped storage is a native int type, not an always-big-endian byte array
	::uint32_t data;
};

// Used for amounts
struct uint64_struct
{
public:
	uint64_struct () = default;
	uint64_struct (std::string const &);
	uint64_struct (rai::uint64_t);
	uint64_struct (rai::uint64_struct const &) = default;
	//uint64_struct (rai::uint64_t const &);
	bool operator== (rai::uint64_struct const &) const;
	bool operator!= (rai::uint64_struct const &) const;
	bool operator< (rai::uint64_struct const &) const;
	bool operator> (rai::uint64_struct const &) const;
	void encode_hex (std::string &) const;
	bool decode_hex (std::string const &);
	void encode_dec (std::string &) const;
	bool decode_dec (std::string const &);
	std::string format_balance (rai::uint64_t scale, int precision, bool group_digits);
	std::string format_balance (rai::uint64_t scale, int precision, bool group_digits, const std::locale & locale);
	rai::uint64_t number () const;
	void clear ();
	bool is_zero () const;
	std::string to_string () const;
	std::string to_string_dec () const;
	// wrapped storage is a native int type, not an always-big-endian byte array
	::uint64_t data;
};

// Balances are 128 bit.
using amount_t = ::uint64_t;
using amount = uint64_struct;

// SI dividers
rai::amount_t const Gxrb_ratio = rai::amount_t(10000000000000); // 10^13  // TODO test-only
rai::amount_t const Mxrb_ratio = rai::amount_t(10000000000); // 10^10
rai::amount_t const xrb_ratio = rai::amount_t(10000); // 10^4

// Used in keys
union uint128_struct
{
public:
	uint128_struct () = default;
	//uint128_struct (std::string const &);
	//uint128_struct (uint64_t);
	//uint128_struct (rai::uint128_struct const &) = default;
	uint128_struct (rai::uint128_t const &);
	//bool operator== (rai::uint128_struct const &) const;
	//bool operator!= (rai::uint128_struct const &) const;
	//bool operator< (rai::uint128_struct const &) const;
	//bool operator> (rai::uint128_struct const &) const;
	//void encode_hex (std::string &) const;
	//bool decode_hex (std::string const &);
	void encode_dec (std::string &) const;
	bool decode_dec (std::string const &);
	//std::string format_balance (rai::uint64_t scale, int precision, bool group_digits);
	//std::string format_balance (rai::uint64_t scale, int precision, bool group_digits, const std::locale & locale);
	rai::uint128_t number () const;
	//void clear ();
	//bool is_zero () const;
	//std::string to_string () const;
	std::string to_string_dec () const;
	std::array<uint8_t, 16> bytes;
	//std::array<char, 16> chars;
	//std::array<uint32_t, 4> dwords;
	//std::array<uint64_t, 2> qwords;
};

class raw_key;
union uint256_union
{
	uint256_union () = default;
	uint256_union (std::string const &);
	uint256_union (uint64_t);
	uint256_union (rai::uint256_t const &);
	void encrypt (rai::raw_key const &, rai::raw_key const &, uint128_struct const &);
	uint256_union & operator^= (rai::uint256_union const &);
	uint256_union operator^ (rai::uint256_union const &) const;
	bool operator== (rai::uint256_union const &) const;
	bool operator!= (rai::uint256_union const &) const;
	bool operator< (rai::uint256_union const &) const;
	void encode_hex (std::string &) const;
	bool decode_hex (std::string const &);
	void encode_dec (std::string &) const;
	bool decode_dec (std::string const &);
	void encode_account (std::string &) const;
	std::string to_account () const;
	std::string to_account_split () const;
	bool decode_account (std::string const &);
	std::array<uint8_t, 32> bytes;
	std::array<char, 32> chars;
	std::array<uint32_t, 8> dwords;
	std::array<uint64_t, 4> qwords;
	std::array<uint128_struct, 2> owords;
	void clear ();
	bool is_zero () const;
	std::string to_string () const;
	rai::uint256_t number () const;
};
// All keys and hashes are 256 bit.
using block_hash = uint256_union;
using account = uint256_union;
using public_key = uint256_union;
using private_key = uint256_union;
using secret_key = uint256_union;
using checksum = uint256_union;
class raw_key
{
public:
	raw_key () = default;
	~raw_key ();
	void decrypt (rai::uint256_union const &, rai::raw_key const &, uint128_struct const &);
	bool operator== (rai::raw_key const &) const;
	bool operator!= (rai::raw_key const &) const;
	rai::uint256_union data;
};
union uint512_union
{
	uint512_union () = default;
	uint512_union (rai::uint512_t const &);
	bool operator== (rai::uint512_union const &) const;
	bool operator!= (rai::uint512_union const &) const;
	rai::uint512_union & operator^= (rai::uint512_union const &);
	void encode_hex (std::string &) const;
	bool decode_hex (std::string const &);
	std::array<uint8_t, 64> bytes;
	std::array<uint32_t, 16> dwords;
	std::array<uint64_t, 8> qwords;
	std::array<uint256_union, 2> uint256s;
	void clear ();
	rai::uint512_t number () const;
	std::string to_string () const;
};
// Only signatures are 512 bit.
using signature = uint512_union;

rai::uint512_union sign_message (rai::raw_key const &, rai::public_key const &, rai::uint256_union const &);
bool validate_message (rai::public_key const &, rai::uint256_union const &, rai::uint512_union const &);
void deterministic_key (rai::uint256_union const &, uint32_t, rai::uint256_union &);
rai::public_key pub_key (rai::private_key const &);
}

namespace std
{
template <>
struct hash<rai::uint256_union>
{
	size_t operator() (rai::uint256_union const & data_a) const
	{
		return *reinterpret_cast<size_t const *> (data_a.bytes.data ());
	}
};
template <>
struct hash<rai::uint256_t>
{
	size_t operator() (rai::uint256_t const & number_a) const
	{
		return number_a.convert_to<size_t> ();
	}
};
}
