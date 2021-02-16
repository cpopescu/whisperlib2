#include "whisperlib/net/selector.h"

#include <fcntl.h>
#include <unistd.h>
#include <algorithm>

#ifdef __linux__
#include <sys/eventfd.h>
#endif  // __linux__

#include "whisperlib/io/errno.h"

namespace whisper {
namespace net {

namespace {
absl::Status SetupNonBlocking(int fd) {
  const int flags = ::fcntl(fd, F_GETFL, 0);
  if (flags < 0) {
    return error::ErrnoToStatus(error::Errno())
      << "Obtaining file descriptor flags with ::fcntl(..) for: " << fd;
  }
  if (::fcntl(fd, F_SETFL, flags | O_NONBLOCK) < 0) {
    return error::ErrnoToStatus(error::Errno())
      << "Setting up non blocking property with ::fcntl(..) for: " << fd;
  }
  return absl::OkStatus();
}
bool CompareAlarms(const std::pair<absl::Time, uint64_t>& a,
                   const std::pair<absl::Time, uint64_t>& b) {
  return a.first > b.first;
};
}  // namespace

Selector::Selector(Params params)
  : params_(params) {
}

absl::StatusOr<std::unique_ptr<Selector>> Selector::Create(Params params) {
  auto selector = absl::WrapUnique(new Selector(std::move(params)));
  RETURN_IF_ERROR(selector->Initialize());
  return selector;
}

absl::Status Selector::Initialize() {
  if (params_.use_event_fd) {
    event_fd_ = ::eventfd(0, 0);
    if (event_fd_ < 0) {
      return error::ErrnoToStatus(error::Errno())
        << "Creating ::eventfd(..) file descriptor.";
    }
    RETURN_IF_ERROR(SetupNonBlocking(event_fd_))
      << "For event file descriptor.";
    signal_fd_ = event_fd_;
  } else {
    if (::pipe(signal_pipe_)) {
      return error::ErrnoToStatus(error::Errno())
        << "Creating ::pipe(..) file descriptors.";
    }
    RETURN_IF_ERROR(SetupNonBlocking(signal_pipe_[0]))
      << "For pipe file descriptor 0.";
    RETURN_IF_ERROR(SetupNonBlocking(signal_pipe_[1]))
      << "For pipe file descriptor 1.";
    signal_fd_ = signal_pipe_[0];
  }
  if (params_.use_epoll) {
    ASSIGN_OR_RETURN(loop_, EpollSelectorLoop::Create(
        signal_fd_, params_.max_events_per_step),
                     _ << "Creating the selector loop based on epoll.");
  } else {
    ASSIGN_OR_RETURN(loop_, PollSelectorLoop::Create(
        signal_fd_, params_.max_events_per_step),
                     _ << "Creating the selector loop based on poll.");
  }
  return absl::OkStatus();
}

Selector::~Selector() {
  CHECK(registered_.empty());
  if (params_.use_event_fd) {
    close(event_fd_);
  } else {
    close(signal_pipe_[0]);
    close(signal_pipe_[1]);
  }
}

Selector::Params Selector::params() const {
  return params_;
}

absl::Time Selector::now() const {
  return absl::FromUnixNanos(now_.load());
}
void Selector::UpdateNow() {
  now_.store(absl::GetCurrentTimeNanos());
}

absl::Status Selector::EnableWriteCallback(Selectable* s, bool enable) {
  return UpdateDesire(s, enable, SelectDesire::kWantWrite);
}
absl::Status Selector::EnableReadCallback(Selectable* s, bool enable) {
  return UpdateDesire(s, enable, SelectDesire::kWantRead);
}
void Selector::set_call_on_close(std::function<void()> call_on_close) {
  call_on_close_ = std::move(call_on_close);
}
bool Selector::IsExiting() const {
  return should_end_.load();
}
bool Selector::IsInSelectThread() const {
  return pthread_t(tid_.load()) == pthread_self();
}

absl::Status Selector::Register(Selectable* s) {
  RET_CHECK(tid_.load() == 0 || IsInSelectThread())
    << "Register only with a stopped selector or from the selector thread.";
  if (s->selector() == nullptr) {
    s->set_selector(this);
  } else {
    RET_CHECK(s->selector() == this)
      << "Selectable registered w/ a different selector.";
  }
  const int fd = s->GetFd();
  const auto it = registered_.find(s);
  if ( it != registered_.end() ) {
    return absl::OkStatus();
  }
  // Insert in the local set of registered objs
  registered_.insert(s);
  return loop_->Add(fd, s, s->desire_);
}

absl::Status Selector::Unregister(Selectable* s) {
  RET_CHECK(tid_.load() == 0 || IsInSelectThread())
    << "Unregister only with a stopped selector or from the selector thread.";
  RET_CHECK(s->selector() == this)
    << "Selectable registered w/ a different selector.";
  registered_.erase(s);
  s->set_selector(nullptr);
  return loop_->Delete(s->GetFd());
}

void Selector::RunInSelectLoop(std::function<void()> callback) {
  {
    absl::MutexLock l(&to_run_mutex_);
    to_run_.emplace_back(std::move(callback));
    have_to_run_.store(true);
  }
  if (!IsInSelectThread()) {
    SendWakeSignal();
  }
}

uint64_t Selector::RegisterAlarm(std::function<void()> callback,
                                 absl::Duration timeout) {
  absl::Time deadline = absl::Now() + timeout;
  absl::MutexLock l(&alarm_mutex_);
  const uint64_t alarm_id = alarm_id_.fetch_add(1, std::memory_order_acq_rel);
  alarms_.emplace(alarm_id, std::move(callback));
  alarm_timeouts_.push_back(std::make_pair(deadline, alarm_id));
  std::push_heap(alarm_timeouts_.begin(), alarm_timeouts_.end(),
                 &CompareAlarms);
  next_alarm_time_.store(absl::ToUnixNanos(alarm_timeouts_.back().first),
                         std::memory_order_acq_rel);
  num_registered_alarms_.store(alarms_.size());
  return alarm_id;
}

void Selector::UnregisterAlarm(uint64_t alarm_id) {
  absl::MutexLock l(&alarm_mutex_);
  alarms_.erase(alarm_id);
  num_registered_alarms_.store(alarms_.size());
}

void Selector::CleanAndCloseAll() {
  // It is some discussion, whether to do some CHECK if connections are
  // left at this point or to close them or to just skip it. The ideea is
  // that we preffer forcing a close on them for the server case and also
  // client connections when we end a program.
  // We just close the fd and care about nothing ..
  while (!registered_.empty()) {
    (*registered_.begin())->Close();
  }
}

size_t Selector::RunCallbacks(size_t max_num_to_run) {
  // Clean some bytes from the signaling file descriptor.
  char buffer[512];
  int cb = 0;
  while ((cb = ::read(signal_fd_, buffer, sizeof(buffer))) > 0) {
  }
  std::deque<std::function<void()>> to_run;
  {
    absl::MutexLock m(&to_run_mutex_);
    size_t num_to_run = std::min(max_num_to_run, to_run_.size());
    std::move(to_run_.begin(), to_run.begin() + num_to_run,
              std::back_inserter(to_run));
    if (num_to_run == to_run_.size()) {
      to_run_.clear();
    } else {
      for (size_t i = 0; i < num_to_run; ++i) {
        to_run_.pop_front();
      }
    }
    have_to_run_.store(!to_run_.empty());
  }
  const absl::Time start_time = absl::Now();
  const absl::Time deadline = start_time + params_.callbacks_timeout_per_event;
  size_t num_run = 0;
  while (!to_run.empty() && absl::Time() < deadline) {
    to_run.front()();
    to_run.pop_front();
    ++num_run;
  }
  if (!to_run.empty()) {
    absl::MutexLock m(&to_run_mutex_);
    while (!to_run.empty()) {
      to_run_.emplace_front(std::move(to_run.back()));
      to_run.pop_back();
    }
    have_to_run_.store(true);
  }
  return num_run;
}

void Selector::SendWakeSignal() {
  if (params_.use_event_fd) {
    uint64_t value = 1ULL;
    const int cb = ::write(event_fd_, &value, sizeof(value));
    if (ABSL_PREDICT_FALSE(cb < 0)) {
      LOG_EVERY_N(WARNING, 1000)
        << error::ErrnoToString(error::Errno())
        << "Error writing a wake-up value to selector event file descriptor.";
    }
  } else {
    int8_t byte = 0;
    const int cb = ::write(signal_pipe_[1], &byte, sizeof(byte));
    if (ABSL_PREDICT_FALSE(cb < 0)) {
      LOG_EVERY_N(WARNING, 1000)
        << error::ErrnoToString(error::Errno())
        << "Error writing a wake-up value to selector pipe file descriptor.";
    }
  }
}

absl::Status Selector::UpdateDesire(Selectable* s,
                                    bool enable, uint32_t desire) {
  RET_CHECK(s->selector() == this)
    << "Selectable registered w/ a different selector.";
  if ((((s->desire_ & desire) == desire) && enable) ||
      (((~s->desire_ & desire) == desire) && !enable)) {
    return absl::OkStatus();  // already set ..
  }
  if (enable) {
    s->desire_ |= desire;
  } else {
    s->desire_ &= ~desire;
  }
  return loop_->Update(s->GetFd(), s, s->desire_);
}

absl::Status Selector::Loop() {
  should_end_.store(false);
  tid_.store(uint64_t(pthread_self()));

  while (!should_end_.load()) {
    absl::Duration loop_timeout = params_.default_loop_timeout;
    UpdateNow();
    if (!to_run_.empty()) {
      loop_timeout = absl::ZeroDuration();
    } else {
      const absl::Duration alarm_delta = std::max(
          absl::FromUnixNanos(next_alarm_time_.load()) - now(),
          absl::ZeroDuration());
      if (alarm_delta < absl::ZeroDuration()) {
        loop_timeout = absl::ZeroDuration();
      } else if (alarm_delta < loop_timeout) {
        loop_timeout = alarm_delta;
      }
    }
    ASSIGN_OR_RETURN(std::vector<SelectorEventData> events,
                     loop_->LoopStep(loop_timeout),
                     _ << "During selector loop execution.");
    UpdateNow();
    for (auto event : events) {
      Selectable* const s = reinterpret_cast<Selectable*>(event.user_data);
      if (s == nullptr || s->selector() != this) {
        // was probably a wake signal or already unregistered
        continue;
      }
      // During HandleXEvent the obj may be closed loosing so track of
      // it's fd value.
      const uint32_t desire = event.desires;
      bool keep_processing = true;
      if (desire & SelectDesire::kWantError) {
        keep_processing = (s->HandleErrorEvent(event)
                           && s->GetFd() != kInvalidFdValue);
      }
      if (keep_processing && (desire & SelectDesire::kWantRead) ) {
        keep_processing = (s->HandleReadEvent(event) &&
                           s->GetFd() != kInvalidFdValue);
      }
      if (keep_processing && (desire & SelectDesire::kWantWrite)) {
        s->HandleWriteEvent(event);
      }
    }
    // TODO(cpopescu): track down long calls and many calls.
    LoopCallbacks();
    LoopAlarms();
  }
  CleanAndCloseAll();
  if (call_on_close_) {
    call_on_close_();
  }
  return absl::OkStatus();
}

size_t Selector::LoopCallbacks() {
  size_t run_count = 0;
  while (have_to_run_.load()
         && run_count < params_.max_num_callbacks_per_event) {
    UpdateNow();
    const size_t n = RunCallbacks(
        params_.max_num_callbacks_per_event - run_count);
    if (n == 0) { return run_count; }
    run_count += n;
  }
  return run_count;
}

size_t Selector::LoopAlarms() {
  if (num_registered_alarms_.load() == 0) {
    return 0;
  }
  UpdateNow();
  std::vector<std::function<void()>> to_run;
  {
    absl::Time end_alarms = now();
    absl::MutexLock l(&alarm_mutex_);
    while (!alarm_timeouts_.empty()
           && alarm_timeouts_.back().first <= end_alarms) {
      const uint64_t alarm_id = alarm_timeouts_.back().second;
      std::pop_heap(alarm_timeouts_.begin(), alarm_timeouts_.end(),
                    &CompareAlarms);
      alarm_timeouts_.pop_back();
      auto it = alarms_.find(alarm_id);
      if (it != alarms_.end()) {
        to_run.emplace_back(std::move(it->second));
        alarms_.erase(it);
      }
    }
    num_registered_alarms_.store(alarms_.size());
    next_alarm_time_.store(absl::ToUnixNanos(
        alarm_timeouts_.empty() ? absl::InfinitePast()
        : alarm_timeouts_.back().first), std::memory_order_acq_rel);
  }
  for (auto callback : to_run) {
    callback();
  }
  return to_run.size();
}

}  // namespace net
}  // namespace whisper
