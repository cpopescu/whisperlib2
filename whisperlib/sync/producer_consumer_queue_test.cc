#include "whisperlib/sync/producer_consumer_queue.h"

#include "absl/functional/bind_front.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "whisperlib/sync/moody/blockingconcurrentqueue.h"
#include "whisperlib/sync/producer_consumer_queue_lockfree.h"
#include "whisperlib/sync/thread.h"
#include "whisperlib/status/testing.h"

namespace whisper {
namespace synch {

TEST(ProducerConsumerQueue, General) {
  ProducerConsumerQueue<int> q(100);
  for (int i = 0; i < 100; ++i) {
    ASSERT_FALSE(q.IsFull());
    EXPECT_FALSE(q.Put(i).has_value());
  }
  EXPECT_EQ(q.Size(), 100);
  EXPECT_TRUE(q.IsFull());
  EXPECT_TRUE(q.Put(101, absl::ZeroDuration()).has_value());
  for (int i = 0; i < 100; ++i) {
    EXPECT_EQ(q.Get(), i);
  }
  EXPECT_EQ(q.Size(), 0);
  int k;
  EXPECT_FALSE(q.TryGet(&k, absl::ZeroDuration()));
}

TEST(ProducerConsumerQueue, GeneralLifo) {
  ProducerConsumerQueue<int> q(100, false);
  for (int i = 0; i < 100; ++i) {
    ASSERT_FALSE(q.IsFull());
    EXPECT_FALSE(q.Put(i).has_value());
  }
  EXPECT_EQ(q.Size(), 100);
  EXPECT_TRUE(q.IsFull());
  EXPECT_TRUE(q.Put(101, absl::ZeroDuration()).has_value());
  for (int i = 100; i > 0; --i) {
    EXPECT_EQ(q.Get() + 1, i);
  }
  EXPECT_EQ(q.Size(), 0);
  int k;
  EXPECT_FALSE(q.TryGet(&k, absl::ZeroDuration()));
}

TEST(ProducerConsumerQueue, GetAll) {
  ProducerConsumerQueue<int> q(100);
  for (int i = 0; i < 100; ++i) {
    ASSERT_FALSE(q.IsFull());
    EXPECT_FALSE(q.Put(i).has_value());
  }
  auto res = q.GetAll();
  EXPECT_EQ(q.Size(), 0);
  EXPECT_EQ(res.size(), 100);
  for (int i = 0; i < 100; ++i) {
    EXPECT_EQ(res[i], i);
  }
}

TEST(ProducerConsumerQueue, Clear) {
  ProducerConsumerQueue<int> q(100);
  for (int i = 0; i < 100; ++i) {
    ASSERT_FALSE(q.IsFull());
    EXPECT_FALSE(q.Put(i).has_value());
  }
  EXPECT_EQ(q.Size(), 100);
  q.Clear();
  EXPECT_EQ(q.Size(), 0);
}

void Produce(ProducerConsumerQueue<int>* q, int num) {
  for (int i = 0; i < num; ++i) {
    q->Put(i);
  }
}
void ConsumeSymmetric(ProducerConsumerQueue<int>* q, int num) {
  for (int i = 0; i < num; ++i) {
    const int result = q->Get();
    ASSERT_EQ(result, i);
  }
}

void Consume(ProducerConsumerQueue<int>* q, int num) {
  int64_t sum = 0;
  for (int i = 0; i < num; ++i) {
    sum += q->Get();
  }
  std::cout << "Consumer sum: " << sum << std::endl;
}

TEST(ProducerConsumerQueue, Multithread) {
  ProducerConsumerQueue<int> q(100);
  static constexpr const int kNumItems = 1000000;
  ASSERT_OK_AND_ASSIGN(auto produce, work::Thread::Create(absl::bind_front(
                                         &Produce, &q, kNumItems)));
  ASSERT_OK_AND_ASSIGN(auto consume, work::Thread::Create(absl::bind_front(
                                         &ConsumeSymmetric, &q, kNumItems)));
  ASSERT_OK(consume->Join());
  ASSERT_OK(produce->Join());
  EXPECT_EQ(q.Size(), 0);
}

TEST(ProducerConsumerQueue, MultithreadMultiProducers) {
  ProducerConsumerQueue<int> q(100);
  static constexpr const int kNumItems = 1000000;
  static constexpr const int kNumProducers = 10;
  static_assert(kNumItems % kNumProducers == 0, "Bad items / producer ratio");
  std::vector<std::unique_ptr<work::Thread>> threads;
  static constexpr const int kNumPerProducer = kNumItems / kNumProducers;
  for (size_t i = 0; i < kNumProducers; ++i) {
    ASSERT_OK_AND_ASSIGN(auto produce, work::Thread::Create(absl::bind_front(
                                           &Produce, &q, kNumPerProducer)));
    threads.emplace_back(std::move(produce));
  }
  ASSERT_OK_AND_ASSIGN(auto consume, work::Thread::Create(absl::bind_front(
                                         &Consume, &q, kNumItems)));
  threads.emplace_back(std::move(consume));
  for (auto it = threads.rbegin(); it != threads.rend(); ++it) {
    ASSERT_OK((*it)->Join());
  }
  EXPECT_EQ(q.Size(), 0);
}

TEST(ProducerConsumerQueue, MultithreadMultiConsumers) {
  ProducerConsumerQueue<int> q(100);
  static constexpr const int kNumItems = 1000000;
  static constexpr const int kNumConsumers = 10;
  static_assert(kNumItems % kNumConsumers == 0, "Bad items / consumer ratio");
  std::vector<std::unique_ptr<work::Thread>> threads;
  static constexpr const int kNumPerConsumer = kNumItems / kNumConsumers;
  ASSERT_OK_AND_ASSIGN(auto produce, work::Thread::Create(absl::bind_front(
                                         &Produce, &q, kNumItems)));
  threads.emplace_back(std::move(produce));
  for (size_t i = 0; i < kNumConsumers; ++i) {
    ASSERT_OK_AND_ASSIGN(auto consume, work::Thread::Create(absl::bind_front(
                                           &Consume, &q, kNumPerConsumer)));
    threads.emplace_back(std::move(consume));
  }
  for (auto it = threads.rbegin(); it != threads.rend(); ++it) {
    ASSERT_OK((*it)->Join());
  }
  EXPECT_EQ(q.Size(), 0);
}

void LockfreeProduce(LockFreeProducerConsumerQueue<int>* q, size_t id,
                     int num) {
  for (int i = 0; i < num; ++i) {
    q->Put(i, id);
  }
}

void LockfreeConsumeSymmetric(LockFreeProducerConsumerQueue<int>* q, size_t id,
                              int num) {
  for (int i = 0; i < num; ++i) {
    const int result = q->Get(id);
    if (ABSL_PREDICT_FALSE(i != result)) {
      std::cerr << q->ToString() << std::endl;
      ASSERT_EQ(result, i);
    }
  }
}

void LockfreeConsume(LockFreeProducerConsumerQueue<int>* q, size_t id,
                     int num) {
  int64_t sum = 0;
  for (int i = 0; i < num; ++i) {
    sum += q->Get(id);
  }
  std::cout << "Consumer: " << id << " sum: " << sum << std::endl;
}

static const absl::Duration kLockfreeWait = absl::ZeroDuration();
// static const absl::Duration kLockfreeWait = absl::Microseconds(50);

TEST(LockFreeProducerConsumerQueue, LockFreeMultithread) {
  LockFreeProducerConsumerQueue<int> q(100, 1, 1, kLockfreeWait);
  static constexpr const int kNumItems = 1000000;
  ASSERT_OK_AND_ASSIGN(auto produce, work::Thread::Create(absl::bind_front(
                                         &LockfreeProduce, &q, 0, kNumItems)));
  ASSERT_OK_AND_ASSIGN(auto consume,
                       work::Thread::Create(absl::bind_front(
                           &LockfreeConsumeSymmetric, &q, 0, kNumItems)));
  ASSERT_OK(consume->Join());
  ASSERT_OK(produce->Join());
  EXPECT_EQ(q.Size(), 0);
}

TEST(LockFreeProducerConsumerQueue, LockfreeMultithreadMultiProducers) {
  static constexpr const int kNumItems = 1000000;
  static constexpr const int kNumProducers = 10;
  static_assert(kNumItems % kNumProducers == 0, "Bad items / producer ratio");
  LockFreeProducerConsumerQueue<int> q(100, kNumProducers, 1, kLockfreeWait);
  std::vector<std::unique_ptr<work::Thread>> threads;
  static constexpr const int kNumPerProducer = kNumItems / kNumProducers;
  for (size_t i = 0; i < kNumProducers; ++i) {
    ASSERT_OK_AND_ASSIGN(auto produce,
                         work::Thread::Create(absl::bind_front(
                             &LockfreeProduce, &q, i, kNumPerProducer)));
    threads.emplace_back(std::move(produce));
  }
  ASSERT_OK_AND_ASSIGN(auto consume, work::Thread::Create(absl::bind_front(
                                         &LockfreeConsume, &q, 0, kNumItems)));
  threads.emplace_back(std::move(consume));
  for (auto it = threads.rbegin(); it != threads.rend(); ++it) {
    ASSERT_OK((*it)->Join());
  }
  EXPECT_EQ(q.Size(), 0);
}

TEST(LockFreeProducerConsumerQueue, LockfreeMultithreadMultiConsumers) {
  static constexpr const int kNumItems = 1000000;
  static constexpr const int kNumConsumers = 10;
  static_assert(kNumItems % kNumConsumers == 0, "Bad items / consumer ratio");
  LockFreeProducerConsumerQueue<int> q(100, 1, kNumConsumers, kLockfreeWait);
  std::vector<std::unique_ptr<work::Thread>> threads;
  static constexpr const int kNumPerConsumer = kNumItems / kNumConsumers;
  ASSERT_OK_AND_ASSIGN(auto produce, work::Thread::Create(absl::bind_front(
                                         &LockfreeProduce, &q, 0, kNumItems)));
  threads.emplace_back(std::move(produce));
  for (size_t i = 0; i < kNumConsumers; ++i) {
    ASSERT_OK_AND_ASSIGN(auto consume,
                         work::Thread::Create(absl::bind_front(
                             &LockfreeConsume, &q, i, kNumPerConsumer)));
    threads.emplace_back(std::move(consume));
  }
  for (auto it = threads.rbegin(); it != threads.rend(); ++it) {
    ASSERT_OK((*it)->Join());
  }
  EXPECT_EQ(q.Size(), 0);
}

// Multi producers - consumers - a bit of a benchmark or so..
static constexpr const int kManyItems = 5000000;

TEST(ProducerConsumerQueue, MultithreadMultiProducersConsumers) {
  ProducerConsumerQueue<int> q(100);
  static constexpr const int kNumItems = kManyItems / 10;  // this is very slow
  static constexpr const int kNumProducers = 8;
  static constexpr const int kNumConsumers = 8;
  static_assert(kNumItems % kNumConsumers == 0, "Bad items / consumer ratio");
  static_assert(kNumItems % kNumProducers == 0, "Bad items / producer ratio");
  std::vector<std::unique_ptr<work::Thread>> threads;
  static constexpr const int kNumPerProducer = kNumItems / kNumProducers;
  for (size_t i = 0; i < kNumProducers; ++i) {
    ASSERT_OK_AND_ASSIGN(auto produce, work::Thread::Create(absl::bind_front(
                                           &Produce, &q, kNumPerProducer)));
    threads.emplace_back(std::move(produce));
  }
  static constexpr const int kNumPerConsumer = kNumItems / kNumConsumers;
  for (size_t i = 0; i < kNumConsumers; ++i) {
    ASSERT_OK_AND_ASSIGN(auto consume, work::Thread::Create(absl::bind_front(
                                           &Consume, &q, kNumPerConsumer)));
    threads.emplace_back(std::move(consume));
  }
  for (auto it = threads.rbegin(); it != threads.rend(); ++it) {
    ASSERT_OK((*it)->Join());
  }
  EXPECT_EQ(q.Size(), 0);
}

void RunLockfreeMultiProducersConsumers(absl::Duration wait_duration) {
  static constexpr const int kNumItems = kManyItems;
  static constexpr const int kNumProducers = 8;
  static constexpr const int kNumConsumers = 8;
  static_assert(kNumItems % kNumConsumers == 0, "Bad items / consumer ratio");
  static_assert(kNumItems % kNumProducers == 0, "Bad items / producer ratio");
  LockFreeProducerConsumerQueue<int> q(100, kNumProducers, kNumConsumers,
                                       wait_duration);
  static constexpr const int kNumPerProducer = kNumItems / kNumProducers;
  std::vector<std::unique_ptr<work::Thread>> threads;
  for (size_t i = 0; i < kNumProducers; ++i) {
    ASSERT_OK_AND_ASSIGN(auto produce,
                         work::Thread::Create(absl::bind_front(
                             &LockfreeProduce, &q, i, kNumPerProducer)));
    threads.emplace_back(std::move(produce));
  }
  static constexpr const int kNumPerConsumer = kNumItems / kNumConsumers;
  for (size_t i = 0; i < kNumConsumers; ++i) {
    ASSERT_OK_AND_ASSIGN(auto consume,
                         work::Thread::Create(absl::bind_front(
                             &LockfreeConsume, &q, i, kNumPerConsumer)));
    threads.emplace_back(std::move(consume));
  }
  for (auto it = threads.rbegin(); it != threads.rend(); ++it) {
    ASSERT_OK((*it)->Join());
  }
  EXPECT_EQ(q.Size(), 0);
}

TEST(LockFreeProducerConsumerQueue, LockfreeMultiProducersConsumers) {
  RunLockfreeMultiProducersConsumers(absl::Microseconds(50));
}

TEST(LockFreeProducerConsumerQueue, LockfreeMultiProducersConsumersSpin) {
  RunLockfreeMultiProducersConsumers(absl::ZeroDuration());
}

void MoodyProduce(
    moodycamel::BlockingConcurrentQueue<int>* q,
    moodycamel::BlockingConcurrentQueue<int>::producer_token_t* token,
    int num) {
  for (int i = 0; i < num; ++i) {
    // this crashes for some reason - I think I messed up the way to use it ?
    // q->enqueue(*token, i);
    q->enqueue(i);
  }
}
void MoodyConsume(moodycamel::BlockingConcurrentQueue<int>* q, int num) {
  int64_t sum = 0;
  int val;
  for (int i = 0; i < num; ++i) {
    q->wait_dequeue(val);
    sum += val;
  }
  std::cout << "Consumer sum " << sum << std::endl;
}

TEST(MoodyProducerConsumerQueue, MoodyMultiProducersConsumers) {
  static constexpr const int kNumItems = kManyItems;
  static constexpr const int kNumProducers = 8;
  static constexpr const int kNumConsumers = 8;
  static_assert(kNumItems % kNumConsumers == 0, "Bad items / consumer ratio");
  static_assert(kNumItems % kNumProducers == 0, "Bad items / producer ratio");
  moodycamel::BlockingConcurrentQueue<int> q(100, kNumProducers, kNumProducers);
  std::vector<moodycamel::BlockingConcurrentQueue<int>::producer_token_t>
      tokens;
  static constexpr const int kNumPerProducer = kNumItems / kNumProducers;
  std::vector<std::unique_ptr<work::Thread>> threads;
  for (size_t i = 0; i < kNumProducers; ++i) {
    tokens.emplace_back(
        moodycamel::BlockingConcurrentQueue<int>::producer_token_t(q));
    ASSERT_OK_AND_ASSIGN(
        auto produce, work::Thread::Create(absl::bind_front(
                          &MoodyProduce, &q, &tokens.back(), kNumPerProducer)));
    threads.emplace_back(std::move(produce));
  }
  static constexpr const int kNumPerConsumer = kNumItems / kNumConsumers;
  for (size_t i = 0; i < kNumConsumers; ++i) {
    ASSERT_OK_AND_ASSIGN(
        auto consume, work::Thread::Create(absl::bind_front(&MoodyConsume, &q,
                                                            kNumPerConsumer)));
    threads.emplace_back(std::move(consume));
  }
  for (auto it = threads.rbegin(); it != threads.rend(); ++it) {
    ASSERT_OK((*it)->Join());
  }
  EXPECT_EQ(q.size_approx(), 0);
}

// Performance on my machine:
//
// Locking:
// --------
// ProducerConsumerQueue:             47327 ops / sec / thread
//
// Lockfree:
// ---------
// Lockfreee w/ Sempahore Wait:      370875 ops / sec / thread
// Lockfreee w/ Spin Wait:          1068376 ops / sec / thread
// ModdyCamel:                       467045 ops / sec / thread
//
// So pick your poison, so to say:
//   * The api requires for both moody and our lockfree to specify upfront
//     the number of producers (&consumers - but that is easy for a thread-pool)
//   * the locking one does not require anything, but look at the numbers :)
//   * the lockfree requires to specify a producer / consumer ID, but that
//     can be solved outside with some `thread local` foo.
//
// Considering all these, probably the best to use is the MoodyCamel for now,
// until we figure out that we really need the extra performance boost for
// a high-volume message passing application.
//

}  // namespace synch
}  // namespace whisper
