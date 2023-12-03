#include "whisperlib/sync/thread.h"

#include <signal.h>
#ifdef __linux__
#include <sched.h>
#endif  // __linux__

#include <string>
#include <utility>

#include "absl/log/die_if_null.h"
#include "absl/strings/str_format.h"
#include "whisperlib/status/status.h"

namespace whisper {
namespace work {

absl::StatusOr<std::unique_ptr<Thread>> Thread::Create(
    absl::AnyInvocable<void()> thread_function,
    absl::optional<absl::AnyInvocable<void() &&>> completion_callback,
    absl::optional<size_t> stack_size, bool joinable, bool low_priority) {
  auto thr = absl::WrapUnique(new Thread(std::move(thread_function),
                                         std::move(completion_callback),
                                         stack_size, joinable, low_priority));
  RETURN_IF_ERROR(thr->Initialize());
  return {std::move(thr)};
}

Thread::Thread(
    absl::AnyInvocable<void()> thread_function,
    absl::optional<absl::AnyInvocable<void() &&>> completion_callback,
    absl::optional<size_t> stack_size, bool joinable, bool low_priority)
    : thread_function_(ABSL_DIE_IF_NULL(std::move(thread_function))),
      completion_callback_(std::move(completion_callback)),
      stack_size_(stack_size),
      joinable_(joinable),
      low_priority_(low_priority) {}

Thread::~Thread() {
  if (attr_created_) {
    pthread_attr_destroy(&attr_);
  }
}

namespace {
std::string StrThreadId(pthread_t thread_id) {
  return absl::StrFormat("%p", reinterpret_cast<void*>(thread_id));
}
}  // namespace

absl::Status Thread::Initialize() {
  int error = pthread_attr_init(&attr_);
  if (ABSL_PREDICT_FALSE(error != 0)) {
    return status::InternalErrorBuilder()
           << "pthread_attr_init() failed, error: " << error;
  }
  attr_created_ = true;
  error = pthread_attr_setdetachstate(
      &attr_, joinable_ ? PTHREAD_CREATE_JOINABLE : PTHREAD_CREATE_DETACHED);
  if (ABSL_PREDICT_FALSE(error != 0)) {
    return status::InternalErrorBuilder()
           << "pthread_attr_setdetachstate() failed, error: " << error;
  }
  if (low_priority_) {
#ifdef __linux__
    struct sched_param param;
    param.sched_priority = sched_get_priority_min(SCHED_RR);
    error = pthread_attr_setschedparam(&attr_, &param);
    if (ABSL_PREDICT_FALSE(error != 0)) {
      return status::InternalErrorBuilder()
             << "pthread_attr_setschedparam() failed, error: " << error;
    }
#else
    LOG(WARNING) << "Skipping setting thread to low priority for: "
                 << StrThreadId(thread_id_);
#endif
  }

  if (stack_size_.has_value()) {
#if defined(__linux__) || defined(__APPLE__)
#if defined(PTHREAD_STACK_MIN) && defined(PAGE_SIZE)
    RET_CHECK_LE(PTHREAD_STACK_MIN, stack_size_.value())
        << "Invalid stack size for system.";
#endif
    error = pthread_attr_setstacksize(&attr_, stack_size_.value());
    if (ABSL_PREDICT_FALSE(error != 0)) {
      return status::InternalErrorBuilder()
             << "pthread_attr_setstacksize() failed, error: " << error
             << " for a stack size of: " << stack_size_.value();
    }
#endif
  }

  // Ready to start:
  error = pthread_create(&thread_id_, &attr_, Thread::InternalRun, this);
  if (error != 0) {
    return status::InternalErrorBuilder()
           << "pthread_create() failed to start thread, error: " << error;
  }
  return absl::OkStatus();
}

absl::Status Thread::Join() {
  void* unused = nullptr;
  const int error = pthread_join(thread_id_, &unused);
  if (ABSL_PREDICT_FALSE(error != 0)) {
    return status::InternalErrorBuilder()
           << "pthread_join() failed for thread: " << StrThreadId(thread_id_)
           << " error : " << error;
  }
  return absl::OkStatus();
}

absl::StatusOr<bool> Thread::IsJoinable() const {
#if defined(__linux__) || defined(__APPLE__)
  int state = 0;
  const int error = pthread_attr_getdetachstate(&attr_, &state);
  if (ABSL_PREDICT_FALSE(error != 0)) {
    return status::InternalErrorBuilder()
           << "pthread_attr_getdetachstate() failed for thread: "
           << StrThreadId(thread_id_) << " error : " << error;
  }
  return state == PTHREAD_CREATE_JOINABLE;
#else
  return joinable_;
#endif
}

absl::StatusOr<size_t> Thread::GetStackSize() const {
#if defined(__linux__) || defined(__APPLE__)
  size_t stack_size;
  const int error = pthread_attr_getstacksize(&attr_, &stack_size);
  if (ABSL_PREDICT_FALSE(error != 0)) {
    return status::InternalErrorBuilder()
           << "pthread_attr_getstacksize() failed for thread: "
           << StrThreadId(thread_id_) << " error: " << error;
  }
  return stack_size;
#else
  return stack_size_.has_value() ? stack_size_.value() : 0;
#endif
}

bool Thread::IsInThread() const {
  return pthread_equal(pthread_self(), thread_id_);
}

absl::Status Thread::Kill() {
#if defined(__linux__) || defined(__APPLE__)
  const int error = pthread_kill(thread_id_, SIGKILL);
  if (ABSL_PREDICT_FALSE(error != 0)) {
    return status::InternalErrorBuilder()
           << "pthread_kill() failed for thread: " << StrThreadId(thread_id_)
           << " error: " << error;
  }
  return absl::OkStatus();
#else
  return absl::UnimplementedError("This::Kill not supported on this system");
#endif
}

void* Thread::InternalRun(void* param) {
  Thread* th = reinterpret_cast<Thread*>(param);
  th->thread_function_();
  if (th->completion_callback_.has_value()) {
    std::move(th->completion_callback_.value())();
  }
  pthread_exit(nullptr);
  return nullptr;
}

}  // namespace work
}  // namespace whisper
