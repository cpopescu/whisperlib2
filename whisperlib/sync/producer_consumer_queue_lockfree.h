#ifndef WHISPERLIB_SYNC_PRODUCER_CONSUMER_QUEUE_LOCKFREE_H_
#define WHISPERLIB_SYNC_PRODUCER_CONSUMER_QUEUE_LOCKFREE_H_

#include <algorithm>
#include <atomic>
#include <limits>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "absl/time/time.h"
#include "whisperlib/sync/moody/lightweightsemaphore.h"

/**
 * Lock free producer - consumer queue w/ max num_reader / writers for Linux.
 *
 * Adapted from:
 * http://www.linuxjournal.com/
 *   content/lock-free-multi-producer-multi-consumer-queue-ring-buffer
 *
 * This is a fast, lock free producer / consumer queue for Linux.
 */
namespace whisper {
namespace synch {

template <typename C>
class LockFreeProducerConsumerQueue {
 public:
  static_assert(sizeof(C) <= sizeof(uint64_t), "Type too large for this");
  LockFreeProducerConsumerQueue(
      size_t q_size, size_t num_producers, size_t num_consumers,
      // The performance is highly dependent on this:
      //   - can use ZeroDuration for a spinlock-like behaviour and super
      //     high operation/sec.
      absl::Duration wait_duration = absl::Microseconds(10))
      : num_producers_(num_producers),
        num_consumers_(num_consumers),
        num_clients_(std::max(num_consumers_, num_producers_)),
        q_size_(NextPow2(q_size)),
        q_mask_(q_size_ - 1),
        wait_duration_(wait_duration) {
    clients_.resize(num_clients_);
    const int error = posix_memalign(reinterpret_cast<void**>(&data_),
                                     getpagesize(), q_size_ * sizeof(C));
    CHECK(data_ != nullptr) << "Error: " << error;
  }
  ~LockFreeProducerConsumerQueue() { ::free(data_); }

  void Put(C data, size_t producer_id) {
    CHECK_LT(producer_id, num_producers_);
    //
    // << [[C-H]]  Release: clients_[producer_id].head_
    // To a value that puts me in the 'game'.
    clients_[producer_id].head_.store(head_.load(std::memory_order_acquire),
                                      std::memory_order_release);
    //
    // <<>> [[H]]  Acquire & Release: head_
    const size_t my_head = head_.fetch_add(1, std::memory_order_acq_rel);
    //
    // << [[C-H]]  Release: clients_[producer_id].head_
    clients_[producer_id].head_.store(my_head, std::memory_order_release);
    //
    // >> [[LT]]   Acquire: last_tail_
    size_t pos_min = last_tail_.load(std::memory_order_acquire);
    while (ABSL_PREDICT_FALSE(my_head >= pos_min + q_size_)) {
      //
      // >> [[LT]]   Acquire: last_tail_
      pos_min = tail_.load(std::memory_order_acquire);
      for (size_t i = 0; i < num_consumers_; ++i) {
        //
        // >> [[C-T]]    Acquire: clients_[i].tail_
        const size_t tmp_tail =
            clients_[i].tail_.load(std::memory_order_acquire);
        if (tmp_tail < pos_min) {
          pos_min = tmp_tail;
        }
      }
      //
      // << [[LT]]   Release: last_tail_
      last_tail_.store(pos_min, std::memory_order_release);
      if (my_head < pos_min + q_size_) {
        break;
      }
      if (ABSL_PREDICT_TRUE(wait_duration_ > absl::ZeroDuration())) {
        get_semaphore_.timed_wait(absl::ToInt64Microseconds(wait_duration_));
      } else {
        _mm_pause();
      }
    }
    data_[my_head & q_mask_] = std::move(data);
    // << [[C-H]]    Release: Clear reservation on clients_[producer_id].head_
    clients_[producer_id].head_.store(std::numeric_limits<size_t>::max(),
                                      std::memory_order_release);
    if (last_head_.load(std::memory_order_acquire) < my_head &&
        wait_duration_ > absl::ZeroDuration()) {
      put_semaphore_.signal();
    }
  }

