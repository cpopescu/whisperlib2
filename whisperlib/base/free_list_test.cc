#include "whisperlib/base/free_list.h"

#include "gtest/gtest.h"

namespace whisper {
namespace base {

template <class FL>
void TestSimpleFreeList(FL* fl, size_t size) {
  EXPECT_EQ(fl->max_size(), size);
  EXPECT_EQ(fl->outstanding(), 0);
  std::vector<typename FL::PtrType> v;
  for (size_t i = 0; i < 2 * size; ++i) {
    auto p = fl->New();
    EXPECT_EQ(fl->outstanding(), i + 1);
    v.emplace_back(std::move(p));
  }
  std::vector<int*> dv;
  for (size_t i = 0; i < v.size(); ++i) {
    if (i < size) {
      dv.push_back(v[i].get());
      EXPECT_FALSE(fl->Dispose(v[i].release()));
    } else {
      EXPECT_TRUE(fl->Dispose(v[i].release()));
    }
    EXPECT_EQ(fl->outstanding(), v.size() - i - 1);
  }
  v.clear();
  for (size_t i = 0; i < size; ++i) {
    auto p = fl->New();
    EXPECT_EQ(p.get(), dv.back());
    dv.pop_back();
    v.emplace_back(std::move(p));
  }
  for (size_t i = 0; i < v.size(); ++i) {
    EXPECT_FALSE(fl->Dispose(v[i].release()));
  }
}

TEST(FreeList, Simple) {
  FreeList<int> fl(10);
  TestSimpleFreeList(&fl, 10);
}

TEST(ThreadSafeFreeList, Simple) {
  ThreadSafeFreeList<int> fl(10);
  TestSimpleFreeList(&fl, 10);
}

}  // namespace base
}  // namespace whisper
