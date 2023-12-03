#include "whisperlib/net/selector_loop.h"

#include "whisperlib/io/errno.h"
#include "whisperlib/status/status.h"

namespace whisper {
namespace net {

int PollTimeout(absl::Duration timeout) {
  static const absl::Duration kMinTimeout = absl::Milliseconds(1);
  if (timeout < kMinTimeout) {
    timeout = kMinTimeout;
  }
  return absl::ToInt64Milliseconds(timeout);
}

#ifdef HAVE_EPOLL
absl::StatusOr<std::unique_ptr<EpollSelectorLoop>> EpollSelectorLoop::Create(
    int signal_fd, size_t max_events_per_step) {
  auto loop = absl::WrapUnique(
      new EpollSelectorLoop(signal_fd, max_events_per_step));
  RETURN_IF_ERROR(loop->Initialize());
  return loop;
}

EpollSelectorLoop::EpollSelectorLoop(int signal_fd,
                                     size_t max_events_per_step)
  : signal_fd_(signal_fd),
    events_(max_events_per_step) {
}

EpollSelectorLoop::~EpollSelectorLoop() {
  close(epfd_);
}

absl::Status EpollSelectorLoop::Initialize() {
  // argument outdated anyway - needs to be > 0
  epfd_ = ::epoll_create(1);
  if (epfd_ < 0) {
    return error::ErrnoToStatus(error::Errno())
      << "Creating epoll file descriptor during ::epoll_create()";
  }
  RETURN_IF_ERROR(Add(signal_fd_, nullptr,
                      SelectDesire::kWantRead | SelectDesire::kWantError))
    << "Adding the signaling file descriptor " << signal_fd_ << " while "
    "creating the selector loop.";
  return absl::OkStatus();
}

absl::Status EpollSelectorLoop::Add(int fd, void* user_data, uint32_t desires) {
  RET_CHECK(fd >= 0) << "Invalid file descriptor cannot be added to epoll.";
  epoll_event event;
  event.events = static_cast<unsigned int>(DesiresToEpollEvents(desires));
  event.data.ptr = user_data;

  if (epoll_ctl(epfd_, EPOLL_CTL_ADD, fd, &event) < 0) {
    return error::ErrnoToStatus(error::Errno())
      << "Adding event to epoll structure; file descriptor: " << fd
      << " for events: " << event.events;
  }
  return absl::OkStatus();
}

absl::Status EpollSelectorLoop::Update(
    int fd, void* user_data, uint32_t desires) {
  RET_CHECK(fd >= 0) << "Invalid file descriptor cannot be updated in epoll.";
  epoll_event event;
  event.events = static_cast<unsigned int>(DesiresToEpollEvents(desires));
  event.data.ptr = user_data;

  if (epoll_ctl(epfd_, EPOLL_CTL_MOD, fd, &event)) {
    return error::ErrnoToStatus(error::Errno())
      << "Updating event to epoll structure; file descriptor: " << fd
      << " for events: " << event.events;
  }
  return absl::OkStatus();
}

absl::Status EpollSelectorLoop::Delete(int fd) {
  RET_CHECK(fd >= 0) << "Invalid file descriptor cannot be deleted from epoll.";
  epoll_event event = { 0, };
  if (epoll_ctl(epfd_, EPOLL_CTL_DEL, fd, &event) < 0) {
    return error::ErrnoToStatus(error::Errno())
      << "Deleting event to epoll structure; file descriptor: " << fd;
  }
  return absl::OkStatus();
}

uint32_t EpollSelectorLoop::DesiresToEpollEvents(uint32_t desires) {
  uint32_t events = 0;
  if (desires & SelectDesire::kWantRead) {
    events |= EPOLLIN | EPOLLRDHUP;
  }
  if (desires & SelectDesire::kWantWrite) {
    events |= EPOLLOUT;
  }
  if (desires & SelectDesire::kWantError) {
    events |= EPOLLERR | EPOLLHUP;
  }
  return events;
}

absl::StatusOr<std::vector<SelectorEventData>>
EpollSelectorLoop::LoopStep(absl::Duration timeout) {
  const int num_events = epoll_wait(epfd_, &events_[0], events_.size(),
                                    PollTimeout(timeout));
  if (num_events < 0 && errno != EINTR) {
    return error::ErrnoToStatus(error::Errno())
      << "Encountered during epoll_wait.";
  }
  std::vector<SelectorEventData> events;
  events.reserve(num_events);
  for (int i = 0; i < num_events; ++i) {
    const struct epoll_event& event = events_[i];
    uint32_t desire = 0;
    if (event.events & (EPOLLERR | EPOLLHUP | EPOLLRDHUP)) {
      desire |= SelectDesire::kWantError;
    }
    if (event.events & (EPOLLIN | EPOLLPRI)) {
      desire |= SelectDesire::kWantRead;
    }
    if (event.events & EPOLLOUT) {
      desire |= SelectDesire::kWantWrite;
    }
    events.push_back(SelectorEventData {
        event.data.ptr, desire, event.events });
  }
  return events;
}
bool EpollSelectorLoop::IsHangUpEvent(int event_value) const {
  return (event_value & EPOLLHUP) != 0;
}
bool EpollSelectorLoop::IsRemoteHangUpEvent(int event_value) const {
  return (event_value & EPOLLRDHUP) != 0;
}
bool EpollSelectorLoop::IsAnyHangUpEvent(int event_value) const {
  return (event_value & (EPOLLHUP | EPOLLRDHUP)) != 0;
}
bool EpollSelectorLoop::IsErrorEvent(int event_value) const {
  return (event_value & EPOLLERR) != 0;
}
bool EpollSelectorLoop::IsInputEvent(int event_value) const {
  return (event_value & EPOLLIN) != 0;
}
#endif  // HAVE_EPOLL

PollSelectorLoop::PollSelectorLoop(int signal_fd)
  : signal_fd_(signal_fd) {
}

const size_t PollSelectorLoop::kMaxFds;

absl::StatusOr<std::unique_ptr<PollSelectorLoop>> PollSelectorLoop::Create(
    int signal_fd) {
  auto loop = absl::WrapUnique(new PollSelectorLoop(signal_fd));
  RETURN_IF_ERROR(loop->Initialize());
  return loop;
}

absl::Status PollSelectorLoop::Initialize() {
  RETURN_IF_ERROR(Add(signal_fd_, nullptr,
                      SelectDesire::kWantRead | SelectDesire::kWantError))
    << "Adding the signaling file descriptor " << signal_fd_ << " while "
    "creating the selector loop.";
  return absl::OkStatus();
}

PollSelectorLoop::~PollSelectorLoop() {}

absl::Status PollSelectorLoop::Add(int fd, void* user_data, uint32_t desires) {
  RET_CHECK(fd >= 0) << "Invalid file descriptor cannot be added to epoll.";
  if (fds_size_ >= kMaxFds) {
    return status::ResourceExhaustedErrorBuilder()
      << "Too many file descriptors in the poll structure. Reached the limit "
      "of " << kMaxFds << " file descriptors.";
  }
  fds_[fds_size_].fd = fd;
  fds_[fds_size_].events = DesiresToPollEvents(desires);
  fds_[fds_size_].revents = 0;
  fd_data_.emplace(fd, std::make_pair(fds_size_, user_data));
  ++fds_size_;
  return absl::OkStatus();
}

absl::Status PollSelectorLoop::Update(
    int fd, void* user_data, uint32_t desires) {
  auto it = fd_data_.find(fd);
  if (it == fd_data_.end()) {
    return status::NotFoundErrorBuilder()
      << "Cannot update select data for file descriptor: " << fd << " as it "
      "cannot be found in poll selector registered file descriptors.";
  }
  const size_t index = it->second.first;
  fds_[index].events = DesiresToPollEvents(desires);
  it->second.second = user_data;
  return absl::OkStatus();
}

absl::Status PollSelectorLoop::Delete(int fd) {
  auto it = fd_data_.find(fd);
  if (it == fd_data_.end()) {
    return status::NotFoundErrorBuilder()
      << "Cannot delete select data for file descriptor: " << fd << " as it "
      "cannot be found in poll selector registered file descriptors.";
  }
  const size_t index = it->second.first;
  // We don't compact now, as we may loose processing for these events
  // if we do this in the middle of a step, however we delete the fd.
  indices_to_compact_.push_back(index);
  fds_[index].fd = -1;
  fd_data_.erase(it);
  return absl::OkStatus();
}

void PollSelectorLoop::Compact() {
  if (indices_to_compact_.empty()) {
    return;
  }
  std::sort(indices_to_compact_.begin(), indices_to_compact_.end());
  for (size_t i = indices_to_compact_.size(); i > 0 && fds_size_ > 0; --i) {
    const size_t index = indices_to_compact_[i - 1];
    --fds_size_;
    if (ABSL_PREDICT_FALSE(fds_size_ == 0 || index == fds_size_)) {
      continue;
    }
    auto it = fd_data_.find(fds_[fds_size_].fd);
    if (ABSL_PREDICT_FALSE(it == fd_data_.end())) {
      continue;
    }
    it->second.first = index;
    fds_[index].fd = fds_[fds_size_].fd;
    fds_[index].events = fds_[fds_size_].events;
    fds_[index].revents = 0;
  }
}

int PollSelectorLoop::DesiresToPollEvents(uint32_t desires) {
  int events = 0;
  if (desires & SelectDesire::kWantRead) {
    events |= POLLIN | POLLRDHUP;
  }
  if (desires & SelectDesire::kWantWrite) {
    events |= POLLOUT;
  }
  if (desires & SelectDesire::kWantError) {
    events |= POLLERR | POLLHUP;
  }
  return events;
}

absl::StatusOr<std::vector<SelectorEventData>>
PollSelectorLoop::LoopStep(absl::Duration timeout) {
  Compact();
  int num_events = poll(fds_, fds_size_, PollTimeout(timeout));
  if (num_events < 0 && errno != EINTR) {
    return error::ErrnoToStatus(error::Errno())
      << "Encountered during poll.";
  }
  std::vector<SelectorEventData> events;
  events.reserve(num_events);
  for (size_t i = 0; i < fds_size_ && num_events > 0 ; ++i) {
    const struct pollfd& event = fds_[i];
    if (event.revents == 0) {
      continue;
    }
    uint32_t desire = 0;
    if (event.revents & (POLLERR | POLLHUP | POLLRDHUP)) {
      desire |= SelectDesire::kWantError;
    }
    if (event.revents & (POLLIN | POLLPRI)) {
      desire |= SelectDesire::kWantRead;
    }
    if (event.events & POLLOUT) {
      desire |= SelectDesire::kWantWrite;
    }
    const DataMap::iterator it = fd_data_.find(event.fd);
    if (it != fd_data_.end()) {
      events.push_back(SelectorEventData {
          it->second.second, desire, uint32_t(event.revents) });
    }
    --num_events;
  }
  return events;
}

bool PollSelectorLoop::IsHangUpEvent(int event_value) const {
  return (event_value & POLLHUP) != 0;
}
bool PollSelectorLoop::IsRemoteHangUpEvent(int event_value) const {
  return (event_value & POLLRDHUP) != 0;
}
bool PollSelectorLoop::IsAnyHangUpEvent(int event_value) const {
  return (event_value & (POLLHUP | POLLRDHUP)) != 0;
}
bool PollSelectorLoop::IsErrorEvent(int event_value) const {
  return (event_value & POLLERR) != 0;
}
bool PollSelectorLoop::IsInputEvent(int event_value) const {
  return (event_value & POLLIN) != 0;
}

#ifdef HAVE_KQUEUE

static const int kKQueueHangUp = 1;
static const int kKQueueErrorEvent = 2;
static const int kKQueueInputEvent = 4;


bool KQueueSelectorLoop::IsHangUpEvent(int event_value) const {
  return (event_value & kKQueueHangUp) != 0;
}

bool KQueueSelectorLoop::IsRemoteHangUpEvent(int event_value) const {
  return false;
}

bool KQueueSelectorLoop::IsAnyHangUpEvent(int event_value) const {
  return (event_value & kKQueueHangUp) != 0;
}

bool KQueueSelectorLoop::IsErrorEvent(int event_value) const {
  return (event_value & kKQueueErrorEvent) != 0;
}

bool KQueueSelectorLoop::IsInputEvent(int event_value) const {
  return (event_value & kKQueueInputEvent) != 0;
}

#endif  // HAVE_KQUEUE

}  // namespace net
}  // namespace whisper
