#include <boost/algorithm/string.hpp>
#include <boost/property_tree/ptree.hpp>
#include <rai/node/rpc.hpp>

#include <rai/lib/interface.h>
#include <rai/node/node.hpp>

#ifdef RAIBLOCKS_SECURE_RPC
#include <rai/node/rpc_secure.hpp>
#endif

#include <rai/lib/errors.hpp>

rai::rpc_secure_config::rpc_secure_config () :
enable (false),
verbose_logging (false)
{
}

void rai::rpc_secure_config::serialize_json (boost::property_tree::ptree & tree_a) const
{
	tree_a.put ("enable", enable);
	tree_a.put ("verbose_logging", verbose_logging);
	tree_a.put ("server_key_passphrase", server_key_passphrase);
	tree_a.put ("server_cert_path", server_cert_path);
	tree_a.put ("server_key_path", server_key_path);
	tree_a.put ("server_dh_path", server_dh_path);
	tree_a.put ("client_certs_path", client_certs_path);
}

bool rai::rpc_secure_config::deserialize_json (boost::property_tree::ptree const & tree_a)
{
	auto error (false);
	try
	{
		enable = tree_a.get<bool> ("enable");
		verbose_logging = tree_a.get<bool> ("verbose_logging");
		server_key_passphrase = tree_a.get<std::string> ("server_key_passphrase");
		server_cert_path = tree_a.get<std::string> ("server_cert_path");
		server_key_path = tree_a.get<std::string> ("server_key_path");
		server_dh_path = tree_a.get<std::string> ("server_dh_path");
		client_certs_path = tree_a.get<std::string> ("client_certs_path");
	}
	catch (std::runtime_error const &)
	{
		error = true;
	}
	return error;
}

rai::rpc_config::rpc_config () :
address (boost::asio::ip::address_v6::loopback ()),
port (rai::rpc::rpc_port),
enable_control (false),
frontier_request_limit (16384),
chain_request_limit (16384),
max_json_depth (20)
{
}

rai::rpc_config::rpc_config (bool enable_control_a) :
address (boost::asio::ip::address_v6::loopback ()),
port (rai::rpc::rpc_port),
enable_control (enable_control_a),
frontier_request_limit (16384),
chain_request_limit (16384),
max_json_depth (20)
{
}

void rai::rpc_config::serialize_json (boost::property_tree::ptree & tree_a) const
{
	tree_a.put ("address", address.to_string ());
	tree_a.put ("port", std::to_string (port));
	tree_a.put ("enable_control", enable_control);
	tree_a.put ("frontier_request_limit", frontier_request_limit);
	tree_a.put ("chain_request_limit", chain_request_limit);
	tree_a.put ("max_json_depth", max_json_depth);
}

bool rai::rpc_config::deserialize_json (boost::property_tree::ptree const & tree_a)
{
	auto result (false);
	try
	{
		auto rpc_secure_l (tree_a.get_child_optional ("secure"));
		if (rpc_secure_l)
		{
			result = secure.deserialize_json (rpc_secure_l.get ());
		}

		if (!result)
		{
			auto address_l (tree_a.get<std::string> ("address"));
			auto port_l (tree_a.get<std::string> ("port"));
			enable_control = tree_a.get<bool> ("enable_control");
			auto frontier_request_limit_l (tree_a.get<std::string> ("frontier_request_limit"));
			auto chain_request_limit_l (tree_a.get<std::string> ("chain_request_limit"));
			max_json_depth = tree_a.get<uint8_t> ("max_json_depth", max_json_depth);
			try
			{
				port = std::stoul (port_l);
				result = port > std::numeric_limits<uint16_t>::max ();
				frontier_request_limit = std::stoull (frontier_request_limit_l);
				chain_request_limit = std::stoull (chain_request_limit_l);
			}
			catch (std::logic_error const &)
			{
				result = true;
			}
			boost::system::error_code ec;
			address = boost::asio::ip::address_v6::from_string (address_l, ec);
			if (ec)
			{
				result = true;
			}
		}
	}
	catch (std::runtime_error const &)
	{
		result = true;
	}
	return result;
}

rai::rpc::rpc (boost::asio::io_service & service_a, rai::node & node_a, rai::rpc_config const & config_a) :
acceptor (service_a),
config (config_a),
node (node_a)
{
}

void rai::rpc::start ()
{
	auto endpoint (rai::tcp_endpoint (config.address, config.port));
	acceptor.open (endpoint.protocol ());
	acceptor.set_option (boost::asio::ip::tcp::acceptor::reuse_address (true));

	boost::system::error_code ec;
	acceptor.bind (endpoint, ec);
	if (ec)
	{
		BOOST_LOG (node.log) << boost::str (boost::format ("Error while binding for RPC on port %1%: %2%") % endpoint.port () % ec.message ());
		throw std::runtime_error (ec.message ());
	}

	acceptor.listen ();
	node.observers.blocks.add ([this](std::shared_ptr<rai::block> block_a, rai::account const & account_a, rai::amount_t const &, bool) {
		observer_action (account_a);
	});

	accept ();
}

void rai::rpc::accept ()
{
	auto connection (std::make_shared<rai::rpc_connection> (node, *this));
	acceptor.async_accept (connection->socket, [this, connection](boost::system::error_code const & ec) {
		if (!ec)
		{
			accept ();
			connection->parse_connection ();
		}
		else
		{
			BOOST_LOG (this->node.log) << boost::str (boost::format ("Error accepting RPC connections: %1%") % ec);
		}
	});
}

void rai::rpc::stop ()
{
	acceptor.close ();
}

rai::rpc_handler::rpc_handler (rai::node & node_a, rai::rpc & rpc_a, std::string const & body_a, std::string const & request_id_a, std::function<void(boost::property_tree::ptree const &)> const & response_a) :
body (body_a),
node (node_a),
rpc (rpc_a),
response (response_a),
request_id (request_id_a)
{
}

void rai::rpc::observer_action (rai::account const & account_a)
{
	std::shared_ptr<rai::payment_observer> observer;
	{
		std::lock_guard<std::mutex> lock (mutex);
		auto existing (payment_observers.find (account_a));
		if (existing != payment_observers.end ())
		{
			observer = existing->second;
		}
	}
	if (observer != nullptr)
	{
		observer->observe ();
	}
}

void rai::error_response (std::function<void(boost::property_tree::ptree const &)> response_a, std::string const & message_a)
{
	boost::property_tree::ptree response_l;
	response_l.put ("error", message_a);
	response_a (response_l);
}

void rai::rpc_handler::response_errors ()
{
	if (ec || response_l.empty ())
	{
		boost::property_tree::ptree response_error;
		response_error.put ("error", ec ? ec.message () : "Empty response");
		response (response_error);
	}
	else
	{
		response (response_l);
	}
}

std::shared_ptr<rai::wallet> rai::rpc_handler::wallet_impl ()
{
	if (!ec)
	{
		std::string wallet_text (request.get<std::string> ("wallet"));
		rai::uint256_union wallet;
		if (!wallet.decode_hex (wallet_text))
		{
			auto existing (node.wallets.items.find (wallet));
			if (existing != node.wallets.items.end ())
			{
				return existing->second;
			}
			else
			{
				ec = nano::error_common::wallet_not_found;
			}
		}
		else
		{
			ec = nano::error_common::bad_wallet_number;
		}
	}
	return nullptr;
}

rai::account rai::rpc_handler::account_impl (std::string account_text)
{
	rai::account result (0);
	if (!ec)
	{
		if (account_text.empty ())
		{
			account_text = request.get<std::string> ("account");
		}
		if (result.decode_account (account_text))
		{
			ec = nano::error_common::bad_account_number;
		}
	}
	return result;
}

rai::amount rai::rpc_handler::amount_impl ()
{
	rai::amount result (0);
	if (!ec)
	{
		std::string amount_text (request.get<std::string> ("amount"));
		if (result.decode_dec (amount_text))
		{
			ec = nano::error_common::invalid_amount;
		}
	}
	return result;
}

rai::block_hash rai::rpc_handler::hash_impl (std::string search_text)
{
	rai::block_hash result (0);
	if (!ec)
	{
		std::string hash_text (request.get<std::string> (search_text));
		if (result.decode_hex (hash_text))
		{
			ec = nano::error_blocks::invalid_block_hash;
		}
	}
	return result;
}

rai::amount rai::rpc_handler::threshold_optional_impl ()
{
	rai::amount result (0);
	boost::optional<std::string> threshold_text (request.get_optional<std::string> ("threshold"));
	if (!ec && threshold_text.is_initialized ())
	{
		if (result.decode_dec (threshold_text.get ()))
		{
			ec = nano::error_common::bad_threshold;
		}
	}
	return result;
}

uint64_t rai::rpc_handler::work_optional_impl ()
{
	uint64_t result (0);
	boost::optional<std::string> work_text (request.get_optional<std::string> ("work"));
	if (!ec && work_text.is_initialized ())
	{
		if (rai::from_string_hex (work_text.get (), result))
		{
			ec = nano::error_common::bad_work_format;
		}
	}
	return result;
}

namespace
{
bool decode_unsigned (std::string const & text, uint64_t & number)
{
	bool result;
	size_t end;
	try
	{
		number = std::stoull (text, &end);
		result = false;
	}
	catch (std::invalid_argument const &)
	{
		result = true;
	}
	catch (std::out_of_range const &)
	{
		result = true;
	}
	result = result || end != text.size ();
	return result;
}
}

uint64_t rai::rpc_handler::count_impl ()
{
	uint64_t result (0);
	if (!ec)
	{
		std::string count_text (request.get<std::string> ("count"));
		if (decode_unsigned (count_text, result) || result == 0)
		{
			ec = nano::error_common::invalid_count;
		}
	}
	return result;
}

uint64_t rai::rpc_handler::count_optional_impl (uint64_t result)
{
	boost::optional<std::string> count_text (request.get_optional<std::string> ("count"));
	if (!ec && count_text.is_initialized ())
	{
		if (decode_unsigned (count_text.get (), result))
		{
			ec = nano::error_common::invalid_count;
		}
	}
	return result;
}

bool rai::rpc_handler::rpc_control_impl ()
{
	bool result (false);
	if (!ec)
	{
		if (!rpc.config.enable_control)
		{
			ec = nano::error_rpc::rpc_control_disabled;
		}
		else
		{
			result = true;
		}
	}
	return result;
}

void rai::rpc_handler::account_balance ()
{
	auto account (account_impl ());
	if (!ec)
	{
		auto balance (node.balance_pending (account));
		response_l.put ("balance", std::to_string (balance.first));
		response_l.put ("pending", std::to_string (balance.second));
	}
	response_errors ();
}

void rai::rpc_handler::account_block_count ()
{
	auto account (account_impl ());
	if (!ec)
	{
		rai::transaction transaction (node.store.environment, nullptr, false);
		rai::account_info info;
		if (!node.store.account_get (transaction, account, info))
		{
			response_l.put ("block_count", std::to_string (info.block_count));
		}
		else
		{
			ec = nano::error_common::account_not_found;
		}
	}
	response_errors ();
}

void rai::rpc_handler::account_create ()
{
	rpc_control_impl ();
	auto wallet (wallet_impl ());
	if (!ec)
	{
		const bool generate_work = request.get<bool> ("work", true);
		rai::account new_key (wallet->deterministic_insert (generate_work));
		if (!new_key.is_zero ())
		{
			response_l.put ("account", new_key.to_account ());
		}
		else
		{
			ec = nano::error_common::wallet_locked;
		}
	}
	response_errors ();
}

void rai::rpc_handler::account_get ()
{
	std::string key_text (request.get<std::string> ("key"));
	rai::uint256_union pub;
	if (!pub.decode_hex (key_text))
	{
		response_l.put ("account", pub.to_account ());
	}
	else
	{
		ec = nano::error_common::bad_public_key;
	}
	response_errors ();
}

void rai::rpc_handler::account_info_intern (rai::transaction & transaction_in, const rai::account & account_in, boost::property_tree::ptree & ptree_inout, bool representative_in, bool weight_in, bool pending_in)
{
	if (ec)
		return;
	rai::account_info info;
	if (node.store.account_get (transaction_in, account_in, info))
	{
		ec = nano::error_common::account_not_found;
		return;
	}
	ptree_inout.put ("frontier", info.head.to_string ());
	ptree_inout.put ("open_block", info.open_block.to_string ());
	ptree_inout.put ("representative_block", info.rep_block.to_string ());
	std::string balance;
	rai::amount (info.balance).encode_dec (balance);
	ptree_inout.put ("balance", balance);
	ptree_inout.put ("last_block_time", std::to_string (rai::short_timestamp::convert_to_posix_time (info.last_block_time ())));
	ptree_inout.put ("block_count", std::to_string (info.block_count));
	if (representative_in)
	{
		auto block (node.store.block_get (transaction_in, info.rep_block));
		assert (block != nullptr);
		ptree_inout.put ("representative", block->representative ().to_account ());
	}
	if (weight_in)
	{
		auto account_weight (node.ledger.weight (transaction_in, account_in));
		ptree_inout.put ("weight", std::to_string (account_weight));
	}
	if (pending_in)
	{
		auto account_pending (node.ledger.account_pending (transaction_in, account_in));
		ptree_inout.put ("pending", std::to_string (account_pending));
	}
}

void rai::rpc_handler::account_info ()
{
	auto account (account_impl ());
	if (!ec)
	{
		const bool representative = request.get<bool> ("representative", false);
		const bool weight = request.get<bool> ("weight", false);
		const bool pending = request.get<bool> ("pending", false);
		rai::transaction transaction (node.store.environment, nullptr, false);
		account_info_intern (transaction, account, response_l, representative, weight, pending);
	}
	response_errors ();
}

void rai::rpc_handler::accounts_infos ()
{
	boost::property_tree::ptree infos_ptree;
	for (auto & accounts : request.get_child ("accounts"))
	{
		auto account (account_impl (accounts.second.data ()));
		if (!ec)
		{
			const bool representative = request.get<bool> ("representative", false);
			const bool weight = request.get<bool> ("weight", false);
			const bool pending = request.get<bool> ("pending", false);
			rai::transaction transaction (node.store.environment, nullptr, false);
			boost::property_tree::ptree info_ptree;
			account_info_intern (transaction, account, info_ptree, representative, weight, pending);
			if (!ec)
			{
				infos_ptree.push_back (std::make_pair (account.to_account (), info_ptree));
			}
		}
	}
	response_l.add_child ("infos", infos_ptree);
	response_errors ();
}

