#ifndef WHISPER_NET_TIMEOUTER_H_
#define WHISPER_NET_TIMEOUTER_H_

#include <cstdint>

#include "absl/base/thread_annotations.h"
#include "absl/container/flat_hash_map.h"
#include "absl/synchronization/mutex.h"
#include "absl/time/time.h"
#include "whisperlib/net/selector.h"

namespace whisper {
namespace net {

class Timeouter {
 public:
  using TimeoutId = int64_t;
  using TimeoutCallback = std::function<void(TimeoutId)>;

  // Creates a timeout register with provided selector, which is
  // just a reference pointer. The provided callback will get called
  // upon every raised timeout.
  Timeouter(Selector* selector, TimeoutCallback callback);
  ~Timeouter();

  // Registers (or re-registers) a timeout call in timeout_duration ms
  // from this moment, with the provided timeout_id.
  void SetTimeout(TimeoutId timeout_id, absl::Duration timeout);
  // Clears a previously set timeout call, with the provided id.
  // Returns true if indeed a timeout was cleared for provided timeout_id.
  bool ClearTimeout(TimeoutId timeout_id);
  // Clears all timeouts.
  void ClearAllTimeouts();

 private:
  void ProcessTimeout(TimeoutId timeout_id);

  Selector* const selector_;
  const TimeoutCallback callback_;

  // Protects timeouts.
  absl::Mutex mutex_;
  using TimeoutMap = absl::flat_hash_map<TimeoutId, Selector::AlarmId>;
  // Maps from timeout Id to registered alarm ids.
  TimeoutMap timeouts_ ABSL_GUARDED_BY(mutex_);
  ;
};

}  // namespace net
}  // namespace whisper

#endif  // WHISPER_NET_TIMEOUTER_H_
