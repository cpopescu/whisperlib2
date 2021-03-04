#ifndef WHISPERLIB_NET_SELECTOR_H_
#define WHISPERLIB_NET_SELECTOR_H_

#include <atomic>
#include <deque>
#include <functional>
#include <thread>
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

// A Selector is an object that performs asynchronous I/O operations on a
// set of file descriptors, abstracted as `Selectable` objects.
// Its execution happens mostly as a closed loop in a single thread.
// All I/O operations happen on that particular thread, as indicated by the
// desires of the Selectable objects.
// It can also schedule functions to be run at certain times (alarms) or
// general functions to be run on the selector thread.
//
// Note: most of the functions that deal with registration and changes of
// selectable objects need to be executed from the selector loop.
// Functions that register alarms, or functions to be run in the selector
// loop, can be executed from any thread, so adding a selectable from
// a different thread can be done as:
//   selector->RunInSelectLoop([selector, selectable]() {
//       HandleError(selector->Register(selectable), selectable);;
//   });
//
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

  // Sets the function to be called upon exiting the loop - set before starting
  // the loop.
  void set_call_on_close(std::function<void()> call_on_close);

  // Runs the main select loop - blocks the thread until the loop ends.
  absl::Status Loop();

  // Returns true if this call was made from the select server thread.
  bool IsInSelectThread() const;

  // Schedules the exit from the select loop.
  void MakeLoopExit();

  // Register an I/O object for read/write/error event callbacks.
  // By default all callbacks are enabled.
  // NOTE: Call only for from the Selector loop thread.
  absl::Status Register(Selectable* s);

  // Unregister a previously registered I/O object.
  // NOTE: Call only from the Selector loop thread.
  absl::Status Unregister(Selectable* s);

  // Enable/disable a certain event callback for the given selectable
  // NOTE: Call this only from the select loop.
  absl::Status EnableWriteCallback(Selectable* s, bool enable);
  absl::Status EnableReadCallback(Selectable* s, bool enable);

  // Cleans and closes the entire list of selectable objects.
  // Note: Call only from the Selector loop thread.
  // Error status is returned only if not called from the selector thread.
  absl::Status CleanAndCloseAll();

  // Returns true if the selector is no longer in the loop.
  // Note: in this state the registered callbacks can still execute.
  bool IsExiting() const;

  // Runs this function in the select loop.
  // NOTE: safe to call from any thread.
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

  using AlarmId = uint64_t;
  // Runs the provided function after a timeout in the select loop.
  // Returns an alarm_id that can be used to unregister the alarm.
  // NOTE: safe to call from any thread.
  AlarmId RegisterAlarm(std::function<void()> callback,
                        absl::Duration timeout);
  // Unregistered a previously registered alarm.
  // NOTE: safe to call from any thread.
  void UnregisterAlarm(AlarmId alarm_id);

  // Parameters of this selector.
  Params params() const;

  // The last time we were in the select loop not executing anything.
  absl::Time now() const;

  // The current moment when the select loop was broken:
  absl::Time loop_now() const;

  // Identifies various signals in the provided event value, based
  // on the underlying loop_ implementation.
  bool IsHangUpEvent(int event_value) const;
  bool IsRemoteHangUpEvent(int event_value) const;
  bool IsAnyHangUpEvent(int event_value) const;
  bool IsErrorEvent(int event_value) const;
  bool IsInputEvent(int event_value) const;

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
  absl::flat_hash_map<AlarmId, std::function<void()>> alarms_
  ABSL_GUARDED_BY(alarm_mutex_);
  // Heap of alarm times and alarm ids.
  std::vector<std::pair<absl::Time, AlarmId>> alarm_timeouts_
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

class SelectorThread {
public:
  // Creates a *stopped* selector thread.
  static absl::StatusOr<std::unique_ptr<SelectorThread>>
  Create(Selector::Params params = {});

  // Starts the selector in the side thread.
  // Returns true if started now, false if it is already started.
  bool Start();
  // Ends the selector thread via a MakeLoopExit, then waiting for thread
  // to end.
  // Returns true if stopped now, false if it is already stopped.
  bool Stop();

  // Close all file handles in the selector, effectively preparing for a clean
  // exit of the selector thread.
  void CleanAndCloseAll();

  // Returns the underlying selector.
  const Selector* selector() const;
  Selector* selector();
  // Returns if the selector thread is started and running.
  bool is_started() const;
  // Returns the last status of the underlying selector loop.
  absl::Status selector_status() const;

  ~SelectorThread();

 private:
  SelectorThread();
  absl::Status Initialize(Selector::Params params);
  void Run();

  // Underlying selector, created from provided creation params.
  std::unique_ptr<Selector> selector_;
  // Locks access to internal members.
  mutable absl::Mutex mutex_;
  // Running thread for the selector loop.
  std::unique_ptr<std::thread> thread_ ABSL_GUARDED_BY(mutex_);
  // Status of the last selector loop.
  absl::Status selector_status_ ABSL_GUARDED_BY(mutex_);
  // If the selector loop is currently running.
  std::atomic_bool is_started_ = ATOMIC_VAR_INIT(false);
};

}  // namespace net
}  // namespace whisper

#endif  // WHISPERLIB_NET_SELECTOR_H_
