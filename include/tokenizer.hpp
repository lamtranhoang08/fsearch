#ifndef TOKENIZER_HPP_
#define TOKENIZER_HPP_

#include <string>
#include <vector>

// Splits text into lowercase alphanumeric tokens.
// Drops tokens shorter than 2 chars (cuts noise from punctuation/single letters).
std::vector<std::string> Tokenize(const std::string& text);

#endif // TOKENIZER_HPP_