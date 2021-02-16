#include "whisperlib/net/selectable.h"

#include <unistd.h>
#include <sys/uio.h>

#include "whisperlib/base/call_on_return.h"
#include "whisperlib/io/cord_io.h"
#include "whisperlib/io/errno.h"
#include "whisperlib/status/status.h"

namespace whisper {
namespace net {

const Selector* Selectable::selector() const {
  return selector_;
}
Selector* Selectable::selector() {
  return selector_;
}
void Selectable::set_selector(Selector* value) {
  CHECK(selector_ == nullptr || value == nullptr);
  selector_ = value;
}

absl::StatusOr<size_t> Selectable::Write(const char* buffer, size_t size) {
  const int fd = GetFd();
  const ssize_t cb = ::write(fd, buffer, size);
  if (cb >= 0) {
    return cb;
  }
  const int write_error = error::Errno();
  if (error::IsUnavailableAndShouldRetry(write_error)) {
    return 0;
  }
  return error::ErrnoToStatus(write_error)
    << "Writing data to file descriptor: " << fd << " size: " << size;
}

absl::StatusOr<size_t> Selectable::Read(char* buffer, size_t size) {
  const int fd = GetFd();
  const ssize_t cb = ::read(fd, buffer, size);
  if (cb >= 0) {
    return cb;
  }
  const int read_error = error::Errno();
  if (error::IsUnavailableAndShouldRetry(read_error)) {
    return 0;
  }
  return error::ErrnoToStatus(read_error)
    << "Reading data to file descriptor: " << fd << " size: " << size;
}

absl::StatusOr<size_t> Selectable::ReadToCord(absl::Cord* cord, size_t len) {
  char* buffer = new char[len];
  base::CallOnReturn clear_buffer([buffer]() { delete [] buffer; });
  ASSIGN_OR_RETURN(size_t cb, Read(buffer, len));
  if (cb == 0) { return 0; }
  cord->Append(absl::MakeCordFromExternal(
      absl::string_view(buffer, cb), clear_buffer.reset()));
  return cb;
}

absl::StatusOr<size_t> Selectable::WriteCord(const absl::Cord& cord,
                                             absl::optional<size_t> size) {
  const size_t size_to_write = io::CordIo::SizeToWrite(cord, size);
  size_t cb = 0;
  for (absl::string_view chunk : cord.Chunks()) {
    if (chunk.size() + cb > size_to_write) {
      chunk = chunk.substr(0, size_to_write - cb);
    }
    ASSIGN_OR_RETURN(size_t crt_cb, Write(chunk.data(), chunk.size()),
                     _ << "Writing cord chunk in file.");
    cb += crt_cb;
    if (cb >= size_to_write || cb == 0) {
      break;
    }
  }
  return cb;
}

absl::StatusOr<size_t> Selectable::WriteCordVec(
    const absl::Cord& cord, absl::optional<size_t> size) {
  const size_t size_to_write = io::CordIo::SizeToWrite(cord, size);
  auto io_vec_pair = io::CordIo::ToIovec(cord, size_to_write);
  const int fd = GetFd();
  const ssize_t cb = ::writev(
      fd, &io_vec_pair.first[0], io_vec_pair.second);

  if (ABSL_PREDICT_FALSE(cb < 0)) {
    const int write_error = error::Errno();
    if (error::IsUnavailableAndShouldRetry(write_error)) {
      return 0;
    }
    return error::ErrnoToStatus(error::Errno())
      << "Writing data to file descriptor with writeev: " << fd
      << " size: " << size_to_write;
  }
  return cb;
}

}  // namespace net
}  // namespace whisper
