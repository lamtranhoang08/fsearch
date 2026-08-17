#include "crawler.hpp"
#include "inverted_index.hpp"
#include "tokenizer.hpp"

#include <chrono>
#include <iomanip>
#include <iostream>
#include <string>
#include <vector>

namespace {

const std::vector<std::string> kTextExtensions = {
    ".txt",  ".md",  ".cpp", ".h",    ".hpp", ".c",   ".py",  ".js",
    ".ts",   ".json", ".csv", ".java", ".go",  ".rs",  ".yaml", ".yml",
    ".xml",  ".html", ".css", ".sh",   ".sql", ".log"};

void PrintUsage() {
  std::cerr
      << "fsearch - a local file indexer and search tool\n\n"
      << "Usage:\n"
      << "  fsearch build <directory> <index_file>   Crawl and index a "
         "directory\n"
      << "  fsearch search <index_file> <query...>   Search a previously built "
         "index\n";
}

int CmdBuild(const std::string& dir, const std::string& index_file) {
  auto start = std::chrono::steady_clock::now();

  auto files = Crawl(dir, kTextExtensions);
  InvertedIndex index;
  for (const auto& f : files) {
    auto tokens = Tokenize(f.content);
    index.AddDocument(f.path, tokens);
  }
  index.Save(index_file);

  auto end = std::chrono::steady_clock::now();
  double seconds = std::chrono::duration<double>(end - start).count();

  std::cout << "Indexed " << index.DocumentCount() << " files, "
            << index.TermCount() << " unique terms in " << std::fixed
            << std::setprecision(3) << seconds << "s\n"
            << "Saved to " << index_file << "\n";
  return 0;
}

int CmdSearch(const std::string& index_file,
              const std::vector<std::string>& query_words) {
  InvertedIndex index;
  index.Load(index_file);

  std::string query_text;
  for (const auto& w : query_words) {
    query_text += w;
    query_text += " ";
  }
  auto tokens = Tokenize(query_text);

  auto start = std::chrono::steady_clock::now();
  auto results = index.Search(tokens);
  auto end = std::chrono::steady_clock::now();
  double ms = std::chrono::duration<double, std::milli>(end - start).count();

  if (results.empty()) {
    std::cout << "No matches.\n";
  } else {
    for (const auto& [path, score] : results) {
      std::cout << std::fixed << std::setprecision(1) << score << "\t" << path
                << "\n";
    }
  }
  std::cout << "(" << results.size() << " results in " << std::setprecision(3)
            << ms << " ms, index has " << index.DocumentCount() << " docs)\n";
  return 0;
}

}  // namespace

int main(int argc, char** argv) {
  if (argc < 2) {
    PrintUsage();
    return 1;
  }

  std::string cmd = argv[1];

  if (cmd == "build") {
    if (argc != 4) {
      PrintUsage();
      return 1;
    }
    return CmdBuild(argv[2], argv[3]);
  }

  if (cmd == "search") {
    if (argc < 4) {
      PrintUsage();
      return 1;
    }
    std::vector<std::string> query_words(argv + 3, argv + argc);
    return CmdSearch(argv[2], query_words);
  }

  PrintUsage();
  return 1;
}