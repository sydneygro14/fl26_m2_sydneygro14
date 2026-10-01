#pragma once

#include "aiws/corpus_index.hpp"
#include "aiws/processing_types.hpp"

#include <string>
#include <vector>

namespace aiws {

// virtual destructor allows deletion through a base-class pointer
// pure virtual search() forces every derived strategy to implement it
// and blocks direct instantiation of retrievalstrategy itself
class RetrievalStrategy {
public:
    virtual ~RetrievalStrategy() = default;

    virtual std::vector<SearchResult> search(const std::string&,
                                             int,
                                             const std::vector<Chunk>&,
                                             const CorpusIndex&) const = 0;
};

}  // namespace aiws