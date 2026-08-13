#pragma once
#include <string>
#include <vector>
#include <unordered_map>
#include <cstdint>

struct Posting {
    uint32_t doc_id;
    uint32_t term_freq;
};

class InvertedIndex {
public:
    // Registers a document and its tokens. Returns the assigned doc id.
    uint32_t addDocument(const std::string& path, const std::vector<std::string>& tokens);

    // AND-semantics search: a document must contain every query token.
    // Score = sum of term frequencies across matched terms.
    std::vector<std::pair<std::string, double>> search(
        const std::vector<std::string>& queryTokens, size_t topK = 20) const;

    void save(const std::string& outPath) const;
    void load(const std::string& inPath);

    size_t documentCount() const { return docPaths_.size(); }
    size_t termCount() const { return index_.size(); }

private:
    std::vector<std::string> docPaths_;
    std::unordered_map<std::string, std::vector<Posting>> index_;
};
