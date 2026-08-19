#ifndef TOKENIZER_HPP_
#define TOKENIZER_HPP_

#include <string>
#include <vector>

/**
 * @brief Splits text into lowercase alphanumeric tokens, ignoring punctuation,
 * whitespace, and symbols.
 *
 * @param text The raw text to tokenize.
 * @return std::vector<std::string> A vector of lowercase tokens, in the order
 * they appear.
 */
std::vector<std::string> Tokenize(const std::string& text);

#endif  // TOKENIZER_HPP_