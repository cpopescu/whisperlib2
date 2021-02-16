#ifndef WHISPERLIB_STATUS_STATUS_H_
#define WHISPERLIB_STATUS_STATUS_H_

#include <string>
#include <utility>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/str_cat.h"
#include "glog/logging.h"

namespace whisper {
namespace status {

absl::Status Annotate(
    const absl::Status& status, absl::string_view message);

absl::Status& UpdateOrAnnotate(
    absl::Status& status,
    const absl::Status& annotation);

class StatusWriter {
 public:
  explicit StatusWriter(absl::Status status)
    : status_(std::move(status)) {}

  operator absl::Status() const {
    if (message_.empty() || status_.ok()) {
      return status_;
    }
    return Annotate(status_, message_);
  }
  template <class T> StatusWriter& operator <<(const T& value) {
    absl::StrAppend(&message_, value);
    return *this;
  }

 protected:
  mutable absl::Status status_;
  std::string message_;
};
// Logs the status built so far to the error log.
class LogToError {
};
template<> inline
StatusWriter& StatusWriter::operator<<(const LogToError& req) {
  LOG(ERROR) << absl::Status(*this);
  return *this;
}
}  // namespace status
}  // namespace whisper

#define STATUS_MACROS_CONCAT_NAME_INNER(x, y) x##y
#define STATUS_MACROS_CONCAT_NAME(x, y) STATUS_MACROS_CONCAT_NAME_INNER(x, y)


#define RETURN_IF_ERROR_IMPL(status_name, expr)                 \
absl::Status status_name = (expr);                              \
if (ABSL_PREDICT_FALSE(!status_name.ok()))                      \
  return ::whisper::status::StatusWriter(std::move(status_name))

#define RETURN_IF_ERROR(expr)                                           \
RETURN_IF_ERROR_IMPL(                                                   \
    STATUS_MACROS_CONCAT_NAME(_return_if_error, __COUNTER__), (expr))


#define ASSIGN_OR_RETURN_IMPL(status_name, lhs, rexpr, ...)             \
auto&& status_name = (rexpr);                                           \
if (ABSL_PREDICT_FALSE(!status_name.ok())) {                            \
  ::whisper::status::StatusWriter _(status_name.status());              \
  __VA_ARGS__;                                                          \
  return _;                                                             \
}                                                                       \
lhs = std::move(status_name).value()

#define ASSIGN_OR_RETURN(lhs, rexpr, ...)                       \
ASSIGN_OR_RETURN_IMPL(                                          \
    STATUS_MACROS_CONCAT_NAME(_assign_or_return, __COUNTER__),  \
    lhs, rexpr, __VA_ARGS__)

#define RET_CHECK(condition)                                            \
  if (ABSL_PREDICT_FALSE(!(condition)))                                 \
    return ::whisper::status::FailedPreconditionErrorBuilder(           \
        "Invalid state in the program. Precondition: `" #condition      \
        "` does not hold")                                              \
      << "In file: " << __FILE__ << " at line: " << __LINE__


#define CHECK_OK_IMPL(status_name, expr)                        \
absl::Status status_name = (expr);                              \
if (ABSL_PREDICT_FALSE(!status_name.ok()))                      \
  LOG(FATAL) << "Check failed with status: " << status_name

#define CHECK_OK(expr)                                          \
CHECK_OK_IMPL(STATUS_MACROS_CONCAT_NAME(_check_ok, __COUNTER__), (expr))

namespace whisper {
namespace status {
inline StatusWriter AbortedErrorBuilder(
    absl::string_view message = "") {
  return StatusWriter(absl::AbortedError(message));
}
inline StatusWriter AlreadyExistsErrorBuilder(
    absl::string_view message = "") {
  return StatusWriter(absl::AlreadyExistsError(message));
}
inline StatusWriter CancelledErrorBuilder(
    absl::string_view message = "") {
  return StatusWriter(absl::CancelledError(message));
}
inline StatusWriter DataLossErrorBuilder(
    absl::string_view message = "") {
  return StatusWriter(absl::DataLossError(message));
}
inline StatusWriter DeadlineExceededErrorBuilder(
    absl::string_view message = "") {
  return StatusWriter(absl::DeadlineExceededError(message));
}
inline StatusWriter FailedPreconditionErrorBuilder(
    absl::string_view message = "") {
  return StatusWriter(absl::FailedPreconditionError(message));
}
inline StatusWriter InternalErrorBuilder(
    absl::string_view message = "") {
  return StatusWriter(absl::InternalError(message));
}
inline StatusWriter InvalidArgumentErrorBuilder(
    absl::string_view message = "") {
  return StatusWriter(absl::InvalidArgumentError(message));
}
inline StatusWriter NotFoundErrorBuilder(
    absl::string_view message = "") {
  return StatusWriter(absl::NotFoundError(message));
}
inline StatusWriter OutOfRangeErrorBuilder(
    absl::string_view message = "") {
  return StatusWriter(absl::OutOfRangeError(message));
}
inline StatusWriter PermissionDeniedErrorBuilder(
    absl::string_view message = "") {
  return StatusWriter(absl::PermissionDeniedError(message));
}
inline StatusWriter ResourceExhaustedErrorBuilder(
    absl::string_view message = "") {
  return StatusWriter(absl::ResourceExhaustedError(message));
}
inline StatusWriter UnauthenticatedErrorBuilder(
    absl::string_view message = "") {
  return StatusWriter(absl::UnauthenticatedError(message));
}
inline StatusWriter UnavailableErrorBuilder(
    absl::string_view message = "") {
  return StatusWriter(absl::UnavailableError(message));
}
inline StatusWriter UnimplementedErrorBuilder(
    absl::string_view message = "") {
  return StatusWriter(absl::UnimplementedError(message));
}
inline StatusWriter UnknownErrorBuilder(
    absl::string_view message = "") {
  return StatusWriter(absl::UnknownError(message));
}

}  // namespace status
}  // namespace whisper

#endif  // WHISPERLIB_STATUS_STATUS_H_