void rai::rpc_handler::account_key ()
{
	auto account (account_impl ());
	if (!ec)
	{
		response_l.put ("key", account.to_string ());
	}
	response_errors ();
}

void rai::rpc_handler::account_list ()
{
	auto wallet (wallet_impl ());
	if (!ec)
	{
		boost::property_tree::ptree accounts;
		rai::transaction transaction (node.store.environment, nullptr, false);
		for (auto i (wallet->store.begin (transaction)), j (wallet->store.end ()); i != j; ++i)
		{
			boost::property_tree::ptree entry;
			entry.put ("", rai::uint256_union (i->first.uint256 ()).to_account ());
			accounts.push_back (std::make_pair ("", entry));
		}
		response_l.add_child ("accounts", accounts);
	}
	response_errors ();
}

void rai::rpc_handler::account_move ()
{
	rpc_control_impl ();
	auto wallet (wallet_impl ());
	if (!ec)
	{
		std::string source_text (request.get<std::string> ("source"));
		auto accounts_text (request.get_child ("accounts"));
		rai::uint256_union source;
		if (!source.decode_hex (source_text))
		{
			auto existing (node.wallets.items.find (source));
			if (existing != node.wallets.items.end ())
			{
				auto source (existing->second);
				std::vector<rai::public_key> accounts;
				for (auto i (accounts_text.begin ()), n (accounts_text.end ()); i != n; ++i)
				{
					rai::public_key account;
					account.decode_hex (i->second.get<std::string> (""));
					accounts.push_back (account);
				}
				rai::transaction transaction (node.store.environment, nullptr, true);
				auto error (wallet->store.move (transaction, source->store, accounts));
				response_l.put ("moved", error ? "0" : "1");
			}
			else
			{
				ec = nano::error_rpc::source_not_found;
			}
		}
		else
		{
			ec = nano::error_rpc::bad_source;
		}
	}
	response_errors ();
}

void rai::rpc_handler::account_remove ()
{
	rpc_control_impl ();
	auto wallet (wallet_impl ());
	auto account (account_impl ());
	if (!ec)
	{
		rai::transaction transaction (node.store.environment, nullptr, true);
		if (wallet->store.valid_password (transaction))
		{
			if (wallet->store.find (transaction, account) != wallet->store.end ())
			{
				wallet->store.erase (transaction, account);
				response_l.put ("removed", "1");
			}
			else
			{
				ec = nano::error_common::account_not_found_wallet;
			}
		}
		else
		{
			ec = nano::error_common::wallet_locked;
		}
	}
	response_errors ();
}

void rai::rpc_handler::account_representative ()
{
	auto account (account_impl ());
	if (!ec)
	{
		rai::transaction transaction (node.store.environment, nullptr, false);
		rai::account_info info;
		if (!node.store.account_get (transaction, account, info))
		{
			auto block (node.store.block_get (transaction, info.rep_block));
			assert (block != nullptr);
			response_l.put ("representative", block->representative ().to_account ());
		}
		else
		{
			ec = nano::error_common::account_not_found;
		}
	}
	response_errors ();
}

void rai::rpc_handler::account_representative_set ()
{
	rpc_control_impl ();
	auto wallet (wallet_impl ());
	auto account (account_impl ());
	if (!ec)
	{
		if (wallet->valid_password ())
		{
			std::string representative_text (request.get<std::string> ("representative"));
			rai::account representative;
			if (!representative.decode_account (representative_text))
			{
				auto work (work_optional_impl ());
				if (!ec && work)
				{
					rai::transaction transaction (node.store.environment, nullptr, true);
					rai::account_info info;
					if (!node.store.account_get (transaction, account, info))
					{
						if (!rai::work_validate (info.head, work))
						{
							wallet->store.work_put (transaction, account, work);
						}
						else
						{
							ec = nano::error_common::invalid_work;
						}
					}
					else
					{
						ec = nano::error_common::account_not_found;
					}
				}
				if (!ec)
				{
					auto response_a (response);
					wallet->change_async (account, representative, [response_a](std::shared_ptr<rai::block> block) {
						rai::block_hash hash (0);
						if (block != nullptr)
						{
							hash = block->hash ();
						}
						boost::property_tree::ptree response_l;
						response_l.put ("block", hash.to_string ());
						response_a (response_l);
					},
					work == 0);
				}
			}
			else
			{
				ec = nano::error_rpc::bad_representative_number;
			}
		}
		else
		{
			ec = nano::error_common::wallet_locked;
		}
	}
	// Because of change_async
	if (ec)
	{
		response_errors ();
	}
}

void rai::rpc_handler::account_weight ()
{
	auto account (account_impl ());
	if (!ec)
	{
		auto balance (node.weight (account));
		response_l.put ("weight", std::to_string (balance));
	}
	response_errors ();
}

void rai::rpc_handler::accounts_balances ()
{
	boost::property_tree::ptree balances;
	for (auto & accounts : request.get_child ("accounts"))
	{
		auto account (account_impl (accounts.second.data ()));
		if (!ec)
		{
			boost::property_tree::ptree entry;
			auto balance (node.balance_pending (account));
			entry.put ("balance", std::to_string (balance.first));
			entry.put ("pending", std::to_string (balance.second));
			balances.push_back (std::make_pair (account.to_account (), entry));
		}
	}
	response_l.add_child ("balances", balances);
	response_errors ();
}

void rai::rpc_handler::accounts_create ()
{
	rpc_control_impl ();
	auto wallet (wallet_impl ());
	auto count (count_impl ());
	if (!ec)
	{
		const bool generate_work = request.get<bool> ("work", false);
		boost::property_tree::ptree accounts;
		for (auto i (0); accounts.size () < count; ++i)
		{
			rai::account new_key (wallet->deterministic_insert (generate_work));
			if (!new_key.is_zero ())
			{
				boost::property_tree::ptree entry;
				entry.put ("", new_key.to_account ());
				accounts.push_back (std::make_pair ("", entry));
			}
		}
		response_l.add_child ("accounts", accounts);
	}
	response_errors ();
}

void rai::rpc_handler::accounts_frontiers ()
{
	boost::property_tree::ptree frontiers;
	rai::transaction transaction (node.store.environment, nullptr, false);
	for (auto & accounts : request.get_child ("accounts"))
	{
		auto account (account_impl (accounts.second.data ()));
		if (!ec)
		{
			auto latest (node.ledger.latest (transaction, account));
			if (!latest.is_zero ())
			{
				frontiers.put (account.to_account (), latest.to_string ());
			}
		}
	}
	response_l.add_child ("frontiers", frontiers);
	response_errors ();
}

void rai::rpc_handler::accounts_pending ()
{
	auto count (count_optional_impl ());
	auto threshold (threshold_optional_impl ());
	const bool source = request.get<bool> ("source", false);
	const bool include_active = request.get<bool> ("include_active", false);
	boost::property_tree::ptree pending;
	rai::transaction transaction (node.store.environment, nullptr, false);
	for (auto & accounts : request.get_child ("accounts"))
	{
		auto account (account_impl (accounts.second.data ()));
		if (!ec)
		{
			boost::property_tree::ptree peers_l;
			rai::account end (account.number () + 1);
			for (auto i (node.store.pending_begin (transaction, rai::pending_key (account, 0))), n (node.store.pending_begin (transaction, rai::pending_key (end, 0))); i != n && peers_l.size () < count; ++i)
			{
				rai::pending_key key (i->first);
				std::shared_ptr<rai::block> block (node.store.block_get (transaction, key.hash));
				assert (block);
				if (include_active || (block && !node.active.active (*block)))
				{
					if (threshold.is_zero () && !source)
					{
						boost::property_tree::ptree entry;
						entry.put ("", key.hash.to_string ());
						peers_l.push_back (std::make_pair ("", entry));
					}
					else
					{
						rai::pending_info info (i->second);
						if (info.amount.number () >= threshold.number ())
						{
							if (source)
							{
								boost::property_tree::ptree pending_tree;
								pending_tree.put ("amount", std::to_string (info.amount.number ()));
								pending_tree.put ("source", info.source.to_account ());
								peers_l.add_child (key.hash.to_string (), pending_tree);
							}
							else
							{
								peers_l.put (key.hash.to_string (), std::to_string (info.amount.number ()));
							}
						}
					}
				}
			}
			pending.add_child (account.to_account (), peers_l);
		}
	}
	response_l.add_child ("blocks", pending);
	response_errors ();
}

void rai::rpc_handler::available_supply ()
{
	auto genesis_balance (node.balance (rai::genesis_account)); // Cold storage genesis
	auto landing_balance (node.balance (rai::account ("059F68AAB29DE0D3A27443625C7EA9CDDB6517A8B76FE37727EF6A4D76832AD5"))); // Active unavailable account
	auto faucet_balance (node.balance (rai::account ("8E319CE6F3025E5B2DF66DA7AB1467FE48F1679C13DD43BFDB29FA2E9FC40D3B"))); // Faucet account
	auto burned_balance ((node.balance_pending (rai::account (0))).second); // Burning 0 account
	auto available (rai::genesis_amount - genesis_balance - landing_balance - faucet_balance - burned_balance);
	response_l.put ("available", std::to_string (available));
	response_errors ();
}

void rai::rpc_handler::block ()
{
	auto hash (hash_impl ());
	if (!ec)
	{
		rai::transaction transaction (node.store.environment, nullptr, false);
		auto block (node.store.block_get (transaction, hash));
		if (block != nullptr)
		{
			std::string contents;
			block->serialize_json (contents);
			response_l.put ("contents", contents);
		}
		else
		{
			ec = nano::error_blocks::not_found;
		}
	}
	response_errors ();
}

void rai::rpc_handler::block_confirm ()
{
	auto hash (hash_impl ());
	if (!ec)
	{
		rai::transaction transaction (node.store.environment, nullptr, false);
		auto block_l (node.store.block_get (transaction, hash));
		if (block_l != nullptr)
		{
			node.block_confirm (std::move (block_l));
			response_l.put ("started", "1");
		}
		else
		{
			ec = nano::error_blocks::not_found;
		}
	}
	response_errors ();
}

void rai::rpc_handler::blocks ()
{
	std::vector<std::string> hashes;
	boost::property_tree::ptree blocks;
	rai::transaction transaction (node.store.environment, nullptr, false);
	for (boost::property_tree::ptree::value_type & hashes : request.get_child ("hashes"))
	{
		if (!ec)
		{
			std::string hash_text = hashes.second.data ();
			rai::uint256_union hash;
			if (!hash.decode_hex (hash_text))
			{
				auto block (node.store.block_get (transaction, hash));
				if (block != nullptr)
				{
					std::string contents;
					block->serialize_json (contents);
					blocks.put (hash_text, contents);
				}
				else
				{
					ec = nano::error_blocks::not_found;
				}
			}
			else
			{
				ec = nano::error_blocks::bad_hash_number;
			}
		}
	}
	response_l.add_child ("blocks", blocks);
	response_errors ();
}

void rai::rpc_handler::blocks_info ()
{
	const bool pending = request.get<bool> ("pending", false);
	const bool source = request.get<bool> ("source", false);
	const bool balance = request.get<bool> ("balance", false);
	std::vector<std::string> hashes;
	boost::property_tree::ptree blocks;
	rai::transaction transaction (node.store.environment, nullptr, false);
	for (boost::property_tree::ptree::value_type & hashes : request.get_child ("hashes"))
	{
		if (!ec)
		{
			std::string hash_text = hashes.second.data ();
			rai::uint256_union hash;
			if (!hash.decode_hex (hash_text))
			{
				auto block (node.store.block_get (transaction, hash));
				if (block != nullptr)
				{
					boost::property_tree::ptree entry;
					auto account (node.ledger.account (transaction, hash));
					entry.put ("block_account", account.to_account ());
					int amount_sign = 0;
					auto amount (node.ledger.amount_with_sign (transaction, hash, amount_sign));
					entry.put ("amount", std::to_string (amount));
					entry.put ("amount_sign", std::to_string (amount_sign));
					std::string contents;
					block->serialize_json (contents);
					entry.put ("contents", contents);
					if (pending)
					{
						bool exists (false);
						auto destination (node.ledger.block_destination (transaction, *block));
						if (!destination.is_zero ())
						{
							exists = node.store.pending_exists (transaction, rai::pending_key (destination, hash));
						}
						entry.put ("pending", exists ? "1" : "0");
					}
					if (source)
					{
						rai::block_hash source_hash (node.ledger.block_source (transaction, *block));
						std::unique_ptr<rai::block> block_a (node.store.block_get (transaction, source_hash));
						if (block_a != nullptr)
						{
							auto source_account (node.ledger.account (transaction, source_hash));
							entry.put ("source_account", source_account.to_account ());
						}
						else
						{
							entry.put ("source_account", "0");
						}
					}
					if (balance)
					{
						auto balance (node.ledger.balance (transaction, hash));
						entry.put ("balance", std::to_string (balance));
					}
					blocks.push_back (std::make_pair (hash_text, entry));
				}
				else
				{
					ec = nano::error_blocks::not_found;
				}
			}
			else
			{
				ec = nano::error_blocks::bad_hash_number;
			}
		}
	}
	response_l.add_child ("blocks", blocks);
	response_errors ();
}

void rai::rpc_handler::block_account ()
{
	auto hash (hash_impl ());
	if (!ec)
	{
		rai::transaction transaction (node.store.environment, nullptr, false);
		if (node.store.block_exists (transaction, hash))
		{
			auto account (node.ledger.account (transaction, hash));
			response_l.put ("account", account.to_account ());
		}
		else
		{
			ec = nano::error_blocks::not_found;
		}
	}
	response_errors ();
}

void rai::rpc_handler::block_count ()
{
	rai::transaction transaction (node.store.environment, nullptr, false);
	response_l.put ("count", std::to_string (node.store.block_count (transaction).sum ()));
	response_l.put ("unchecked", std::to_string (node.store.unchecked_count (transaction)));
	response_errors ();
}

