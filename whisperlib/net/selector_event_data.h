#ifndef WHISPERLIB_NET_SELECTOR_EVENT_DATA_H_
#define WHISPERLIB_NET_SELECTOR_EVENT_DATA_H_

#include <cstdint>

namespace whisper {
namespace net {

// The operations desired to be watched / performed on a file descriptor
// registered with a select loop.
struct SelectDesire {
  static constexpr uint32_t kWantRead  = 1;
  static constexpr uint32_t kWantWrite = 2;
  static constexpr uint32_t kWantError = 4;
};

// Data associated with an event detected by a select loop.
struct SelectorEventData {
  // User data associated with the file descriptor file descriptor.
  void* user_data;
  // What is desired to be performed on this file descriptor.
  // A bitmask of SelectDesire values.
  uint32_t desires;
  // Internal event value - specific to the system implementation.
  // (e.g. for epoll the EPOLL events triggered bitmask etc).
  uint32_t internal_event;
};

static constexpr int kInvalidFdValue = -1;
}  // namespace whisperlib
}  // namespace net

#endif  // WHISPERLIB_NET_SELECTOR_EVENT_DATA_H_
