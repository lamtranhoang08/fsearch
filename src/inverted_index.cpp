#include "inverted_index.hpp"
#include <fstream>
#include <algorithm>
#include <stdexcept>

uint32_t InvertedIndex::addDocument(const std::string& path, const std::vector<std::string>& tokens) {
    uint32_t docId = static_cast<uint32_t>(docPaths_.size());
    docPaths_.push_back(path);

    std::unordered_map<std::string, uint32_t> termFreq;
    termFreq.reserve(tokens.size());
    for (const auto& t : tokens) {
        termFreq[t]++;
    }

    for (const auto& [term, freq] : termFreq) {
        index_[term].push_back(Posting{docId, freq});
    }

    return docId;
}

std::vector<std::pair<std::string, double>> InvertedIndex::search(
    const std::vector<std::string>& queryTokens, size_t topK) const {

    if (queryTokens.empty()) return {};

    std::vector<const std::vector<Posting>*> lists;
    lists.reserve(queryTokens.size());
    for (const auto& t : queryTokens) {
        auto it = index_.find(t);
        if (it == index_.end()) {
            // Strict AND: any missing term means zero results.
            return {};
        }
        lists.push_back(&it->second);
    }

    // Intersect starting from the smallest posting list — standard
    // inverted-index optimization, keeps the hot loop small.
    std::sort(lists.begin(), lists.end(),
              [](const auto* a, const auto* b) { return a->size() < b->size(); });

    std::unordered_map<uint32_t, double> scores;
    scores.reserve(lists[0]->size());
    for (const auto& p : *lists[0]) {
        scores[p.doc_id] = p.term_freq;
    }

    for (size_t i = 1; i < lists.size() && !scores.empty(); ++i) {
        std::unordered_map<uint32_t, uint32_t> freqLookup;
        freqLookup.reserve(lists[i]->size());
        for (const auto& p : *lists[i]) {
            freqLookup[p.doc_id] = p.term_freq;
        }

        std::unordered_map<uint32_t, double> next;
        next.reserve(scores.size());
        for (const auto& [docId, score] : scores) {
            auto found = freqLookup.find(docId);
            if (found != freqLookup.end()) {
                next[docId] = score + found->second;
            }
        }
        scores = std::move(next);
    }

    std::vector<std::pair<uint32_t, double>> ranked(scores.begin(), scores.end());
    std::sort(ranked.begin(), ranked.end(),
              [](const auto& a, const auto& b) { return a.second > b.second; });
    if (ranked.size() > topK) ranked.resize(topK);

    std::vector<std::pair<std::string, double>> result;
    result.reserve(ranked.size());
    for (const auto& [docId, score] : ranked) {
        result.emplace_back(docPaths_[docId], score);
    }
    return result;
}

// --- Binary persistence -----------------------------------------------
// Layout: [docCount][doc paths...][termCount][term, postingCount, postings...]
// Simple, no compression — good enough for v1, called out as a stretch
// goal in the README (delta + varint encoding would shrink this a lot).

void InvertedIndex::save(const std::string& outPath) const {
    std::ofstream out(outPath, std::ios::binary);
    if (!out) throw std::runtime_error("cannot open output file: " + outPath);

    uint32_t docCount = static_cast<uint32_t>(docPaths_.size());
    out.write(reinterpret_cast<const char*>(&docCount), sizeof(docCount));
    for (const auto& p : docPaths_) {
        uint32_t len = static_cast<uint32_t>(p.size());
        out.write(reinterpret_cast<const char*>(&len), sizeof(len));
        out.write(p.data(), len);
    }

    uint32_t termCount = static_cast<uint32_t>(index_.size());
    out.write(reinterpret_cast<const char*>(&termCount), sizeof(termCount));
    for (const auto& [term, postings] : index_) {
        uint32_t tlen = static_cast<uint32_t>(term.size());
        out.write(reinterpret_cast<const char*>(&tlen), sizeof(tlen));
        out.write(term.data(), tlen);

        uint32_t plen = static_cast<uint32_t>(postings.size());
        out.write(reinterpret_cast<const char*>(&plen), sizeof(plen));
        out.write(reinterpret_cast<const char*>(postings.data()), plen * sizeof(Posting));
    }
}

void InvertedIndex::load(const std::string& inPath) {
    std::ifstream in(inPath, std::ios::binary);
    if (!in) throw std::runtime_error("cannot open index file: " + inPath);

    docPaths_.clear();
    index_.clear();

    uint32_t docCount = 0;
    in.read(reinterpret_cast<char*>(&docCount), sizeof(docCount));
    docPaths_.reserve(docCount);
    for (uint32_t i = 0; i < docCount; ++i) {
        uint32_t len = 0;
        in.read(reinterpret_cast<char*>(&len), sizeof(len));
        std::string p(len, '\0');
        in.read(p.data(), len);
        docPaths_.push_back(std::move(p));
    }

    uint32_t termCount = 0;
    in.read(reinterpret_cast<char*>(&termCount), sizeof(termCount));
    index_.reserve(termCount);
    for (uint32_t i = 0; i < termCount; ++i) {
        uint32_t tlen = 0;
        in.read(reinterpret_cast<char*>(&tlen), sizeof(tlen));
        std::string term(tlen, '\0');
        in.read(term.data(), tlen);

        uint32_t plen = 0;
        in.read(reinterpret_cast<char*>(&plen), sizeof(plen));
        std::vector<Posting> postings(plen);
        in.read(reinterpret_cast<char*>(postings.data()), plen * sizeof(Posting));

        index_.emplace(std::move(term), std::move(postings));
    }
}
