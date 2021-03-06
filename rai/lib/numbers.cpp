#include <rai/lib/numbers.hpp>
#include <rai/lib/blocks.hpp>

#include <rai/node/common.hpp>

#include <ed25519-donna/ed25519.h>

#include <blake2/blake2.h>

#include <cryptopp/aes.h>
#include <cryptopp/modes.h>

#include <boost/endian/conversion.hpp>

thread_local CryptoPP::AutoSeededRandomPool rai::random_pool;

namespace
{
char const * base58_reverse ("~012345678~~~~~~~9:;<=>?@~ABCDE~FGHIJKLMNOP~~~~~~QRSTUVWXYZ[~\\]^_`abcdefghi");
uint8_t base58_decode (char value)
{
	assert (value >= '0');
	assert (value <= '~');
	auto result (base58_reverse[value - 0x30] - 0x30);
	return result;
}
char const * account_lookup ("13456789abcdefghijkmnopqrstuwxyz");
char const * account_reverse ("~0~1234567~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~89:;<=>?@AB~CDEFGHIJK~LMNO~~~~~");
char account_encode (uint8_t value)
{
	assert (value < 32);
	auto result (account_lookup[value]);
	return result;
}
uint8_t account_decode (char value)
{
	assert (value >= '0');
	assert (value <= '~');
	auto result (account_reverse[value - 0x30] - 0x30);
	return result;
}
}

rai::uint32_struct::uint32_struct (std::string const & string_a)
{
	decode_dec (string_a);
}

rai::uint32_struct::uint32_struct (uint32_t value_a)
{
	data = value_a;
}

bool rai::uint32_struct::operator== (rai::uint32_struct const & other_a) const
{
	return data == other_a.data;
}

void rai::uint32_struct::encode_dec (std::string & text) const
{
	assert (text.empty ());
	std::stringstream stream;
	stream << std::dec << std::noshowbase;
	stream << data;
	text = stream.str ();
}

