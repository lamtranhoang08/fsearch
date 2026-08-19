#include "inverted_index.hpp"
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <fstream>
#include <stdexcept>

uint32_t InvertedIndex::AddDocument(const std::string &path,
                                    const std::vector<std::string> &tokens) {
  auto it = path_to_doc_id_.find(path);
  if (it != path_to_doc_id_.end()) {
    // Path already indexed: wipe its old postings, then re-index
    uint32_t doc_id = it->second;
    RemovePostingsForDoc(doc_id);
    IndexTokensForDoc(doc_id, tokens);
    doc_valid_[doc_id] = true;
    return doc_id;
  }

  uint32_t doc_id = static_cast<uint32_t>(doc_paths_.size());
  doc_paths_.push_back(path);
  doc_valid_.push_back(true);
  path_to_doc_id_[path] = doc_id;
  IndexTokensForDoc(doc_id, tokens);
  return doc_id;
}

void InvertedIndex::RemoveDocument(const std::string &path) {
  auto it = path_to_doc_id_.find(path);
  if (it == path_to_doc_id_.end())
    return;

  uint32_t doc_id = it->second;
  RemovePostingsForDoc(doc_id);
  forward_index_.erase(doc_id);
  doc_valid_[doc_id] = false;
  path_to_doc_id_.erase(it);
}

void InvertedIndex::RemovePostingsForDoc(uint32_t doc_id) {
  auto it = forward_index_.find(doc_id);
  if (it == forward_index_.end())
    return;

  for (const auto &term : it->second) {
    auto idx_it = index_.find(term);
    if (idx_it == index_.end())
      continue;

    auto &postings = idx_it->second;
    postings.erase(std::remove_if(postings.begin(), postings.end(),
                                  [doc_id](const Posting &p) {
                                    return p.doc_id == doc_id;
                                  }),
                   postings.end());
    if (postings.empty()) {
      index_.erase(idx_it);
    }
  }
  forward_index_.erase(it);
}

void InvertedIndex::IndexTokensForDoc(uint32_t doc_id,
                                      const std::vector<std::string> &tokens) {
  std::unordered_map<std::string, uint32_t> term_freq;
  for (const auto &t : tokens) {
    ++term_freq[t];
  }

  std::vector<std::string> unique_terms;
  unique_terms.reserve(term_freq.size());
  for (const auto &[term, freq] : term_freq) {
    index_[term].push_back({doc_id, freq});
    unique_terms.push_back(term);
  }
  forward_index_[doc_id] = std::move(unique_terms);
}

void InvertedIndex::RebuildForwardIndex() {
  forward_index_.clear();
  for (const auto &[term, postings] : index_) {
    for (const auto &p : postings) {
      forward_index_[p.doc_id].push_back(term);
    }
  }
}

std::vector<std::pair<std::string, double>>
InvertedIndex::Search(const std::vector<std::string> &query_tokens,
                      size_t top_k) const {

  if (query_tokens.empty())
    return {};

  std::vector<const std::vector<Posting> *> lists;
  lists.reserve(query_tokens.size());
  for (const auto &t : query_tokens) {
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
            [](const auto *a, const auto *b) { return a->size() < b->size(); });

  std::unordered_map<uint32_t, double> scores;
  scores.reserve(lists[0]->size());
  for (const auto &p : *lists[0]) {
    if (p.doc_id < doc_valid_.size() && doc_valid_[p.doc_id]) {
      scores[p.doc_id] = p.term_freq;
    }
  }

  for (size_t i = 1; i < lists.size() && !scores.empty(); ++i) {
    std::unordered_map<uint32_t, uint32_t> freq_lookup;
    freq_lookup.reserve(lists[i]->size());
    for (const auto &p : *lists[i]) {
      freq_lookup[p.doc_id] = p.term_freq;
    }

    std::unordered_map<uint32_t, double> next;
    next.reserve(scores.size());
    for (const auto &[doc_id, score] : scores) {
      auto found = freq_lookup.find(doc_id);
      if (found != freq_lookup.end()) {
        next[doc_id] = score + found->second;
      }
    }
    scores = std::move(next);
  }

  std::vector<std::pair<uint32_t, double>> ranked(scores.begin(), scores.end());
  std::sort(ranked.begin(), ranked.end(),
            [](const auto &a, const auto &b) { return a.second > b.second; });
  if (ranked.size() > top_k) {
    ranked.resize(top_k);
  }

  std::vector<std::pair<std::string, double>> result;
  result.reserve(ranked.size());
  for (const auto &[doc_id, score] : ranked) {
    result.push_back(std::make_pair(doc_paths_[doc_id], score));
  }
  return result;
}

