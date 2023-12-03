#include "whisperlib/io/file.h"

#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/uio.h>
#include <unistd.h>

#include <algorithm>
#include <utility>

#include "whisperlib/base/call_on_return.h"
#include "whisperlib/io/cord_io.h"
#include "whisperlib/io/errno.h"
#include "whisperlib/status/status.h"

namespace whisper {
namespace io {

absl::string_view File::AccessName(Access access) {
  switch (access) {
    case GENERIC_READ:
      return "GENERIC_READ";
    case GENERIC_WRITE:
      return "GENERIC_WRITE";
    case GENERIC_READ_WRITE:
      return "GENERIC_READ_WRITE";
  }
  return "UNKNOWN";
}

absl::string_view File::CreationDispositionName(CreationDisposition cd) {
  switch (cd) {
    case CREATE_ALWAYS:
      return "CREATE_ALWAYS";
    case CREATE_NEW:
      return "CREATE_NEW";
    case OPEN_ALWAYS:
      return "OPEN_ALWAYS";
    case OPEN_EXISTING:
      return "OPEN_EXISTING";
    case TRUNCATE_EXISTING:
      return "TRUNCATE_EXISTING";
  }
  return "UNKNOWN";
}

absl::string_view File::MoveMethodName(MoveMethod mm) {
  switch (mm) {
    case FILE_SET:
      return "FILE_SET";
    case FILE_CUR:
      return "FILE_CUR";
    case FILE_END:
      return "FILE_END";
  }
  return "UNKNOWN";
}

absl::StatusOr<std::unique_ptr<File>> File::Create(absl::string_view filename) {
  auto file = absl::make_unique<File>();
  RETURN_IF_ERROR(file->Open(filename, GENERIC_READ_WRITE, CREATE_ALWAYS));
  return file;
}

absl::StatusOr<std::unique_ptr<File>> File::Open(absl::string_view filename) {
  auto file = absl::make_unique<File>();
  RETURN_IF_ERROR(file->Open(filename, GENERIC_READ, OPEN_EXISTING));
  return file;
}

absl::StatusOr<std::string> File::ReadAsString(absl::string_view filename,
                                               size_t max_size) {
  ASSIGN_OR_RETURN(auto file, File::Open(filename));
  const size_t size = std::min(max_size, file->Size());
  std::string buffer(size, '\0');
  ASSIGN_OR_RETURN(const size_t cb, file->ReadBuffer(&buffer[0], size));
  buffer.resize(cb);
  RETURN_IF_ERROR(file->Close());
  return {std::move(buffer)};
}

absl::StatusOr<size_t> File::WriteFromString(absl::string_view filename,
                                             absl::string_view data) {
  ASSIGN_OR_RETURN(auto file, File::Create(filename));
  ASSIGN_OR_RETURN(const size_t cb, file->Write(data));
  RETURN_IF_ERROR(file->Close());
  return cb;
}

File::~File() { Close().IgnoreError(); }

absl::Status File::Open(absl::string_view filename, Access acc,
                        CreationDisposition cd) {
  RET_CHECK(!is_open()) << "Cannot open an already opened file: `" << filename
                        << "`";
  int flags = O_NOCTTY;
  switch (cd) {
    case CREATE_ALWAYS:
      flags |= O_CREAT | O_TRUNC;
      break;
    case CREATE_NEW:
      flags |= O_CREAT | O_EXCL;
      break;
    case OPEN_ALWAYS:
      flags |= O_CREAT;
      break;
    case OPEN_EXISTING:
      flags |= 0;
      break;
    case TRUNCATE_EXISTING:
      flags |= O_TRUNC;
      break;
    default:
      return status::InvalidArgumentErrorBuilder()
             << "Cannot open filename `" << filename
             << "` - Invalid creation disposition: " << cd;
  }

  mode_t mode = 0;
  switch (acc) {
    case GENERIC_READ:
      flags |= O_RDONLY;
      mode = 00444;
      break;
    case GENERIC_WRITE:
      flags |= O_WRONLY;
      mode = 00644;
      break;
    case GENERIC_READ_WRITE:
      flags |= O_RDWR;
      mode = 00644;
      break;
    default:
      return status::InvalidArgumentErrorBuilder()
             << "Cannot open filename `" << filename
             << "` - Invalid access: " << acc;
  }

  std::string filename_str(filename);
  const int fd = ::open(filename_str.c_str(), flags, mode);
  if (fd < 0) {
    return error::ErrnoToStatus(error::Errno())
           << "Cannot open file `" << filename << "` "
           << " using access: " << AccessName(acc)
           << " creation disposition: " << CreationDispositionName(cd);
  }

  return Set(filename, fd);
}

absl::Status File::Set(absl::string_view filename, int fd) {
  RET_CHECK(!is_open()) << "Cannot set an already opened file: `" << filename
                        << "`";
  filename_ = std::string(filename);
  fd_ = fd;
  RETURN_IF_ERROR(UpdateSize());
  RETURN_IF_ERROR(UpdatePosition());
  return absl::OkStatus();
}

absl::Status File::Close() {
  if (!is_open()) {
    return absl::OkStatus();
  }
  absl::Status status;
  if (::close(fd_) < 0) {
    status = error::ErrnoToStatus(error::Errno())
             << "Closing filename: `" << filename_ << "`";
  }
  fd_ = kInvalidFdValue;
  filename_.clear();
  size_ = 0;
  position_ = 0;
  return absl::OkStatus();
}

absl::Status File::UpdateSize() {
  struct stat st;
  if (0 != ::fstat(fd_, &st)) {
    return error::ErrnoToStatus(error::Errno())
           << "Obtaining file size for: `" << filename_ << "`";
  }
  size_ = st.st_size;
  return absl::OkStatus();
}

absl::Status File::UpdatePosition() {
  const off_t position = ::lseek(fd_, 0, SEEK_CUR);
  if (ABSL_PREDICT_FALSE(position < 0)) {
    return error::ErrnoToStatus(error::Errno())
           << "Obtaining the current position in file `" << filename_ << "`";
  }
  position_ = position;
  return absl::OkStatus();
}

size_t File::Size() const { return size_; }

size_t File::Position() const { return position_; }

size_t File::Remaining() const {
  return size_ > position_ ? size_ - position_ : 0;
}

absl::StatusOr<uint64_t> File::SetPosition(int64_t distance,
                                           MoveMethod move_method) {
  RET_CHECK(is_open());
  int whence = SEEK_SET;
  switch (move_method) {
    case FILE_SET:
      whence = SEEK_SET;
      break;
    case FILE_CUR:
      whence = SEEK_CUR;
      break;
    case FILE_END:
      whence = SEEK_END;
      break;
    default:
      return status::InvalidArgumentErrorBuilder()
             << "Invalid Move Method for SetPosition of file: `" << filename_
             << "` :" << move_method;
  }

  const off_t crt = ::lseek(fd_, distance, whence);
  if (ABSL_PREDICT_FALSE(crt < 0)) {
    return error::ErrnoToStatus(error::Errno())
           << "Seeing in file: `" << filename_ << "` to position: " << distance
           << " with: " << MoveMethodName(move_method);
  }
  // update cached position_
  position_ = uint64_t(crt);
  return position_;
}

absl::Status File::Rewind() { return SetPosition(0, FILE_SET).status(); }

absl::Status File::Truncate(absl::optional<uint64_t> pos) {
  RET_CHECK(is_open());
  const uint64_t trunc_pos = pos.has_value() ? pos.value() : Position();
  if (ABSL_PREDICT_FALSE(::ftruncate(fd_, trunc_pos) < 0)) {
    return error::ErrnoToStatus(error::Errno())
           << "Truncating file `" << filename_
           << "` to position: " << trunc_pos;
  }
  RETURN_IF_ERROR(SetPosition(0, FILE_END).status())
      << "Setting position at the end of the file upon truncation.";
  RETURN_IF_ERROR(UpdateSize()) << "Updating the file size upon truncation.";
  return absl::OkStatus();
}

absl::StatusOr<size_t> File::ReadBuffer(void* buffer, size_t size) {
  RET_CHECK(is_open());
  const ssize_t cb = ::read(fd_, buffer, size);
  if (ABSL_PREDICT_FALSE(cb < 0)) {
    return error::ErrnoToStatus(error::Errno())
           << "::read() failed for file: `" << filename_ << "`";
  }
  position_ += cb;
  if (size_ < position_) {
    // happens when the file gets bigger while we're reading
    RETURN_IF_ERROR(UpdateSize());
  }
  return size_t(cb);
}

absl::StatusOr<size_t> File::ReadToCord(absl::Cord* cord, size_t size) {
  RET_CHECK(is_open());
  char* buffer = new char[size];
  base::CallOnReturn clear_buffer([buffer]() { delete[] buffer; });
  ASSIGN_OR_RETURN(size_t cb, ReadBuffer(buffer, size));
  cord->Append(absl::MakeCordFromExternal(absl::string_view(buffer, cb),
                                          clear_buffer.reset()));
  return cb;
}

absl::Status File::Skip(int64_t size) {
  return SetPosition(size, FILE_CUR).status();
}

absl::StatusOr<size_t> File::WriteBuffer(const void* buffer, size_t size) {
  RET_CHECK(is_open());
  const ssize_t cb = ::write(fd_, buffer, size);
  if (ABSL_PREDICT_FALSE(cb < 0)) {
    absl::Status status = error::ErrnoToStatus(error::Errno())
                          << "::write() failed for file: `" << filename_ << "`";
    // don't know where the file pointer ended-up
    UpdatePosition().IgnoreError();
    return status;
  }
  position_ += cb;
  size_ = std::max(size_, position_);
  return size_t(cb);
}

absl::StatusOr<size_t> File::Write(absl::string_view s) {
  return WriteBuffer(s.data(), s.size());
}

absl::StatusOr<size_t> File::WriteCord(const absl::Cord& cord,
                                       absl::optional<size_t> size) {
  RET_CHECK(is_open());
  const size_t size_to_write = CordIo::SizeToWrite(cord, size);
  size_t cb = 0;
  for (absl::string_view chunk : cord.Chunks()) {
    if (chunk.size() + cb > size_to_write) {
      chunk = chunk.substr(0, size_to_write - cb);
    }
    ASSIGN_OR_RETURN(size_t crt_cb, Write(chunk),
                     _ << "Writing cord chunk in file.");
    if (ABSL_PREDICT_FALSE(crt_cb != chunk.size())) {
      return status::InternalErrorBuilder()
             << "Write to file operation failed. Expected a write of "
             << chunk.size() << " bytes, but: " << crt_cb
             << " bytes were written "
                "to file `"
             << filename_ << "`";
    }
    cb += crt_cb;
    if (cb >= size_to_write) {
      break;
    }
  }
  return cb;
}

absl::StatusOr<size_t> File::WriteCordVec(const absl::Cord& cord,
                                          absl::optional<size_t> size) {
  const size_t size_to_write = CordIo::SizeToWrite(cord, size);
  auto io_vec_pair = CordIo::ToIovec(cord, size_to_write);
  const ssize_t cb = ::writev(fd_, &io_vec_pair.first[0], io_vec_pair.second);
  if (ABSL_PREDICT_FALSE(cb < 0)) {
    absl::Status status = error::ErrnoToStatus(error::Errno())
                          << "::writeev() failed for file: `" << filename_
                          << "` with: " << io_vec_pair.first.size()
                          << " chunks and: " << io_vec_pair.second << " bytes.";
  }
  position_ += cb;
  size_ = std::max(size_, position_);
  if (ABSL_PREDICT_FALSE(size_t(cb) != io_vec_pair.second)) {
    return status::InternalErrorBuilder()
           << "::writeev() file operation failed. Expected a write of "
           << io_vec_pair.second << " bytes, but: " << cb
           << " bytes were written "
              "to file `"
           << filename_ << "`";
  }
  return io_vec_pair.second;
}

absl::Status File::Flush() {
  RET_CHECK(is_open());
#ifdef F_FULLFSYNC
  if (ABSL_PREDICT_FALSE(fcntl((fd_), F_FULLFSYNC) < 0)) {
    return error::ErrnoToStatus(error::Errno())
           << "Syncing data of file `" << filename_ << "` with fcntl";
  }
#else
  if (ABSL_PREDICT_FALSE(fdatasync((fd_)) < 0)) {
    return error::ErrnoToStatus(error::Errno())
           << "Syncing data of file `" << filename_ << "` with fdatasync";
  }
#endif
  return absl::OkStatus();
}

}  // namespace io
}  // namespace whisper