bool rai::uint32_struct::decode_dec (std::string const & text)
{
	auto error (text.size () > 10 || (text.size () > 1 && text[0] == '0') || (text.size () > 0 && text[0] == '-'));
	if (!error)
	{
		std::stringstream stream (text);
		stream << std::dec << std::noshowbase;
		uint32_t number_l;
		try
		{
			stream >> number_l;
			uint32_t unchecked (number_l);
			*this = unchecked;
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
	return error;
}

std::string rai::uint32_struct::to_string_dec () const
{
	std::string result;
	encode_dec (result);
	return result;
}

bool rai::uint32_struct::is_zero () const
{
	return (data == 0);
}

void rai::uint32_struct::clear ()
{
	data = 0;
}

uint32_t rai::uint32_struct::number () const
{
	return data;
}

uint32_t rai::uint32_struct::number_big_endian () const
{
	return boost::endian::native_to_big (data);
}

void rai::uint32_struct::serialize (rai::stream & stream_a) const
{
	write (stream_a, boost::endian::native_to_big (data));
}

bool rai::uint32_struct::deserialize (rai::stream & stream_a)
{
	auto error (rai::read (stream_a, data));
	if (error) return error;
	boost::endian::big_to_native_inplace (data);
	return false;
}

void rai::uint256_union::encode_account (std::string & destination_a) const
{
	assert (destination_a.empty ());
	destination_a.reserve (64);
	uint64_t check (0);
	blake2b_state hash;
	blake2b_init (&hash, 5);
	blake2b_update (&hash, bytes.data (), bytes.size ());
	blake2b_final (&hash, reinterpret_cast<uint8_t *> (&check), 5);
	rai::uint512_t number_l (number ());
	number_l <<= 40;
	number_l |= rai::uint512_t (check);
	for (auto i (0); i < 60; ++i)
	{
		uint8_t r (number_l & static_cast<uint8_t> (0x1f));
		number_l >>= 5;
		destination_a.push_back (account_encode (r));
	}
	destination_a.append ("_kim"); // "mik_"
	std::reverse (destination_a.begin (), destination_a.end ());
}

std::string rai::uint256_union::to_account_split () const
{
	auto result (to_account ());
	assert (result.size () == 64);
	result.insert (32, "\n");
	return result;
}

std::string rai::uint256_union::to_account () const
{
	std::string result;
	encode_account (result);
	return result;
}

bool rai::uint256_union::decode_account (std::string const & source_a)
{
	auto error (source_a.size () < 5);
	if (!error)
	{
		//auto xrb_prefix (source_a[0] == 'x' && source_a[1] == 'r' && source_a[2] == 'b' && (source_a[3] == '_' || source_a[3] == '-'));
		auto mik_prefix ((source_a[0] == 'm' || source_a[0] == 'M') && source_a[1] == 'i' && source_a[2] == 'k' && (source_a[3] == '_' || source_a[3] == '-'));
		error = (mik_prefix && source_a.size () != 64);
		if (!error)
		{
			if (mik_prefix)
			{
				auto i (source_a.begin () + 4);
				if (*i == '1' || *i == '3')
				{
					rai::uint512_t number_l;
					for (auto j (source_a.end ()); !error && i != j; ++i)
					{
						uint8_t character (*i);
						error = character < 0x30 || character >= 0x80;
						if (!error)
						{
							uint8_t byte (account_decode (character));
							error = byte == '~';
							if (!error)
							{
								number_l <<= 5;
								number_l += byte;
							}
						}
					}
					if (!error)
					{
						*this = (number_l >> 40).convert_to<rai::uint256_t> ();
						uint64_t check (number_l & static_cast<uint64_t> (0xffffffffff));
						uint64_t validation (0);
						blake2b_state hash;
						blake2b_init (&hash, 5);
						blake2b_update (&hash, bytes.data (), bytes.size ());
						blake2b_final (&hash, reinterpret_cast<uint8_t *> (&validation), 5);
						error = check != validation;
					}
				}
				else
				{
					error = true;
				}
			}
			else
			{
				error = true;
			}
		}
	}
	return error;
}

rai::uint256_union::uint256_union (rai::uint256_t const & number_a)
{
	rai::uint256_t number_l (number_a);
	for (auto i (bytes.rbegin ()), n (bytes.rend ()); i != n; ++i)
	{
		*i = static_cast<uint8_t> (number_l & static_cast<uint8_t> (0xff));
		number_l >>= 8;
	}
}

bool rai::uint256_union::operator== (rai::uint256_union const & other_a) const
{
	return bytes == other_a.bytes;
}

// Construct a uint256_union = AES_ENC_CTR (cleartext, key, iv)
void rai::uint256_union::encrypt (rai::raw_key const & cleartext, rai::raw_key const & key, uint128_struct const & iv)
{
	CryptoPP::AES::Encryption alg (key.data.bytes.data (), sizeof (key.data.bytes));
	CryptoPP::CTR_Mode_ExternalCipher::Encryption enc (alg, iv.bytes.data ());
	enc.ProcessData (bytes.data (), cleartext.data.bytes.data (), sizeof (cleartext.data.bytes));
}

bool rai::uint256_union::is_zero () const
{
	return qwords[0] == 0 && qwords[1] == 0 && qwords[2] == 0 && qwords[3] == 0;
}

std::string rai::uint256_union::to_string () const
{
	std::string result;
	encode_hex (result);
	return result;
}

bool rai::uint256_union::operator< (rai::uint256_union const & other_a) const
{
	return number () < other_a.number ();
}

rai::uint256_union & rai::uint256_union::operator^= (rai::uint256_union const & other_a)
{
	auto j (other_a.qwords.begin ());
	for (auto i (qwords.begin ()), n (qwords.end ()); i != n; ++i, ++j)
	{
		*i ^= *j;
	}
	return *this;
}

rai::uint256_union rai::uint256_union::operator^ (rai::uint256_union const & other_a) const
{
	rai::uint256_union result;
	auto k (result.qwords.begin ());
	for (auto i (qwords.begin ()), j (other_a.qwords.begin ()), n (qwords.end ()); i != n; ++i, ++j, ++k)
	{
		*k = *i ^ *j;
	}
	return result;
}

rai::uint256_union::uint256_union (std::string const & hex_a)
{
	decode_hex (hex_a);
}

void rai::uint256_union::clear ()
{
	qwords.fill (0);
}

rai::uint256_t rai::uint256_union::number () const
{
	rai::uint256_t result = 0;
	auto shift (0);
	for (auto i (bytes.begin ()), n (bytes.end ()); i != n; ++i)
	{
		result <<= shift;
		result |= *i;
		shift = 8;
	}
	return result;
}

void rai::uint256_union::encode_hex (std::string & text) const
{
	assert (text.empty ());
	std::stringstream stream;
	stream << std::hex << std::noshowbase << std::setw (64) << std::setfill ('0');
	stream << number ();
	text = stream.str ();
}

bool rai::uint256_union::decode_hex (std::string const & text)
{
	auto error (false);
	if (!text.empty () && text.size () <= 64)
	{
		std::stringstream stream (text);
		stream << std::hex << std::noshowbase;
		rai::uint256_t number_l;
		try
		{
			stream >> number_l;
			*this = number_l;
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
	else
	{
		error = true;
	}
	return error;
}

void rai::uint256_union::encode_dec (std::string & text) const
{
	assert (text.empty ());
	std::stringstream stream;
	stream << std::dec << std::noshowbase;
	stream << number ();
	text = stream.str ();
}

bool rai::uint256_union::decode_dec (std::string const & text)
{
	auto error (text.size () > 78 || (text.size () > 1 && text[0] == '0') || (text.size () > 0 && text[0] == '-'));
	if (!error)
	{
		std::stringstream stream (text);
		stream << std::dec << std::noshowbase;
		rai::uint256_t number_l;
		try
		{
			stream >> number_l;
			*this = number_l;
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
	return error;
}

rai::uint256_union::uint256_union (uint64_t value0)
{
	*this = rai::uint256_t (value0);
}

bool rai::uint256_union::operator!= (rai::uint256_union const & other_a) const
{
	return !(*this == other_a);
}

bool rai::uint512_union::operator== (rai::uint512_union const & other_a) const
{
	return bytes == other_a.bytes;
}

rai::uint512_union::uint512_union (rai::uint512_t const & number_a)
{
	rai::uint512_t number_l (number_a);
	for (auto i (bytes.rbegin ()), n (bytes.rend ()); i != n; ++i)
	{
		*i = static_cast<uint8_t> (number_l & static_cast<uint8_t> (0xff));
		number_l >>= 8;
	}
}

void rai::uint512_union::clear ()
{
	bytes.fill (0);
}

rai::uint512_t rai::uint512_union::number () const
{
	rai::uint512_t result = 0;
	auto shift (0);
	for (auto i (bytes.begin ()), n (bytes.end ()); i != n; ++i)
	{
		result <<= shift;
		result |= *i;
		shift = 8;
	}
	return result;
}

void rai::uint512_union::encode_hex (std::string & text) const
{
	assert (text.empty ());
	std::stringstream stream;
	stream << std::hex << std::noshowbase << std::setw (128) << std::setfill ('0');
	stream << number ();
	text = stream.str ();
}

bool rai::uint512_union::decode_hex (std::string const & text)
{
	auto error (text.size () > 128);
	if (!error)
	{
		std::stringstream stream (text);
		stream << std::hex << std::noshowbase;
		rai::uint512_t number_l;
		try
		{
			stream >> number_l;
			*this = number_l;
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
	return error;
}

bool rai::uint512_union::operator!= (rai::uint512_union const & other_a) const
{
	return !(*this == other_a);
}

rai::uint512_union & rai::uint512_union::operator^= (rai::uint512_union const & other_a)
{
	uint256s[0] ^= other_a.uint256s[0];
	uint256s[1] ^= other_a.uint256s[1];
	return *this;
}

std::string rai::uint512_union::to_string () const
{
	std::string result;
	encode_hex (result);
	return result;
}

rai::raw_key::~raw_key ()
{
	data.clear ();
}

bool rai::raw_key::operator== (rai::raw_key const & other_a) const
{
	return data == other_a.data;
}

bool rai::raw_key::operator!= (rai::raw_key const & other_a) const
{
	return !(*this == other_a);
}

// This this = AES_DEC_CTR (ciphertext, key, iv)
void rai::raw_key::decrypt (rai::uint256_union const & ciphertext, rai::raw_key const & key_a, uint128_struct const & iv)
{
	CryptoPP::AES::Encryption alg (key_a.data.bytes.data (), sizeof (key_a.data.bytes));
	CryptoPP::CTR_Mode_ExternalCipher::Decryption dec (alg, iv.bytes.data ());
	dec.ProcessData (data.bytes.data (), ciphertext.bytes.data (), sizeof (ciphertext.bytes));
}

rai::uint512_union rai::sign_message (rai::raw_key const & private_key, rai::public_key const & public_key, rai::uint256_union const & message)
{
	rai::uint512_union result;
	ed25519_sign (message.bytes.data (), sizeof (message.bytes), private_key.data.bytes.data (), public_key.bytes.data (), result.bytes.data ());
	return result;
}

void rai::deterministic_key (rai::uint256_union const & seed_a, uint32_t index_a, rai::uint256_union & prv_a)
{
	blake2b_state hash;
	blake2b_init (&hash, prv_a.bytes.size ());
	blake2b_update (&hash, seed_a.bytes.data (), seed_a.bytes.size ());
	rai::uint256_union index (index_a);
	blake2b_update (&hash, reinterpret_cast<uint8_t *> (&index.dwords[7]), sizeof (uint32_t));
	blake2b_final (&hash, prv_a.bytes.data (), prv_a.bytes.size ());
}

rai::public_key rai::pub_key (rai::private_key const & privatekey_a)
{
	rai::uint256_union result;
	ed25519_publickey (privatekey_a.bytes.data (), result.bytes.data ());
	return result;
}

bool rai::validate_message (rai::public_key const & public_key, rai::uint256_union const & message, rai::uint512_union const & signature)
{
	auto result (0 != ed25519_sign_open (message.bytes.data (), sizeof (message.bytes), public_key.bytes.data (), signature.bytes.data ()));
	return result;
}

rai::uint128_struct::uint128_struct (rai::uint128_t const & value_a)
{
	rai::uint128_t number_l (value_a);
	for (auto i (bytes.rbegin ()), n (bytes.rend ()); i != n; ++i)
	{
		*i = static_cast<uint8_t> (number_l & static_cast<uint8_t> (0xff));
		number_l >>= 8;
	}
}

rai::uint128_t rai::uint128_struct::number () const
{
	rai::uint128_t result = 0;
	auto shift (0);
	for (auto i (bytes.begin ()), n (bytes.end ()); i != n; ++i)
	{
		result <<= shift;
		result |= *i;
		shift = 8;
	}
	return result;
}

void rai::uint128_struct::encode_dec (std::string & text) const
{
	assert (text.empty ());
	std::stringstream stream;
	stream << std::dec << std::noshowbase;
	stream << number ();
	text = stream.str ();
}

bool rai::uint128_struct::decode_dec (std::string const & text)
{
	auto error (text.size () > 39 || (text.size () > 1 && text[0] == '0') || (text.size () > 0 && text[0] == '-'));
	if (!error)
	{
		std::stringstream stream (text);
		stream << std::dec << std::noshowbase;
		boost::multiprecision::checked_uint128_t number_l;
		try
		{
			stream >> number_l;
			rai::uint128_t unchecked (number_l);
			*this = unchecked;
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
	return error;
}

std::string rai::uint128_struct::to_string_dec () const
{
	std::string result;
	encode_dec (result);
	return result;
}

rai::uint64_struct::uint64_struct (std::string const & string_a)
{
	decode_hex (string_a);
}

rai::uint64_struct::uint64_struct (rai::uint64_t value_a)
{
	data = value_a;
}

bool rai::uint64_struct::operator== (rai::uint64_struct const & other_a) const
{
	return (data == other_a.data);
}

bool rai::uint64_struct::operator!= (rai::uint64_struct const & other_a) const
{
	return (data != other_a.data);
}

bool rai::uint64_struct::operator< (rai::uint64_struct const & other_a) const
{
	return (data < other_a.data);
}

bool rai::uint64_struct::operator> (rai::uint64_struct const & other_a) const
{
	return (data > other_a.data);
}

rai::uint64_t rai::uint64_struct::number () const
{
	return data;
}

rai::uint64_t rai::uint64_struct::number_big_endian () const
{
	return boost::endian::native_to_big (data);
}

void rai::uint64_struct::encode_hex (std::string & text) const
{
	assert (text.empty ());
	std::stringstream stream;
	stream << std::hex << std::noshowbase << std::setw (16) << std::setfill ('0');
	stream << data;
	text = stream.str ();
}

bool rai::uint64_struct::decode_hex (std::string const & text)
{
	auto error (text.size () > 16);
	if (!error)
	{
		std::stringstream stream (text);
		stream << std::hex << std::noshowbase;
		rai::uint64_t number_l;
		try
		{
			stream >> number_l;
			*this = number_l;
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
	return error;
}

void rai::uint64_struct::encode_dec (std::string & text) const
{
	assert (text.empty ());
	std::stringstream stream;
	stream << std::dec << std::noshowbase;
	stream << data;
	text = stream.str ();
}

bool rai::uint64_struct::decode_dec (std::string const & text)
{
	if (text.size () > 20 || (text.size () > 1 && text[0] == '0') || (text.size () > 0 && text[0] == '-'))
	{
		return true;
	}
	std::stringstream stream (text);
	stream << std::dec << std::noshowbase;
	boost::multiprecision::checked_uint128_t number_l; // checked and larger
	try
	{
		stream >> number_l;
		if ((rai::uint128_t)number_l > (rai::uint128_t)std::numeric_limits<uint64_t>::max ())
		{
			// overflow
			return true;
		}
		rai::uint64_t unchecked (number_l);
		*this = unchecked;
		if (!stream.eof ())
		{
			return true;
		}
	}
	catch (std::runtime_error &)
	{
		return true;
	}
	return false;
}

void rai::uint64_struct::clear ()
{
	data = 0;
}

bool rai::uint64_struct::is_zero () const
{
	return (data == 0);
}

std::string rai::uint64_struct::to_string () const
{
	std::string result;
	encode_hex (result);
	return result;
}

std::string rai::uint64_struct::to_string_dec () const
{
	std::string result;
	encode_dec (result);
	return result;
}

void rai::uint64_struct::serialize (rai::stream & stream_a) const
{
	write (stream_a, boost::endian::native_to_big (data));
}

bool rai::uint64_struct::deserialize (rai::stream & stream_a)
{
	auto error (rai::read (stream_a, data));
	if (error) return error;
	boost::endian::big_to_native_inplace (data);
	return false;
}

rai::amount::amount (std::string const & string_a)
{
	decode_dec (string_a);
}

rai::amount::amount (rai::amount_t value_a)
{
	data = value_a;
}

void format_frac (std::ostringstream & stream, rai::uint64_t value, rai::uint64_t scale, int precision)
{
	auto reduce = scale;
	auto rem = value;
	while (reduce > 1 && rem > 0 && precision > 0)
	{
		reduce /= 10;
		auto val = rem / reduce;
		rem -= val * reduce;
		stream << val;
		precision--;
	}
}

void format_dec (std::ostringstream & stream, rai::uint64_t value, char group_sep, const std::string & groupings)
{
	auto largestPow10 = rai::uint128_t (1); // larger range
	int dec_count = 1;
	while (1)
	{
		auto next = largestPow10 * 10;
		if (next > value)
		{
			break;
		}
		largestPow10 = next;
		dec_count++;
	}

	if (dec_count > 20)
	{
		// Impossible.
		return;
	}

	// This could be cached per-locale.
	bool emit_group[20];
	if (group_sep != 0)
	{
		int group_index = 0;
		int group_count = 0;
		for (int i = 0; i < dec_count; i++)
		{
			group_count++;
			if (group_count > groupings[group_index])
			{
				group_index = std::min (group_index + 1, (int)groupings.length () - 1);
				group_count = 1;
				emit_group[i] = true;
			}
			else
			{
				emit_group[i] = false;
			}
		}
	}

	auto reduce = rai::uint64_t (largestPow10);
	rai::amount_t rem = value;
	while (reduce > 0)
	{
		auto val = rem / reduce;
		rem -= val * reduce;
		stream << val;
		dec_count--;
		if (group_sep != 0 && emit_group[dec_count] && reduce > 1)
		{
			stream << group_sep;
		}
		reduce /= 10;
	}
}

std::string format_balance (rai::uint64_t balance, rai::uint64_t scale, int precision, bool group_digits, char thousands_sep, char decimal_point, std::string & grouping)
{
	std::ostringstream stream;
	auto int_part = balance / scale;
	auto frac_part = balance % scale;
	auto prec_scale = scale;
	for (int i = 0; i < precision; i++)
	{
		prec_scale /= 10;
	}
	if (int_part == 0 && frac_part > 0 && frac_part / prec_scale == 0)
	{
		// Display e.g. "< 0.01" rather than 0.
		stream << "< ";
		if (precision > 0)
		{
			stream << "0";
			stream << decimal_point;
			for (int i = 0; i < precision - 1; i++)
			{
				stream << "0";
			}
		}
		stream << "1";
	}
	else
	{
		format_dec (stream, int_part, group_digits && grouping.length () > 0 ? thousands_sep : 0, grouping);
		if (precision > 0 && frac_part > 0)
		{
			stream << decimal_point;
			format_frac (stream, frac_part, scale, precision);
		}
	}
	return stream.str ();
}

std::string rai::amount::format_balance (rai::uint64_t scale, int precision, bool group_digits)
{
	auto thousands_sep = std::use_facet<std::numpunct<char>> (std::locale ()).thousands_sep ();
	auto decimal_point = std::use_facet<std::numpunct<char>> (std::locale ()).decimal_point ();
	std::string grouping = "\3";
	return ::format_balance (data, scale, precision, group_digits, thousands_sep, decimal_point, grouping);
}

std::string rai::amount::format_balance (rai::uint64_t scale, int precision, bool group_digits, const std::locale & locale)
{
	auto thousands_sep = std::use_facet<std::moneypunct<char>> (locale).thousands_sep ();
	auto decimal_point = std::use_facet<std::moneypunct<char>> (locale).decimal_point ();
	std::string grouping = std::use_facet<std::moneypunct<char>> (locale).grouping ();
	return ::format_balance (data, scale, precision, group_digits, thousands_sep, decimal_point, grouping);
}

const uint16_t rai::var_len_bytes16::max_length;

rai::var_len_bytes16::var_len_bytes16 (std::vector<uint8_t> const & bytes_a)
{
	auto len_a (bytes_a.size ());
	if (len_a < rai::var_len_bytes16::max_length)
	{
		len = len_a;
		bytes = bytes_a;
		return;
	}
	// size must be truncated
	len = rai::var_len_bytes16::max_length;
	auto begin (bytes_a.begin ());
	auto end (begin + len);
	std::copy (begin, end, bytes.begin ());
}

void rai::var_len_bytes16::clear ()
{
	bytes.clear ();
	len = 0;
}

std::string rai::var_len_bytes16::to_string () const
{
	std::string result;
	encode_hex (result);
	return result;
}

void rai::var_len_bytes16::encode_hex (std::string & text) const
{
	assert (text.empty ());
	std::stringstream stream;
	stream << std::hex << std::noshowbase << std::setfill ('0');
	// length in 2 bytes
	stream << std::setw (4) << len;
	// contents
	stream << std::setw (2);
	for (int i = 0; i < len; ++i)
	{
		stream << (uint16_t)bytes[i]; // uint8 is char, printed as char
	}
	text = stream.str ();
}

bool rai::var_len_bytes16::decode_hex (std::string const & hex)
{
	// minimum size is 4 hex chars (=2 bytes) for length
	if (hex.size () < 2 * 2)
	{
		return true;
	}
	try
	{
		// read length, 2 bytes
		{
			std::stringstream stream (hex.substr (0, 4));
			stream << std::hex << std::noshowbase;
			uint16_t len_l;
			stream >> len_l;
			len = len_l;
		}
		// minimum size (incl. len)
		if (hex.size () < 2 * (2 + len))
		{
			return true;
		}
		bytes.clear ();
		size_t idx = 2 * 2;
		while (bytes.size () < len)
		{
			std::stringstream stream (hex.substr (idx, 2));
			idx += 2;
			stream << std::hex << std::noshowbase;
			uint16_t v1; // uint8_t is char
			stream >> v1;
			bytes.push_back ((uint8_t)v1);
		}
	}
	catch (std::runtime_error &)
	{
		return true;
	}
	return false;
}

void rai::var_len_bytes16::serialize (rai::stream & stream_a) const
{
	write (stream_a, boost::endian::native_to_big (len));
	rai::write_len (stream_a, len, bytes.data ());
}

bool rai::var_len_bytes16::deserialize (rai::stream & stream_a)
{
	// read length
	auto error (rai::read (stream_a, len));
	if (error)
		return error;
	boost::endian::big_to_native_inplace (len);
	// read bytes: create the vector object then fill it
	bytes = std::vector<uint8_t> (len);
	error = rai::read_len (stream_a, len, bytes.data ());
	return error;
}

bool rai::var_len_bytes16::operator== (rai::var_len_bytes16 const & other_a) const
{
	if (len != other_a.len)
	{
		return false;
	}
	return (bytes == other_a.bytes);
}

rai::var_len_string::var_len_string (std::string const & string_a) :
var_len_bytes16 ()
{
	set_from_string (string_a, max_length);
}

rai::var_len_string::var_len_string (std::string const & string_a, size_t max_len_a) :
var_len_bytes16 ()
{
	set_from_string (string_a, max_len_a);
}

std::string rai::var_len_string::value_string () const
{
	return raw_to_string (bytes, len);
}

void rai::var_len_string::set_from_string (std::string const & string_a, size_t max_len_a)
{
	// TODO UTF-8 conversion
	size_t len_l (string_a.length ());
	clear ();
	len = (uint16_t)std::min ((size_t)max_length, std::min ((size_t)max_len_a, (size_t)len_l));
	for (uint16_t i (0); i < len; ++i)
	{
		bytes.push_back ((uint8_t)string_a[i]);
	}
}

std::string rai::var_len_string::raw_to_string (std::vector<uint8_t> const & raw_a, uint16_t len_a)
{
	// TODO UTF-8 conversion
	if (raw_a.size () == len_a)
	{
		std::string string_l (raw_a.begin (), raw_a.end ());
		return string_l;
	}
	// need to truncate
	len_a = (uint16_t)std::min ((size_t)max_length, std::min (raw_a.size (), (size_t)len_a));
	auto end_l (raw_a.begin () + len_a);
	std::string string_l (raw_a.begin (), end_l);
	return string_l;
}
