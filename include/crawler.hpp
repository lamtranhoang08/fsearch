#ifndef CRAWLER_HPP_
#define CRAWLER_HPP_

#include <cstddef>
#include <string>
#include <vector>

struct CrawlResult {
  std::string path;
  std::string content;  // filename + file contents, ready for tokenizing
};

// Recursively walks root_dir. Reads full contents of files whose extension is
// in `extensions` (treated as text); for everything else it still indexes
// the filename, so binaries/images remain findable by name.
// max_file_bytes caps how much of a single file we read, to keep memory bounded.
std::vector<CrawlResult> Crawl(const std::string& root_dir,
                               const std::vector<std::string>& extensions,
                               size_t max_file_bytes = 1024 * 1024);

#endif  // CRAWLER_HPP_