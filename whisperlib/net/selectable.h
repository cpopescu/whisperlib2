#ifndef WHISPERLIB_NET_SELECTABLE_H_
#define WHISPERLIB_NET_SELECTABLE_H_

#include "absl/status/statusor.h"
#include "absl/strings/cord.h"
#include "whisperlib/net/selector_event_data.h"

namespace whisper {
namespace net {

class Selector;

class Selectable {
 public:
  Selectable() = default;
  Selectable(Selector* selector);

  // Returns the selector associated with thei dile descriptor.
  const Selector* selector() const;
  Selector* selector();
  // Sets the selector - can be called only to reset the selector
  // (value is null) or to set the selector if selector_ is null.
  void set_selector(Selector* value);

  // In any of the following events the obj is safe to close itself.
  // The selector will notice the closure and will remove this obj from
  // its queue.

  // Signal that informs the selectable object that it should read from its
  // registered file descriptor.
  // Return true if events should be contiued to be processed for object.
  virtual bool HandleReadEvent(SelectorEventData event) { return true; }

  // Signal that informs the selectable object that it can write data out.
  // Return true if events should be contiued to be processed for object.
  virtual bool HandleWriteEvent(SelectorEventData event) { return true; }

  // Signal an error(exception) has occurred on the file descriptor.
  // Return true if events should be contiued to be processed for object.
  virtual bool HandleErrorEvent(SelectorEventData event) { return true; }

  // Returns the file descriptor associated w/ this Selectable object (if any)
  virtual int GetFd() const = 0;

  // Closes this selector and its associated file descriptor.
  virtual void Close() = 0;

 protected:
  static constexpr int kInvalidFdValue = (-1);

  // Writes the provided buffer to the associated file descriptor.
  // Returns the size actually written to the file descriptor.
  absl::StatusOr<size_t> Write(const char* buffer, size_t size);
  // Reads `size` bytes from the file descriptor to the provided buffer.
  absl::StatusOr<size_t> Read(char* buffer, size_t size);

  // Reads data from the file descriptor, at most size bytes, and appends it
  // to the provided Cord.
  absl::StatusOr<size_t> ReadToCord(absl::Cord* cord, size_t len);
  // Writes data from but from a Cord to the associated file descriptor.
  // If provided, len is the maximum number of bytes to write to the file.
  // Returns the number of bytes written.
  absl::StatusOr<size_t> WriteCord(const absl::Cord& cord,
                                   absl::optional<size_t> len = {});
  // Same as above, but uses vectorized iovec operations, which are
  // significantly faster for many (smaller) blocks.
  // Returns the number of bytes written.
  absl::StatusOr<size_t> WriteCordVec(const absl::Cord& cord,
                                      absl::optional<size_t> len = {});

  Selector* selector_ = nullptr;
  // the desire for read or write **DO NOT TOUCH** updated by the selector only
  uint32_t desire_ = SelectDesire::kWantRead | SelectDesire::kWantError;

  friend class Selector;
};

}  // namespace net
}  // namespace whisper

#endif  // WHISPERLIB_NET_SELECTABLE_H_
