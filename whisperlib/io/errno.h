#ifndef WHISPERLIB_IO_ERRNO_H_
#define WHISPERLIB_IO_ERRNO_H_

#include <string>

#include "absl/status/status.h"
#include "whisperlib/status/status.h"

namespace whisper {
namespace error {

// Returns the last system error encountered.
int Errno();

// Creates a status stream-like writer from the provided system error number.
status::StatusWriter ErrnoToStatus(int error);

// Returns the string description for the provided system error number.
std::string ErrnoToString(int error);

// Returns true if a file operation should be retried due to unavailable
// write on a descriptor that would block.
bool IsUnavailableAndShouldRetry(int error);

}  // namespace error
}  // namespace whisper

#endif  // WHISPERLIB_IO_ERRNO_H_
