
#include "whisperlib/sys/signal_handlers.h"

#include <iostream>
#include <string>
#include <vector>

#include "absl/strings/match.h"
#include "absl/strings/str_format.h"
#include "absl/strings/str_join.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "whisperlib/status/status.h"

namespace whisper {
namespace sys {

std::vector<std::string> __attribute__ ((noinline)) SymTest(
    int max_depth, bool symbolize, size_t depth) {
  if (depth > 0) {
    return SymTest(max_depth, symbolize, depth - 1);
  }
  return GetStackTrace(max_depth, symbolize);
}

void RaiseSignal(int signo) {
  ::raise(signo);
}

TEST(SignalHandlers, Symbolizer) {
  {
    auto s = SymTest(30, true, 10);
    std::cerr << "========\n" << absl::StrJoin(s, "\n") << "\n========\n";
    ASSERT_GE(s.size(), 10);
    EXPECT_FALSE(absl::EndsWith(s.back(), "(unknown)"));
  }
  {
    auto s = SymTest(10, true, 10);
    std::cerr << "========\n" << absl::StrJoin(s, "\n") << "\n========\n";
    EXPECT_EQ(s.size(), 10);
  }
  {
    auto s = SymTest(30, false, 10);
    std::cerr << "========\n" << absl::StrJoin(s, "\n") << "\n========\n";
    ASSERT_GE(s.size(), 10);
    for (const auto& sym : s) {
      EXPECT_TRUE(absl::EndsWith(sym, "(unknown)"));
    }
  }
}

TEST(SignalHandlers, Signal) {
  for (auto signo : {
      SIGSEGV, SIGILL, SIGFPE, SIGABRT, SIGTERM, SIGBUS, SIGHUP }) {
    EXPECT_EXIT(RaiseSignal(signo), testing::KilledBySignal(signo),
                absl::StrFormat(".* Signal intercepted %d - %s.*",
                                signo, strsignal(signo)));
  }
}

}  // namespace sys
}  // namespace whisper

int main(int argc, char **argv) {
  CHECK_OK(whisper::sys::InstallDefaultSignalHandlers(argv[0], false));
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
