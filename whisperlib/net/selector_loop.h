#ifndef WHISPERLIB_NET_SELECTOR_LOOP_H_
#define WHISPERLIB_NET_SELECTOR_LOOP_H_

#include <cstdint>
#include <memory>
#include <vector>

#include "absl/container/flat_hash_map.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/time/time.h"
#include "whisperlib/net/selector_event_data.h"

#ifdef __linux__
#include <sys/poll.h>
#include <sys/epoll.h>
// We need this definde to a safe value anyway
#ifndef EPOLLRDHUP
#define EPOLLRDHUP 0
#endif
#define HAVE_EPOLL
#else
#include <poll.h>
#ifndef POLLRDHUP
#define POLLRDHUP 0
#endif
#endif  // __linux

namespace whisper {
namespace net {

class SelectorLoop {
public:
  SelectorLoop() = default;
  virtual ~SelectorLoop() = default;

  // Adds a file descriptor tot the select loop, with some associated
  // user data and desires for operations to be performed (OR of SelectDesire
  // values).
  virtual absl::Status Add(int fd, void* user_data, uint32_t desires) = 0;
  // Updates the desires and the user data for the file descriptor in the poll.
  virtual absl::Status Update(int fd, void* user_data, uint32_t desires) = 0;
  // Remove a file descriptor from the poll.
  virtual absl::Status Delete(int fd) = 0;

  // Run a selector loop step. It fills in events two things:
  //  -- the user data associted with the fd that was triggered
  //  -- the event that happended (an or of Selector desires)
  virtual absl::StatusOr<std::vector<SelectorEventData>>
  LoopStep(absl::Duration timeout) = 0;

  // Abstracts away identification of signals received through (poll) events:
  virtual bool IsHangUpEvent(int event_value) const = 0;
  virtual bool IsRemoteHangUpEvent(int event_value) const = 0;
  virtual bool IsAnyHangUpEvent(int event_value) const = 0;
  virtual bool IsErrorEvent(int event_value) const = 0;
  virtual bool IsInputEvent(int event_value) const = 0;
};

#ifdef HAVE_EPOLL
// A selector loop implementation based on epoll - a linux special.
class EpollSelectorLoop : public SelectorLoop {
public:
  static absl::StatusOr<std::unique_ptr<EpollSelectorLoop>> Create(
      int signal_fd, size_t max_events_per_step);
  ~EpollSelectorLoop();

  absl::Status Add(int fd, void* user_data, uint32_t desires) override;
  absl::Status Update(int fd, void* user_data, uint32_t desires) override;
  absl::Status Delete(int fd) override;

  absl::StatusOr<std::vector<SelectorEventData>> LoopStep(
      absl::Duration timeout) override;

  bool IsHangUpEvent(int event_value) const override;
  bool IsRemoteHangUpEvent(int event_value) const override;
  bool IsAnyHangUpEvent(int event_value) const override;
  bool IsErrorEvent(int event_value) const override;
  bool IsInputEvent(int event_value) const override;

private:
  EpollSelectorLoop(int signal_fd, int max_events_per_step);

  absl::Status Initialize();

  const int signal_fd_;
  const size_t max_events_per_step_;

  // Converts a Selector desire in some epoll flags.
  uint32_t DesiresToEpollEvents(uint32_t desires);
  // epoll file descriptor
  int epfd_;
  // here we get events that we poll
  std::unique_ptr<struct epoll_event[]> events_;
};
#endif  // HAVE_EPOLL


// A selector loop implementation based on poll - available on most systems,
// but with some limitations and of lower speed.
class PollSelectorLoop : public SelectorLoop {
public:
  static absl::StatusOr<std::unique_ptr<PollSelectorLoop>> Create(
      int signal_fd, size_t max_events_per_step);
  ~PollSelectorLoop();

  absl::Status Add(int fd, void* user_data, uint32_t desires) override;
  absl::Status Update(int fd, void* user_data, uint32_t desires) override;
  absl::Status Delete(int fd) override;

  absl::StatusOr<std::vector<SelectorEventData>> LoopStep(
      absl::Duration timeout) override;

  bool IsHangUpEvent(int event_value) const override;
  bool IsRemoteHangUpEvent(int event_value) const override;
  bool IsAnyHangUpEvent(int event_value) const override;
  bool IsErrorEvent(int event_value) const override;
  bool IsInputEvent(int event_value) const override;

private:
  PollSelectorLoop(int signal_fd, int max_events_per_step);
  absl::Status Initialize();

  const int signal_fd_;
  const size_t max_events_per_step_;

  // Converts a Selector desire in some poll flags.
  int DesiresToPollEvents(uint32_t desires);
  // Compacts the fds table at the end of a loop step
  void Compact();

  static const size_t kMaxFds = 4096;
  // epoll file descriptors
  struct pollfd fds_[kMaxFds];
  // how many in fds are used in fds_
  size_t fds_size_ = 0;
  // maps from fd to index in fds_ and user data
  typedef absl::flat_hash_map< int, std::pair<size_t, void*> > DataMap;
  DataMap fd_data_;
  // indices that we need to compact at the end of the step
  std::vector<size_t> indices_to_compact_;
};

}  // namespace net
}  // namespace whisper

#endif  // WHISPERLIB_NET_SELECTOR_LOOP_H_
