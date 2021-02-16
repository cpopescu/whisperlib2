#include "whisperlib/sys/signal_handlers.h"

#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <gperftools/heap-profiler.h>

#include <atomic>
#include <iostream>

#include "absl/time/clock.h"
#include "absl/time/time.h"
#include "glog/logging.h"
#include "whisperlib/io/errno.h"
#include "whisperlib/status/status.h"

namespace whisper {
namespace sys {

namespace {
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
            << " Signal cought " << signum << " - " << strsignal(signum)
            << "\033[0m" << std::endl;
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

  // if you want to continue, do not call default handler
  if ( signum == SIGUSR1 ) {
    HeapProfilerDump("on user command");
    return;
  }

  // call default handler
  ::signal(signum, SIG_DFL);
  ::raise(signum);
}
}  // namespace

absl::Status InstallDefaultSignalHandlers(bool hang_on_bad_signals) {
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
      SIGABRT, SIGBUS, SIGHUP, SIGFPE, SIGILL, SIGUSR1, SIGSEGV}) {
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
