#include "whisperlib/io/cord_io.h"

namespace whisper {
namespace io {

size_t CordIo::SizeToWrite(const absl::Cord& cord,
                           absl::optional<size_t> size) {
  size_t size_to_write = cord.size();
  if (size.has_value() && size.value() < size_to_write) {
    size_to_write = size.value();
  }
  return size_to_write;
}

std::pair<std::vector<struct ::iovec>, size_t>
CordIo::ToIovec(const absl::Cord& cord, size_t size) {
  size_t cb = 0;
  std::vector<struct ::iovec> result;
  for (absl::string_view chunk : cord.Chunks()) {
    if (chunk.size() + cb > size) {
      chunk = chunk.substr(0, size - cb);
    }
    struct ::iovec v;
    v.iov_base = const_cast<void*>(reinterpret_cast<const void*>(chunk.data()));
    v.iov_len = chunk.size();
    result.push_back(v);

    cb += chunk.size();
    if (cb >= size) {
      break;
    }
  }
  return std::make_pair(std::move(result), cb);
}

}  // namespace io
}  // namespace whisper
