#ifndef WHISPERLIB_IO_CORD_IO_H_
#define WHISPERLIB_IO_CORD_IO_H_

#include <sys/uio.h>

#include <utility>
#include <vector>

#include "absl/strings/cord.h"
#include "absl/types/optional.h"

namespace whisper {
namespace io {

// Helpers for absl::Cord class, in the context of i/o operations .
class CordIo {
 public:
  // Returns the size to write from a cord, with an optional side limit.
  // If specified, we return the size limited by the cord size; else we use
  // the cord size.
  static size_t SizeToWrite(const absl::Cord& cord,
                            absl::optional<size_t> size);

  // Returns the chunks in the cord, up to the provided size, as a vector
  // of iovec structures to be used for Write operations.
  // Returns the prepare iovec structures and the size prepared.
  static std::pair<std::vector<struct ::iovec>, size_t> ToIovec(
      const absl::Cord& cord, size_t size);
};

}  // namespace io
}  // namespace whisper

#endif  // WHISPERLIB_IO_CORD_IO_H_
