#pragma once
#include <cassert>
#include <cstdlib>
#include <new>
#include <utility>

template <typename T> class RawMemory {
public:
  RawMemory() = default;

  explicit RawMemory(size_t capacity)
      : buffer_(Allocate(capacity)), capacity_(capacity) {}

  ~RawMemory() { Deallocate(buffer_); }

  T *operator+(size_t offset) noexcept {
    assert(offset <= capacity_);
    return buffer_ + offset;
  }

  const T *operator+(size_t offset) const noexcept {
    return const_cast<RawMemory &>(*this) + offset;
  }

  const T &operator[](size_t index) const noexcept {
    return const_cast<RawMemory &>(*this)[index];
  }

  T &operator[](size_t index) noexcept {
    assert(index < capacity_);
    return buffer_[index];
  }

  void Swap(RawMemory &other) noexcept {
    std::swap(buffer_, other.buffer_);
    std::swap(capacity_, other.capacity_);
  }

  const T *GetAddress() const noexcept { return buffer_; }
  T *GetAddress() noexcept { return buffer_; }

  size_t Capacity() const { return capacity_; }

private:
  static T *Allocate(size_t n) {
    return n != 0 ? static_cast<T *>(operator new(n * sizeof(T))) : nullptr;
  }

  static void Deallocate(T *buf) noexcept { operator delete(buf); }

  T *buffer_ = nullptr;
  size_t capacity_ = 0;
};

template <typename T> class Vector {
public:
  Vector() = default;

  explicit Vector(size_t size) : data_(size), size_(size) {
    for (size_t i = 0; i != size; ++i) {
      try {
        new (data_ + i) T();
      } catch (...) {
        DestroyN(data_.GetAddress(), i);
        throw;
      }
    }
  }

  Vector(const Vector &other) : data_(other.size_), size_(other.size_) {
    for (size_t i = 0; i != other.size_; ++i) {
      try {
        CopyConstruct(data_ + i, other[i]);
      } catch (...) {
        DestroyN(data_.GetAddress(), i);
        throw;
      }
    }
  }

  ~Vector() { DestroyN(data_.GetAddress(), size_); }

  size_t Size() const noexcept { return size_; }
  size_t Capacity() const noexcept { return data_.Capacity(); }

  void Reserve(size_t new_capacity) {
    if (new_capacity <= data_.Capacity())
      return;

    RawMemory<T> new_data(new_capacity);
    size_t copied = 0;
    try {
      for (; copied != size_; ++copied) {
        CopyConstruct(new_data + copied, data_[copied]);
      }
    } catch (...) {
      DestroyN(new_data.GetAddress(), copied);
      throw;
    }

    DestroyN(data_.GetAddress(), size_);
    data_.Swap(new_data);
  }

  const T &operator[](size_t index) const noexcept {
    return const_cast<Vector &>(*this)[index];
  }

  T &operator[](size_t index) noexcept {
    assert(index < size_);
    return data_[index];
  }

private:
  static void DestroyN(T *buf, size_t n) noexcept {
    for (size_t i = 0; i != n; ++i)
      Destroy(buf + i);
  }

  static void CopyConstruct(T *buf, const T &elem) { new (buf) T(elem); }
  static void Destroy(T *buf) noexcept { buf->~T(); }

  RawMemory<T> data_;
  size_t size_ = 0;
};