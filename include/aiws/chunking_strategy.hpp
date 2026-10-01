#pragma once

#include "aiws/document.hpp"
#include "aiws/processing_types.hpp"

#include <cstddef>
#include <vector>

namespace aiws {

// safe abstract polymorphic base class for configurable chunking
// virtual destructor allows deletion through a base-class pointer
// pure virtual chunk() forces every derived strategy to implement it
// and blocks direct instantiation of chunkingstrategy itself
class ChunkingStrategy {
public:
    virtual ~ChunkingStrategy() = default;

    virtual std::vector<Chunk> chunk(const Document&,
                                     std::size_t) const = 0;
};

}  // namespace aiws