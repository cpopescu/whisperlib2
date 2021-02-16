#ifndef WHISPERLIB_BASE_CALL_ON_RETURN_H_
#define WHISPERLIB_BASE_CALL_ON_RETURN_H_

#include <functional>
#include <utility>

namespace whisper {
namespace base {

class CallOnReturn {
 public:
  explicit CallOnReturn(std::function<void()> on_return)
    : on_return_(std::move(on_return)) {}
  ~CallOnReturn() {
    run();
  }
  void run() {
    if (on_return_ != nullptr) {
      on_return_();
      on_return_ = nullptr;
    }
  }
  std::function<void()> reset() {
    auto on_return = on_return_;
    on_return_ = nullptr;
    return on_return;
  }
 private:
  std::function<void()> on_return_;
};

}  // namespace base
}  // namespace whisper

#endif  // WHISPERLIB_BASE_CALL_ON_RETURN_H_