void rai::rpc_handler::block_count_type ()
{
	rai::transaction transaction (node.store.environment, nullptr, false);
	rai::block_counts count (node.store.block_count (transaction));
	response_l.put ("state", std::to_string (count.state));
	response_errors ();
}

void rai::rpc_handler::block_create ()
{
	rpc_control_impl ();
	if (!ec)
	{
		std::string type (request.get<std::string> ("type"));
		rai::uint256_union wallet (0);
		boost::optional<std::string> wallet_text (request.get_optional<std::string> ("wallet"));
		if (wallet_text.is_initialized ())
		{
			if (wallet.decode_hex (wallet_text.get ()))
			{
				ec = nano::error_common::bad_wallet_number;
			}
		}
		rai::uint256_union account (0);
		boost::optional<std::string> account_text (request.get_optional<std::string> ("account"));
		if (!ec && account_text.is_initialized ())
		{
			if (account.decode_account (account_text.get ()))
			{
				ec = nano::error_common::bad_account_number;
			}
		}
		// creation_time; accept if missing, treat it as 0 = now
		rai::short_timestamp creation_time (0);
		boost::optional<std::string> creation_time_text (request.get_optional<std::string> ("creation_time"));
		if (!ec && creation_time_text.is_initialized ())
		{
			if (creation_time.decode_dec (creation_time_text.get ()))
			{
				creation_time = 0; // missing, default 0 = now
			}
		}
		rai::uint256_union representative (0);
		boost::optional<std::string> representative_text (request.get_optional<std::string> ("representative"));
		if (!ec && representative_text.is_initialized ())
		{
			if (representative.decode_account (representative_text.get ()))
			{
				ec = nano::error_rpc::bad_representative_number;
			}
		}
		rai::uint256_union destination (0);
		boost::optional<std::string> destination_text (request.get_optional<std::string> ("destination"));
		if (!ec && destination_text.is_initialized ())
		{
			if (destination.decode_account (destination_text.get ()))
			{
				ec = nano::error_rpc::bad_destination;
			}
		}
		rai::block_hash source (0);
		boost::optional<std::string> source_text (request.get_optional<std::string> ("source"));
		if (!ec && source_text.is_initialized ())
		{
			if (source.decode_hex (source_text.get ()))
			{
				ec = nano::error_rpc::bad_source;
			}
		}
		rai::amount amount (0);
		boost::optional<std::string> amount_text (request.get_optional<std::string> ("amount"));
		if (!ec && amount_text.is_initialized ())
		{
			if (amount.decode_dec (amount_text.get ()))
			{
				ec = nano::error_common::invalid_amount;
			}
		}
		auto work (work_optional_impl ());
		rai::raw_key prv;
		prv.data.clear ();
		rai::uint256_union previous (0);
		rai::amount balance (0);
		if (!ec && wallet != 0 && account != 0)
		{
			auto existing (node.wallets.items.find (wallet));
			if (existing != node.wallets.items.end ())
			{
				rai::transaction transaction (node.store.environment, nullptr, false);
				if (existing->second->store.valid_password (transaction))
				{
					if (existing->second->store.find (transaction, account) != existing->second->store.end ())
					{
						existing->second->store.fetch (transaction, account, prv);
						previous = node.ledger.latest (transaction, account);
						balance = node.ledger.account_balance (transaction, account);
					}
					else
					{
						ec = nano::error_common::account_not_found_wallet;
					}
				}
				else
				{
					ec = nano::error_common::wallet_locked;
				}
			}
			else
			{
				ec = nano::error_common::wallet_not_found;
			}
		}
		boost::optional<std::string> key_text (request.get_optional<std::string> ("key"));
		if (!ec && key_text.is_initialized ())
		{
			if (prv.data.decode_hex (key_text.get ()))
			{
				ec = nano::error_common::bad_private_key;
			}
		}
		boost::optional<std::string> previous_text (request.get_optional<std::string> ("previous"));
		if (!ec && previous_text.is_initialized ())
		{
			if (previous.decode_hex (previous_text.get ()))
			{
				ec = nano::error_rpc::bad_previous;
			}
		}
		boost::optional<std::string> balance_text (request.get_optional<std::string> ("balance"));
		if (!ec && balance_text.is_initialized ())
		{
			if (balance.decode_dec (balance_text.get ()))
			{
				ec = nano::error_rpc::invalid_balance;
			}
		}
		rai::uint256_union link (0);
		boost::optional<std::string> link_text (request.get_optional<std::string> ("link"));
		if (!ec && link_text.is_initialized ())
		{
			if (link.decode_account (link_text.get ()))
			{
				if (link.decode_hex (link_text.get ()))
				{
					ec = nano::error_rpc::bad_link;
				}
			}
		}
		else
		{
			// Retrieve link from source or destination
			link = source.is_zero () ? destination : source;
		}
		if (prv.data != 0)
		{
			rai::uint256_union pub (rai::pub_key (prv.data));
			// Fetching account balance & previous for send blocks (if aren't given directly)
			if (!previous_text.is_initialized () && !balance_text.is_initialized ())
			{
				rai::transaction transaction (node.store.environment, nullptr, false);
				previous = node.ledger.latest (transaction, pub);
				balance = node.ledger.account_balance (transaction, pub);
			}
			// Double check current balance if previous block is specified
			else if (previous_text.is_initialized () && balance_text.is_initialized () && type == "send")
			{
				rai::transaction transaction (node.store.environment, nullptr, false);
				if (node.store.block_exists (transaction, previous) && node.store.block_balance (transaction, previous) != balance.number ())
				{
					ec = nano::error_rpc::block_create_balance_mismatch;
				}
			}
			// Check for incorrect account key
			if (!ec && account_text.is_initialized ())
			{
				if (account != pub)
				{
					ec = nano::error_rpc::block_create_public_key_mismatch;
				}
			}
			if (type == "state")
			{
				if (previous_text.is_initialized () && !representative.is_zero () && (!link.is_zero () || link_text.is_initialized ()))
				{
					if (creation_time.is_zero ())
					{
						creation_time.set_time_now ();
					}
					if (work == 0)
					{
						work = node.work_generate_blocking (previous.is_zero () ? pub : previous);
					}
					rai::state_block state (pub, previous, creation_time.number (), representative, balance, link, prv, pub, work);
					response_l.put ("hash", state.hash ().to_string ());
					std::string contents;
					state.serialize_json (contents);
					response_l.put ("block", contents);
				}
				else
				{
					ec = nano::error_rpc::block_create_requirements_state;
				}
			}
			else
			{
				ec = nano::error_blocks::invalid_type;
			}
		}
		else
		{
			ec = nano::error_rpc::block_create_key_required;
		}
	}
	response_errors ();
}

void rai::rpc_handler::block_hash ()
{
	std::string block_text (request.get<std::string> ("block"));
	boost::property_tree::ptree block_l;
	std::stringstream block_stream (block_text);
	boost::property_tree::read_json (block_stream, block_l);
	block_l.put ("signature", "0");
	block_l.put ("work", "0");
	auto block (rai::deserialize_block_json (block_l));
	if (block != nullptr)
	{
		response_l.put ("hash", block->hash ().to_string ());
	}
	else
	{
		ec = nano::error_blocks::invalid_block;
	}
	response_errors ();
}

void rai::rpc_handler::bootstrap ()
{
	std::string address_text = request.get<std::string> ("address");
	std::string port_text = request.get<std::string> ("port");
	boost::system::error_code address_ec;
	auto address (boost::asio::ip::address_v6::from_string (address_text, address_ec));
	if (!address_ec)
	{
		uint16_t port;
		if (!rai::parse_port (port_text, port))
		{
			node.bootstrap_initiator.bootstrap (rai::endpoint (address, port));
			response_l.put ("success", "");
		}
		else
		{
			ec = nano::error_common::invalid_port;
		}
	}
	else
	{
		ec = nano::error_common::invalid_ip_address;
	}
	response_errors ();
}

void rai::rpc_handler::bootstrap_any ()
{
	node.bootstrap_initiator.bootstrap ();
	response_l.put ("success", "");
	response_errors ();
}

void rai::rpc_handler::chain (bool successors)
{
	auto hash (hash_impl ("block"));
	auto count (count_impl ());
	if (!ec)
	{
		boost::property_tree::ptree blocks;
		rai::transaction transaction (node.store.environment, nullptr, false);
		while (!hash.is_zero () && blocks.size () < count)
		{
			auto block_l (node.store.block_get (transaction, hash));
			if (block_l != nullptr)
			{
				boost::property_tree::ptree entry;
				entry.put ("", hash.to_string ());
				blocks.push_back (std::make_pair ("", entry));
				hash = successors ? node.store.block_successor (transaction, hash) : block_l->previous ();
			}
			else
			{
				hash.clear ();
			}
		}
		response_l.add_child ("blocks", blocks);
	}
	response_errors ();
}

void rai::rpc_handler::comment_search ()
{
	// No real implementation, always return empty
	boost::property_tree::ptree accounts;
	response_l.add_child ("accounts", accounts);
	response_l.add ("count", 0);
	response_l.add ("error", "NotImeplemented");
}

void rai::rpc_handler::confirmation_history ()
{
	boost::property_tree::ptree elections;
	{
		std::lock_guard<std::mutex> lock (node.active.mutex);
		for (auto i (node.active.confirmed.begin ()), n (node.active.confirmed.end ()); i != n; ++i)
		{
			boost::property_tree::ptree election;
			election.put ("hash", i->winner->hash ().to_string ());
			election.put ("tally", i->tally.to_string_dec ());
			elections.push_back (std::make_pair ("", election));
		}
	}
	response_l.add_child ("confirmations", elections);
	response_errors ();
}

void rai::rpc_handler::delegators ()
{
	auto account (account_impl ());
	if (!ec)
	{
		boost::property_tree::ptree delegators;
		rai::transaction transaction (node.store.environment, nullptr, false);
		for (auto i (node.store.latest_begin (transaction)), n (node.store.latest_end ()); i != n; ++i)
		{
			rai::account_info info (i->second);
			auto block (node.store.block_get (transaction, info.rep_block));
			assert (block != nullptr);
			if (block->representative () == account)
			{
				std::string balance;
				rai::amount (info.balance).encode_dec (balance);
				delegators.put (rai::account (i->first.uint256 ()).to_account (), balance);
			}
		}
		response_l.add_child ("delegators", delegators);
	}
	response_errors ();
}

void rai::rpc_handler::delegators_count ()
{
	auto account (account_impl ());
	if (!ec)
	{
		uint64_t count (0);
		rai::transaction transaction (node.store.environment, nullptr, false);
		for (auto i (node.store.latest_begin (transaction)), n (node.store.latest_end ()); i != n; ++i)
		{
			rai::account_info info (i->second);
			auto block (node.store.block_get (transaction, info.rep_block));
			assert (block != nullptr);
			if (block->representative () == account)
			{
				++count;
			}
		}
		response_l.put ("count", std::to_string (count));
	}
	response_errors ();
}

void rai::rpc_handler::deterministic_key ()
{
	std::string seed_text (request.get<std::string> ("seed"));
	std::string index_text (request.get<std::string> ("index"));
	rai::raw_key seed;
	if (!seed.data.decode_hex (seed_text))
	{
		try
		{
			uint32_t index (std::stoul (index_text));
			rai::uint256_union prv;
			rai::deterministic_key (seed.data, index, prv);
			rai::uint256_union pub (rai::pub_key (prv));
			response_l.put ("private", prv.to_string ());
			response_l.put ("public", pub.to_string ());
			response_l.put ("account", pub.to_account ());
		}
		catch (std::logic_error const &)
		{
			ec = nano::error_common::invalid_index;
		}
	}
	else
	{
		ec = nano::error_common::bad_seed;
	}
	response_errors ();
}

void rai::rpc_handler::frontiers ()
{
	auto start (account_impl ());
	auto count (count_impl ());
	if (!ec)
	{
		boost::property_tree::ptree frontiers;
		rai::transaction transaction (node.store.environment, nullptr, false);
		for (auto i (node.store.latest_begin (transaction, start)), n (node.store.latest_end ()); i != n && frontiers.size () < count; ++i)
		{
			frontiers.put (rai::account (i->first.uint256 ()).to_account (), rai::account_info (i->second).head.to_string ());
		}
		response_l.add_child ("frontiers", frontiers);
	}
	response_errors ();
}

void rai::rpc_handler::account_count ()
{
	rai::transaction transaction (node.store.environment, nullptr, false);
	auto size (node.store.account_count (transaction));
	response_l.put ("count", std::to_string (size));
	response_errors ();
}

namespace
{
class history_visitor : public rai::block_visitor
{
public:
	history_visitor (rai::rpc_handler & handler_a, bool raw_a, rai::transaction & transaction_a, boost::property_tree::ptree & tree_a, rai::block_hash const & hash_a) :
	handler (handler_a),
	raw (raw_a),
	transaction (transaction_a),
	tree (tree_a),
	hash (hash_a)
	{
	}
	virtual ~history_visitor () = default;
	void state_block (rai::state_block const & block_a)
	{
		if (raw)
		{
			tree.put ("type", "state");
			tree.put ("representative", block_a.hashables.representative.to_account ());
			tree.put ("link", block_a.hashables.link.to_string ());
			tree.put ("previous", block_a.hashables.previous.to_string ());
		}
		auto cur_balance (block_a.hashables.balance.number ());
		auto previous_balance = handler.node.ledger.balance (transaction, block_a.hashables.previous);
		auto amount_manna (handler.node.ledger.amount (transaction, block_a.hash ()));
		rai::state_block_subtype subtype = handler.node.ledger.state_subtype (transaction, block_a);
		switch (subtype)
		{
			case rai::state_block_subtype::open_receive:
				if (raw)
				{
					tree.put ("subtype", "open_receive");
				}
				else
				{
					tree.put ("type", "receive");
				}
				tree.put ("amount", block_a.hashables.balance.to_string_dec ());
				tree.put ("account", handler.node.ledger.account (transaction, block_a.hashables.link).to_account ());
				tree.put ("balance", block_a.hashables.balance.to_string_dec ());
				break;

			case rai::state_block_subtype::open_genesis:
				if (raw)
				{
					tree.put ("subtype", "open_genesis");
				}
				else
				{
					tree.put ("type", "receive");
				}
				tree.put ("amount", block_a.hashables.balance.to_string_dec ());
				tree.put ("account", block_a.hashables.account.to_account ()); // self
				tree.put ("balance", block_a.hashables.balance.to_string_dec ());
				break;

			case rai::state_block_subtype::send:
				if (raw)
				{
					tree.put ("subtype", "send");
				}
				else
				{
					tree.put ("type", "send");
				}
				tree.put ("account", block_a.hashables.link.to_account ());
				tree.put ("amount", std::to_string (amount_manna));
				tree.put ("balance", block_a.hashables.balance.to_string_dec ());
				break;

			case rai::state_block_subtype::receive:
				if (raw)
				{
					tree.put ("subtype", "receive");
				}
				else
				{
					tree.put ("type", "receive");
				}
				tree.put ("account", handler.node.ledger.account (transaction, block_a.hashables.link).to_account ());
				tree.put ("amount", std::to_string (amount_manna));
				tree.put ("balance", block_a.hashables.balance.to_string_dec ());
				break;

			// epoch and undefined not handled
			case rai::state_block_subtype::undefined:
			default:
				break;
		}
	}
	rai::rpc_handler & handler;
	bool raw;
	rai::transaction & transaction;
	boost::property_tree::ptree & tree;
	rai::block_hash const & hash;
};
}

