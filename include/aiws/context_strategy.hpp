#pragma once

#include "aiws/processing_types.hpp"

#include <cstddef>
#include <vector>

namespace aiws {

// safe abstract polymorphic base class for configurable context construction
// virtual destructor allows deletion through a base-class pointer
// pure virtual build() forces every derived strategy to implement it
// and blocks direct instantiation of contextstrategy itself
class ContextStrategy {
public:
    virtual ~ContextStrategy() = default;

    virtual std::vector<ContextItem> build(const std::vector<SearchResult>&,
                                           std::size_t) const = 0;
};

}  // namespace aiws