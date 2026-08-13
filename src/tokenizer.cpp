#include "tokenizer.hpp"
#include <cctype>

std::vector<std::string> tokenize(const std::string& text) {
    std::vector<std::string> tokens;
    std::string current;
    current.reserve(32);

    auto flush = [&]() {
        if (current.size() >= 2) {
            tokens.push_back(current);
        }
        current.clear();
    };

    for (unsigned char ch : text) {
        if (std::isalnum(ch)) {
            current.push_back(static_cast<char>(std::tolower(ch)));
        } else {
            flush();
        }
    }
    flush();

    return tokens;
}
