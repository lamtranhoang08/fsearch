#ifndef CRAWLER_HPP_
#define CRAWLER_HPP_

#include <cstddef>
#include <string>
#include <vector>

struct CrawlResult {
  std::string path;
  std::string content;  // filename + file contents, ready for tokenizing
};

/**
 * @brief Recursively walks root_dir and reads file contents.
 *
 * Reads full contents of files whose extension is in `extensions` (treated as
 * text); for everything else it still indexes the filename so binaries/images
 * remain findable by name.
 *
 * @param root_dir Root directory path to begin crawling from.
 * @param extensions List of file extensions to read as text.
 * @param max_file_bytes Caps how much of a single file is read into memory.
 * @return std::vector<CrawlResult> Array of crawl results containing paths and
 * contents.
 */
std::vector<CrawlResult> Crawl(const std::string& root_dir,
                               const std::vector<std::string>& extensions,
                               size_t max_file_bytes = 1024 * 1024);

#endif  // CRAWLER_HPP_