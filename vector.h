#pragma once

#include <algorithm>
#include <cassert>
#include <cstddef>
#include <memory>
#include <type_traits>
#include <utility>

template <typename T> class RawMemory {
public:
  RawMemory() = default;

  explicit RawMemory(size_t capacity)
      : buffer_(capacity == 0
                    ? nullptr
                    : static_cast<T *>(::operator new(capacity * sizeof(T)))),
        capacity_(capacity) {}

  ~RawMemory() { ::operator delete(buffer_); }

  RawMemory(const RawMemory &) = delete;
  RawMemory &operator=(const RawMemory &) = delete;

  RawMemory(RawMemory &&other) noexcept { Swap(other); }

  RawMemory &operator=(RawMemory &&other) noexcept {
    if (this != &other) {
      RawMemory tmp(std::move(other));
      Swap(tmp);
    }
    return *this;
  }

  constexpr T *data() noexcept { return buffer_; }
  constexpr const T *data() const noexcept { return buffer_; }
  constexpr size_t capacity() const noexcept { return capacity_; }

  void Swap(RawMemory &other) noexcept {
    std::swap(buffer_, other.buffer_);
    std::swap(capacity_, other.capacity_);
  }

private:
  T *buffer_ = nullptr;
  size_t capacity_ = 0;
};

template <typename T> class Vector {
private:
  static constexpr bool MoveIsNothrow = std::is_nothrow_move_constructible_v<T>;
  static constexpr bool CanCopy = std::is_copy_constructible_v<T>;
  static constexpr bool ShouldMoveSafely = MoveIsNothrow || !CanCopy;

  void RelocateElements(T *dst) {
    if constexpr (ShouldMoveSafely) {
      std::uninitialized_move_n(data_.data(), size_, dst);
    } else {
      std::uninitialized_copy_n(data_.data(), size_, dst);
    }
  }

public:
  Vector() = default;

  explicit Vector(size_t size) : data_(size), size_(size) {
    std::uninitialized_value_construct_n(data_.data(), size);
  }

  Vector(const Vector &other) : data_(other.size_), size_(other.size_) {
    std::uninitialized_copy_n(other.data_.data(), other.size_, data_.data());
  }

  Vector(Vector &&other) noexcept : size_(std::exchange(other.size_, 0)) {
    data_.Swap(other.data_);
  }

  ~Vector() { std::destroy_n(data_.data(), size_); }

  Vector &operator=(const Vector &rhs) {
    if (this == &rhs)
      return *this;

    if (data_.capacity() < rhs.size_) {
      Vector temp(rhs);
      Swap(temp);
      return *this;
    }

    if (size_ >= rhs.size_) {
      std::copy(rhs.data_.data(), rhs.data_.data() + rhs.size_, data_.data());
      std::destroy_n(data_.data() + rhs.size_, size_ - rhs.size_);
    } else {
      std::copy(rhs.data_.data(), rhs.data_.data() + size_, data_.data());
      std::uninitialized_copy_n(rhs.data_.data() + size_, rhs.size_ - size_,
                                data_.data() + size_);
    }
    size_ = rhs.size_;
    return *this;
  }

  Vector &operator=(Vector &&rhs) noexcept {
    Swap(rhs);
    return *this;
  }

  void Swap(Vector &other) noexcept {
    data_.Swap(other.data_);
    std::swap(size_, other.size_);
  }

  constexpr size_t Size() const noexcept { return size_; }
  constexpr size_t Capacity() const noexcept { return data_.capacity(); }

  T &operator[](size_t index) noexcept { return data_.data()[index]; }
  const T &operator[](size_t index) const noexcept {
    return data_.data()[index];
  }

  template <typename... Args> T &EmplaceBack(Args &&...args) {
    if (size_ < data_.capacity()) {
      T *item_ptr = ::new (static_cast<void *>(data_.data() + size_))
          T(std::forward<Args>(args)...);
      ++size_;
      return *item_ptr;
    }

    const size_t new_capacity = size_ == 0 ? 1 : size_ * 2;
    RawMemory<T> new_data(new_capacity);

    T *item_ptr = ::new (static_cast<void *>(new_data.data() + size_))
        T(std::forward<Args>(args)...);

    try {
      RelocateElements(new_data.data());
    } catch (...) {
      std::destroy_at(item_ptr);
      throw;
    }

    std::destroy_n(data_.data(), size_);
    data_ = std::move(new_data);
    ++size_;
    return *item_ptr;
  }

  void Reserve(size_t new_capacity) {
    if (new_capacity <= data_.capacity())
      return;

    RawMemory<T> new_data(new_capacity);
    RelocateElements(new_data.data());

    std::destroy_n(data_.data(), size_);
    data_ = std::move(new_data);
  }

  void Resize(size_t new_size) {
    if (new_size < size_) {
      std::destroy_n(data_.data() + new_size, size_ - new_size);
      size_ = new_size;
      return;
    }

    if (new_size <= data_.capacity()) {
      std::uninitialized_value_construct_n(data_.data() + size_,
                                           new_size - size_);
      size_ = new_size;
      return;
    }

    const size_t new_capacity = std::max(new_size, data_.capacity() * 2);
    RawMemory<T> new_data(new_capacity);

    std::uninitialized_value_construct_n(new_data.data() + size_,
                                         new_size - size_);

    try {
      RelocateElements(new_data.data());
    } catch (...) {
      std::destroy_n(new_data.data() + size_, new_size - size_);
      throw;
    }

    std::destroy_n(data_.data(), size_);
    data_ = std::move(new_data);
    size_ = new_size;
  }

  void PushBack(const T &value) { EmplaceBack(value); }
  void PushBack(T &&value) { EmplaceBack(std::move(value)); }

  void PopBack() noexcept {
    --size_;
    std::destroy_at(data_.data() + size_);
  }

private:
  RawMemory<T> data_;
  size_t size_ = 0;
};
