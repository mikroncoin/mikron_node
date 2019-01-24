#include <boost/filesystem.hpp>
#include <map>
#include <rai/lib/numbers.hpp>
#include <rai/node/node.hpp>
#include <set>
#include <vector>

namespace rai
{
class span_info
{
public:
	rai::block_hash hash;
	std::set <rai::account> accounts;
	std::set <rai::block_hash> blocks;
	void merge (rai::span_info &);
	void merge_acc (const rai::account &);
	void merge_block (const rai::block_hash &);
};

class block_span_analyzer
{
public:
	void analyze (boost::filesystem::path);
	//static std::vector <rai::block_hash> pick_random_blocks (int, std::shared_ptr <rai::node> &);
	span_info analyze_block (rai::block_hash);
	span_info get_block_span (rai::block_hash);
private:
	std::shared_ptr <rai::node> node;
	rai::transaction * transaction;
	std::vector <rai::block_hash> blocks;
	std::map <rai::block_hash, span_info> span_map;
};
}
