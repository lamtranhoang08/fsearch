#include "crawler.hpp"
#include <filesystem>
#include <fstream>
#include <sstream>
#include <algorithm>
#include <iostream>

namespace fs = std::filesystem;

static bool hasExtension(const fs::path& p, const std::vector<std::string>& extensions) {
    std::string ext = p.extension().string();
    std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
    return std::find(extensions.begin(), extensions.end(), ext) != extensions.end();
}

std::vector<CrawlResult> crawl(const std::string& rootDir,
                                const std::vector<std::string>& extensions,
                                size_t maxFileBytes) {
    std::vector<CrawlResult> results;

    std::error_code ec;
    auto it = fs::recursive_directory_iterator(
        rootDir, fs::directory_options::skip_permission_denied, ec);
    auto end = fs::recursive_directory_iterator();

    for (; it != end; it.increment(ec)) {
        if (ec) {
            ec.clear();
            continue;
        }

        const auto& entry = *it;
        if (!entry.is_regular_file(ec)) continue;

        const fs::path& path = entry.path();
        CrawlResult result;
        result.path = path.string();

        // Filename always contributes to the index (helps find binaries, images, etc).
        result.content = path.filename().string();

        if (hasExtension(path, extensions)) {
            std::ifstream file(path, std::ios::binary);
            if (file) {
                std::ostringstream buf;
                buf << file.rdbuf();
                std::string data = buf.str();
                if (data.size() > maxFileBytes) {
                    data.resize(maxFileBytes);
                }
                result.content += " ";
                result.content += data;
            }
        }

        results.push_back(std::move(result));
    }

    return results;
}
