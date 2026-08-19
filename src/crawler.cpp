#include "crawler.hpp"

#include <filesystem>
#include <fstream>
#include <sstream>
#include <algorithm>
#include <iostream>
#include <cctype>
#include <system_error>

namespace fs = std::filesystem;

namespace {
bool HasExtension(const fs::path& p, const std::vector<std::string>& extensions) {
    if(!p.has_extension()) return false;
    
    std::string ext = p.extension().string();
    std::transform(ext.begin(),  ext.end(), ext.begin(), [](unsigned char c) {
        return std::tolower(c);
    });
    return std::find(extensions.begin(), extensions.end(), ext) != extensions.end();
}
}

std::vector<CrawlResult> Crawl(const std::string& root_dir,
                                const std::vector<std::string>& extensions,
                                size_t max_file_bytes) {
    std::vector<CrawlResult> results;
    std::error_code ec;
    auto it = fs::recursive_directory_iterator(root_dir, fs::directory_options::skip_permission_denied, ec);
    auto end = fs::recursive_directory_iterator();

    for(; it != end; it.increment(ec)) {
        if(ec) {
            ec.clear();
            continue;   
        }

        const auto& entry = *it;
        if(!entry.is_regular_file(ec)) continue;
        const fs::path& path = entry.path();
        CrawlResult result;
        result.path = path.string();

        // Filename always contributes to the index
        result.content = path.filename().string();

        if(HasExtension(path, extensions)) {
            std::ifstream file(path, std::ios::binary);
            if(file) {
                std::ostringstream buf;
                buf << file.rdbuf();
                std::string data = buf.str();
                if(data.size() > max_file_bytes) {
                    data.resize(max_file_bytes);
                }
                result.content += " ";
                result.content += data;
            }
        }
        results.push_back(std::move(result));
    }
    return results;     
}