void rai::rpc_handler::account_history ()
{
	rai::account account;
	bool output_raw (request.get_optional<bool> ("raw") == true);
	rai::block_hash hash;
	auto head_str (request.get_optional<std::string> ("head"));
	rai::transaction transaction (node.store.environment, nullptr, false);
	if (head_str)
	{
		if (!hash.decode_hex (*head_str))
		{
			account = node.ledger.account (transaction, hash);
		}
		else
		{
			ec = nano::error_blocks::bad_hash_number;
		}
	}
	else
	{
		account = account_impl ();
		if (!ec)
		{
			hash = node.ledger.latest (transaction, account);
		}
	}
	auto count (count_impl ());
	if (!ec)
	{
		uint64_t offset = 0;
		auto offset_text (request.get_optional<std::string> ("offset"));
		if (!offset_text || !decode_unsigned (*offset_text, offset))
		{
			boost::property_tree::ptree history;
			response_l.put ("account", account.to_account ());
			auto block (node.store.block_get (transaction, hash));
			while (block != nullptr && count > 0)
			{
				if (offset > 0)
				{
					--offset;
				}
				else
				{
					boost::property_tree::ptree entry;
					history_visitor visitor (*this, output_raw, transaction, entry, hash);
					block->visit (visitor);
					if (!entry.empty ())
					{
						entry.put ("block_time", block->creation_time ().to_posix_time ());
						entry.put ("hash", hash.to_string ());
						if (output_raw)
						{
							entry.put ("block_time_utc", block->creation_time ().to_date_string_utc ());
							entry.put ("work", rai::to_string_hex (block->block_work ()));
							entry.put ("signature", block->block_signature ().to_string ());
						}
						history.push_back (std::make_pair ("", entry));
					}
					--count;
				}
				hash = block->previous ();
				block = node.store.block_get (transaction, hash);
			}
			response_l.add_child ("history", history);
			if (!hash.is_zero ())
			{
				response_l.put ("previous", hash.to_string ());
			}
		}
		else
		{
			ec = nano::error_rpc::invalid_offset;
		}
	}
	response_errors ();
}

void rai::rpc_handler::keepalive ()
{
	rpc_control_impl ();
	if (!ec)
	{
		std::string address_text (request.get<std::string> ("address"));
		std::string port_text (request.get<std::string> ("port"));
		uint16_t port;
		if (!rai::parse_port (port_text, port))
		{
			node.keepalive (address_text, port);
			response_l.put ("started", "1");
		}
		else
		{
			ec = nano::error_common::invalid_port;
		}
	}
	response_errors ();
}

void rai::rpc_handler::key_create ()
{
	rai::keypair pair;
	response_l.put ("private", pair.prv.data.to_string ());
	response_l.put ("public", pair.pub.to_string ());
	response_l.put ("account", pair.pub.to_account ());
	response_errors ();
}

void rai::rpc_handler::key_expand ()
{
	std::string key_text (request.get<std::string> ("key"));
	rai::uint256_union prv;
	if (!prv.decode_hex (key_text))
	{
		rai::uint256_union pub (rai::pub_key (prv));
		response_l.put ("private", prv.to_string ());
		response_l.put ("public", pub.to_string ());
		response_l.put ("account", pub.to_account ());
	}
	else
	{
		ec = nano::error_common::bad_private_key;
	}
	response_errors ();
}

bool ledger_sort_by_balance (std::pair<rai::amount, std::pair<rai::account, rai::account_info>> & l1, std::pair<rai::amount, std::pair<rai::account, rai::account_info>> & l2)
{
	return l1.first.number () > l2.first.number ();
}

bool ledger_sort_by_time (std::pair<uint64_t, std::pair<rai::account, rai::account_info>> & l1, std::pair<uint64_t, std::pair<rai::account, rai::account_info>> & l2)
{
	return l1.first > l2.first;
}

void rai::rpc_handler::ledger ()
{
	auto count (count_optional_impl ());
	// For large counts it requires control
	if (count >= 100)
	{
		rpc_control_impl ();
	}
	if (!ec)
	{
		rai::account start (0);
		boost::optional<std::string> account_text (request.get_optional<std::string> ("account"));
		if (account_text.is_initialized ())
		{
			if (start.decode_account (account_text.get ()))
			{
				ec = nano::error_common::bad_account_number;
			}
		}
		rai::timestamp_t modified_since (0);
		boost::optional<std::string> modified_since_text (request.get_optional<std::string> ("modified_since"));
		if (modified_since_text.is_initialized ())
		{
			uint64_t modified_since_posix = strtoul (modified_since_text.get ().c_str (), NULL, 10);
			modified_since = rai::short_timestamp::convert_from_posix_time (modified_since_posix);
		}
		const bool sorting = request.get<bool> ("sorting", false);
		const bool sorting_by_time = request.get<bool> ("sorting_by_time", false);
		const bool representative = request.get<bool> ("representative", false);
		const bool weight = request.get<bool> ("weight", false);
		const bool pending = request.get<bool> ("pending", false);
		boost::property_tree::ptree accounts;
		rai::transaction transaction (node.store.environment, nullptr, false);
		if (!ec && !sorting && !sorting_by_time) // Simple unsorted
		{
			std::vector<std::pair<rai::account, rai::account_info>> account_infos_l;
			for (auto i (node.store.latest_begin (transaction, start)), n (node.store.latest_end ()); i != n && account_infos_l.size () < count; ++i)
			{
				rai::account_info info (i->second);
				if (info.last_block_time () >= modified_since)
				{
					account_infos_l.push_back (std::make_pair (i->first.uint256 (), info));
				}
			}
			ledger_helper_fill (transaction, account_infos_l, accounts, representative, weight, pending);
		}
		else if (!ec && sorting) // Sorted by balance
		{
			std::vector<std::pair<rai::amount, std::pair<rai::account, rai::account_info>>> ledger_l;
			for (auto i (node.store.latest_begin (transaction, start)), n (node.store.latest_end ()); i != n; ++i)
			{
				rai::account_info info (i->second);
				if (info.last_block_time () >= modified_since)
				{
					ledger_l.push_back (std::make_pair (info.balance, std::make_pair (i->first.uint256 (), info)));
				}
			}
			std::sort (ledger_l.begin (), ledger_l.end (), ::ledger_sort_by_balance);
			std::vector<std::pair<rai::account, rai::account_info>> account_infos_l;
			for (auto i (ledger_l.begin ()), n (ledger_l.end ()); i != n && account_infos_l.size () < count; ++i)
			{
				account_infos_l.push_back (i->second);
			}
			ledger_helper_fill (transaction, account_infos_l, accounts, representative, weight, pending);
		}
		else if (!ec && sorting_by_time) // Sorted by time
		{
			std::vector<std::pair<uint64_t, std::pair<rai::account, rai::account_info>>> ledger_l;
			for (auto i (node.store.latest_begin (transaction, start)), n (node.store.latest_end ()); i != n; ++i)
			{
				rai::account_info info (i->second);
				if (info.last_block_time () >= modified_since)
				{
					ledger_l.push_back (std::make_pair (info.last_block_time_intern, std::make_pair (i->first.uint256 (), info)));
				}
			}
			std::sort (ledger_l.begin (), ledger_l.end (), ::ledger_sort_by_time);
			std::vector<std::pair<rai::account, rai::account_info>> account_infos_l;
			for (auto i (ledger_l.begin ()), n (ledger_l.end ()); i != n && account_infos_l.size () < count; ++i)
			{
				account_infos_l.push_back (i->second);
			}
			ledger_helper_fill (transaction, account_infos_l, accounts, representative, weight, pending);
		}
		response_l.add_child ("accounts", accounts);
	}
	response_errors ();
}

void rai::rpc_handler::ledger_helper_fill (rai::transaction & transaction_a, std::vector<std::pair<rai::account, rai::account_info>> const & account_list_a, boost::property_tree::ptree & accounts_a, bool representative, bool weight, bool pending)
{
	for (auto i (account_list_a.begin ()), n (account_list_a.end ()); i != n; ++i)
	{
		rai::account account (i->first);
		rai::account_info info = i->second;
		boost::property_tree::ptree response_a;
		response_a.put ("frontier", info.head.to_string ());
		response_a.put ("open_block", info.open_block.to_string ());
		response_a.put ("representative_block", info.rep_block.to_string ());
		std::string balance;
		(info.balance).encode_dec (balance);
		response_a.put ("balance", balance);
		response_a.put ("last_block_time", rai::short_timestamp::convert_to_posix_time (info.last_block_time ()));
		response_a.put ("block_count", std::to_string (info.block_count));
		if (representative)
		{
			auto block (node.store.block_get (transaction_a, info.rep_block));
			assert (block != nullptr);
			response_a.put ("representative", block->representative ().to_account ());
		}
		if (weight)
		{
			auto account_weight (node.ledger.weight (transaction_a, account));
			response_a.put ("weight", std::to_string (account_weight));
		}
		if (pending)
		{
			auto account_pending (node.ledger.account_pending (transaction_a, account));
			response_a.put ("pending", std::to_string (account_pending));
		}
		accounts_a.push_back (std::make_pair (account.to_account (), response_a));
	}
}

void rai::rpc_handler::mrai_from_raw (rai::amount_t ratio)
{
	auto amount (amount_impl ());
	if (!ec)
	{
		auto result (amount.number () / ratio);
		response_l.put ("amount", std::to_string (result));
	}
	response_errors ();
}

void rai::rpc_handler::mrai_to_raw (rai::amount_t ratio)
{
	auto amount (amount_impl ());
	if (!ec)
	{
		auto result (amount.number () * ratio);
		if (result > amount.number ())
		{
			response_l.put ("amount", std::to_string (result));
		}
		else
		{
			ec = nano::error_common::invalid_amount_big;
		}
	}
	response_errors ();
}

void rai::rpc_handler::node_id_get ()
{
	rai::public_key node_id = node.node_id_pub_get ();
	response_l.put ("node_id", node_id.to_account ());
	response_errors ();
}

void rai::rpc_handler::node_id_reset ()
{
	node.node_id_reset ();
	rai::public_key node_id = node.node_id_pub_get ();
	response_l.put ("node_id", node_id.to_account ());
	response_errors ();
}

void rai::rpc_handler::node_id_set ()
{
	rpc_control_impl ();
	auto wallet (wallet_impl ());
	if (!ec)
	{
		bool wallet_unlocked = false;
		{
			rai::transaction transaction (node.store.environment, nullptr, true);
			wallet_unlocked = wallet->store.valid_password (transaction);
		}
		if (!wallet_unlocked)
		{
			ec = nano::error_common::wallet_locked;
		}
		else
		{
			std::string index_text (request.get<std::string> ("index"));
			int account_index = 0;
			try
			{
				account_index = std::stoi (index_text);
			}
			catch (...)
			{
				ec = nano::error_common::bad_account_index_number;
			}
			if (!ec)
			{
				auto error (node.set_node_id_from_wallet (wallet, account_index));
				if (error != 0)
				{
					ec = nano::error_common::wallet_not_found;
				}
				else
				{
					rai::public_key node_id = node.node_id_pub_get ();
					response_l.put ("node_id", node_id.to_account ());
				}
			}
		}
	}
	response_errors ();
}

void rai::rpc_handler::password_change ()
{
	rpc_control_impl ();
	auto wallet (wallet_impl ());
	if (!ec)
	{
		rai::transaction transaction (node.store.environment, nullptr, true);
		std::string password_text (request.get<std::string> ("password"));
		auto error (wallet->store.rekey (transaction, password_text));
		response_l.put ("changed", error ? "0" : "1");
	}
	response_errors ();
}

void rai::rpc_handler::password_enter ()
{
	auto wallet (wallet_impl ());
	if (!ec)
	{
		std::string password_text (request.get<std::string> ("password"));
		auto error (wallet->enter_password (password_text));
		response_l.put ("valid", error ? "0" : "1");
	}
	response_errors ();
}

void rai::rpc_handler::password_valid (bool wallet_locked)
{
	auto wallet (wallet_impl ());
	if (!ec)
	{
		rai::transaction transaction (node.store.environment, nullptr, false);
		auto valid (wallet->store.valid_password (transaction));
		if (!wallet_locked)
		{
			response_l.put ("valid", valid ? "1" : "0");
		}
		else
		{
			response_l.put ("locked", valid ? "0" : "1");
		}
	}
	response_errors ();
}

