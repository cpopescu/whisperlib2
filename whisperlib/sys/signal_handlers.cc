#include "whisperlib/sys/signal_handlers.h"

#include <string.h>
#include <unistd.h>
#include <signal.h>

#include <atomic>
#include <iostream>

#include "absl/debugging/stacktrace.h"
#include "absl/debugging/symbolize.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/str_format.h"
#include "absl/strings/str_join.h"
#include "absl/time/clock.h"
#include "absl/time/time.h"
#include "glog/logging.h"
#include "whisperlib/io/errno.h"
#include "whisperlib/status/status.h"

namespace whisper {
namespace sys {

namespace {
std::atomic_bool g_symbolizer_initialized = ATOMIC_VAR_INIT(false);
std::atomic_bool g_hang_on_signal_stack_trace = ATOMIC_VAR_INIT(false);
// true if the application is already hanging
std::atomic_bool g_application_is_hanging = ATOMIC_VAR_INIT(false);


void HandleSignal(int signum, siginfo_t* /*info*/, void* /*context*/) {
  // Using LOG functions here is unwise. If the initial exception happened
  // inside a LOG statement, then using LOG here would recursively generate a
  // new exception.
  const absl::Time now = absl::Now();
  std::cerr << "\033[31m On [" << absl::FormatTime(now)
            << " (" << absl::FormatTime(now, absl::UTCTimeZone())
            << ")]" << std::endl
            << " Signal intercepted " << signum << " - " << strsignal(signum)
            << "\033[0m" << std::endl
            << " Stack trace: " << std::endl
            << absl::StrJoin(GetStackTrace(), "\n") << std::endl;
  std::cerr.flush();

  if (g_hang_on_signal_stack_trace.load()) {
    g_application_is_hanging.store(true);
    while ( true ) {
      std::cerr
        << "Program pid=" << getpid() << " tid="
        << static_cast<int64_t>(pthread_self()) << " is now hanging now. "
        << " You can Debug it or Kill (Ctrl+C) it." << std::endl;
      absl::SleepFor(absl::Seconds(30));
    }
  }

  // call default handler
  ::signal(signum, SIG_DFL);
  ::raise(signum);
}
}  // namespace

std::vector<std::string> GetStackTrace(int max_depth, bool symbolize) {
  auto stacks = absl::make_unique<void*[]>(max_depth);
  auto sizes = absl::make_unique<int[]>(max_depth);
  const int num_frames = absl::GetStackFrames(
      stacks.get(), sizes.get(), max_depth, 1);
  if (!g_symbolizer_initialized.load()) {
    symbolize = false;
  }
  std::vector<std::string> result;
  char buffer[1024];
  result.reserve(num_frames);
  for (int i = 0; i < num_frames; ++i) {
    result.emplace_back(absl::StrFormat("  @%p [%6d]  ", stacks[i], sizes[i]));
    if (symbolize && absl::Symbolize(stacks[i], buffer, sizeof(buffer))) {
      absl::StrAppend(&result.back(), buffer);
    } else {
      absl::StrAppend(&result.back(), "(unknown)");
    }
  }
  return result;
}

absl::Status InstallDefaultSignalHandlers(
    const char* argv0, bool hang_on_bad_signals) {
  bool symbolizer_initialized = false;
  if (argv0 != 0 &&
      g_symbolizer_initialized.compare_exchange_strong(
          symbolizer_initialized, true)) {
    absl::InitializeSymbolizer(argv0);
  }
  g_hang_on_signal_stack_trace.store(hang_on_bad_signals);

  // install signal handler routines
  struct sigaction sa;
  sa.sa_handler = NULL;
  sa.sa_sigaction = HandleSignal;
  sigemptyset(&sa.sa_mask);
  //  Restart functions if interrupted by handler
  //  We want to process RT signals..
  sa.sa_flags = SA_RESTART | SA_SIGINFO;

  absl::Status status;
  for (auto sig_num : {
      SIGSEGV, SIGILL, SIGFPE, SIGABRT, SIGTERM, SIGBUS, SIGHUP }) {
    // SIGUSR1 => Dump heap profile ?
    if (-1 == ::sigaction(sig_num, &sa, NULL)) {
      status::UpdateOrAnnotate(
          status, error::ErrnoToStatus(error::Errno())
          << "Installing signal handler for signal: " << sig_num);
    }
  }
  RETURN_IF_ERROR(status) << "During InstallDefaultSignalHandlers call."
                          << status::LogToError();
  // Ignore SIGPIPE. Writing to a disconnected socket causes SIGPIPE.
  // All system calls that would cause SIGPIPE to be sent will return -1 and
  // set errno to EPIPE.
  if (SIG_ERR == ::signal(SIGPIPE, SIG_IGN)) {
    return error::ErrnoToStatus(error::Errno())
      << "Installing SIGPIPE handler." << status::LogToError();
  }
  return absl::OkStatus();
}

bool IsApplicationHanging() {
  return g_application_is_hanging.load();
}
void SetApplicationHanging(bool hanging) {
  g_application_is_hanging.store(hanging);
}

}  // namespace sys
}  // namespace whisper
