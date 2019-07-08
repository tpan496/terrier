#pragma once
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/uio.h>
#include <unistd.h>
#include <cerrno>
#include <cstring>
#include <string>
#include "common/constants.h"
#include "common/macros.h"
#include "loggers/storage_logger.h"

namespace terrier::storage {

/**
 * Modernized wrappers around Posix I/O sys calls to hide away the ugliness and use exceptions for error reporting.
 */
struct PosixIoWrappers {
  PosixIoWrappers() = delete;  // Un-instantiable

  // TODO(Tianyu): Use a better exception than runtime_error.
  /**
   * Wrapper around posix open call
   * @tparam Args type of varlen arguments
   * @param path posix path arg
   * @param oflag posix oflag arg
   * @param args posix mode arg
   * @throws runtime_error if the underlying posix call failed
   * @return a non-negative interger that is the file descriptor if the opened file.
   */
  template <class... Args>
  static int Open(const char *path, int oflag, Args... args) {
    while (true) {
      int ret = open(path, oflag, args...);
      if (ret == -1) {
        if (errno == EINTR) continue;
        throw std::runtime_error("Failed to open file with errno " + std::to_string(errno));
      }
      return ret;
    }
  }
  /**
   * Wrapper around posix close call
   * @param fd posix filedes arg
   * @throws runtime_error if the underlying posix call failed
   */
  static void Close(int fd);

  /**
   * Call fsync to persist file on disk
   */
  static void Persist(int fd) {
    if (fsync(fd) == -1) throw std::runtime_error("fsync failed with errno " + std::to_string(errno));
  }

  /**
   * Wrapper around the posix read call, where a single function call will always read the specified amount of bytes
   * unless eof is read. (unlike posix read, which can read arbitrarily many bytes less than the given amount)
   * @param fd posix fildes arg
   * @param buf posix buf arg
   * @param nbyte posix nbyte arg
   * @throws runtime_error if the underlying posix call failed
   * @return nbyte if the read is successful, or the number of bytes actually read if eof is read before nbytes are
   *         read. (i.e. there aren't enough bytes left in the file to read out nbyte many)
   */
  static uint32_t ReadFully(int fd, void *buf, size_t nbyte);

  /**
   * Wrapper around the posix write call, where a single function call will always write the entire buffer out.
   * (unlike posix write, which can write arbitrarily many bytes less than the given amount)
   * @param fd posix fildes arg
   * @param buf posix buf arg
   * @param nbyte posix nbyte arg
   * @throws runtime_error if the underlying posix call failed
   */
  static void WriteFully(int fd, const void *buf, size_t nbyte);
};
// TODO(Tianyu):  we need control over when and what to flush as the log manager. Thus, we need to write our
// own wrapper around lower level I/O functions. I could be wrong, and in that case we should
// revert to using STL.
/**
 * Handles buffered writes to the write ahead log, and provides control over flushing.
 */
class BufferedLogWriter {
  // TODO(Tianyu): Checksum
 public:
  /**
   * Instantiates a new BufferedLogWriter. A BufferedLogWriter is a light wrapper over a char buffer that handles
   * buffering multiple writes into the same buffer. The contents can be written to a destination through the
   * FlushBuffer method.
   */
  explicit BufferedLogWriter() {}

  /**
   * Write to the log file the given amount of bytes from the given location in memory, but buffer the write so the
   * update is only written out when the BufferedLogWriter is persisted. Note that this function writes to the buffer
   * only until it is full. If buffer gets full, then call FlushBuffer() and call BufferWrite(..) again with the correct
   * offset of the data, depending on the number of bytes that were already written.
   * @param data memory location of the bytes to write
   * @param size number of bytes to write
   * @return number of bytes written. This function only writes until the buffer gets full, so this can be used as the
   * offset when calling this function again after flushing.
   */
  uint32_t BufferWrite(const void *data, uint32_t size) {
    // If we still do not have buffer space after flush, the write is too large to be buffered. We partially write the
    // buffer and return the number of bytes written
    if (!CanBuffer(size)) {
      size = common::Constants::LOG_BUFFER_SIZE - buffer_size_;
    }
    std::memcpy(buffer_ + buffer_size_, data, size);
    buffer_size_ += size;
    return size;
  }

  /**
   * Flush buffered writes
   * @param fd file descriptor of file to flush to
   * @return amount of data flushed
   */
  uint64_t FlushBuffer(int fd) {
    auto size = buffer_size_;
    PosixIoWrappers::WriteFully(fd, buffer_, buffer_size_);
    buffer_size_ = 0;
    return size;
  }

  /**
   * @return if the buffer is full
   */
  bool IsBufferFull() { return buffer_size_ == common::Constants::LOG_BUFFER_SIZE; }

 private:
  char buffer_[common::Constants::LOG_BUFFER_SIZE];

  uint32_t buffer_size_ = 0;

  bool CanBuffer(uint32_t size) { return common::Constants::LOG_BUFFER_SIZE - buffer_size_ >= size; }
};

/**
 * Buffered reads from the write ahead log
 */
class BufferedLogReader {
  // TODO(Tianyu): Checksum
 public:
  /**
   * Instantiates a new BufferedLogReader to read from the specified log file.
   * @param log_file_path path to the the log file to read from.
   */
  explicit BufferedLogReader(const char *log_file_path) : in_(PosixIoWrappers::Open(log_file_path, O_RDONLY)) {}

  /**
   * @return if there are contents left in the write ahead log
   */
  bool HasMore() { return filled_size_ > read_head_ || in_ != -1; }

  /**
   * Read the specified number of bytes into the target location from the write ahead log. The method reads as many as
   * possible if there are not enough bytes in the log and returns false. The underlying log file fd is automatically
   * closed when all remaining bytes are buffered.
   *
   * @param dest pointer location to read into
   * @param size number of bytes to read
   * @return whether the log has the given number of bytes left
   */
  bool Read(void *dest, uint32_t size);

  /**
   * Read a value of the specified type from the log. An exception is thrown if the log file does not
   * have enough bytes left for a well formed value
   * @tparam T type of value to read
   * @return the value read
   */
  template <class T>
  T ReadValue() {
    T result;
    bool ret UNUSED_ATTRIBUTE = Read(&result, sizeof(T));
    TERRIER_ASSERT(ret, "Reading of value failed");
    return result;
  }

 private:
  int in_;  // or -1 if closed
  uint32_t read_head_ = 0, filled_size_ = 0;
  char buffer_[common::Constants::LOG_BUFFER_SIZE];

  void ReadFromBuffer(void *dest, uint32_t size) {
    TERRIER_ASSERT(read_head_ + size <= filled_size_, "Not enough bytes in buffer for the read");
    std::memcpy(dest, buffer_ + read_head_, size);
    read_head_ += size;
  }

  void RefillBuffer();
};
}  // namespace terrier::storage
