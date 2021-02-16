#ifndef WHISPERLIB_NET_SELECTOR_H_
#define WHISPERLIB_NET_SELECTOR_H_

#include <atomic>
#include <deque>
#include <functional>
#include <vector>

#include "absl/base/thread_annotations.h"
#include "absl/container/flat_hash_set.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/synchronization/mutex.h"
#include "absl/time/time.h"
#include "whisperlib/net/selectable.h"
#include "whisperlib/net/selector_loop.h"
#include "whisperlib/net/selector_event_data.h"


namespace whisper {
namespace net {

class Selector {
public:
  struct Params {
    // Maximum number of epoll I/O events to accept per loop step.
    size_t max_events_per_step = 128;
    // Maximum number of epoll I/O events to accept per loop step.
    size_t max_num_callbacks_per_event = 64;
    // Maximum number of registered callbacks to run per each loop step.
    absl::Duration callbacks_timeout_per_event = absl::Seconds(1);
    // Default timeout to break a epoll / poll wait in case of no event.
    absl::Duration default_loop_timeout = absl::Seconds(1);
    // Use event_fd linux API for signaling breaks in the select loop.
    bool use_event_fd = true;
    // Use epoll for the select loop (as opposed to poll).
    bool use_epoll = true;

    Params& set_use_event_fd(bool value) {
      use_event_fd = value;
      return *this;
    }
    Params& set_use_epoll(bool value) {
      use_epoll = value;
      return *this;
    }
    Params& set_max_events_per_step(size_t value) {
      max_events_per_step = value;
      return *this;
    }
    Params& set_max_num_callbacks_per_event(size_t value) {
      max_num_callbacks_per_event = value;
      return *this;
    }
    Params& set_callbacks_timeout_per_event(absl::Duration value) {
      callbacks_timeout_per_event = value;
      return *this;
    }
    Params& set_default_loop_timeout(absl::Duration value) {
      default_loop_timeout = value;
      return *this;
    }
  };
  // Creation method - use to create a selector object.
  static absl::StatusOr<std::unique_ptr<Selector>> Create(Params params);

  // Register an I/O object for read/write/error event callbacks.
  // By default all callbacks are enabled.
  absl::Status Register(Selectable* s);

  // Unregister a previously registered I/O object.
  absl::Status Unregister(Selectable* s);

  // Enable/disable a certain event callback for the given selectable
  // NOTE: Call this only from the select loop.
  absl::Status EnableWriteCallback(Selectable* s, bool enable);
  absl::Status EnableReadCallback(Selectable* s, bool enable);

  // Sets the function to be called upon exiting the loop.
  void set_call_on_close(std::function<void()> call_on_close);

  // Runs the main select loop - blocks the thread until the loop ends.
  absl::Status Loop();
  // Schedules the exit from the select loop.
  void MakeLoopExit();

  // Returns true if the selector is no longer in the loop.
  // Note: in this state the registered callbacks can still execute.
  bool IsExiting() const;

  // Returns true if this call was made from the select server thread.
  bool IsInSelectThread() const;

  // Runs this function in the select loop.
  // NOTE: safe to call from another thread.
  void RunInSelectLoop(std::function<void()> callback);
  // Schedules the deletion of the provided object in the select loop.
  template <typename T> void DeleteInSelectLoop(T* t) {
    RunInSelectLoop([t]() { delete t; });
  }
  template <typename T> void DeleteInSelectLoop(std::unique_ptr<T> t) {
    // TODO(cp): use a helper object to be able to move this in c++11.
    T* pt = t.release();
    RunInSelectLoop([t]() { delete t; });
  }

  // Runs the provided function after a timeout in the select loop.
  // Returns an alarm_id that can be used to unregister the alarm.
  // NOTE: unsafe to call from another thread - call only from the select loop.
  uint64_t RegisterAlarm(std::function<void()> callback,
                         absl::Duration timeout);
  // Unregistered a previously registered alarm.
  // NOTE: unsafe to call from another thread - call only from the select loop.
  void UnregisterAlarm(uint64_t alarm_id);

