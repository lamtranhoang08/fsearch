#pragma once
#include <string>
#include <vector>

// Splits text into lowercase alphanumeric tokens.
// Drops tokens shorter than 2 chars (cuts noise from punctuation/single letters).
std::vector<std::string> tokenize(const std::string& text);
