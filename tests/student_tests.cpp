#include "aiws/chunker.hpp"
#include "aiws/chunking_strategy.hpp"
#include "aiws/context_builder.hpp"
#include "aiws/context_strategy.hpp"
#include "aiws/processing_core.hpp"
#include "aiws/retrieval_engine.hpp"
#include "aiws/retrieval_strategy.hpp"

#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <vector>

namespace {

int failures = 0;

void check(bool condition, const std::string& message) {
    if (!condition) {
        ++failures;
        std::cerr << "FAIL: " << message << '\n';
    }
}

// builds "w0 w1 ... w(count-1)", long enough to force multiple m1 chunks
// once count exceeds the 120-token default chunk size
std::string numbered_words(int count) {
    std::string s;
    for (int i = 0; i < count; ++i) {
        if (!s.empty()) s += ' ';
        s += "w" + std::to_string(i);
    }
    return s;
}

// custom chunking strategy producing exactly one chunk per document no
// matter how long the document is, unlike the default 120/20/20 chunker
class WholeDocumentChunking final : public aiws::ChunkingStrategy {
public:
    std::vector<aiws::Chunk> chunk(const aiws::Document& document,
                                   std::size_t document_order) const override {
        return {aiws::Chunk{document.id() + "#0", document.id(), document_order, 0,
                            aiws::ProcessingCore::normalize(document.text()),
                            1, 0, document.text().size()}};
    }
};

// custom retrieval strategy that ignores the query entirely and returns
// chunks in reverse source order, unlike the default relevance ranking
class ReverseOrderRetrieval final : public aiws::RetrievalStrategy {
public:
    std::vector<aiws::SearchResult> search(const std::string&, int k,
                                           const std::vector<aiws::Chunk>& chunks,
                                           const aiws::CorpusIndex&) const override {
        std::vector<aiws::SearchResult> results;
        for (auto it = chunks.rbegin(); it != chunks.rend() && static_cast<int>(results.size()) < k; ++it) {
            results.push_back(aiws::SearchResult{it->id, it->document_id, it->sequence, it->text, 1.0, 0});
        }
        return results;
    }
};

}  // namespace

int main() {
    using namespace aiws;

    // default m1 behavior under processingcore
    {
        ProcessingCore core;
        Workspace ws;
        ws.add_document(Document{"match", "", "turtles are slow but steady"});
        ws.add_document(Document{"nomatch", "", "completely unrelated sentence"});
        core.rebuild(ws);

        const auto results = core.search("turtles", 5);
        check(results.size() == 1 && results[0].document_id == "match",
              "default configuration ranks only the matching document, standard m1 behavior");
    }

    // custom chunking strategy exercised through processingcore, producing
    // behavior observably different from the default chunker
    {
        ProcessingCore default_core;
        ProcessingCore custom_core(std::make_unique<WholeDocumentChunking>(),
                                   std::make_unique<RetrievalEngine>(),
                                   std::make_unique<ContextBuilder>());

        Workspace ws;
        ws.add_document(Document{"long", "", numbered_words(150)});

        default_core.rebuild(ws);
        custom_core.rebuild(ws);

        check(default_core.chunk_count() == 2,
              "default chunker splits a 150-token document into two chunks");
        check(custom_core.chunk_count() == 1,
              "injected WholeDocumentChunking overrides that into a single chunk, proving runtime dispatch");
    }

    // custom retrieval strategy producing a result order observably
    // different from the default relevance ranking
    {
        ProcessingCore core(std::make_unique<Chunker>(),
                            std::make_unique<ReverseOrderRetrieval>(),
                            std::make_unique<ContextBuilder>());
        Workspace ws;
        ws.add_document(Document{"first", "", "alpha"});
        ws.add_document(Document{"second", "", "beta"});
        core.rebuild(ws);

        const auto results = core.search("completely irrelevant query text", 5);
        check(results.size() == 2 && results[0].document_id == "second" && results[1].document_id == "first",
              "injected ReverseOrderRetrieval ignores the query and reverses source order, "
              "demonstrating processingcore::search dispatches to the injected object");
    }

    // null strategy rejection, checked for each of the three constructor slots
    {
        auto chunking = [] { return std::make_unique<Chunker>(); };
        auto retrieval = [] { return std::make_unique<RetrievalEngine>(); };
        auto context = [] { return std::make_unique<ContextBuilder>(); };

        bool chunking_threw = false;
        try { ProcessingCore bad(nullptr, retrieval(), context()); }
        catch (const std::invalid_argument&) { chunking_threw = true; }
        check(chunking_threw, "a null chunking strategy throws std::invalid_argument");

        bool retrieval_threw = false;
        try { ProcessingCore bad(chunking(), nullptr, context()); }
        catch (const std::invalid_argument&) { retrieval_threw = true; }
        check(retrieval_threw, "a null retrieval strategy throws std::invalid_argument");

        bool context_threw = false;
        try { ProcessingCore bad(chunking(), retrieval(), nullptr); }
        catch (const std::invalid_argument&) { context_threw = true; }
        check(context_threw, "a null context strategy throws std::invalid_argument");
    }

    // move-only ownership: not copyable, is movable, and a moved-to
    // instance keeps working with the strategies it now owns
    {
        check(!std::is_copy_constructible_v<ProcessingCore> && !std::is_copy_assignable_v<ProcessingCore>,
              "processingcore is not copy-constructible or copy-assignable");
        check(std::is_move_constructible_v<ProcessingCore> && std::is_move_assignable_v<ProcessingCore>,
              "processingcore is move-constructible and move-assignable");

        ProcessingCore original(std::make_unique<Chunker>(),
                                std::make_unique<ReverseOrderRetrieval>(),
                                std::make_unique<ContextBuilder>());
        Workspace ws;
        ws.add_document(Document{"only", "", "content"});
        original.rebuild(ws);

        ProcessingCore moved(std::move(original));
        const auto results = moved.search("irrelevant", 1);
        check(results.size() == 1 && results[0].document_id == "only",
              "a moved-to processingcore retains ownership of its injected strategies and still functions");
    }

    // integration scenario mixing a custom strategy with default ones,
    // crossing chunking, corpusindex, retrieval, and context construction
    {
        ProcessingCore core(std::make_unique<WholeDocumentChunking>(),
                            std::make_unique<RetrievalEngine>(),
                            std::make_unique<ContextBuilder>());
        Workspace ws;
        ws.add_document(Document{"doc", "", "apple banana apple cherry"});
        core.rebuild(ws);

        check(core.chunk_count() == 1, "custom chunking produced the single expected chunk");
        check(core.document_frequency("apple") == 1,
              "corpusindex built correctly from the custom chunker's output");

        const auto results = core.search("apple", 5);
        check(!results.empty() && results[0].matched_terms == 1,
              "default retrieval ranks correctly against a corpus built from a custom chunking strategy");

        const auto ctx = core.build_context("apple", 5, 2);
        check(!ctx.empty() && ctx[0].token_count <= 2,
              "default context building respects the token budget end to end through the mixed pipeline");
    }

    if (failures == 0) {
        std::cout << "All student tests passed.\n";
        return 0;
    }
    std::cerr << failures << " student test(s) failed.\n";
    return 1;
}