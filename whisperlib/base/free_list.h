#ifndef WHISPERLIB_BASE_FREE_LIST_H_
#define WHISPERLIB_BASE_FREE_LIST_H_

#include <atomic>
#include <memory>
#include <utility>
#include <vector>

#include "absl/memory/memory.h"
#include "absl/synchronization/mutex.h"
#include "glog/logging.h"

namespace whisper {
namespace base {


template <typename T>
class FreeList {
 public:
  using PtrType = std::unique_ptr<T>;
  explicit FreeList(size_t max_size)
    : max_size_(max_size) {
    free_list_.reserve(max_size);
  }
  std::unique_ptr<T> New() {
    outstanding_.fetch_add(1);
    if (!free_list_.empty()) {
      std::unique_ptr<T> p = std::move(free_list_.back());
      free_list_.pop_back();
      return p;
    }
    return absl::make_unique<T>();
  }
  bool Dispose(std::unique_ptr<T> p) {
    CHECK_GT(outstanding_.load(), 0UL);
    outstanding_.fetch_sub(1);
    if (free_list_.size() < max_size_) {
      free_list_.emplace_back(std::move(p));
      return false;
    }
    return true;
  }
  size_t max_size() const {
    return max_size_;
  }
  size_t outstanding() const {
    return outstanding_.load();
  }

 private:
  const size_t max_size_;
  std::atomic_size_t outstanding_ = ATOMIC_VAR_INIT(0);
  std::vector<std::unique_ptr<T>> free_list_;
};

template <typename T>
class ThreadSafeFreeList {
 public:
  using PtrType = std::unique_ptr<T>;
  explicit ThreadSafeFreeList(size_t max_size)
    : max_size_(max_size) {
    free_list_.reserve(max_size);
  }
  std::unique_ptr<T> New() {
    {
      absl::MutexLock ml(&mutex_);
      outstanding_.fetch_add(1);
      if (!free_list_.empty()) {
        std::unique_ptr<T> p = std::move(free_list_.back());
        free_list_.pop_back();
        return p;
      }
    }
    return absl::make_unique<T>();
  }
  bool Dispose(std::unique_ptr<T> p) {
    {
      absl::MutexLock ml(&mutex_);
      CHECK_GT(outstanding_.load(), 0UL);
      outstanding_.fetch_sub(1);
      if (free_list_.size() < max_size_) {
        free_list_.emplace_back(std::move(p));
        return false;
      }
    }
    return true;
  }
  size_t max_size() const {
    return max_size_;
  }
  size_t outstanding() const {
    return outstanding_.load();
  }

 private:
  const size_t max_size_;
  std::atomic_size_t outstanding_ = ATOMIC_VAR_INIT(0);
  std::vector<std::unique_ptr<T>> free_list_;
  absl::Mutex mutex_;
};

// A FreeList for arrays of fixed size - established in the constructor.
template <typename T>
class FreeArrayList {
 public:
  using PtrType = std::unique_ptr<T[]>;
  // array_size: size of the arrays of type T we allocate.
  // max_size: max size of the free list.
  FreeArrayList(size_t array_size, size_t max_size)
    : array_size_(array_size),
      max_size_(max_size) {
  }
  std::unique_ptr<T[]> New() {
    outstanding_.fetch_add(1);
    if (!free_list_.empty()) {
      std::unique_ptr<T[]> p = std::move(free_list_.back());
      free_list_.pop_back();
      return p;
    }
    return absl::make_unique<T[]>(array_size_);
  }
  bool Dispose(std::unique_ptr<T[]> p) {
    CHECK_GT(outstanding_.load(), 0UL);
    outstanding_.fetch_sub(1);
    if (free_list_.size() < max_size_) {
      free_list_.emplace_back(std::move(p));
      return false;
    }
    return true;
  }
  size_t array_size() const {
    return array_size_;
  }
  size_t max_size() const {
    return max_size_;
  }
  size_t outstanding() const {
    return outstanding_.load();
  }

 private:
  const size_t array_size_;
  const size_t max_size_;
  std::atomic_size_t outstanding_ = ATOMIC_VAR_INIT(0);
  std::vector<std::unique_ptr<T[]>> free_list_;
};

template <typename T>
class ThreadSafeFreeArrayList {
 public:
  using PtrType = std::unique_ptr<T[]>;
  ThreadSafeFreeArrayList(size_t array_size, size_t max_size)
    : array_size_(array_size),
      max_size_(max_size) {
  }
  std::unique_ptr<T[]> New() {
    {
      absl::MutexLock ml(&mutex_);
      outstanding_.fetch_add(1);
      if (!free_list_.empty()) {
        std::unique_ptr<T[]> p = std::move(free_list_.back());
        free_list_.pop_back();
        return p;
      }
    }
    return absl::make_unique<T[]>(array_size_);
  }
  bool Dispose(std::unique_ptr<T[]> p) {
    {
      absl::MutexLock ml(&mutex_);
      CHECK_GT(outstanding_.load(), 0UL);
      outstanding_.fetch_sub(1);
      if (free_list_.size() < max_size_) {
        free_list_.emplace_back(std::move(p));
        return false;
      }
    }
    return true;
  }
  size_t array_size() const {
    return array_size_;
  }
  size_t max_size() const {
    return max_size_;
  }
  size_t outstanding() const {
    return outstanding_.load();
  }

 private:
  const size_t array_size_;
  const size_t max_size_;
  std::atomic_size_t outstanding_ = ATOMIC_VAR_INIT(0);
  std::vector<std::unique_ptr<T[]>> free_list_;
  absl::Mutex mutex_;
};

}  // namespace base
}  // namespace whisper

#endif  // WHISPERLIB_BASE_FREE_LIST_H_
