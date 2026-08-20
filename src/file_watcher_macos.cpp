// kqueue-based FileWatcher backend (macOS/BSD only).
#ifdef __APPLE__

#include "file_watcher.hpp"

#include <fcntl.h>
#include <sys/event.h>
#include <sys/time.h>
#include <unistd.h>

#include <atomic>
#include <filesystem>
#include <functional>
#include <memory>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <utility>

namespace fs = std::filesystem;

namespace {

class KqueueWatcher : public FileWatcher {
public:
  KqueueWatcher() {
    kq_ = kqueue();
    if (kq_ < 0)
      throw std::runtime_error("kqueue() failed");
  }

  ~KqueueWatcher() override {
    for (const auto &[fd, path] : fd_to_path_) {
      close(fd);
    }
    if (kq_ >= 0)
      close(kq_);
  }

  void AddDirectory(const std::string &path) override {
    int fd = open(path.c_str(), O_EVTONLY);
    if (fd < 0)
      throw std::runtime_error("open() failed for " + path);

    struct kevent change;
    EV_SET(&change, fd, EVFILT_VNODE, EV_ADD | EV_CLEAR,
           NOTE_WRITE | NOTE_DELETE | NOTE_RENAME, 0, nullptr);
    if (kevent(kq_, &change, 1, nullptr, 0, nullptr) < 0) {
      close(fd);
      throw std::runtime_error("kevent() registration failed for " + path);
    }

    fd_to_path_[fd] = path;
    snapshots_[path] = SnapshotDirectory(path);
  }

  void Run(const std::function<void(const FileEvent &)> &callback) override {
    struct kevent event;
    struct timespec timeout{
        0, 500'000'000}; // 500ms timeout to periodically check stop_ flag

    while (!stop_.load()) {
      int n = kevent(kq_, nullptr, 0, &event, 1, &timeout);

      if (n > 0) {
        // Immediate event on a watched directory descriptor
        int fd = static_cast<int>(event.ident);
        auto it = fd_to_path_.find(fd);
        if (it != fd_to_path_.end()) {
          DiffAndEmit(it->second, callback);
        }
      } else {
        // Periodic 500ms timeout wakeup: poll all watched directory snapshots
        // to catch in-place edits to existing files (e.g. src/main.cpp)
        for (const auto &[fd, path] : fd_to_path_) {
          DiffAndEmit(path, callback);
        }
      }
    }
  }

  void Stop() override { stop_.store(true); }

private:
  using Snapshot = std::unordered_map<std::string, fs::file_time_type>;

  static Snapshot SnapshotDirectory(const std::string &dir_path) {
    Snapshot snap;
    std::error_code ec;
    for (const auto &entry : fs::directory_iterator(dir_path, ec)) {
      if (entry.is_regular_file(ec)) {
        snap[entry.path().filename().string()] = entry.last_write_time(ec);
      }
    }
    return snap;
  }

  void DiffAndEmit(const std::string &dir_path,
                   const std::function<void(const FileEvent &)> &callback) {
    Snapshot fresh = SnapshotDirectory(dir_path);
    Snapshot &old = snapshots_[dir_path];

    for (const auto &[name, mtime] : fresh) {
      auto old_it = old.find(name);
      std::string full_path = dir_path + "/" + name;
      if (old_it == old.end()) {
        callback(FileEvent{full_path, FileEventType::Created});
      } else if (old_it->second != mtime) {
        callback(FileEvent{full_path, FileEventType::Modified});
      }
    }

    for (const auto &[name, mtime] : old) {
      (void)mtime;
      if (fresh.find(name) == fresh.end()) {
        callback(FileEvent{dir_path + "/" + name, FileEventType::Deleted});
      }
    }

    old = std::move(fresh);
  }

  int kq_ = -1;
  std::unordered_map<int, std::string> fd_to_path_;
  std::unordered_map<std::string, Snapshot> snapshots_;
  std::atomic<bool> stop_{false};
};

} // namespace

std::unique_ptr<FileWatcher> FileWatcher::CreateFileWatcher() {
  return std::make_unique<KqueueWatcher>();
}

#endif // __APPLE__