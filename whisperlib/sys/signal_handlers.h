#ifndef WHISPERLIB_SYS_SIGNAL_HANDLERS_H_
#define WHISPERLIB_SYS_SIGNAL_HANDLERS_H_

#include "absl/status/status.h"

namespace whisper {
namespace sys {

// Sets default signal handlers for SIGABRT and SIGSEGV.
// The default signal handler prints the stack trace, and either
//  - hangs the program if hang_on_bad_signals==true.
//  - exits the program if hang_on_bad_signals==false.
absl::Status InstallDefaultSignalHandlers(bool hang_on_bad_signals);

// returns:
//  - true if the signal handler cought a signal and is hanging
//  - false otherwise
bool IsApplicationHanging();

// Set a flag in which the application would hang upon a signal to allow
// a debugger attach or other operation.
void SetApplicationHanging(bool hanging);

}  // namespace sys
}  // namespace whisper

#endif  // WHISPERLIB_SYS_SIGNAL_HANDLERS_H_
