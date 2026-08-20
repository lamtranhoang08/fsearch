#include "crawler.hpp"
#include "file_watcher.hpp"
#include "inverted_index.hpp"
#include "tokenizer.hpp"

#include <algorithm>
#include <cctype>
#include <chrono>
#include <csignal>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>

namespace fs = std::filesystem;

namespace {

const std::vector<std::string> kTextExtensions = {
    ".txt", ".md",   ".cpp", ".h",    ".hpp", ".c",  ".py",   ".js",
    ".ts",  ".json", ".csv", ".java", ".go",  ".rs", ".yaml", ".yml",
    ".xml", ".html", ".css", ".sh",   ".sql", ".log"};

// Set by main() before Run() is called, so the signal handler below can
// reach the watcher instance and ask it to stop cleanly.
FileWatcher *g_watcher_for_signal = nullptr;

void HandleStopSignal(int) {
  if (g_watcher_for_signal) {
    g_watcher_for_signal->Stop();
  }
}

void PrintUsage() {
  std::cerr
      << "fsearch - a local file indexer and search tool\n\n"
      << "Usage:\n"
      << "  fsearch build <directory> <index_file>   Crawl and index a "
         "directory\n"
      << "  fsearch search <index_file> <query...>   Search a previously built "
         "index\n"
      << "  fsearch watch <directory> <index_file>   Build, then keep the "
         "index "
         "live\n";
}

int CmdBuild(const std::string &dir, const std::string &index_file) {
  auto start = std::chrono::steady_clock::now();

  auto files = Crawl(dir, kTextExtensions);
  InvertedIndex index;
  for (const auto &f : files) {
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

int CmdSearch(const std::string &index_file,
              const std::vector<std::string> &query_words) {
  InvertedIndex index;
  index.Load(index_file);

  std::string query_text;
  for (const auto &w : query_words) {
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
    for (const auto &[path, score] : results) {
      std::cout << std::fixed << std::setprecision(1) << score << "\t" << path
                << "\n";
    }
  }
  std::cout << "(" << results.size() << " results in " << std::setprecision(3)
            << ms << " ms, index has " << index.DocumentCount() << " docs)\n";
  return 0;
}

// Reads one file's indexable content the same way Crawl() does: filename
// always, full contents too if the extension is in kTextExtensions.
bool ReadFileContent(const std::string &path, std::string *out_content) {
  fs::path p(path);
  std::error_code ec;
  if (!fs::is_regular_file(p, ec))
    return false;

  *out_content = p.filename().string();

  std::string ext = p.extension().string();
  std::transform(ext.begin(), ext.end(), ext.begin(),
                 [](unsigned char c) { return std::tolower(c); });
  bool indexable = std::find(kTextExtensions.begin(), kTextExtensions.end(),
                             ext) != kTextExtensions.end();

  if (indexable) {
    std::ifstream file(p, std::ios::binary);
    if (file) {
      std::ostringstream buf;
      buf << file.rdbuf();
      *out_content += " ";
      *out_content += buf.str();
    }
  }
  return true;
}

int CmdWatch(const std::string &dir, const std::string &index_file) {
  std::cout << "Building initial index for " << dir << "...\n";

  auto files = Crawl(dir, kTextExtensions);
  InvertedIndex index;
  for (const auto &f : files) {
    index.AddDocument(f.path, Tokenize(f.content));
  }
  index.Save(index_file);
  std::cout << "Indexed " << index.ValidDocumentCount() << " files. "
            << "Watching for changes (Ctrl+C to stop)...\n";

  auto watcher = FileWatcher::CreateFileWatcher();
  g_watcher_for_signal = watcher.get();
  std::signal(SIGINT, HandleStopSignal);
  std::signal(SIGTERM, HandleStopSignal);

  for (const auto &subdir : ListDictionaries(dir)) {
    watcher->AddDirectory(subdir);
  }

  watcher->Run([&](const FileEvent &event) {
    switch (event.type) {
    case FileEventType::Deleted: {
      index.RemoveDocument(event.path);
      std::cout << "[removed] " << event.path << "\n";
      break;
    }
    case FileEventType::Created:
    case FileEventType::Modified: {
      std::string content;
      if (!ReadFileContent(event.path, &content)) {
        // File vanished again before we could read it, or it's
        // a directory event -- nothing to index.
        return;
      }
      index.AddDocument(event.path, Tokenize(content));
      std::cout << "[indexed] " << event.path << "\n";
      break;
    }
    }
    index.Save(index_file);
  });

  std::cout << "\nStopped. Final index: " << index.ValidDocumentCount()
            << " docs.\n";
  return 0;
}

} // namespace

int main(int argc, char **argv) {
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

  if (cmd == "watch") {
    if (argc != 4) {
      PrintUsage();
      return 1;
    }
    return CmdWatch(argv[2], argv[3]);
  }

  PrintUsage();
  return 1;
}