#pragma once
#include <string>
#include <vector>

struct CrawlResult {
    std::string path;
    std::string content;   // filename + file contents, ready for tokenizing
};

// Recursively walks rootDir. Reads full contents of files whose extension is
// in `extensions` (treated as text); for everything else it still indexes
// the filename, so binaries/images remain findable by name.
// maxFileBytes caps how much of a single file we read, to keep memory bounded.
std::vector<CrawlResult> crawl(const std::string& rootDir,
                                const std::vector<std::string>& extensions,
                                size_t maxFileBytes = 1024 * 1024);
