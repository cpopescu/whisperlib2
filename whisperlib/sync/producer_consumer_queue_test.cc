#include "whisperlib/sync/producer_consumer_queue.h"

#include <thread>  // NOLINT(build/c++11)
#include "gmock/gmock.h"
#include "gtest/gtest.h"

namespace whisper {
namespace synch {

TEST(ProducerConsumerQueue, General) {
  ProducerConsumerQueue<int> q(100);
  for (int i = 0; i < 100; ++i) {
    ASSERT_FALSE(q.IsFull());
    EXPECT_TRUE(q.Put(i));
  }
  EXPECT_EQ(q.Size(), 100);
  EXPECT_TRUE(q.IsFull());
  EXPECT_FALSE(q.Put(101, absl::ZeroDuration()));
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
    EXPECT_TRUE(q.Put(i));
  }
  EXPECT_EQ(q.Size(), 100);
  EXPECT_TRUE(q.IsFull());
  EXPECT_FALSE(q.Put(101, absl::ZeroDuration()));
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
    EXPECT_TRUE(q.Put(i));
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
    EXPECT_TRUE(q.Put(i));
  }
  EXPECT_EQ(q.Size(), 100);
  q.Clear();
  EXPECT_EQ(q.Size(), 0);
}

const int kNumItems = 1000000;
void Produce(ProducerConsumerQueue<int>* q) {
  for (int i = 0; i < kNumItems; ++i) {
    q->Put(i);
  }
}
void Consume(ProducerConsumerQueue<int>* q) {
  for (int i = 0; i < kNumItems; ++i) {
    ASSERT_EQ(q->Get(), i);
  }
}

TEST(ProducerConsumerQueue, Multithread) {
  ProducerConsumerQueue<int> q(100);
  std::thread produce(&Produce, &q);
  std::thread consume(&Consume, &q);
  consume.join();
  produce.join();
  EXPECT_EQ(q.Size(), 0);
}

}  // namespace synch
}  // namespace whisper
