#ifndef WHISPERLIB_BASE_FREE_LIST_H_
#define WHISPERLIB_BASE_FREE_LIST_H_

#include <atomic>
#include <memory>
#include <utility>
#include <vector>

#include "absl/log/check.h"
#include "absl/memory/memory.h"
#include "absl/synchronization/mutex.h"

namespace whisper {
namespace base {

template <typename T, typename FL>
struct _FreeListDeleter {
  static_assert(!std::is_array<T>::value, "array types are unsupported");
  // Pre c++23 compliant:
  _FreeListDeleter() = default;
  // Post c++23 compliant:
  _FreeListDeleter(FL* free_list) : free_list(free_list) {}

  constexpr void operator()(T* ptr) const {
    if (!ptr) {
      return;
    }
    if (free_list) {
      free_list->Dispose(ptr);
    }
  }
  FL* free_list = nullptr;
};

// Thread unsafe version:
template <typename T>
class FreeList {
  static_assert(!std::is_array<T>::value, "array types are unsupported");
  static_assert(std::is_object<T>::value, "non-object types are unsupported");
  static_assert(std::is_trivially_constructible<T>::value,
                "non-trivially constructible types are unsupported");

 public:
  using PtrType = std::unique_ptr<T, _FreeListDeleter<T, FreeList<T>>>;
  explicit FreeList(size_t max_size) : max_size_(max_size) {
    free_list_.reserve(max_size);
  }
  ~FreeList() {
    outstanding_.fetch_add(free_list_.size(), std::memory_order_acq_rel);
  }
  PtrType New() {
    outstanding_.fetch_add(1, std::memory_order_acq_rel);
    if (!free_list_.empty()) {
      PtrType p = std::move(free_list_.back());
      free_list_.pop_back();
      return p;
    }
    PtrType ptr(new T());
    ptr.get_deleter().free_list = this;
    return ptr;
  }
  bool Dispose(T* ptr) {
    if (!ptr) {
      return false;
    }
    CHECK_GT(outstanding_.load(std::memory_order_consume), 0UL);
    outstanding_.fetch_sub(1, std::memory_order_acq_rel);
    if (free_list_.size() >= max_size_) {
      delete ptr;
      return true;
    }
    // We keep it in the free_list_.
    PtrType stored_ptr(ptr);
    stored_ptr.get_deleter().free_list = this;
    free_list_.emplace_back(std::move(stored_ptr));
    return false;
  }
  size_t max_size() const { return max_size_; }
  size_t outstanding() const { return outstanding_.load(); }

 private:
  const size_t max_size_;
  std::atomic_size_t outstanding_ = ATOMIC_VAR_INIT(0);
  std::vector<PtrType> free_list_;
};

template <typename T>
class ThreadSafeFreeList {
  static_assert(!std::is_array<T>::value, "array types are unsupported");
  static_assert(std::is_object<T>::value, "non-object types are unsupported");
  static_assert(std::is_trivially_constructible<T>::value,
                "non-trivially constructible types are unsupported");

 public:
  using PtrType =
      std::unique_ptr<T, _FreeListDeleter<T, ThreadSafeFreeList<T>>>;

  explicit ThreadSafeFreeList(size_t max_size) : max_size_(max_size) {
    free_list_.reserve(max_size);
  }
  ~ThreadSafeFreeList() {
    outstanding_.fetch_add(free_list_.size(), std::memory_order_acq_rel);
  }
  PtrType New() {
    {
      absl::MutexLock ml(&mutex_);
      outstanding_.fetch_add(1, std::memory_order_acq_rel);
      if (!free_list_.empty()) {
        PtrType p = std::move(free_list_.back());
        free_list_.pop_back();
        return p;
      }
    }
    PtrType ptr(new T());
    ptr.get_deleter().free_list = this;
    return ptr;
  }
  bool Dispose(T* ptr) {
    if (!ptr) {
      return false;
    }
    CHECK_GT(outstanding_.load(std::memory_order_consume), 0UL);
    outstanding_.fetch_sub(1, std::memory_order_acq_rel);
    {
      absl::MutexLock ml(&mutex_);
      if (free_list_.size() < max_size_) {
        // We keep ptr in free_list_:
        PtrType stored_ptr(ptr);
        stored_ptr.get_deleter().free_list = this;
        free_list_.emplace_back(std::move(stored_ptr));
        return false;
      }
    }
    delete ptr;
    return true;
  }
  size_t max_size() const { return max_size_; }
  size_t outstanding() const { return outstanding_.load(); }

 private:
  const size_t max_size_;
  std::atomic_size_t outstanding_ = ATOMIC_VAR_INIT(0);
  std::vector<PtrType> free_list_;
  absl::Mutex mutex_;
};

}  // namespace base
}  // namespace whisper

#endif  // WHISPERLIB_BASE_FREE_LIST_H_
