#pragma once

#include <utility>
#include <vector>
#include "storage/write_ahead_log/log_consumer_task.h"

namespace terrier::storage {

/**
 * A DiskLogConsumerTask is responsible for writing serialized log records out to disk by processing buffers in the log
 * manager's filled buffer queue
 */
class DiskLogConsumerTask final : public LogConsumerTask {
 public:
  /**
   * Constructs a new DiskLogConsumerTask
   * @param log_file_path path to file to write logs to
   * @param persist_interval Interval time for when to persist log file
   * @param persist_threshold threshold of data written since the last persist to trigger another persist

   */
  explicit DiskLogConsumerTask(const char *log_file_path, const std::chrono::milliseconds persist_interval,
                               uint64_t persist_threshold,
                               common::ConcurrentBlockingQueue<BufferedLogWriter *> *empty_buffer_queue,
                               common::ConcurrentQueue<storage::SerializedLogs> *filled_buffer_queue)
      : LogConsumerTask(empty_buffer_queue, filled_buffer_queue),
        out_(PosixIoWrappers::Open(log_file_path, O_WRONLY | O_APPEND | O_CREAT, S_IRUSR | S_IWUSR)),
        persist_interval_(persist_interval),
        persist_threshold_(persist_threshold),
        current_data_written_(0) {}

  /**
   * Runs main disk log writer loop. Called by thread registry upon initialization of thread
   */
  void RunTask() override;

  /**
   * Signals task to stop. Called by thread registry upon termination of thread
   */
  void Terminate() override;

 private:
  friend class LogManager;
  // File descriptor for log file
  int out_;
  // Stores callbacks for commit records written to disk but not yet persisted
  std::vector<storage::CommitCallback> commit_callbacks_;

  // Interval time for when to persist log file
  const std::chrono::milliseconds persist_interval_;
  // Threshold of data written since the last persist to trigger another persist
  uint64_t persist_threshold_;
  // Amount of data written since last persist
  uint64_t current_data_written_;

  // Flag used by the serializer thread to signal the disk log consumer task thread to persist the data on disk
  volatile bool do_persist_;

  // Synchronisation primitives to synchronise persisting buffers to disk
  std::mutex persist_lock_;
  std::condition_variable persist_cv_;
  // Condition variable to signal disk log consumer task thread to wake up and flush buffers to disk or if shutdown has
  // initiated, then quit
  std::condition_variable disk_log_writer_thread_cv_;

  /**
   * Main disk log consumer task loop. Flushes buffers to disk when new buffers are handed to it via
   * filled_buffer_queue_, or when notified by LogManager to persist buffers
   */
  void DiskLogConsumerTaskLoop();

  /**
   * Flush all buffers in the filled buffers queue to the log file
   */
  void WriteBuffersToLogFile();

  /*
   * Persists the log file on disk by calling fsync, as well as calling callbacks for all committed transactions that
   * were persisted
   * @return number of commit records persisted, used for metrics
   */
  uint64_t PersistLogFile();
};
}  // namespace terrier::storage
