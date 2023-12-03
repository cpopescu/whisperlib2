// This implements a thread by using pthread library, it allows
// for more options to be set.

#ifndef WHISPERLIB_SYNC_THREAD_H_
#define WHISPERLIB_SYNC_THREAD_H_

#include <pthread.h>

#include <memory>

#include "absl/functional/any_invocable.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"

namespace whisper {
namespace work {

class Thread {
 public:
  // Create and starts a thread that runs the given function.
  // Note on stack size:
  //    Min value is: PTHREAD_STACK_MIN - system dependent/
  //    Max value is: system-imposed limit - not defined.
  //    Default value is system dependent (normally 32MB)
  //
  static absl::StatusOr<std::unique_ptr<Thread>> Create(
      absl::AnyInvocable<void()> thread_function,
      absl::optional<absl::AnyInvocable<void() &&>> completion_callback = {},
      absl::optional<size_t> stack_size = {}, bool joinable = true,
      bool low_priority = false);

  ~Thread();

  // Waits for the thread to ends its execution.
  absl::Status Join();

  // If this thread is joinable, obtained via internal pthread call.
  absl::StatusOr<bool> IsJoinable() const;

  // Obtains the stack size for this thread, from internal pthread library.
  absl::StatusOr<size_t> GetStackSize() const;

  // Stops the thread unconditionally
  absl::Status Kill();

  // Test if the caller is in this thread context.
  bool IsInThread() const;

 private:
  Thread(absl::AnyInvocable<void()> thread_function,
         absl::optional<absl::AnyInvocable<void() &&>> completion_callback,
         absl::optional<size_t> stack_size, bool joinable, bool low_priority);

  // Prepares and runs the thread:
  absl::Status Initialize();

  Thread(const Thread&) = delete;
  Thread& operator=(const Thread&) = delete;

  static void* InternalRun(void* param);

  absl::AnyInvocable<void()> thread_function_;
  absl::optional<absl::AnyInvocable<void() &&>> completion_callback_;
  const absl::optional<size_t> stack_size_;
  const bool joinable_;
  const bool low_priority_;
  pthread_t thread_id_{};
  pthread_attr_t attr_;
  bool attr_created_ = false;
};

}  // namespace work
}  // namespace whisper

#endif  // WHISPERLIB_SYNC_THREAD_H_
