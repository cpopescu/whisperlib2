#ifndef WHISPERLIB_SYNC_PRODUCER_CONSUMER_QUEUE_H_
#define WHISPERLIB_SYNC_PRODUCER_CONSUMER_QUEUE_H_

#include <deque>
#include <vector>
#include <utility>

#include "absl/base/thread_annotations.h"
#include "absl/synchronization/mutex.h"
#include "absl/time/time.h"
#include "whisperlib/base/call_on_return.h"
#include "whisperlib/status/status.h"

namespace whisper {
namespace synch {

template<typename C>
class ProducerConsumerQueue {
 public:
  // Creates a ProducerConsumerQueue with a maximum size.
  // Normally we have a FIFO policy, but if fifo_policy is false, we
  // employ a lifo policy.
  explicit ProducerConsumerQueue(size_t max_size, bool fifo_policy = true)
    : max_size_(max_size), fifo_policy_(fifo_policy) {}

  // Places a datum into the queue. If timeout is not infinite, we bail out when
  // the queue has no empty space after that duration, and return false.
  bool Put(C c, absl::Duration timeout = absl::InfiniteDuration())
    ABSL_LOCKS_EXCLUDED(mutex_) {
    return PutAt(std::move(c), timeout, !fifo_policy_);
  }
  // Places a datum into the queue, possibly at the front of the queue.
  // If timeout is not infinite, we bail out when the queue has no empty
  // space after that duration, and return false.
  bool PutAt(C c, absl::Duration timeout = absl::InfiniteDuration(),
             bool at_front = false)
    ABSL_LOCKS_EXCLUDED(mutex_) {
    mutex_.Lock();
    base::CallOnReturn unlock([this]() { mutex_.Unlock(); });
    if (!HasEmptySpace()) {
      mutex_.AwaitWithTimeout(absl::Condition(
          this, &ProducerConsumerQueue<C>::HasEmptySpace), timeout);
      if (!HasEmptySpace()) {
        return false;
      }
    }
    if (at_front) {
      data_.emplace_front(std::move(c));
    } else {
      data_.emplace_back(std::move(c));
    }
    return true;
  }
  // Returns the first available element at the front of the queue.
  ABSL_MUST_USE_RESULT C Get() ABSL_LOCKS_EXCLUDED(mutex_) {
    mutex_.Lock();
    base::CallOnReturn unlock([this]() { mutex_.Unlock(); });
    if (!HasData()) {
      mutex_.Await(absl::Condition(
          this, &ProducerConsumerQueue<C>::HasData));
    }
    C result = std::move(data_.front());
    data_.pop_front();
    return result;
  }
  // Returns the first available element at the front of the queue in the
  // provided pointer, waiting at most timeout duration for one to be available.
  // If none is available, we return false.
  ABSL_MUST_USE_RESULT bool TryGet(
      C* c, absl::Duration timeout = absl::ZeroDuration())
    ABSL_LOCKS_EXCLUDED(mutex_) {
    mutex_.Lock();
    base::CallOnReturn unlock([this]() { mutex_.Unlock(); });
    if (!HasData()) {
      mutex_.AwaitWithTimeout(absl::Condition(
          this, &ProducerConsumerQueue<C>::HasData), timeout);
      if (!HasData()) {
        return false;
      }
    }
    *c = std::move(data_.front());
    data_.pop_front();
    return true;
  }
  // Empties the queue, returning everything that is stored in it.
  ABSL_MUST_USE_RESULT std::vector<C> GetAll() ABSL_LOCKS_EXCLUDED(mutex_) {
    std::vector<C> result;
    absl::MutexLock l(&mutex_);
    result.reserve(data_.size());
    std::move(data_.begin(), data_.end(), std::back_inserter(result));
    data_.clear();
    return result;
  }
  // Empties the queue.
  void Clear() ABSL_LOCKS_EXCLUDED(mutex_) {
    absl::MutexLock l(&mutex_);
    data_.clear();
  }
  // Returns true if the queue has reached its maximum size.
  bool IsFull() const ABSL_LOCKS_EXCLUDED(mutex_) {
    if (!max_size_) return false;
    absl::MutexLock l(&mutex_);
    return data_.size() >= max_size_;
  }
  // Returns the current number of elements in the queue.
  size_t Size() const ABSL_LOCKS_EXCLUDED(mutex_) {
    absl::MutexLock l(&mutex_);
    return data_.size();
  }

 protected:
  bool HasEmptySpace() const ABSL_EXCLUSIVE_LOCKS_REQUIRED(mutex_) {
    return max_size_ == 0 || data_.size() < max_size_;
  }
  bool HasData() const ABSL_EXCLUSIVE_LOCKS_REQUIRED(mutex_) {
    return !data_.empty();
  }
  const size_t max_size_;
  const bool fifo_policy_;
  mutable absl::Mutex mutex_;
  std::deque<C> data_ GUARDED_BY(mutex_);
};

}  // namespace synch
}  // namespace whisper

#endif  // WHISPERLIB_SYNC_PRODUCER_CONSUMER_QUEUE_H_
