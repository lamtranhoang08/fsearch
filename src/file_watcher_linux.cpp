// inotify-based FileWatcher backend for Linux only
#ifdef __linux__

#include "file_watcher.hpp"

#include <poll.h>
#include <sys/inotify.h>
#include <unistd.h>

#include <atomic>
#include <climits>
#include <cstring>
#include <functional>
#include <memory>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <vector>

namespace {

class InotifyFileWatcher : public FileWatcher {
public:
  InotifyFileWatcher() {
    fd_ = inotify_init1(IN_NONBLOCK);
    if (fd_ < 0) {
      throw std::runtime_error("inotify_init1 failed: " +
                               std::string(strerror(errno)));
    }
  }

  ~InotifyFileWatcher() override {
    if (fd_ >= 0) {
      close(fd_);
    }
  }

  void AddDirectory(const std::string &path) override {
    int wd = inotify_add_watch(fd_, path.c_str(),
                               IN_CREATE | IN_DELETE | IN_CLOSE_WRITE |
                                   IN_MOVED_FROM | IN_MOVED_TO);
    if (wd < 0) {
      throw std::runtime_error("inotify_add_watch failed for " + path + ": " +
                               std::string(strerror(errno)));
    }
    wd_to_path_[wd] = path;
  }

  void Run(const std::function<void(const FileEvent &)> &callback) override {
    // Buffer sized for several events at once
    constexpr size_t kBufferSize =
        16 * (sizeof(struct inotify_event) + NAME_MAX + 1);
    std::vector<char> buf(kBufferSize);

    pollfd pfd{fd_, POLLIN, 0};

    while (!stop_.load()) {
      // 500ms timeout so we periodically check stop_ flag
      int ready = poll(&pfd, 1, 500);
      if (ready <= 0) {
        continue;
      }

      ssize_t len = read(fd_, buf.data(), buf.size());
      if (len <= 0) {
        continue;
      }

      size_t offset = 0;
      while (offset < static_cast<size_t>(len)) {
        auto *event =
            reinterpret_cast<struct inotify_event *>(buf.data() + offset);
        auto it = wd_to_path_.find(event->wd);
        if (it != wd_to_path_.end() && event->len > 0) {
          std::string full_path = it->second + "/" + event->name;
          FileEventType type = FileEventType::Modified;

          if (event->mask & (IN_CREATE | IN_MOVED_TO)) {
            type = FileEventType::Created;
          } else if (event->mask & (IN_DELETE | IN_MOVED_FROM)) {
            type = FileEventType::Deleted;
          } else if (event->mask & IN_CLOSE_WRITE) {
            type = FileEventType::Modified;
          }

          // Fixed argument order to match struct FileEvent { std::string path;
          // FileEventType type; }
          callback(FileEvent{full_path, type});
        }
        offset += sizeof(struct inotify_event) + event->len;
      }
    }
  }

  void Stop() override { stop_.store(true); }

private:
  int fd_ = -1;
  std::unordered_map<int, std::string> wd_to_path_;
  std::atomic<bool> stop_{false};
};

} // namespace

std::unique_ptr<FileWatcher> FileWatcher::CreateFileWatcher() {
  return std::make_unique<InotifyFileWatcher>();
}

#endif // __linux__