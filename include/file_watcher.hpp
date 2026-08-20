#ifndef FILE_WATCHER_HPP_
#define FILE_WATCHER_HPP_

#include <functional>
#include <memory>
#include <string>

/**
 * @brief Kind of filesystem change reported by the file watcher.
 */
enum class FileEventType { Created, Modified, Deleted };

/**
 * @brief Represents a filesystem event reported by the file watcher.
 */
struct FileEvent {
  std::string path;
  FileEventType type;
};

/**
 * @brief Watches a set of directories for filesystem changes
 *
 * This is a platform abstraction: the concrete implementation is
 * platform-specific and is provided by the factory function
 * `CreateFileWatcher`. Caller should only depend on this interface, not the
 * concrete implementation.
 */
class FileWatcher {
public:
  virtual ~FileWatcher() = default;

  /**
   * @brief Registers a directory to watch.
   * Must be called before `Run()`. The watcher will monitor the directory and all its
   * subdirectories for changes.
   * 
   * @param path Directory path to watch. 
   */
  virtual void AddDirectory(const std::string& path) = 0;

  /**
   * @brief Blocks, invoking @p callback for each filesystem event,
   * until Stop() is called (typically from a signal handler).
   * 
   * @param callback Invoked once per detected change.
   */
  virtual void Run(const std::function<void(const FileEvent&)>& callback) = 0;

  /**
   * @brief Signals Run() to return. Safe to call from a signal handler. 
   * 
   */
   virtual void Stop() = 0;

   /**
    * @brief Create a File Watcher backend for the current platform. 
    * Caller takes ownership of the returned object.
    */
   static std::unique_ptr<FileWatcher> CreateFileWatcher();
};

#endif // FILE_WATCHER_HPP_