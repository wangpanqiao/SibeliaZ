#include "blocksfinder.h"


namespace Sibelia
{
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>

#ifdef _WIN32
#include <direct.h>
#endif

	void CreateOutDirectory(const std::string & path)
	{
		int result = 0;
#ifdef _WIN32
		result = _mkdir(path.c_str());
#else
		result = mkdir(path.c_str(), 0755);
#endif
		if (result != 0 && errno != EEXIST)
		{
			throw std::runtime_error(("Cannot create dir " + path).c_str());
		}
	}

	JunctionStorage * JunctionStorage::this_;
	extern const std::string VERSION = "1.0.0";

	bool compareById(const BlockInstance & a, const BlockInstance & b)
	{
		return CompareBlocks(a, b, &BlockInstance::GetBlockId);
	}

	bool compareByChrId(const BlockInstance & a, const BlockInstance & b)
	{
		return CompareBlocks(a, b, &BlockInstance::GetChrId);
	}

	bool compareByStart(const BlockInstance & a, const BlockInstance & b)
	{
		return CompareBlocks(a, b, &BlockInstance::GetChrId);
	}

	const std::string DELIMITER(80, '-');

	int BlockInstance::GetSignedBlockId() const
	{
		return id_;
	}

	bool BlockInstance::GetDirection() const
	{
		return id_ > 0;
	}

	int BlockInstance::GetSign() const
	{
		return GetSignedBlockId() > 0 ? +1 : -1;
	}

	int BlockInstance::GetBlockId() const
	{
		return abs(id_);
	}

	size_t BlockInstance::GetChrId() const
	{
		return chr_;
	}

	size_t BlockInstance::GetStart() const
	{
		return start_;
	}

	size_t BlockInstance::GetEnd() const
	{
		return end_;
	}

	size_t BlockInstance::GetConventionalStart() const
	{
		if (GetDirection())
		{
			return start_ + 1;
		}

		return end_;
	}

	size_t BlockInstance::GetConventionalEnd() const
	{
		if (GetDirection())
		{
			return end_;
		}

		return start_ + 1;
	}

	std::pair<size_t, size_t> BlockInstance::CalculateOverlap(const BlockInstance & instance) const
	{
		if (GetChrId() == instance.GetChrId())
		{
			size_t overlap = 0;
			if (GetStart() >= instance.GetStart() && GetStart() <= instance.GetEnd())
			{
				return std::pair<size_t, size_t>(GetStart(), min(GetEnd(), instance.GetEnd()));
			}

			if (instance.GetStart() >= GetStart() && instance.GetStart() <= GetEnd())
			{
				return std::pair<size_t, size_t>(instance.GetStart(), min(GetEnd(), instance.GetEnd()));
			}
		}

		return std::pair<size_t, size_t>(0, 0);
	}

	bool BlockInstance::operator == (const BlockInstance & toCompare) const
	{
		return start_ == toCompare.start_ && end_ == toCompare.end_ && GetChrId() == toCompare.GetChrId() && id_ == toCompare.id_;
	}

	bool BlockInstance::operator != (const BlockInstance & toCompare) const
	{
		return !(*this == toCompare);
	}

	void BlockInstance::Reverse()
	{
		id_ = -id_;
	}

	size_t BlockInstance::GetLength() const
	{
		return end_ - start_;
	}

	bool BlockInstance::operator < (const BlockInstance & toCompare) const
	{
		return std::make_pair(GetBlockId(), std::make_pair(GetChrId(), GetStart())) < std::make_pair(toCompare.GetBlockId(), std::make_pair(toCompare.GetChrId(), toCompare.GetStart()));
	}

	double BlocksFinder::CalculateCoverage(const BlockList & block) const
	{
		size_t totalSize = 0;
		for (int64_t i = 0; i < storage_.GetChrNumber(); i++)
		{
			totalSize += storage_.GetChrSequence(i).size();
		}

		size_t totalBlockLength = 0;
		for (const auto & b : block)
		{
			totalBlockLength += b.GetLength();
		}

		return double(totalBlockLength) / totalSize;
	}

	void BlocksFinder::GenerateReport(const BlockList & block, const std::string & fileName) const
	{
		std::ofstream out;
		TryOpenFile(fileName, out);
		GroupedBlockList sepBlock;
		std::vector<IndexPair> group;
		BlockList blockList = block;
		GroupBy(blockList, compareById, std::back_inserter(group));
		for (std::vector<IndexPair>::iterator it = group.begin(); it != group.end(); ++it)
		{
			sepBlock.push_back(std::make_pair(it->second - it->first, std::vector<BlockInstance>(blockList.begin() + it->first, blockList.begin() + it->second)));
		}

		ListChrs(out);
		out << "Degree\tCount\tTotal";
		for (int64_t i = 0; i < storage_.GetChrNumber(); i++)
		{
			out << "\tSeq " << i + 1;
		}

		size_t totalSize = 0;
		for (int64_t i = 0; i < storage_.GetChrNumber(); i++)
		{
			totalSize += storage_.GetChrSequence(i).size();
		}

		out << std::endl;
		group.clear();
		std::vector<size_t> coverage;
		GroupBy(sepBlock, ByFirstElement, std::back_inserter(group));
		group.push_back(IndexPair(0, sepBlock.size()));
		for (std::vector<IndexPair>::iterator it = group.begin(); it != group.end(); ++it)
		{
			if (it != group.end() - 1)
			{
				out << sepBlock[it->first].first << '\t' << it->second - it->first << '\t';
			}
			else
			{
				out << "All\t" << it->second - it->first << "\t";
			}

			out.precision(2);
			out.setf(std::ostream::fixed);
			coverage.assign(storage_.GetChrNumber(), 0);
			for (GroupedBlockList::const_iterator jt = sepBlock.begin() + it->first; jt != sepBlock.begin() + it->second; ++jt)
			{
				for (size_t i = 0; i < jt->second.size(); i++)
				{
					coverage[jt->second[i].GetChrId()] += jt->second[i].GetLength();
				}				
			}

			size_t total = std::accumulate(coverage.begin(), coverage.end(), size_t(0));
			out << double(total) / totalSize * 100 << '%';
			for (int64_t i = 0; i < storage_.GetChrNumber(); i++)
			{
				out << '\t' << double(coverage[i]) / storage_.GetChrSequence(i).size() * 100 << '%';
			}

			out << std::endl;
		}
	}

