#pragma once

#include <cassert>
#include <cstdlib>
#include <memory>
#include <new>
#include <type_traits>
#include <utility>

template <typename T> class RawMemory {
public:
  RawMemory() = default;

  explicit RawMemory(size_t capacity)
      : buffer_(Allocate(capacity)), capacity_(capacity) {}

  RawMemory(const RawMemory &) = delete;
  RawMemory &operator=(const RawMemory &) = delete;

  RawMemory(RawMemory &&other) noexcept { Swap(other); }

  RawMemory &operator=(RawMemory &&other) noexcept {
    if (this != &other) {
      Deallocate(buffer_);
      buffer_ = nullptr;
      capacity_ = 0;
      Swap(other);
    }
    return *this;
  }

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

  size_t Capacity() const noexcept { return capacity_; }

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

  explicit Vector(size_t size) : data_(size) {
    std::uninitialized_value_construct_n(data_.GetAddress(), size);
    size_ = size;
  }

  Vector(const Vector &other) : data_(other.size_) {
    std::uninitialized_copy_n(other.data_.GetAddress(), other.size_,
                              data_.GetAddress());
    size_ = other.size_;
  }

  Vector(Vector &&other) noexcept { Swap(other); }

  Vector &operator=(const Vector &other) {
    if (this != &other) {
      Vector tmp(other);
      Swap(tmp);
    }
    return *this;
  }

  Vector &operator=(Vector &&other) noexcept {
    if (this != &other) {
      Swap(other);
    }
    return *this;
  }

  ~Vector() { std::destroy_n(data_.GetAddress(), size_); }

  size_t Size() const noexcept { return size_; }

  size_t Capacity() const noexcept { return data_.Capacity(); }

  const T &operator[](size_t index) const noexcept { return data_[index]; }

  T &operator[](size_t index) noexcept { return data_[index]; }

  void Swap(Vector &other) noexcept {
    data_.Swap(other.data_);
    std::swap(size_, other.size_);
  }

  void Reserve(size_t new_capacity) {
    if (new_capacity <= data_.Capacity()) {
      return;
    }

    RawMemory<T> new_data(new_capacity);

    constexpr bool use_move = std::is_nothrow_move_constructible_v<T> ||
                              !std::is_copy_constructible_v<T>;

    if constexpr (use_move) {
      std::uninitialized_move_n(data_.GetAddress(), size_,
                                new_data.GetAddress());
    } else {
      std::uninitialized_copy_n(data_.GetAddress(), size_,
                                new_data.GetAddress());
    }

    std::destroy_n(data_.GetAddress(), size_);
    data_.Swap(new_data);
  }

private:
  RawMemory<T> data_;
  size_t size_ = 0;
};