void rai::rpc_handler::peers ()
{
	boost::property_tree::ptree peers_l;
	auto peers_map (node.peers.map_by_endpoint ());
	for (auto i (peers_map.begin ()), n (peers_map.end ()); i != n; ++i)
	{
		boost::property_tree::ptree peer_l;
		std::stringstream endpoint;
		endpoint << i->second.endpoint; //i->first;
		peer_l.push_back (boost::property_tree::ptree::value_type ("endpoint", boost::property_tree::ptree (endpoint.str ())));
		peer_l.push_back (boost::property_tree::ptree::value_type ("net_version", boost::property_tree::ptree (std::to_string (i->second.protocol_info.version))));
		peer_l.push_back (boost::property_tree::ptree::value_type ("net_version_min", boost::property_tree::ptree (std::to_string (i->second.protocol_info.version_min))));
		peer_l.push_back (boost::property_tree::ptree::value_type ("net_version_max", boost::property_tree::ptree (std::to_string (i->second.protocol_info.version_max))));
		peer_l.push_back (boost::property_tree::ptree::value_type ("proto_extensions", boost::property_tree::ptree (std::to_string (i->second.protocol_info.extensions.to_ulong ()))));
		peer_l.push_back (boost::property_tree::ptree::value_type ("is_full_node", boost::property_tree::ptree (i->second.protocol_info.full_node_get () ? "true" : "false")));
		peer_l.push_back (boost::property_tree::ptree::value_type ("is_validating_node", boost::property_tree::ptree (i->second.protocol_info.validating_node_get () ? "true" : "false")));
		peer_l.push_back (boost::property_tree::ptree::value_type ("rep_weight", boost::property_tree::ptree (i->second.rep_weight.to_string ())));
		peer_l.push_back (boost::property_tree::ptree::value_type ("node_id", boost::property_tree::ptree (!i->second.node_id ? "" : i->second.node_id.get ().to_account ())));
		peers_l.push_back (std::make_pair ("", peer_l));
	}
	response_l.add_child ("peers", peers_l);
	response_errors ();
}

void rai::rpc_handler::pending ()
{
	auto account (account_impl ());
	auto count (count_optional_impl ());
	auto threshold (threshold_optional_impl ());
	const bool source = request.get<bool> ("source", false);
	const bool min_version = request.get<bool> ("min_version", false);
	if (!ec)
	{
		boost::property_tree::ptree peers_l;
		rai::transaction transaction (node.store.environment, nullptr, false);
		rai::account end (account.number () + 1);
		for (auto i (node.store.pending_begin (transaction, rai::pending_key (account, 0))), n (node.store.pending_begin (transaction, rai::pending_key (end, 0))); i != n && peers_l.size () < count; ++i)
		{
			rai::pending_key key (i->first);
			if (threshold.is_zero () && !source && !min_version)
			{
				boost::property_tree::ptree entry;
				entry.put ("", key.hash.to_string ());
				peers_l.push_back (std::make_pair ("", entry));
			}
			else
			{
				rai::pending_info info (i->second);
				if (info.amount.number () >= threshold.number ())
				{
					if (source || min_version)
					{
						boost::property_tree::ptree pending_tree;
						pending_tree.put ("amount", std::to_string (info.amount.number ()));
						if (source)
						{
							pending_tree.put ("source", info.source.to_account ());
						}
						peers_l.add_child (key.hash.to_string (), pending_tree);
					}
					else
					{
						peers_l.put (key.hash.to_string (), std::to_string (info.amount.number ()));
					}
				}
			}
		}
		response_l.add_child ("blocks", peers_l);
	}
	response_errors ();
}

void rai::rpc_handler::pending_exists ()
{
	auto hash (hash_impl ());
	if (!ec)
	{
		rai::transaction transaction (node.store.environment, nullptr, false);
		auto block (node.store.block_get (transaction, hash));
		if (block != nullptr)
		{
			auto exists (false);
			auto destination (node.ledger.block_destination (transaction, *block));
			if (!destination.is_zero ())
			{
				exists = node.store.pending_exists (transaction, rai::pending_key (destination, hash));
			}
			response_l.put ("exists", exists ? "1" : "0");
		}
		else
		{
			ec = nano::error_blocks::not_found;
		}
	}
	response_errors ();
}

void rai::rpc_handler::payment_begin ()
{
	std::string id_text (request.get<std::string> ("wallet"));
	rai::uint256_union id;
	if (!id.decode_hex (id_text))
	{
		auto existing (node.wallets.items.find (id));
		if (existing != node.wallets.items.end ())
		{
			rai::transaction transaction (node.store.environment, nullptr, true);
			std::shared_ptr<rai::wallet> wallet (existing->second);
			if (wallet->store.valid_password (transaction))
			{
				rai::account account (0);
				do
				{
					auto existing (wallet->free_accounts.begin ());
					if (existing != wallet->free_accounts.end ())
					{
						account = *existing;
						wallet->free_accounts.erase (existing);
						if (wallet->store.find (transaction, account) == wallet->store.end ())
						{
							BOOST_LOG (node.log) << boost::str (boost::format ("Transaction wallet %1% externally modified listing account %2% as free but no longer exists") % id.to_string () % account.to_account ());
							account.clear ();
						}
						else
						{
							if (node.ledger.account_balance (transaction, account) != 0)
							{
								BOOST_LOG (node.log) << boost::str (boost::format ("Skipping account %1% for use as a transaction account: non-zero balance") % account.to_account ());
								account.clear ();
							}
						}
					}
					else
					{
						account = wallet->deterministic_insert (transaction);
						break;
					}
				} while (account.is_zero ());
				if (!account.is_zero ())
				{
					response_l.put ("account", account.to_account ());
				}
				else
				{
					ec = nano::error_rpc::payment_unable_create_account;
				}
			}
			else
			{
				ec = nano::error_common::wallet_locked;
			}
		}
		else
		{
			ec = nano::error_common::wallet_not_found;
		}
	}
	else
	{
		ec = nano::error_common::bad_wallet_number;
	}
	response_errors ();
}

void rai::rpc_handler::payment_init ()
{
	auto wallet (wallet_impl ());
	if (!ec)
	{
		rai::transaction transaction (node.store.environment, nullptr, true);
		if (wallet->store.valid_password (transaction))
		{
			wallet->init_free_accounts (transaction);
			response_l.put ("status", "Ready");
		}
		else
		{
			ec = nano::error_common::wallet_locked;
		}
	}
	response_errors ();
}

void rai::rpc_handler::payment_end ()
{
	auto account (account_impl ());
	auto wallet (wallet_impl ());
	if (!ec)
	{
		rai::transaction transaction (node.store.environment, nullptr, false);
		auto existing (wallet->store.find (transaction, account));
		if (existing != wallet->store.end ())
		{
			if (node.ledger.account_balance (transaction, account) == 0)
			{
				wallet->free_accounts.insert (account);
				response_l.put ("ended", "1");
			}
			else
			{
				ec = nano::error_rpc::payment_account_balance;
			}
		}
		else
		{
			ec = nano::error_common::account_not_found_wallet;
		}
	}
	response_errors ();
}

void rai::rpc_handler::payment_wait ()
{
	std::string timeout_text (request.get<std::string> ("timeout"));
	auto account (account_impl ());
	auto amount (amount_impl ());
	if (!ec)
	{
		uint64_t timeout;
		if (!decode_unsigned (timeout_text, timeout))
		{
			{
				auto observer (std::make_shared<rai::payment_observer> (response, rpc, account, amount));
				observer->start (timeout);
				std::lock_guard<std::mutex> lock (rpc.mutex);
				assert (rpc.payment_observers.find (account) == rpc.payment_observers.end ());
				rpc.payment_observers[account] = observer;
			}
			rpc.observer_action (account);
		}
		else
		{
			ec = nano::error_rpc::bad_timeout;
		}
	}
	if (ec)
	{
		response_errors ();
	}
}

void rai::rpc_handler::process ()
{
	std::string block_text (request.get<std::string> ("block"));
	boost::property_tree::ptree block_l;
	std::stringstream block_stream (block_text);
	boost::property_tree::read_json (block_stream, block_l);
	std::shared_ptr<rai::block> block (rai::deserialize_block_json (block_l));
	if (block != nullptr)
	{
		if (!rai::work_validate (*block))
		{
			auto hash (block->hash ());
			node.block_arrival.add (hash);
			rai::process_return result;
			{
				rai::transaction transaction (node.store.environment, nullptr, true);
				result = node.block_processor.process_receive_one (transaction, block, std::chrono::steady_clock::time_point ());
			}
			switch (result.code)
			{
				case rai::process_result::progress:
				{
					response_l.put ("hash", hash.to_string ());
					break;
				}
				case rai::process_result::gap_previous:
				{
					ec = nano::error_process::gap_previous;
					break;
				}
				case rai::process_result::gap_source:
				{
					ec = nano::error_process::gap_source;
					break;
				}
				case rai::process_result::old:
				{
					ec = nano::error_process::old;
					break;
				}
				case rai::process_result::bad_signature:
				{
					ec = nano::error_process::bad_signature;
					break;
				}
				case rai::process_result::negative_spend:
				{
					// TODO once we get RPC versioning, this should be changed to "negative spend"
					ec = nano::error_process::negative_spend;
					break;
				}
				case rai::process_result::balance_mismatch:
				{
					ec = nano::error_process::balance_mismatch;
					break;
				}
				case rai::process_result::unreceivable:
				{
					ec = nano::error_process::unreceivable;
					break;
				}
				case rai::process_result::block_position:
				{
					ec = nano::error_process::block_position;
					break;
				}
				case rai::process_result::fork:
				{
					const bool force = request.get<bool> ("force", false);
					if (force && rpc.config.enable_control)
					{
						node.active.erase (*block);
						node.block_processor.force (block);
						response_l.put ("hash", hash.to_string ());
					}
					else
					{
						ec = nano::error_process::fork;
					}
					break;
				}
				default:
				{
					ec = nano::error_process::other;
					break;
				}
			}
		}
		else
		{
			ec = nano::error_blocks::work_low;
		}
	}
	else
	{
		ec = nano::error_blocks::invalid_block;
	}
	response_errors ();
}

void rai::rpc_handler::receive ()
{
	rpc_control_impl ();
	auto wallet (wallet_impl ());
	auto account (account_impl ());
	auto hash (hash_impl ("block"));
	if (!ec)
	{
		rai::transaction transaction (node.store.environment, nullptr, false);
		if (wallet->store.valid_password (transaction))
		{
			if (wallet->store.find (transaction, account) != wallet->store.end ())
			{
				auto block (node.store.block_get (transaction, hash));
				if (block != nullptr)
				{
					if (node.store.pending_exists (transaction, rai::pending_key (account, hash)))
					{
						auto work (work_optional_impl ());
						if (!ec && work)
						{
							rai::account_info info;
							rai::uint256_union head;
							if (!node.store.account_get (transaction, account, info))
							{
								head = info.head;
							}
							else
							{
								head = account;
							}
							if (!rai::work_validate (head, work))
							{
								rai::transaction transaction_a (node.store.environment, nullptr, true);
								wallet->store.work_put (transaction_a, account, work);
							}
							else
							{
								ec = nano::error_common::invalid_work;
							}
						}
						if (!ec)
						{
							auto response_a (response);
							wallet->receive_async (std::move (block), account, rai::genesis_amount, [response_a](std::shared_ptr<rai::block> block_a) {
								rai::uint256_union hash_a (0);
								if (block_a != nullptr)
								{
									hash_a = block_a->hash ();
								}
								boost::property_tree::ptree response_l;
								response_l.put ("block", hash_a.to_string ());
								response_a (response_l);
							},
							work == 0);
						}
					}
					else
					{
						ec = nano::error_process::unreceivable;
					}
				}
				else
				{
					ec = nano::error_blocks::not_found;
				}
			}
			else
			{
				ec = nano::error_common::account_not_found_wallet;
			}
		}
		else
		{
			ec = nano::error_common::wallet_locked;
		}
	}
	// Because of receive_async
	if (ec)
	{
		response_errors ();
	}
}

void rai::rpc_handler::receive_minimum ()
{
	rpc_control_impl ();
	if (!ec)
	{
		response_l.put ("amount", node.config.receive_minimum.to_string_dec ());
	}
	response_errors ();
}

void rai::rpc_handler::receive_minimum_set ()
{
	rpc_control_impl ();
	auto amount (amount_impl ());
	if (!ec)
	{
		node.config.receive_minimum = amount;
		response_l.put ("success", "");
	}
	response_errors ();
}

void rai::rpc_handler::representatives ()
{
	auto count (count_optional_impl ());
	if (!ec)
	{
		const bool sorting = request.get<bool> ("sorting", false);
		boost::property_tree::ptree representatives;
		rai::transaction transaction (node.store.environment, nullptr, false);
		if (!sorting) // Simple
		{
			for (auto i (node.store.representation_begin (transaction)), n (node.store.representation_end ()); i != n && representatives.size () < count; ++i)
			{
				rai::account account (i->first.uint256 ());
				auto amount (node.store.representation_get (transaction, account));
				representatives.put (account.to_account (), std::to_string (amount));
			}
		}
		else // Sorting
		{
			std::vector<std::pair<rai::amount, std::string>> representation;
			for (auto i (node.store.representation_begin (transaction)), n (node.store.representation_end ()); i != n; ++i)
			{
				rai::account account (i->first.uint256 ());
				auto amount (node.store.representation_get (transaction, account));
				representation.push_back (std::make_pair (amount, account.to_account ()));
			}
			std::sort (representation.begin (), representation.end ());
			std::reverse (representation.begin (), representation.end ());
			for (auto i (representation.begin ()), n (representation.end ()); i != n && representatives.size () < count; ++i)
			{
				representatives.put (i->second, std::to_string ((i->first).number ()));
			}
		}
		response_l.add_child ("representatives", representatives);
	}
	response_errors ();
}

void rai::rpc_handler::representatives_online ()
{
	boost::property_tree::ptree representatives;
	auto reps (node.online_reps.list ());
	for (auto & i : reps)
	{
		representatives.put (i.to_account (), "");
	}
	response_l.add_child ("representatives", representatives);
	response_errors ();
}