	std::string BlocksFinder::OutputIndex(const BlockInstance & block) const
	{
		std::stringstream out;
		out << block.GetChrId() + 1 << '\t' << (block.GetSignedBlockId() < 0 ? '-' : '+') << '\t';
		out << block.GetConventionalStart() << '\t' << block.GetConventionalEnd() << '\t' << block.GetEnd() - block.GetStart();
		return out.str();
	}


	void BlocksFinder::OutputBlocks(const std::vector<BlockInstance>& block, std::ofstream& out) const
	{
		std::vector<IndexPair> group;
		std::vector<BlockInstance> blockList = block;
		GroupBy(blockList, compareById, std::back_inserter(group));
		for (std::vector<IndexPair>::iterator it = group.begin(); it != group.end(); ++it)
		{
			size_t length = it->second - it->first;
			std::sort(blockList.begin() + it->first, blockList.begin() + it->second, compareByChrId);
			out << "Block #" << blockList[it->first].GetBlockId() << std::endl;
			out << "Seq_id\tStrand\tStart\tEnd\tLength" << std::endl;
			for (auto jt = blockList.begin() + it->first; jt < blockList.begin() + it->first + length; ++jt)
			{
				out << OutputIndex(*jt) << std::endl;
			}

			out << DELIMITER << std::endl;
		}
	}


	void BlocksFinder::ListBlocksIndices(const BlockList & block, const std::string & fileName) const
	{
		std::ofstream out;
		TryOpenFile(fileName, out);
		ListChrs(out);
		OutputBlocks(block, out);
	}

	void BlocksFinder::ListChromosomesAsPermutations(const BlockList & block, const std::string & fileName) const
	{
		std::ofstream out;
		TryOpenFile(fileName, out);
		std::vector<IndexPair> group;
		BlockList blockList = block;
		GroupBy(blockList, compareByChrId, std::back_inserter(group));
		for (std::vector<IndexPair>::iterator it = group.begin(); it != group.end(); ++it)
		{
			out.setf(std::ios_base::showpos);
			size_t length = it->second - it->first;
			size_t chr = blockList[it->first].GetChrId();
			out << '>' << storage_.GetChrDescription(chr) << std::endl;
			std::sort(blockList.begin() + it->first, blockList.begin() + it->second);
			for (auto jt = blockList.begin() + it->first; jt < blockList.begin() + it->first + length; ++jt)
			{
				out << jt->GetSignedBlockId() << " ";
			}

			out << "$" << std::endl;
		}
	}

	template<class It>
	std::string Join(It start, It end, const std::string & delimiter)
	{
		It last = --end;
		std::stringstream ss;
		for (; start != end; ++start)
		{
			ss << *start << delimiter;
		}

		ss << *last;
		return ss.str();
	}


	void BlocksFinder::ListBlocksIndicesGFF(const BlockList & blockList, const std::string & fileName) const
	{
		std::ofstream out;
		TryOpenFile(fileName, out);
		BlockList block(blockList);
		std::sort(block.begin(), block.end(), compareById);
		const std::string header[] =
		{
			"##gff-version 2",
			std::string("##source-version SibeliaZ ") + VERSION,
			"##Type DNA"
		};

		out << Join(header, header + 3, "\n") << std::endl;
		for (BlockList::const_iterator it = block.begin(); it != block.end(); ++it)
		{
			size_t start = it->GetStart() + 1;
			size_t end = it->GetEnd();
			const std::string record[] =
			{
				storage_.GetChrDescription(it->GetChrId()), 
				"SibeliaZ",
				"LCB_copy",
				IntToStr(start),
				IntToStr(end),
				".",
				(it->GetDirection() ? "+" : "-"),
				".",
				"id=" + IntToStr(static_cast<size_t>(it->GetBlockId()))
			};

			out << Join(record, record + sizeof(record) / sizeof(record[0]), "\t") << std::endl;
		}
	}

	void BlocksFinder::TryOpenFile(const std::string & fileName, std::ofstream & stream) const
	{
		stream.open(fileName.c_str());
		if (!stream)
		{
			throw std::runtime_error(("Cannot open file " + fileName).c_str());
		}
	}

	void BlocksFinder::ListChrs(std::ostream & out) const
	{
		out << "Seq_id\tSize\tDescription" << std::endl;
		for (int64_t i = 0; i < storage_.GetChrNumber(); i++)
		{
			out << i + 1 << '\t' << storage_.GetChrSequence(i).size() << '\t' << storage_.GetChrDescription(i) << std::endl;
		}

		out << DELIMITER << std::endl;
	}
}
