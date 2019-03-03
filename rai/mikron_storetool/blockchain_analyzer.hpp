#include <boost/filesystem.hpp>
#include <map>
#include <rai/lib/numbers.hpp>
#include <rai/node/node.hpp>
#include <set>
#include <vector>

namespace rai
{
struct chain_length
{
	// Length of the account chain only
	long len;
	// Length of the account chain plus of the opening account chain, recursively up to the genesis block
	long len_full;
	// Number of account chains in the full path
	long account_count;
};

class blockchain_analyzer
{
public:
	void analyze_account_chain_length (boost::filesystem::path);
	void analyze_account_chain_length_full (boost::filesystem::path);
	static std::vector<std::pair<rai::account, rai::block_hash>> pick_random_frontiers (int, std::shared_ptr<rai::node> &);
	static void printMedianStats (std::string const &, std::vector<long> &);
protected:
	long get_account_chain_length (rai::block_hash);
	rai::chain_length get_account_chain_length_full (rai::block_hash);
private:
	std::shared_ptr<rai::node> node;
	rai::transaction* transaction;
};
}