void rai::rpc_handler::republish ()
{
	auto count (count_optional_impl (1024U));
	uint64_t sources (0);
	uint64_t destinations (0);
	boost::optional<std::string> sources_text (request.get_optional<std::string> ("sources"));
	if (!ec && sources_text.is_initialized ())
	{
		if (decode_unsigned (sources_text.get (), sources))
		{
			ec = nano::error_rpc::invalid_sources;
		}
	}
	boost::optional<std::string> destinations_text (request.get_optional<std::string> ("destinations"));
	if (!ec && destinations_text.is_initialized ())
	{
		if (decode_unsigned (destinations_text.get (), destinations))
		{
			ec = nano::error_rpc::invalid_destinations;
		}
	}
	auto hash (hash_impl ());
	if (!ec)
	{
		boost::property_tree::ptree blocks;
		rai::transaction transaction (node.store.environment, nullptr, false);
		auto block (node.store.block_get (transaction, hash));
		if (block != nullptr)
		{
			for (auto i (0); !hash.is_zero () && i < count; ++i)
			{
				block = node.store.block_get (transaction, hash);
				if (sources != 0) // Republish source chain
				{
					rai::block_hash source (node.ledger.block_source (transaction, *block));
					std::unique_ptr<rai::block> block_a (node.store.block_get (transaction, source));
					std::vector<rai::block_hash> hashes;
					while (block_a != nullptr && hashes.size () < sources)
					{
						hashes.push_back (source);
						source = block_a->previous ();
						block_a = node.store.block_get (transaction, source);
					}
					std::reverse (hashes.begin (), hashes.end ());
					for (auto & hash_l : hashes)
					{
						block_a = node.store.block_get (transaction, hash_l);
						node.network.republish_block (transaction, std::move (block_a));
						boost::property_tree::ptree entry_l;
						entry_l.put ("", hash_l.to_string ());
						blocks.push_back (std::make_pair ("", entry_l));
					}
				}
				node.network.republish_block (transaction, std::move (block)); // Republish block
				boost::property_tree::ptree entry;
				entry.put ("", hash.to_string ());
				blocks.push_back (std::make_pair ("", entry));
				if (destinations != 0) // Republish destination chain
				{
					auto block_b (node.store.block_get (transaction, hash));
					auto destination (node.ledger.block_destination (transaction, *block_b));
					if (!destination.is_zero ())
					{
						if (!node.store.pending_exists (transaction, rai::pending_key (destination, hash)))
						{
							rai::block_hash previous (node.ledger.latest (transaction, destination));
							std::unique_ptr<rai::block> block_d (node.store.block_get (transaction, previous));
							rai::block_hash source;
							std::vector<rai::block_hash> hashes;
							while (block_d != nullptr && hash != source)
							{
								hashes.push_back (previous);
								source = node.ledger.block_source (transaction, *block_d);
								previous = block_d->previous ();
								block_d = node.store.block_get (transaction, previous);
							}
							std::reverse (hashes.begin (), hashes.end ());
							if (hashes.size () > destinations)
							{
								hashes.resize (destinations);
							}
							for (auto & hash_l : hashes)
							{
								block_d = node.store.block_get (transaction, hash_l);
								node.network.republish_block (transaction, std::move (block_d));
								boost::property_tree::ptree entry_l;
								entry_l.put ("", hash_l.to_string ());
								blocks.push_back (std::make_pair ("", entry_l));
							}
						}
					}
				}
				hash = node.store.block_successor (transaction, hash);
			}
			response_l.put ("success", ""); // obsolete
			response_l.add_child ("blocks", blocks);
		}
		else
		{
			ec = nano::error_blocks::not_found;
		}
	}
	response_errors ();
}

void rai::rpc_handler::search_pending ()
{
	rpc_control_impl ();
	auto wallet (wallet_impl ());
	if (!ec)
	{
		auto error (wallet->search_pending ());
		response_l.put ("started", !error);
	}
	response_errors ();
}

void rai::rpc_handler::search_pending_all ()
{
	rpc_control_impl ();
	if (!ec)
	{
		node.wallets.search_pending_all ();
		response_l.put ("success", "");
	}
	response_errors ();
}

void rai::rpc_handler::send ()
{
	rpc_control_impl ();
	auto wallet (wallet_impl ());
	auto amount (amount_impl ());
	if (!ec)
	{
		if (!wallet->valid_password ())
		{
			ec = nano::error_common::wallet_locked;
		}
		else
		{
			std::string source_text (request.get<std::string> ("source"));
			rai::account source;
			if (source.decode_account (source_text))
			{
				ec = nano::error_rpc::bad_source;
			}
			else
			{
				std::string destination_text (request.get<std::string> ("destination"));
				rai::account destination;
				if (destination.decode_account (destination_text))
				{
					ec = nano::error_rpc::bad_destination;
				}
				else
				{
					if (source == destination)
					{
						ec = nano::error_common::send_to_self_invalid;
					}
					else
					{
						if (amount == 0)
						{
							ec = nano::error_common::send_zero_invalid;
						}
						else
						{
							auto work (work_optional_impl ());
							rai::amount_t balance (0);
							if (!ec)
							{
								rai::transaction transaction (node.store.environment, nullptr, work != 0); // false if no "work" in request, true if work > 0
								rai::account_info info;
								if (!node.store.account_get (transaction, source, info))
								{
									balance = (info.balance).number ();
								}
								else
								{
									ec = nano::error_common::account_not_found;
								}
								if (!ec && work)
								{
									if (!rai::work_validate (info.head, work))
									{
										wallet->store.work_put (transaction, source, work);
									}
									else
									{
										ec = nano::error_common::invalid_work;
									}
								}
							}
							if (!ec)
							{
								boost::optional<std::string> send_id (request.get_optional<std::string> ("id"));
								if (balance < amount.number ())
								{
									ec = nano::error_common::insufficient_balance;
								}
								else
								{
									auto rpc_l (shared_from_this ());
									auto response_a (response);
									wallet->send_async (source, destination, amount.number (), [response_a](std::shared_ptr<rai::block> block_a) {
										if (block_a == nullptr)
										{
											error_response (response_a, "Error generating block");
										}
										else
										{
											rai::uint256_union hash (block_a->hash ());
											boost::property_tree::ptree response_l;
											response_l.put ("block", hash.to_string ());
											response_a (response_l);
										}
									},
									work == 0, send_id);
								}
							}
						}
					}
				}
			}
		}
	}
	// Because of send_async
	if (ec)
	{
		response_errors ();
	}
}

void rai::rpc_handler::stats ()
{
	auto sink = node.stats.log_sink_json ();
	std::string type (request.get<std::string> ("type", ""));
	if (type == "counters")
	{
		node.stats.log_counters (*sink);
	}
	else if (type == "samples")
	{
		node.stats.log_samples (*sink);
	}
	else
	{
		ec = nano::error_rpc::invalid_missing_type;
	}
	if (!ec)
	{
		response (*static_cast<boost::property_tree::ptree *> (sink->to_object ()));
	}
	else
	{
		response_errors ();
	}
}

void rai::rpc_handler::stop ()
{
	rpc_control_impl ();
	if (!ec)
	{
		response_l.put ("success", "");
	}
	response_errors ();
	if (!ec)
	{
		rpc.stop ();
		node.stop ();
	}
}

void rai::rpc_handler::unchecked ()
{
	auto count (count_optional_impl ());
	if (!ec)
	{
		boost::property_tree::ptree unchecked;
		rai::transaction transaction (node.store.environment, nullptr, false);
		for (auto i (node.store.unchecked_begin (transaction)), n (node.store.unchecked_end ()); i != n && unchecked.size () < count; ++i)
		{
			rai::bufferstream stream (reinterpret_cast<uint8_t const *> (i->second.data ()), i->second.size ());
			auto block (rai::deserialize_block (stream));
			std::string contents;
			block->serialize_json (contents);
			unchecked.put (block->hash ().to_string (), contents);
		}
		response_l.add_child ("blocks", unchecked);
	}
	response_errors ();
}

void rai::rpc_handler::unchecked_clear ()
{
	rpc_control_impl ();
	if (!ec)
	{
		rai::transaction transaction (node.store.environment, nullptr, true);
		node.store.unchecked_clear (transaction);
		response_l.put ("success", "");
	}
	response_errors ();
}

void rai::rpc_handler::unchecked_get ()
{
	auto hash (hash_impl ());
	if (!ec)
	{
		rai::transaction transaction (node.store.environment, nullptr, false);
		for (auto i (node.store.unchecked_begin (transaction)), n (node.store.unchecked_end ()); i != n; ++i)
		{
			rai::bufferstream stream (reinterpret_cast<uint8_t const *> (i->second.data ()), i->second.size ());
			auto block (rai::deserialize_block (stream));
			if (block->hash () == hash)
			{
				std::string contents;
				block->serialize_json (contents);
				response_l.put ("contents", contents);
				break;
			}
		}
		if (response_l.empty ())
		{
			ec = nano::error_blocks::not_found;
		}
	}
	response_errors ();
}

void rai::rpc_handler::unchecked_keys ()
{
	auto count (count_optional_impl ());
	rai::uint256_union key (0);
	boost::optional<std::string> hash_text (request.get_optional<std::string> ("key"));
	if (!ec && hash_text.is_initialized ())
	{
		if (key.decode_hex (hash_text.get ()))
		{
			ec = nano::error_rpc::bad_key;
		}
	}
	if (!ec)
	{
		boost::property_tree::ptree unchecked;
		rai::transaction transaction (node.store.environment, nullptr, false);
		for (auto i (node.store.unchecked_begin (transaction, key)), n (node.store.unchecked_end ()); i != n && unchecked.size () < count; ++i)
		{
			boost::property_tree::ptree entry;
			rai::bufferstream stream (reinterpret_cast<uint8_t const *> (i->second.data ()), i->second.size ());
			auto block (rai::deserialize_block (stream));
			std::string contents;
			block->serialize_json (contents);
			entry.put ("key", rai::block_hash (i->first.uint256 ()).to_string ());
			entry.put ("hash", block->hash ().to_string ());
			entry.put ("contents", contents);
			unchecked.push_back (std::make_pair ("", entry));
		}
		response_l.add_child ("unchecked", unchecked);
	}
	response_errors ();
}

void rai::rpc_handler::version ()
{
	response_l.put ("rpc_version", "1");
	response_l.put ("store_version", std::to_string (node.store_version ()));
	response_l.put ("node_vendor", boost::str (boost::format ("Mikron %1%.%2%.%3%") % RAIBLOCKS_VERSION_MAJOR % RAIBLOCKS_VERSION_MINOR % RAIBLOCKS_VERSION_PATCH));
	response_errors ();
}

void rai::rpc_handler::validate_account_number ()
{
	std::string account_text (request.get<std::string> ("account"));
	rai::uint256_union account;
	auto error (account.decode_account (account_text));
	response_l.put ("valid", error ? "0" : "1");
	response_errors ();
}

void rai::rpc_handler::wallet_add ()
{
	rpc_control_impl ();
	auto wallet (wallet_impl ());
	if (!ec)
	{
		std::string key_text (request.get<std::string> ("key"));
		rai::raw_key key;
		if (!key.data.decode_hex (key_text))
		{
			const bool generate_work = request.get<bool> ("work", true);
			auto pub (wallet->insert_adhoc (key, generate_work));
			if (!pub.is_zero ())
			{
				response_l.put ("account", pub.to_account ());
			}
			else
			{
				ec = nano::error_common::wallet_locked;
			}
		}
		else
		{
			ec = nano::error_common::bad_private_key;
		}
	}
	response_errors ();
}

void rai::rpc_handler::wallet_add_watch ()
{
	rpc_control_impl ();
	auto wallet (wallet_impl ());
	if (!ec)
	{
		rai::transaction transaction (node.store.environment, nullptr, true);
		if (wallet->store.valid_password (transaction))
		{
			for (auto & accounts : request.get_child ("accounts"))
			{
				auto account (account_impl (accounts.second.data ()));
				if (!ec)
				{
					wallet->insert_watch (transaction, account);
				}
			}
			response_l.put ("success", "");
		}
		else
		{
			ec = nano::error_common::wallet_locked;
		}
	}
	response_errors ();
}

void rai::rpc_handler::wallet_info ()
{
	auto wallet (wallet_impl ());
	if (!ec)
	{
		rai::amount_t balance (0);
		rai::amount_t pending (0);
		uint64_t count (0);
		uint64_t deterministic_count (0);
		uint64_t adhoc_count (0);
		rai::transaction transaction (node.store.environment, nullptr, false);
		for (auto i (wallet->store.begin (transaction)), n (wallet->store.end ()); i != n; ++i)
		{
			rai::account account (i->first.uint256 ());
			balance = balance + node.ledger.account_balance (transaction, account);
			pending = pending + node.ledger.account_pending (transaction, account);
			rai::key_type key_type (wallet->store.key_type (i->second));
			if (key_type == rai::key_type::deterministic)
			{
				deterministic_count++;
			}
			else if (key_type == rai::key_type::adhoc)
			{
				adhoc_count++;
			}
			count++;
		}
		uint32_t deterministic_index (wallet->store.deterministic_index_get (transaction));
		response_l.put ("balance", std::to_string (balance));
		response_l.put ("pending", std::to_string (pending));
		response_l.put ("accounts_count", std::to_string (count));
		response_l.put ("deterministic_count", std::to_string (deterministic_count));
		response_l.put ("adhoc_count", std::to_string (adhoc_count));
		response_l.put ("deterministic_index", std::to_string (deterministic_index));
	}
	response_errors ();
}

void rai::rpc_handler::wallet_balances ()
{
	auto wallet (wallet_impl ());
	auto threshold (threshold_optional_impl ());
	if (!ec)
	{
		boost::property_tree::ptree balances;
		rai::transaction transaction (node.store.environment, nullptr, false);
		for (auto i (wallet->store.begin (transaction)), n (wallet->store.end ()); i != n; ++i)
		{
			rai::account account (i->first.uint256 ());
			rai::amount_t balance = node.ledger.account_balance (transaction, account);
			if (balance >= threshold.number ())
			{
				boost::property_tree::ptree entry;
				rai::amount_t pending = node.ledger.account_pending (transaction, account);
				entry.put ("balance", std::to_string (balance));
				entry.put ("pending", std::to_string (pending));
				balances.push_back (std::make_pair (account.to_account (), entry));
			}
		}
		response_l.add_child ("balances", balances);
	}
	response_errors ();
}

void rai::rpc_handler::wallet_change_seed ()
{
	rpc_control_impl ();
	auto wallet (wallet_impl ());
	if (!ec)
	{
		std::string seed_text (request.get<std::string> ("seed"));
		rai::raw_key seed;
		if (!seed.data.decode_hex (seed_text))
		{
			rai::transaction transaction (node.store.environment, nullptr, true);
			if (wallet->store.valid_password (transaction))
			{
				wallet->change_seed (transaction, seed);
				response_l.put ("success", "");
			}
			else
			{
				ec = nano::error_common::wallet_locked;
			}
		}
		else
		{
			ec = nano::error_common::bad_seed;
		}
	}
	response_errors ();
}

