
#include "whisperlib/net/selector.h"

#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "whisperlib/status/testing.h"

namespace whisper {
namespace net {

TEST(Selector, BasicLoop) {
  ASSERT_OK_AND_ASSIGN(std::unique_ptr<Selector> selector,
                       Selector::Create(Selector::Params()));
  bool called = false;
  selector->RunInSelectLoop([&called, &selector]() {
    called = true;
    selector->MakeLoopExit();
  });
  ASSERT_OK(selector->Loop());
  EXPECT_TRUE(called);
}

}  // namespace net
}  // namespace whisper
