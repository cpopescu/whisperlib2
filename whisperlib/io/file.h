#ifndef WHISPERLIB_IO_FILE_H_
#define WHISPERLIB_IO_FILE_H_

#include <memory>
#include <string>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/cord.h"
#include "absl/strings/string_view.h"
#include "absl/types/optional.h"

static_assert(sizeof(off_t) == sizeof(int64_t));

namespace whisper {
namespace io {

class File {
 public:
  enum Access { GENERIC_READ, GENERIC_WRITE, GENERIC_READ_WRITE };
  static absl::string_view AccessName(Access access);

  enum CreationDisposition {
    // Creates a new file, always.
    // If a file exists, the function overwrites the file, clears the existing
    // attributes.
    CREATE_ALWAYS,
    // Creates a new file. The function fails if a specified file exists.
    CREATE_NEW,
    // Opens a file, always.
    // If a file does not exist, the function creates a file as if
    // creation disposition is CREATE_NEW.
    OPEN_ALWAYS,
    // Opens a file.
    // The function fails if the file does not exist.
    // For more information, see the Remarks section of this topic.
    OPEN_EXISTING,
    // Opens a file and truncates it so that its size is zero (0) bytes.
    // The function fails if the file does not exist.
    // The calling process must open the file with the GENERIC_WRITE
    // access right.
    TRUNCATE_EXISTING
  };
  static absl::string_view CreationDispositionName(CreationDisposition cd);

  enum MoveMethod {
    FILE_SET = SEEK_SET,
    FILE_CUR = SEEK_CUR,
    FILE_END = SEEK_END
  };
  static absl::string_view MoveMethodName(MoveMethod mm);

  static constexpr int kInvalidFdValue = (-1);

 public:
  // Convenience function for creating / opening and truncting a file
  static absl::StatusOr<std::unique_ptr<File>> Create(
      absl::string_view filename);
  // Convenience function for opening a file for reading
  static absl::StatusOr<std::unique_ptr<File>> Open(absl::string_view filename);

  // Reads a file as a string. Reads at most length bytes.
  static absl::StatusOr<std::string> ReadAsString(absl::string_view filename,
                                                  size_t max_size = 4 << 20);
  // Writes the specified data to a file. If file exists, the data will be
  // overwritten.
  static absl::StatusOr<size_t> WriteFromString(absl::string_view filename,
                                                absl::string_view data);

  File() = default;
  virtual ~File();

  // Opens the file specified by name, with provided access and opening
  // creation disposition. The file should not be already opened.
  absl::Status Open(absl::string_view filename, Access access,
                    CreationDisposition cd);
  // Set the file from an externally opened file descriptor.
  // The file should not be already opened.
  absl::Status Set(absl::string_view filename, int fd);
  // Closes a file.
  absl::Status Close();

  // If the file is opened.
  bool is_open() const { return fd_ != kInvalidFdValue; }
  // The name of the file (path).
  absl::string_view filename() const { return filename_; }
  // The file descriptor of this file.
  int fd() const { return fd_; }

  // Returns current file size. The file must be opened.
  // (Uses local cached variable: size_)
  size_t Size() const;

  //  Get file pointer position relative to file begin.
  //  (Uses local cached variable: position_)
  size_t Position() const;

  // Remaining bytes to read in file, based on cached size_ and position_;
  size_t Remaining() const;

  // Set file pointer position to the given absolute offset (relative
  // to file begin). Returns the new position from the beginning of the file.
  absl::StatusOr<uint64_t> SetPosition(int64_t distance,
                                       MoveMethod move_method = FILE_SET);

  // Set file pointer to file begin.
  absl::Status Rewind();

  // Truncate the file to the given size (expands or shortens the file).
  // The file pointer is left at the end of file.
  // If pos is not specified, we truncate to current position.
  absl::Status Truncate(absl::optional<uint64_t> pos = {});

  // Reads "size" bytes of data from current file pointer position to
  // the given `buffer`.
  //  [IN/OUT] buffer: buffer that receives the read data.
  //  [IN]  size: the number of bytes to read.
  // Returns the number of bytes read.
  absl::StatusOr<size_t> ReadBuffer(void* buffer, size_t size);
  // Reads data from the file (at most size bytes) and appends it to the
  // provided Cord.
  absl::StatusOr<size_t> ReadToCord(absl::Cord* cord, size_t size);

  // Skip bytes from current position.
  absl::Status Skip(int64_t size);

  // Writes "size" bytes of data from "buf" to file at current file
  // pointer position.
  //   buf: buffer that contains the data to be written.
  //   size: number of bytes to write.
  // Returns the number of bytes written.
  absl::StatusOr<size_t> WriteBuffer(const void* buffer, size_t size);
  // Same write, but from string view buffer.
  // Returns the number of bytes written.
  absl::StatusOr<size_t> Write(absl::string_view s);
  // Same write, but from a Cord. If provided, size is the maximum number
  // of bytes to write to the file.
  // The operation is performed as multiple individual writes.
  // Returns the number of bytes written.
  absl::StatusOr<size_t> WriteCord(const absl::Cord& cord,
                                   absl::optional<size_t> size = {});
  // Same as above, but uses vectorized iovec operations, which are
  // significantly faster for many (smaller) blocks.
  // Returns the number of bytes written.
  absl::StatusOr<size_t> WriteCordVec(const absl::Cord& cord,
                                      absl::optional<size_t> size = {});

  // Forces a disk flush
  absl::Status Flush();

 protected:
  // Name of the opened file:
  std::string filename_;
  // File descriptor of the opened file.
  int fd_ = kInvalidFdValue;
  // Cached file size upon opening.
  size_t size_ = 0;
  // Current file pointer position.
  size_t position_;

  //  Retrieve file size from system and set value in size_.
  absl::Status UpdateSize();
  // Retrieve file pointer from system and set value in position_.
  absl::Status UpdatePosition();
};

}  // namespace io
}  // namespace whisper

#endif  // WHISPERLIB_IO_FILE_H_
