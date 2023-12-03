#include "whisperlib/net/timeouter.h"

#include "whisperlib/status/status.h"

namespace whisper {
namespace net {

Timeouter::Timeouter(Selector* selector, TimeoutCallback callback)
    : selector_(ABSL_DIE_IF_NULL(selector)),
      callback_(ABSL_DIE_IF_NULL(callback)) {}

Timeouter::~Timeouter() { ClearAllTimeouts(); }

void Timeouter::SetTimeout(TimeoutId timeout_id, absl::Duration timeout) {
  auto callback = [this, timeout_id]() { ProcessTimeout(timeout_id); };
  absl::MutexLock l(&mutex_);
  auto it = timeouts_.find(timeout_id);
  if (it != timeouts_.end()) {
    selector_->UnregisterAlarm(it->second);
    it->second = selector_->RegisterAlarm(std::move(callback), timeout);
  } else {
    timeouts_.emplace(timeout_id,
                      selector_->RegisterAlarm(std::move(callback), timeout));
  }
}

bool Timeouter::ClearTimeout(TimeoutId timeout_id) {
  absl::MutexLock l(&mutex_);
  auto it = timeouts_.find(timeout_id);
  if (it == timeouts_.end()) {
    return false;
  }
  selector_->UnregisterAlarm(it->second);
  return true;
}

void Timeouter::ClearAllTimeouts() {
  absl::MutexLock l(&mutex_);
  for (const auto& it : timeouts_) {
    selector_->UnregisterAlarm(it.second);
  }
  timeouts_.clear();
}

void Timeouter::ProcessTimeout(TimeoutId timeout_id) {
  size_t erased = false;
  {
    absl::MutexLock l(&mutex_);
    erased = timeouts_.erase(timeout_id);
  }
  if (ABSL_PREDICT_TRUE(erased)) {
    callback_(timeout_id);
  }
}

}  // namespace net
}  // namespace whisper