void rai::rpc_handler::wallet_contains ()
{
	auto account (account_impl ());
	auto wallet (wallet_impl ());
	if (!ec)
	{
		rai::transaction transaction (node.store.environment, nullptr, false);
		auto exists (wallet->store.find (transaction, account) != wallet->store.end ());
		response_l.put ("exists", exists ? "1" : "0");
	}
	response_errors ();
}

void rai::rpc_handler::wallet_create ()
{
	rpc_control_impl ();
	if (!ec)
	{
		rai::keypair wallet_id;
		node.wallets.create (wallet_id.pub);
		rai::transaction transaction (node.store.environment, nullptr, false);
		auto existing (node.wallets.items.find (wallet_id.pub));
		if (existing != node.wallets.items.end ())
		{
			response_l.put ("wallet", wallet_id.pub.to_string ());
		}
		else
		{
			ec = nano::error_common::wallet_lmdb_max_dbs;
		}
	}
	response_errors ();
}

void rai::rpc_handler::wallet_destroy ()
{
	rpc_control_impl ();
	if (!ec)
	{
		std::string wallet_text (request.get<std::string> ("wallet"));
		rai::uint256_union wallet;
		if (!wallet.decode_hex (wallet_text))
		{
			auto existing (node.wallets.items.find (wallet));
			if (existing != node.wallets.items.end ())
			{
				node.wallets.destroy (wallet);
				bool destroyed (node.wallets.items.find (wallet) == node.wallets.items.end ());
				response_l.put ("destroyed", destroyed ? "1" : "0");
			}
			else
			{
				ec = nano::error_common::wallet_not_found;
			}
		}
		else
		{
			ec = nano::error_common::bad_wallet_number;
		}
	}
	response_errors ();
}

void rai::rpc_handler::wallet_export ()
{
	auto wallet (wallet_impl ());
	if (!ec)
	{
		rai::transaction transaction (node.store.environment, nullptr, false);
		std::string json;
		wallet->store.serialize_json (transaction, json);
		response_l.put ("json", json);
	}
	response_errors ();
}

void rai::rpc_handler::wallet_frontiers ()
{
	auto wallet (wallet_impl ());
	if (!ec)
	{
		boost::property_tree::ptree frontiers;
		rai::transaction transaction (node.store.environment, nullptr, false);
		for (auto i (wallet->store.begin (transaction)), n (wallet->store.end ()); i != n; ++i)
		{
			rai::account account (i->first.uint256 ());
			auto latest (node.ledger.latest (transaction, account));
			if (!latest.is_zero ())
			{
				frontiers.put (account.to_account (), latest.to_string ());
			}
		}
		response_l.add_child ("frontiers", frontiers);
	}
	response_errors ();
}

void rai::rpc_handler::wallet_key_valid ()
{
	auto wallet (wallet_impl ());
	if (!ec)
	{
		rai::transaction transaction (node.store.environment, nullptr, false);
		auto valid (wallet->store.valid_password (transaction));
		response_l.put ("valid", valid ? "1" : "0");
	}
	response_errors ();
}

void rai::rpc_handler::wallet_ledger ()
{
	const bool representative = request.get<bool> ("representative", false);
	const bool weight = request.get<bool> ("weight", false);
	const bool pending = request.get<bool> ("pending", false);
	rai::timestamp_t modified_since (0);
	boost::optional<std::string> modified_since_text (request.get_optional<std::string> ("modified_since"));
	if (modified_since_text.is_initialized ())
	{
		uint64_t modified_since_posix = strtoul (modified_since_text.get ().c_str (), NULL, 10);
		modified_since = rai::short_timestamp::convert_from_posix_time (modified_since_posix);
	}
	auto wallet (wallet_impl ());
	if (!ec)
	{
		boost::property_tree::ptree accounts;
		rai::transaction transaction (node.store.environment, nullptr, false);
		for (auto i (wallet->store.begin (transaction)), n (wallet->store.end ()); i != n; ++i)
		{
			rai::account account (i->first.uint256 ());
			rai::account_info info;
			if (!node.store.account_get (transaction, account, info))
			{
				if (info.last_block_time () >= modified_since)
				{
					boost::property_tree::ptree entry;
					entry.put ("frontier", info.head.to_string ());
					entry.put ("open_block", info.open_block.to_string ());
					entry.put ("representative_block", info.rep_block.to_string ());
					std::string balance;
					rai::amount (info.balance).encode_dec (balance);
					entry.put ("balance", balance);
					entry.put ("last_block_time", rai::short_timestamp::convert_to_posix_time (info.last_block_time ()));
					entry.put ("block_count", std::to_string (info.block_count));
					if (representative)
					{
						auto block (node.store.block_get (transaction, info.rep_block));
						assert (block != nullptr);
						entry.put ("representative", block->representative ().to_account ());
					}
					if (weight)
					{
						auto account_weight (node.ledger.weight (transaction, account));
						entry.put ("weight", std::to_string (account_weight));
					}
					if (pending)
					{
						auto account_pending (node.ledger.account_pending (transaction, account));
						entry.put ("pending", std::to_string (account_pending));
					}
					accounts.push_back (std::make_pair (account.to_account (), entry));
				}
			}
		}
		response_l.add_child ("accounts", accounts);
	}
	response_errors ();
}

void rai::rpc_handler::wallet_lock ()
{
	rpc_control_impl ();
	auto wallet (wallet_impl ());
	if (!ec)
	{
		rai::raw_key empty;
		empty.data.clear ();
		wallet->store.password.value_set (empty);
		response_l.put ("locked", "1");
	}
	response_errors ();
}

void rai::rpc_handler::wallet_pending ()
{
	auto wallet (wallet_impl ());
	auto count (count_optional_impl ());
	auto threshold (threshold_optional_impl ());
	const bool source = request.get<bool> ("source", false);
	const bool min_version = request.get<bool> ("min_version", false);
	const bool include_active = request.get<bool> ("include_active", false);
	if (!ec)
	{
		boost::property_tree::ptree pending;
		rai::transaction transaction (node.store.environment, nullptr, false);
		for (auto i (wallet->store.begin (transaction)), n (wallet->store.end ()); i != n; ++i)
		{
			rai::account account (i->first.uint256 ());
			boost::property_tree::ptree peers_l;
			rai::account end (account.number () + 1);
			for (auto ii (node.store.pending_begin (transaction, rai::pending_key (account, 0))), nn (node.store.pending_begin (transaction, rai::pending_key (end, 0))); ii != nn && peers_l.size () < count; ++ii)
			{
				rai::pending_key key (ii->first);
				std::shared_ptr<rai::block> block (node.store.block_get (transaction, key.hash));
				assert (block);
				if (include_active || (block && !node.active.active (*block)))
				{
					if (threshold.is_zero () && !source)
					{
						boost::property_tree::ptree entry;
						entry.put ("", key.hash.to_string ());
						peers_l.push_back (std::make_pair ("", entry));
					}
					else
					{
						rai::pending_info info (ii->second);
						if (info.amount.number () >= threshold.number ())
						{
							if (source || min_version)
							{
								boost::property_tree::ptree pending_tree;
								pending_tree.put ("amount", std::to_string (info.amount.number ()));
								if (source)
								{
									pending_tree.put ("source", info.source.to_account ());
								}
								peers_l.add_child (key.hash.to_string (), pending_tree);
							}
							else
							{
								peers_l.put (key.hash.to_string (), std::to_string (info.amount.number ()));
							}
						}
					}
				}
			}
			if (!peers_l.empty ())
			{
				pending.add_child (account.to_account (), peers_l);
			}
		}
		response_l.add_child ("blocks", pending);
	}
	response_errors ();
}

void rai::rpc_handler::wallet_representative ()
{
	auto wallet (wallet_impl ());
	if (!ec)
	{
		rai::transaction transaction (node.store.environment, nullptr, false);
		response_l.put ("representative", wallet->store.representative (transaction).to_account ());
	}
	response_errors ();
}

void rai::rpc_handler::wallet_representative_set ()
{
	rpc_control_impl ();
	auto wallet (wallet_impl ());
	if (!ec)
	{
		std::string representative_text (request.get<std::string> ("representative"));
		rai::account representative;
		if (!representative.decode_account (representative_text))
		{
			rai::transaction transaction (node.store.environment, nullptr, true);
			wallet->store.representative_set (transaction, representative);
			response_l.put ("set", "1");
		}
		else
		{
			ec = nano::error_rpc::bad_representative_number;
		}
	}
	response_errors ();
}

void rai::rpc_handler::wallet_republish ()
{
	rpc_control_impl ();
	auto wallet (wallet_impl ());
	auto count (count_impl ());
	if (!ec)
	{
		boost::property_tree::ptree blocks;
		rai::transaction transaction (node.store.environment, nullptr, false);
		for (auto i (wallet->store.begin (transaction)), n (wallet->store.end ()); i != n; ++i)
		{
			rai::account account (i->first.uint256 ());
			auto latest (node.ledger.latest (transaction, account));
			std::unique_ptr<rai::block> block;
			std::vector<rai::block_hash> hashes;
			while (!latest.is_zero () && hashes.size () < count)
			{
				hashes.push_back (latest);
				block = node.store.block_get (transaction, latest);
				latest = block->previous ();
			}
			std::reverse (hashes.begin (), hashes.end ());
			for (auto & hash : hashes)
			{
				block = node.store.block_get (transaction, hash);
				node.network.republish_block (transaction, std::move (block));
				boost::property_tree::ptree entry;
				entry.put ("", hash.to_string ());
				blocks.push_back (std::make_pair ("", entry));
			}
		}
		response_l.add_child ("blocks", blocks);
	}
	response_errors ();
}

void rai::rpc_handler::wallet_work_get ()
{
	rpc_control_impl ();
	auto wallet (wallet_impl ());
	if (!ec)
	{
		boost::property_tree::ptree works;
		rai::transaction transaction (node.store.environment, nullptr, false);
		for (auto i (wallet->store.begin (transaction)), n (wallet->store.end ()); i != n; ++i)
		{
			rai::account account (i->first.uint256 ());
			uint64_t work (0);
			auto error_work (wallet->store.work_get (transaction, account, work));
			works.put (account.to_account (), rai::to_string_hex (work));
		}
		response_l.add_child ("works", works);
	}
	response_errors ();
}

void rai::rpc_handler::work_generate ()
{
	rpc_control_impl ();
	auto hash (hash_impl ());
	if (!ec)
	{
		bool use_peers (request.get_optional<bool> ("use_peers") == true);
		auto rpc_l (shared_from_this ());
		auto callback = [rpc_l](boost::optional<uint64_t> const & work_a) {
			if (work_a)
			{
				boost::property_tree::ptree response_l;
				response_l.put ("work", rai::to_string_hex (work_a.value ()));
				rpc_l->response (response_l);
			}
			else
			{
				error_response (rpc_l->response, "Cancelled");
			}
		};
		if (!use_peers)
		{
			node.work.generate (hash, callback);
		}
		else
		{
			node.work_generate (hash, callback);
		}
	}
	// Because of callback
	if (ec)
	{
		response_errors ();
	}
}

void rai::rpc_handler::work_cancel ()
{
	rpc_control_impl ();
	auto hash (hash_impl ());
	if (!ec)
	{
		node.work.cancel (hash);
	}
	response_errors ();
}

void rai::rpc_handler::work_get ()
{
	rpc_control_impl ();
	auto wallet (wallet_impl ());
	auto account (account_impl ());
	if (!ec)
	{
		rai::transaction transaction (node.store.environment, nullptr, false);
		if (wallet->store.find (transaction, account) != wallet->store.end ())
		{
			uint64_t work (0);
			auto error_work (wallet->store.work_get (transaction, account, work));
			response_l.put ("work", rai::to_string_hex (work));
		}
		else
		{
			ec = nano::error_common::account_not_found_wallet;
		}
	}
	response_errors ();
}

void rai::rpc_handler::work_set ()
{
	rpc_control_impl ();
	auto wallet (wallet_impl ());
	auto account (account_impl ());
	auto work (work_optional_impl ());
	if (!ec)
	{
		rai::transaction transaction (node.store.environment, nullptr, true);
		if (wallet->store.find (transaction, account) != wallet->store.end ())
		{
			wallet->store.work_put (transaction, account, work);
			response_l.put ("success", "");
		}
		else
		{
			ec = nano::error_common::account_not_found_wallet;
		}
	}
	response_errors ();
}

void rai::rpc_handler::work_validate ()
{
	auto hash (hash_impl ());
	auto work (work_optional_impl ());
	if (!ec)
	{
		auto validate (rai::work_validate (hash, work));
		response_l.put ("valid", validate ? "0" : "1");
	}
	response_errors ();
}

void rai::rpc_handler::work_peer_add ()
{
	rpc_control_impl ();
	if (!ec)
	{
		std::string address_text = request.get<std::string> ("address");
		std::string port_text = request.get<std::string> ("port");
		uint16_t port;
		if (!rai::parse_port (port_text, port))
		{
			node.config.work_peers.push_back (std::make_pair (address_text, port));
			response_l.put ("success", "");
		}
		else
		{
			ec = nano::error_common::invalid_port;
		}
	}
	response_errors ();
}

void rai::rpc_handler::work_peers ()
{
	rpc_control_impl ();
	if (!ec)
	{
		boost::property_tree::ptree work_peers_l;
		for (auto i (node.config.work_peers.begin ()), n (node.config.work_peers.end ()); i != n; ++i)
		{
			boost::property_tree::ptree entry;
			entry.put ("", boost::str (boost::format ("%1%:%2%") % i->first % i->second));
			work_peers_l.push_back (std::make_pair ("", entry));
		}
		response_l.add_child ("work_peers", work_peers_l);
	}
	response_errors ();
}

void rai::rpc_handler::work_peers_clear ()
{
	rpc_control_impl ();
	if (!ec)
	{
		node.config.work_peers.clear ();
		response_l.put ("success", "");
	}
	response_errors ();
}

rai::rpc_connection::rpc_connection (rai::node & node_a, rai::rpc & rpc_a) :
node (node_a.shared ()),
rpc (rpc_a),
socket (node_a.service)
{
	responded.clear ();
}

void rai::rpc_connection::parse_connection ()
{
	read ();
}