  // Parameters of this selector.
  Params params() const;

  // The last time we were in the select loop not executing anything.
  absl::Time now() const;

  // The current moment when the select loop was broken:
  absl::Time loop_now() const;

  // Cleans and closes the entire list of selectable objects
  void CleanAndCloseAll();

  ~Selector();

private:
  Selector(Params params);

  // Initializes the selector object.
  absl::Status Initialize();
  // Helper that turns on/off fd desires in the provided selectable.
  absl::Status UpdateDesire(Selectable* s, bool enable, uint32_t desire);
  // This runs functions from to_run_ (if any).
  size_t RunCallbacks(size_t max_num_to_run);
  // Writes a byte in the internal signal_fd_ to make the loop wake up.
  void SendWakeSignal();
  // Runs int the main loop the callbacks at this step.
  size_t LoopCallbacks();
  // Runs int the main loop the alarms at this step.
  size_t LoopAlarms();
  // Updates the now_ to current time.
  void UpdateNow();

  // Parameters for this selector
  Params params_;

  // The thread id of the selector loop.
  std::atomic<uint64_t> tid_ = ATOMIC_VAR_INIT(0);
  // Flags that marks the exit from the loop.
  std::atomic_bool should_end_ = ATOMIC_VAR_INIT(false);

  // Event file descriptor to wake the selector from the loop
  // - when using event_fd.
  int event_fd_ = -1;
  // When not using event fd, we use a pipe, signaling on signal_pipe_[0]
  // - when using pipes.
  int signal_pipe_[2] = {-1, -1};
  // The signal file descriptor registered in poll / epoll.
  // Can be either event_fd_ or signal_pipe_[0]
  int signal_fd_ = -1;

  // Our select loop - does poll / epoll etc on file descriptors.
  std::unique_ptr<SelectorLoop> loop_;

  // Selectables registered with us - modified only from the select loop thread.
  absl::flat_hash_set<Selectable*> registered_;

  // Guards the to_run_ deque.
  absl::Mutex to_run_mutex_;
  // Registered callbacks to run in the select loop.
  std::deque<std::function<void()>> to_run_ ABSL_GUARDED_BY(to_run_mutex_);
  // If we have callbacks to run.
  std::atomic_bool have_to_run_ = ATOMIC_VAR_INIT(false);

  // Guards the alarm structures.
  absl::Mutex alarm_mutex_;
  // Id of the next added alarm.
  std::atomic_uint64_t alarm_id_ = ATOMIC_VAR_INIT(0);
  // Maps from alarm id to alarm callback.
  absl::flat_hash_map<uint64_t, std::function<void()>> alarms_
  ABSL_GUARDED_BY(alarm_mutex_);
  // Heap of alarm times and alarm ids.
  std::vector<std::pair<absl::Time, uint64_t>> alarm_timeouts_
  ABSL_GUARDED_BY(alarm_mutex_);
  // The next alarm time - top of the alarm_timeouts_ heap.
  // This is the nanos of the time - cannot use atomic<absl::Time> on all
  // architectures.
  std::atomic<int64_t> next_alarm_time_ =
    ATOMIC_VAR_INIT(absl::ToUnixNanos(absl::InfiniteFuture()));
  // Number of registered alarms - for quick checking.
  std::atomic_size_t num_registered_alarms_ = ATOMIC_VAR_INIT(0);
  // We call this function upon exiting loop.
  std::function<void()> call_on_close_ = nullptr;
  // The last time we broke the loop.
  std::atomic<int64_t> now_ =
    ATOMIC_VAR_INIT(absl::ToUnixNanos(absl::InfinitePast()));
};

}  // namespace net
}  // namespace whisper

#endif  // WHISPERLIB_NET_SELECTOR_H_
