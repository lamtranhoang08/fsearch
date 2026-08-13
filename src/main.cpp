#include "tokenizer.hpp"
#include "crawler.hpp"
#include "inverted_index.hpp"

#include <iostream>
#include <chrono>
#include <iomanip>

static const std::vector<std::string> kTextExtensions = {
    ".txt", ".md", ".cpp", ".h", ".hpp", ".c", ".py", ".js", ".ts",
    ".json", ".csv", ".java", ".go", ".rs", ".yaml", ".yml", ".xml",
    ".html", ".css", ".sh", ".sql", ".log"
};

static void printUsage() {
    std::cerr <<
        "fsearch - a local file indexer and search tool\n\n"
        "Usage:\n"
        "  fsearch build <directory> <index_file>   Crawl and index a directory\n"
        "  fsearch search <index_file> <query...>   Search a previously built index\n";
}

static int cmdBuild(const std::string& dir, const std::string& indexFile) {
    auto start = std::chrono::steady_clock::now();

    auto files = crawl(dir, kTextExtensions);
    InvertedIndex index;
    for (const auto& f : files) {
        auto tokens = tokenize(f.content);
        index.addDocument(f.path, tokens);
    }
    index.save(indexFile);

    auto end = std::chrono::steady_clock::now();
    double seconds = std::chrono::duration<double>(end - start).count();

    std::cout << "Indexed " << index.documentCount() << " files, "
              << index.termCount() << " unique terms in "
              << std::fixed << std::setprecision(3) << seconds << "s\n"
              << "Saved to " << indexFile << "\n";
    return 0;
}

static int cmdSearch(const std::string& indexFile, const std::vector<std::string>& queryWords) {
    InvertedIndex index;
    index.load(indexFile);

    std::string queryText;
    for (const auto& w : queryWords) {
        queryText += w;
        queryText += " ";
    }
    auto tokens = tokenize(queryText);

    auto start = std::chrono::steady_clock::now();
    auto results = index.search(tokens);
    auto end = std::chrono::steady_clock::now();
    double ms = std::chrono::duration<double, std::milli>(end - start).count();

    if (results.empty()) {
        std::cout << "No matches.\n";
    } else {
        for (const auto& [path, score] : results) {
            std::cout << std::fixed << std::setprecision(1) << score << "\t" << path << "\n";
        }
    }
    std::cout << "(" << results.size() << " results in "
               << std::setprecision(3) << ms << " ms, index has "
               << index.documentCount() << " docs)\n";
    return 0;
}

int main(int argc, char** argv) {
    if (argc < 2) {
        printUsage();
        return 1;
    }

    std::string cmd = argv[1];

    if (cmd == "build") {
        if (argc != 4) { printUsage(); return 1; }
        return cmdBuild(argv[2], argv[3]);
    }

    if (cmd == "search") {
        if (argc < 4) { printUsage(); return 1; }
        std::vector<std::string> queryWords(argv + 3, argv + argc);
        return cmdSearch(argv[2], queryWords);
    }

    printUsage();
    return 1;
}
