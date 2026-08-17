#ifndef INVERTED_INDEX_HPP_
#define INVERTED_INDEX_HPP_

#include <string>
#include <vector>
#include <unordered_map>
#include <cstdint>
#include <utility>
#include <cstddef>

struct Posting {
    uint32_t doc_id;
    uint32_t term_freq;
};

class InvertedIndex {
public:
    // Registers a document and its tokens. Returns the assigned doc id
    uint32_t AddDocument(const std::string& path, const std::vector<std::string>& tokens);

    // AND-semantics search: a document must contain every query token
    // Score = sum of term frequencies across matched terms
    std::vector<std::pair<std::string, double>> Search(
        const std::vector<std::string>& query_tokens, size_t top_k = 20) const;

    void Save(const std::string& out_path) const;
    void Load(const std::string& in_path);

    size_t DocumentCount() const { return doc_paths_.size(); }
    size_t TermCount() const { return index_.size(); }

private:
    std::vector<std::string> doc_paths_;
    std::unordered_map<std::string, std::vector<Posting>> index_;
};

#endif // INVERTED_INDEX_HPP_