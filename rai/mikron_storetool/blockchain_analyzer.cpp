#include <rai/mikron_storetool/blockchain_analyzer.hpp>
#include <rai/node/node.hpp>
#include <random>

void rai::blockchain_analyzer::analyze_account_chain_length (boost::filesystem::path data_path)
{
	std::cout << "debug_blockchain_len " << std::endl;
	rai::inactive_node * node_a = new rai::inactive_node (data_path);
	node = node_a->node;
	transaction = new rai::transaction (node->store.environment, nullptr, false);
	//blocks = pick_random_blocks (100, node);
	//long n = blocks.size ();
	//std::map<rai::block_hash, rai::span_info> account_spans;
	std::vector<long> account_chain_lengths;
	long cnt = 0;
	for (auto i (node->store.latest_begin (*transaction)), n (node->store.latest_end ()); i != n; ++i, ++cnt)
	{
		std::cerr << "i_cnt " << cnt << " "; // std::endl;
		rai::account account = rai::account (i->first.uint256 ());
		rai::account_info account_info = rai::account_info (i->second);
		rai::block_hash block_hash = account_info.head;
		long length = get_account_chain_length (block_hash);
		account_chain_lengths.push_back (length);
		std::cerr << std::endl << "i_cnt " << cnt << " len " << length << " bl " << account.to_account () << " " << block_hash.to_string () << std::endl;
		std::cout << length << std::endl;
	}
	// computing median
	rai::blockchain_analyzer::printMedianStats ("Account chain lengths", account_chain_lengths);
}

long rai::blockchain_analyzer::get_account_chain_length (rai::block_hash hash)
{
	// Non-recursive iterative implementation (recursive does not work with large depths)
	if (hash.number () == 0)
	{
		return 0;
	}
	// current state
	rai::block_hash cur_hash (hash);
	long cur_len = 0;
	while (true)
	{
		if (cur_hash.number () == 0)
		{
			break;
		}
		auto block (node->store.block_get (*transaction, cur_hash));
		if (!block)
		{
			break;
		}
		// visit current block
		++cur_len;
		// move to previous
		cur_hash = block->previous ();
	}
	return cur_len;
}

void rai::blockchain_analyzer::printMedianStats (std::string const & prefix, std::vector<long> & numbers)
{
	std::sort (numbers.begin (), numbers.end ());
	auto n (numbers.size ());
	if (n == 0)
		return;
	auto min (numbers [0]);
	auto max (numbers [n - 1]);
	auto median (numbers [n / 2]);
	long sum = 0;
	for (long i = 0; i < n; ++i) sum += numbers[i];
	double avg = (double)sum / (double)n;
	std::cout << "Stats for " << prefix << ":" << std::endl;
	std::cout << "  cnt " << n << " range " << min << " -- " << max << " avg " << avg << "  median " << median << std::endl;
	std::cout << "  percentiles  0: " << min << "  1: " << numbers [(long)((double)n * 0.01)] << "  5: " << numbers [(long)((double)n * 0.05)] << "  10: " << numbers [(long)((double)n * 0.10)] << "  25: " << numbers [(long)((double)n * 0.25)] << "  50: " << median << "  75: " << numbers [(long)((double)n * 0.75)] << "  90: " << numbers [(long)((double)n * 0.90)] << "  95: " << numbers [(long)((double)n * 0.95)] << "  99: " << numbers [(long)((double)n * 0.99)] << "  100: " << max << std::endl;
}
