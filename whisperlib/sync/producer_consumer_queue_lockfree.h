#ifndef WHISPERLIB_SYNC_PRODUCER_CONSUMER_QUEUE_LOCKFREE_H_
#define WHISPERLIB_SYNC_PRODUCER_CONSUMER_QUEUE_LOCKFREE_H_

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

class LockFreeProducerConsumerQueue {
 public:
  LockFreeProducerConsumerQueue(
      size_t q_size, size_t num_producers, size_t num_consumers,
      sleep_wait_duration = absl::Microseconds(250))
    : num_producers_(num_producers),
      num_consumers_(num_consumers),
      q_size_(NextPow2(q_size)), q_mask_(q_size_ - 1),
      sleep_wait_duration_(sleep_wait_duration) {
    const size_t n = std::max(num_consumers_, num_producers_);
    clients_ = (ClientPos *)::memalign(getpagesize(), sizeof(ClientPos) * n);
    ::memset((void *)clients_, 0xFF, sizeof(ClientPos) * n);
    data_ = (C **)::memalign(getpagesize(), q_size_ * sizeof(C*));
    VLOG(2) << " Lock free created w/ size: " << q_size
            << " producers: " << num_producers
            << " consumers: " << num_consumers
            << " sleep_wait_duration: " << sleep_wait_duration_;
  }
  ~LockFreeProducerConsumerQueue() {
    ::free(data_);
    ::free(clients_);
  }

  void Put(std::unique_ptr<C> data, size_t producer_id) {
    CHECK_LT(producer_id, num_producers_);
    clients_[producer_id].head_ = head_;
    clients_[producer_id].head_ = __sync_fetch_and_add(&head_, 1);

    while (ABSL_PREDICT_FALSE(
               clients_[producer_id].head_ >= last_tail_ + q_size_)) {
      size_t pos_min = tail_;
      for (size_t i = 0; i < num_consumers_; ++i) {
        size_t tmp_tail = clients_[i].tail_;
        // Make sure that the compiler won't move order
        asm volatile("" ::: "memory");
        if (tmp_tail < pos_min) {
          pos_min = tmp_tail;
        }
      }
      last_tail_ = pos_min;
      if (clients_[producer_id].head_ < last_tail_ + q_size_) {
        break;
      }
      if (ABSL_PREDICT_TRUE(sleep_wait_duration_ > absl::ZeroDuration())) {
        absl::SleepFor(sleep_wait_duration_);
      } else {
        _mm_pause();
      }
    }
    data_[clients_[producer_id].head_ & q_mask_] = data.release();
    clients_[producer_id].head_ = ULONG_MAX;
  }

  std::unique_ptr<C> Get(size_t consumer_id) {
    CHECK_LT(consumer_id, num_consumers_);
    clients_[consumer_id].tail_ = tail_;
    clients_[consumer_id].tail_ = __sync_fetch_and_add(&tail_, 1);
    while (ABSL_PREDICT_FALSE(clients_[consumer_id].tail_ >= last_head_)) {
      size_t pos_min = head_;
      for (size_t i = 0; i < num_producers_; ++i) {
        size_t tmp_head = clients_[i].head_;
        // Make sure that the compiler won't move order
        asm volatile("" ::: "memory");
        if (tmp_head < pos_min) {
          pos_min = tmp_head;
        }
      }
      last_head_ = pos_min;
      if (clients_[consumer_id].tail_ < last_head_) {
        break;
      }
      if (ABSL_PREDICT_TRUE(sleep_wait_duration_ > absl::ZeroDuration())) {
        absl::SleepFor(sleep_wait_duration);
      } else {
        _mm_pause();
      }
    }
    unique_ptr<C> ret(data_[clients_[consumer_id].tail_ & q_mask_]);
    clients_[consumer_id].tail_ = ULONG_MAX;
    return ret;
  }

 private:
  size_t NextPow2(size_t sz) {
    size_t ret = 1;
    while (ret < sz) ret <<= 1;
    return ret;
  }

  const size_t num_producers_;
  const size_t num_consumers_;
  const size_t q_size_;
  const size_t q_mask_;
  const absl::Duration sleep_wait_duration_;

  // currently free position (next to insert)
  size_t head_ ____cacheline_aligned = 0;
  // current tail, next to pop
  size_t tail_ ____cacheline_aligned = 0;
  // last not-processed producer's pointer
  size_t last_head_ ____cacheline_aligned = 0;
  // last not-processed consumer's pointer
  size_t last_tail_ ____cacheline_aligned = 0;

  struct ClientPos {
    size_t head_;
    size_t tail_;
  };
  ClientPos* clients_;
  C** data_;
};

}  // namespace synch
}  // namespace whisper

#endif  // WHISPERLIB_SYNC_PRODUCER_CONSUMER_QUEUE_LOCKFREE_H_