void rai::rpc_connection::write_result (std::string body, unsigned version)
{
	if (!responded.test_and_set ())
	{
		res.set ("Content-Type", "application/json");
		res.set ("Access-Control-Allow-Origin", "*");
		res.set ("Access-Control-Allow-Headers", "Accept, Accept-Language, Content-Language, Content-Type");
		res.set ("Connection", "close");
		res.result (boost::beast::http::status::ok);
		res.body () = body;
		res.version (version);
		res.prepare_payload ();
	}
	else
	{
		assert (false && "RPC already responded and should only respond once");
		// Guards `res' from being clobbered while async_write is being serviced
	}
}

void rai::rpc_connection::read ()
{
	auto this_l (shared_from_this ());
	boost::beast::http::async_read (socket, buffer, request, [this_l](boost::system::error_code const & ec, size_t bytes_transferred) {
		if (!ec)
		{
			this_l->node->background ([this_l]() {
				auto start (std::chrono::steady_clock::now ());
				auto version (this_l->request.version ());
				std::string request_id (boost::str (boost::format ("%1%") % boost::io::group (std::hex, std::showbase, reinterpret_cast<uintptr_t> (this_l.get ()))));
				auto response_handler ([this_l, version, start, request_id](boost::property_tree::ptree const & tree_a) {
					std::stringstream ostream;
					boost::property_tree::write_json (ostream, tree_a);
					ostream.flush ();
					auto body (ostream.str ());
					this_l->write_result (body, version);
					boost::beast::http::async_write (this_l->socket, this_l->res, [this_l](boost::system::error_code const & ec, size_t bytes_transferred) {
					});

					if (this_l->node->config.logging.log_rpc ())
					{
						BOOST_LOG (this_l->node->log) << boost::str (boost::format ("RPC request %2% completed in: %1% microseconds") % std::chrono::duration_cast<std::chrono::microseconds> (std::chrono::steady_clock::now () - start).count () % request_id);
					}
				});
				if (this_l->request.method () == boost::beast::http::verb::post)
				{
					auto handler (std::make_shared<rai::rpc_handler> (*this_l->node, this_l->rpc, this_l->request.body (), request_id, response_handler));
					handler->process_request ();
				}
				else
				{
					error_response (response_handler, "Can only POST requests");
				}
			});
		}
		else
		{
			BOOST_LOG (this_l->node->log) << "RPC read error: " << ec.message ();
		}
	});
}

namespace
{
void reprocess_body (std::string & body, boost::property_tree::ptree & tree_a)
{
	std::stringstream stream;
	boost::property_tree::write_json (stream, tree_a);
	body = stream.str ();
}
}

void rai::rpc_handler::process_request ()
{
	try
	{
		auto max_depth_exceeded (false);
		auto max_depth_possible (0);
		for (auto ch : body)
		{
			if (ch == '[' || ch == '{')
			{
				if (max_depth_possible >= rpc.config.max_json_depth)
				{
					max_depth_exceeded = true;
					break;
				}
				++max_depth_possible;
			}
		}
		if (max_depth_exceeded)
		{
			error_response (response, "Max JSON depth exceeded");
		}
		else
		{
			std::stringstream istream (body);
			boost::property_tree::read_json (istream, request);
			std::string action (request.get<std::string> ("action"));
			if (action == "password_enter")
			{
				password_enter ();
				request.erase ("password");
				reprocess_body (body, request);
			}
			else if (action == "password_change")
			{
				password_change ();
				request.erase ("password");
				reprocess_body (body, request);
			}
			else if (action == "wallet_unlock")
			{
				password_enter ();
				request.erase ("password");
				reprocess_body (body, request);
			}
			if (node.config.logging.log_rpc ())
			{
				BOOST_LOG (node.log) << boost::str (boost::format ("%1% ") % request_id) << body;
			}
			if (action == "account_balance")
			{
				account_balance ();
			}
			else if (action == "account_block_count")
			{
				account_block_count ();
			}
			else if (action == "account_count")
			{
				account_count ();
			}
			else if (action == "account_create")
			{
				account_create ();
			}
			else if (action == "account_get")
			{
				account_get ();
			}
			else if (action == "account_history")
			{
				account_history ();
			}
			else if (action == "account_info")
			{
				account_info ();
			}
			else if (action == "account_key")
			{
				account_key ();
			}
			else if (action == "account_list")
			{
				account_list ();
			}
			else if (action == "account_move")
			{
				account_move ();
			}
			else if (action == "account_remove")
			{
				account_remove ();
			}
			else if (action == "account_representative")
			{
				account_representative ();
			}
			else if (action == "account_representative_set")
			{
				account_representative_set ();
			}
			else if (action == "account_weight")
			{
				account_weight ();
			}
			else if (action == "accounts_balances")
			{
				accounts_balances ();
			}
			else if (action == "accounts_create")
			{
				accounts_create ();
			}
			else if (action == "accounts_frontiers")
			{
				accounts_frontiers ();
			}
			else if (action == "accounts_infos")
			{
				accounts_infos ();
			}
			else if (action == "accounts_pending")
			{
				accounts_pending ();
			}
			else if (action == "available_supply")
			{
				available_supply ();
			}
			else if (action == "block")
			{
				block ();
			}
			else if (action == "block_confirm")
			{
				block_confirm ();
			}
			else if (action == "blocks")
			{
				blocks ();
			}
			else if (action == "blocks_info")
			{
				blocks_info ();
			}
			else if (action == "block_account")
			{
				block_account ();
			}
			else if (action == "block_count")
			{
				block_count ();
			}
			else if (action == "block_count_type")
			{
				block_count_type ();
			}
			else if (action == "block_create")
			{
				block_create ();
			}
			else if (action == "block_hash")
			{
				block_hash ();
			}
			else if (action == "successors")
			{
				chain (true);
			}
			else if (action == "bootstrap")
			{
				bootstrap ();
			}
			else if (action == "bootstrap_any")
			{
				bootstrap_any ();
			}
			else if (action == "chain")
			{
				chain ();
			}
			else if (action == "delegators")
			{
				delegators ();
			}
			else if (action == "delegators_count")
			{
				delegators_count ();
			}
			else if (action == "deterministic_key")
			{
				deterministic_key ();
			}
			else if (action == "comment_search")
			{
				comment_search ();
			}
			else if (action == "confirmation_history")
			{
				confirmation_history ();
			}
			else if (action == "frontiers")
			{
				frontiers ();
			}
			else if (action == "frontier_count")
			{
				account_count ();
			}
			else if (action == "history")
			{
				request.put ("head", request.get<std::string> ("hash"));
				account_history ();
			}
			else if (action == "keepalive")
			{
				keepalive ();
			}
			else if (action == "key_create")
			{
				key_create ();
			}
			else if (action == "key_expand")
			{
				key_expand ();
			}
			/*
			else if (action == "krai_from_raw")
			{
				mrai_from_raw (rai::kxrb_ratio);
			}
			else if (action == "krai_to_raw")
			{
				mrai_to_raw (rai::kxrb_ratio);
			}
			*/
			else if (action == "ledger")
			{
				ledger ();
			}
			else if (action == "mrai_from_raw")
			{
				mrai_from_raw ();
			}
			else if (action == "mrai_to_raw")
			{
				mrai_to_raw ();
			}
			else if (action == "node_id_get")
			{
				node_id_get ();
			}
			else if (action == "node_id_reset")
			{
				node_id_reset ();
			}
			else if (action == "node_id_set")
			{
				node_id_set ();
			}
			else if (action == "password_change")
			{
				// Processed before logging
			}
			else if (action == "password_enter")
			{
				// Processed before logging
			}
			else if (action == "password_valid")
			{
				password_valid ();
			}
			else if (action == "payment_begin")
			{
				payment_begin ();
			}
			else if (action == "payment_init")
			{
				payment_init ();
			}
			else if (action == "payment_end")
			{
				payment_end ();
			}
			else if (action == "payment_wait")
			{
				payment_wait ();
			}
			else if (action == "peers")
			{
				peers ();
			}
			else if (action == "pending")
			{
				pending ();
			}
			else if (action == "pending_exists")
			{
				pending_exists ();
			}
			else if (action == "process")
			{
				process ();
			}
			else if (action == "rai_from_raw")
			{
				mrai_from_raw (rai::xrb_ratio);
			}
			else if (action == "rai_to_raw")
			{
				mrai_to_raw (rai::xrb_ratio);
			}
			else if (action == "receive")
			{
				receive ();
			}
			else if (action == "receive_minimum")
			{
				receive_minimum ();
			}
			else if (action == "receive_minimum_set")
			{
				receive_minimum_set ();
			}
			else if (action == "representatives")
			{
				representatives ();
			}
			else if (action == "representatives_online")
			{
				representatives_online ();
			}
			else if (action == "republish")
			{
				republish ();
			}
			else if (action == "search_pending")
			{
				search_pending ();
			}
			else if (action == "search_pending_all")
			{
				search_pending_all ();
			}
			else if (action == "send")
			{
				send ();
			}
			else if (action == "stats")
			{
				stats ();
			}
			else if (action == "stop")
			{
				stop ();
			}
			else if (action == "unchecked")
			{
				unchecked ();
			}
			else if (action == "unchecked_clear")
			{
				unchecked_clear ();
			}
			else if (action == "unchecked_get")
			{
				unchecked_get ();
			}
			else if (action == "unchecked_keys")
			{
				unchecked_keys ();
			}
			else if (action == "validate_account_number")
			{
				validate_account_number ();
			}
			else if (action == "version")
			{
				version ();
			}
			else if (action == "wallet_add")
			{
				wallet_add ();
			}
			else if (action == "wallet_add_watch")
			{
				wallet_add_watch ();
			}
			// Obsolete
			else if (action == "wallet_balance_total")
			{
				wallet_info ();
			}
			else if (action == "wallet_balances")
			{
				wallet_balances ();
			}
			else if (action == "wallet_change_seed")
			{
				wallet_change_seed ();
			}
			else if (action == "wallet_contains")
			{
				wallet_contains ();
			}
			else if (action == "wallet_create")
			{
				wallet_create ();
			}
			else if (action == "wallet_destroy")
			{
				wallet_destroy ();
			}
			else if (action == "wallet_export")
			{
				wallet_export ();
			}
			else if (action == "wallet_frontiers")
			{
				wallet_frontiers ();
			}
			else if (action == "wallet_info")
			{
				wallet_info ();
			}
			else if (action == "wallet_key_valid")
			{
				wallet_key_valid ();
			}
			else if (action == "wallet_ledger")
			{
				wallet_ledger ();
			}
			else if (action == "wallet_lock")
			{
				wallet_lock ();
			}
			else if (action == "wallet_locked")
			{
				password_valid (true);
			}
			else if (action == "wallet_pending")
			{
				wallet_pending ();
			}
			else if (action == "wallet_representative")
			{
				wallet_representative ();
			}
			else if (action == "wallet_representative_set")
			{
				wallet_representative_set ();
			}
			else if (action == "wallet_republish")
			{
				wallet_republish ();
			}
			else if (action == "wallet_unlock")
			{
				// Processed before logging
			}
			else if (action == "wallet_work_get")
			{
				wallet_work_get ();
			}
			else if (action == "work_generate")
			{
				work_generate ();
			}
			else if (action == "work_cancel")
			{
				work_cancel ();
			}
			else if (action == "work_get")
			{
				work_get ();
			}
			else if (action == "work_set")
			{
				work_set ();
			}
			else if (action == "work_validate")
			{
				work_validate ();
			}
			else if (action == "work_peer_add")
			{
				work_peer_add ();
			}
			else if (action == "work_peers")
			{
				work_peers ();
			}
			else if (action == "work_peers_clear")
			{
				work_peers_clear ();
			}
			else
			{
				error_response (response, "Unknown command");
			}
		}
	}
	catch (std::runtime_error const & err)
	{
		error_response (response, "Unable to parse JSON");
	}
	catch (...)
	{
		error_response (response, "Internal server error in RPC");
	}
}

rai::payment_observer::payment_observer (std::function<void(boost::property_tree::ptree const &)> const & response_a, rai::rpc & rpc_a, rai::account const & account_a, rai::amount const & amount_a) :
rpc (rpc_a),
account (account_a),
amount (amount_a),
response (response_a)
{
	completed.clear ();
}

void rai::payment_observer::start (uint64_t timeout)
{
	auto this_l (shared_from_this ());
	rpc.node.alarm.add (std::chrono::steady_clock::now () + std::chrono::milliseconds (timeout), [this_l]() {
		this_l->complete (rai::payment_status::nothing);
	});
}

rai::payment_observer::~payment_observer ()
{
}

void rai::payment_observer::observe ()
{
	if (rpc.node.balance (account) >= amount.number ())
	{
		complete (rai::payment_status::success);
	}
}

void rai::payment_observer::complete (rai::payment_status status)
{
	auto already (completed.test_and_set ());
	if (!already)
	{
		if (rpc.node.config.logging.log_rpc ())
		{
			BOOST_LOG (rpc.node.log) << boost::str (boost::format ("Exiting payment_observer for account %1% status %2%") % account.to_account () % static_cast<unsigned> (status));
		}
		switch (status)
		{
			case rai::payment_status::nothing:
			{
				boost::property_tree::ptree response_l;
				response_l.put ("status", "nothing");
				response (response_l);
				break;
			}
			case rai::payment_status::success:
			{
				boost::property_tree::ptree response_l;
				response_l.put ("status", "success");
				response (response_l);
				break;
			}
			default:
			{
				error_response (response, "Internal payment error");
				break;
			}
		}
		std::lock_guard<std::mutex> lock (rpc.mutex);
		assert (rpc.payment_observers.find (account) != rpc.payment_observers.end ());
		rpc.payment_observers.erase (account);
	}
}

std::unique_ptr<rai::rpc> rai::get_rpc (boost::asio::io_service & service_a, rai::node & node_a, rai::rpc_config const & config_a)
{
	std::unique_ptr<rpc> impl;

	if (config_a.secure.enable)
	{
#ifdef RAIBLOCKS_SECURE_RPC
		impl.reset (new rpc_secure (service_a, node_a, config_a));
#else
		std::cerr << "RPC configured for TLS, but the node is not compiled with TLS support" << std::endl;
#endif
	}
	else
	{
		impl.reset (new rpc (service_a, node_a, config_a));
	}

	return impl;
}
