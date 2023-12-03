#include "whisperlib/status/status.h"

namespace whisper {
namespace status {

absl::Status Annotate(const absl::Status& status, absl::string_view message) {
  absl::Status result(status.code(),
                      status.message().empty()
                          ? message
                          : absl::StrCat(status.message(), "; ", message));
  status.ForEachPayload(
      [&result](absl::string_view name, const absl::Cord& payload) {
        result.SetPayload(name, payload);
      });
  return result;
}

absl::Status& UpdateOrAnnotate(absl::Status& status,
                               const absl::Status& annotation) {
  if (status.ok()) {
    status.Update(annotation);
  } else {
    status = Annotate(status, annotation.message());
    annotation.ForEachPayload(
        [&status](absl::string_view name, const absl::Cord& payload) {
          status.SetPayload(name, payload);
        });
  }
  return status;
}

}  // namespace status
}  // namespace whisper