  C Get(size_t consumer_id) {
    CHECK_LT(consumer_id, num_consumers_);
    //
    // << [[C-T]]    Release: clients_[consumer_id].tail_
    // Make sure no producer considers me out of the game - put a value.
    clients_[consumer_id].tail_.store(tail_.load(std::memory_order_acquire),
                                      std::memory_order_release);

    // <<>> [[T]]    Acquire && Release: tail_
    const size_t my_tail = tail_.fetch_add(1, std::memory_order_acquire);
    //
    // << [[C-T]]    Release: clients_[consumer_id].tail_
    // Update my lease on my_tail.
    clients_[consumer_id].tail_.store(my_tail, std::memory_order_release);
    //
    // >> [[LH]]     Acquire: last_head_
    size_t pos_min = last_head_.load(std::memory_order_acquire);
    while (ABSL_PREDICT_FALSE(my_tail >= pos_min)) {
      //
      // >> [H] Acquire: head_
      pos_min = head_.load(std::memory_order_acquire);
      for (size_t i = 0; i < num_producers_; ++i) {
        //
        // >> [[C-H]]    Acquire: clients_[i].head_
        const size_t tmp_head =
            clients_[i].head_.load(std::memory_order_acquire);
        if (tmp_head < pos_min) {
          pos_min = tmp_head;
        }
      }
      //
      // << [[LH]]   Release: last_head_
      last_head_.store(pos_min, std::memory_order_release);
      if (my_tail < pos_min) {
        break;
      }
      if (ABSL_PREDICT_TRUE(wait_duration_ > absl::ZeroDuration())) {
        put_semaphore_.timed_wait(absl::ToInt64Microseconds(wait_duration_));
      } else {
        _mm_pause();
      }
    }
    //
    // >> [[C-T]]  Consume: clients_[consumer_id].tail_
    C ret = std::move(data_[my_tail & q_mask_]);
    //
    // << [[C-T]]  Release: clients_[consumer_id].tail_
    clients_[consumer_id].tail_.store(std::numeric_limits<size_t>::max(),
                                      std::memory_order_release);
    if (last_tail_.load(std::memory_order_acquire) < my_tail &&
        wait_duration_ > absl::ZeroDuration()) {
      get_semaphore_.signal();
    }
    return ret;
  }
  // Approximately:
  size_t Size() const { return head_.load() - tail_.load(); }
  std::string ToString() const {
    std::string s = absl::StrCat(
        "LockFreeProducerConsumerQueye{ q_size: ", q_size_,
        " num_producer: ", num_producers_, " num_consumers: ", num_consumers_,
        " head: ", head_.load(), " tail: ", tail_.load(),
        " last_tail: ", last_tail_.load(), " last_head: ", last_head_.load(),
        " }");
    for (size_t i = 0; i < num_consumers_; ++i) {
      absl::StrAppend(&s, "\n  C: #", i, " tail@ ", clients_[i].tail_.load());
    }
    for (size_t i = 0; i < num_producers_; ++i) {
      absl::StrAppend(&s, "\n  P: #", i, " head@ ", clients_[i].head_.load());
    }
    return s;
  }

 private:
  LockFreeProducerConsumerQueue(const LockFreeProducerConsumerQueue&) = delete;
  LockFreeProducerConsumerQueue& operator=(
      const LockFreeProducerConsumerQueue&) = delete;
  static size_t NextPow2(size_t sz) {
    size_t ret = 1;
    while (ret < sz) ret <<= 1;
    return ret;
  }
  static inline bool HasBeenNotified(const std::atomic<bool>* notified_yet) {
    return notified_yet->load(std::memory_order_acquire);
  }

  const size_t num_producers_;
  const size_t num_consumers_;
  const size_t num_clients_;
  const size_t q_size_;
  const size_t q_mask_;
  const absl::Duration wait_duration_;

  C* data_ ABSL_CACHELINE_ALIGNED = nullptr;

  // currently free position (next to insert)
  std::atomic<size_t> head_ ABSL_CACHELINE_ALIGNED{0};
  // current tail, next to pop
  std::atomic<size_t> tail_ ABSL_CACHELINE_ALIGNED{0};
  // last not-processed producer's pointer
  std::atomic<size_t> last_head_ ABSL_CACHELINE_ALIGNED{0};
  // last not-processed consumer's pointer
  std::atomic<size_t> last_tail_ ABSL_CACHELINE_ALIGNED{0};

  struct ClientPos {
    std::atomic<size_t> head_ ABSL_CACHELINE_ALIGNED{
        std::numeric_limits<size_t>::max()};
    std::atomic<size_t> tail_ ABSL_CACHELINE_ALIGNED{
        std::numeric_limits<size_t>::max()};
    // We never copy these, just make the vector creation happy
    ClientPos() = default;
    ClientPos(const ClientPos& cp) {
      head_.store(cp.head_.load());
      tail_.store(cp.tail_.load());
    }
    ClientPos& operator=(const ClientPos& cp) {
      head_.store(cp.head_.load());
      tail_.store(cp.tail_.load());
      return *this;
    }
  };
  std::vector<ClientPos> clients_;
  moodycamel::details::Semaphore get_semaphore_;
  moodycamel::details::Semaphore put_semaphore_;
};

}  // namespace synch
}  // namespace whisper

#endif  // WHISPERLIB_SYNC_PRODUCER_CONSUMER_QUEUE_LOCKFREE_H_