size_t InvertedIndex::ValidDocumentCount() const {
  size_t count = 0;
  for (bool v : doc_valid_) {
    if (v)
      ++count;
  }
  return count;
}

// Binary persistence
// Layout: [docCount][doc paths...][termCount][term, postingCount,
// postings...] Simple, no compression — good enough for v1, called out as a
// stretch goal in the README (delta + varint encoding would shrink this a
// lot).

void InvertedIndex::Save(const std::string &out_path) const {
  std::ofstream out(out_path, std::ios::binary);
  if (!out)
    throw std::runtime_error("cannot open output file: " + out_path);

  uint32_t doc_count = static_cast<uint32_t>(doc_paths_.size());
  out.write(reinterpret_cast<const char *>(&doc_count), sizeof(doc_count));
  for (uint32_t i = 0; i < doc_count; ++i) {
    const std::string &p = doc_paths_[i];
    uint32_t len = static_cast<uint32_t>(p.size());
    out.write(reinterpret_cast<const char *>(&len), sizeof(len));
    out.write(p.data(), len);
    uint8_t valid = doc_valid_[i] ? 1 : 0;
    out.write(reinterpret_cast<const char *>(&valid), sizeof(valid));
  }

  uint32_t term_count = static_cast<uint32_t>(index_.size());
  out.write(reinterpret_cast<const char *>(&term_count), sizeof(term_count));
  for (const auto &[term, postings] : index_) {
    uint32_t tlen = static_cast<uint32_t>(term.size());
    out.write(reinterpret_cast<const char *>(&tlen), sizeof(tlen));
    out.write(term.data(), tlen);

    uint32_t plen = static_cast<uint32_t>(postings.size());
    out.write(reinterpret_cast<const char *>(&plen), sizeof(plen));
    out.write(reinterpret_cast<const char *>(postings.data()),
              plen * sizeof(Posting));
  }
}

void InvertedIndex::Load(const std::string &in_path) {
  std::ifstream in(in_path, std::ios::binary);
  if (!in)
    throw std::runtime_error("cannot open index file: " + in_path);

  doc_paths_.clear();
  doc_valid_.clear();
  path_to_doc_id_.clear();
  forward_index_.clear();
  index_.clear();

  uint32_t doc_count = 0;
  in.read(reinterpret_cast<char *>(&doc_count), sizeof(doc_count));
  doc_paths_.reserve(doc_count);
  doc_valid_.reserve(doc_count);
  for (uint32_t i = 0; i < doc_count; ++i) {
    uint32_t len = 0;
    in.read(reinterpret_cast<char *>(&len), sizeof(len));
    std::string p(len, '\0');
    in.read(p.data(), len);
    
    uint8_t valid = 1;
    in.read(reinterpret_cast<char *>(&valid), sizeof(valid));

    doc_paths_.push_back(p);
    doc_valid_.push_back(valid != 0);
    if (valid) {
      path_to_doc_id_[p] = i;
    }
  }

  uint32_t term_count = 0;
  in.read(reinterpret_cast<char *>(&term_count), sizeof(term_count));
  index_.reserve(term_count);
  for (uint32_t i = 0; i < term_count; ++i) {
    uint32_t tlen = 0;
    in.read(reinterpret_cast<char *>(&tlen), sizeof(tlen));
    std::string term(tlen, '\0');
    in.read(term.data(), tlen);

    uint32_t plen = 0;
    in.read(reinterpret_cast<char *>(&plen), sizeof(plen));
    std::vector<Posting> postings(plen);
    in.read(reinterpret_cast<char *>(postings.data()), plen * sizeof(Posting));

    index_.emplace(std::move(term), std::move(postings));
  }

  RebuildForwardIndex();
}
