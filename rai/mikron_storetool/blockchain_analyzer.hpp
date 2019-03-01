#include <boost/filesystem.hpp>
#include <map>
#include <rai/lib/numbers.hpp>
#include <rai/node/node.hpp>
#include <set>
#include <vector>

namespace rai
{
class blockchain_analyzer
{
public:
	void analyze_account_chain_length (boost::filesystem::path);
	//static std::vector <rai::block_hash> pick_random_frontiers (int, std::shared_ptr <rai::node> &);
	long get_account_chain_length (rai::block_hash);
	static void printMedianStats (std::string const &, std::vector<long> &);
private:
	std::shared_ptr<rai::node> node;
	rai::transaction* transaction;
	//std::vector <rai::block_hash> blocks;
};
}
