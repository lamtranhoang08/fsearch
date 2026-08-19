#ifndef INVERTED_INDEX_HPP_
#define INVERTED_INDEX_HPP_

#include <cstddef>
#include <cstdint>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

/**
 * @brief One occurrence record: a document ID and how many times a term
 * appears in it.
 */
struct Posting {
  uint32_t doc_id;    // Index into InvertedIndex's document list.
  uint32_t term_freq; // Number of times the term appears in that doc.
};

class InvertedIndex {
public:
  /**
   * @brief Adds a new document, or re-indexes it in place if path was already
   * indexed.
   *
   * @param path Unique identifier for the document (typically a filesystem
   * path).
   * @param tokens Tokens produced from the document's content via Tokenize().
   * @return uint32_t The document's ID.
   */
  uint32_t AddDocument(const std::string &path,
                       const std::vector<std::string> &tokens);

  /**
   * @brief Removes a previously-indexed document and all its postings.
   *
   * @param path The document's path, as passed to AddDocument().
   */
  void RemoveDocument(const std::string &path);

  /**
   * @brief Searches the index using strict AND semantics.
   *
   * A document is only returned if it contains every token in @p query_tokens.
   * Results are scored by summing term frequencies across the matched terms,
   * then sorted descending.
   *
   * @param query_tokens Tokens to search for, e.g. via Tokenize().
   * @param top_k Maximum number of results to return.
   * @return Up to @p top_k (path, score) pairs, highest score first.
   *         Empty if @p query_tokens is empty or any term has no match.
   */
  std::vector<std::pair<std::string, double>>
  Search(const std::vector<std::string> &query_tokens, size_t top_k = 20) const;

  /**
   * @brief Serializes the index to a binary file.
   *
   * @param out_path Destination file path. Overwritten if it exists.
   * @throws std::runtime_error If the file can't be opened for writing.
   */
  void Save(const std::string &out_path) const;

  /**
   * @brief Loads an index previously written by Save(), replacing any
   * in-memory state.
   *
   * @param in_path Path to a file previously written by Save().
   * @throws std::runtime_error If the file can't be opened for reading.
   */
  void Load(const std::string &in_path);

  /**
   * @brief Total document slots ever assigned, including removed docs.
   */
  size_t DocumentCount() const { return doc_paths_.size(); }

  /**
   * @brief Number of docs currently live in the index (excluding removed
   * docs).
   */
  size_t ValidDocumentCount() const;

  /**
   * @brief Number of unique terms currently in the index.
   */
  size_t TermCount() const { return index_.size(); }

  /**
   * @brief Removes all posting records associated with a specific document ID.
   *
   * Iterates through the posting lists of terms contained in the document
   * and erases any Posting entry matching @p doc_id.
   *
   * @param doc_id The internal ID of the document whose postings should be
   * removed.
   */
  void RemovePostingsForDoc(uint32_t doc_id);

  /**
   * @brief Calculates term frequencies and updates the inverted index for a
   * document.
   *
   * Takes raw tokens, computes how many times each token appears, and appends
   * a new Posting record to the corresponding term's posting list.
   *
   * @param doc_id The target document ID.
   * @param tokens List of normalized tokens extracted from the document.
   */
  void IndexTokensForDoc(uint32_t doc_id,
                         const std::vector<std::string> &tokens);

  /**
   * @brief Reconstructs the forward index (doc_id -> terms mapping) from the
   * inverted index.
   *
   * Useful after loading a serialized index from disk to restore the
   * inverted-to-forward map synchronization required for fast document
   * deletion.
   */
  void RebuildForwardIndex();
  
  std::vector<std::string> doc_paths_; // doc_id -> path
  std::vector<bool> doc_valid_;        // doc_id still indexed?
  std::unordered_map<std::string, uint32_t> path_to_doc_id_; // path -> doc_id
  std::unordered_map<uint32_t, std::vector<std::string>>
      forward_index_; // doc_id -> unique terms
  std::unordered_map<std::string, std::vector<Posting>>
      index_; // term -> postings
};

#endif // INVERTED_INDEX_HPP_