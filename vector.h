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
      : buffer_(static_cast<T *>(::operator new(capacity * sizeof(T)))),
        capacity_(capacity) {}

  ~RawMemory() { ::operator delete(buffer_); }

  RawMemory(RawMemory &&other) noexcept
      : buffer_(other.buffer_), capacity_(other.capacity_) {
    other.buffer_ = nullptr;
    other.capacity_ = 0;
  }

  RawMemory &operator=(RawMemory &&other) noexcept {
    if (this != &other) {
      ::operator delete(buffer_);
      buffer_ = other.buffer_;
      capacity_ = other.capacity_;
      other.buffer_ = nullptr;
      other.capacity_ = 0;
    }
    return *this;
  }

  RawMemory(const RawMemory &) = delete;
  RawMemory &operator=(const RawMemory &) = delete;

  T *data() { return buffer_; }
  const T *data() const { return buffer_; }
  size_t capacity() const { return capacity_; }

  void Swap(RawMemory &other) noexcept {
    std::swap(buffer_, other.buffer_);
    std::swap(capacity_, other.capacity_);
  }

private:
  T *buffer_ = nullptr;
  size_t capacity_ = 0;
};

template <typename T> class Vector {
public:
  Vector() = default;

  explicit Vector(size_t size) : data_(size), size_(size) {
    std::uninitialized_value_construct_n(data_.data(), size);
  }

  Vector(const Vector &other) : data_(other.size_), size_(other.size_) {
    std::uninitialized_copy_n(other.data_.data(), other.size_, data_.data());
  }

  Vector(Vector &&other) noexcept
      : data_(std::move(other.data_)), size_(other.size_) {
    other.size_ = 0;
  }

  ~Vector() { std::destroy_n(data_.data(), size_); }

  Vector &operator=(const Vector &rhs) {
    if (this != &rhs) {
      if (data_.capacity() >= rhs.size_) {
        const size_t common = std::min(size_, rhs.size_);
        for (size_t i = 0; i < common; ++i) {
          data_.data()[i] = rhs.data_.data()[i];
        }
        if (size_ < rhs.size_) {
          size_t i = size_;
          try {
            for (; i < rhs.size_; ++i) {
              new (data_.data() + i) T(rhs.data_.data()[i]);
            }
          } catch (...) {
            std::destroy_n(data_.data() + size_, i - size_);
            throw;
          }
        }
        if (size_ > rhs.size_) {
          std::destroy_n(data_.data() + rhs.size_, size_ - rhs.size_);
        }
        size_ = rhs.size_;
      } else {
        Vector temp(rhs);
        Swap(temp);
      }
    }
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

  size_t Size() const { return size_; }
  size_t Capacity() const { return data_.capacity(); }

  T &operator[](size_t index) { return data_.data()[index]; }
  const T &operator[](size_t index) const { return data_.data()[index]; }

  void Reserve(size_t new_capacity) {
    if (new_capacity > data_.capacity()) {
      RawMemory<T> new_data(new_capacity);
      if constexpr (std::is_nothrow_move_constructible_v<T>) {
        std::uninitialized_move_n(data_.data(), size_, new_data.data());
      } else if constexpr (std::is_copy_constructible_v<T>) {
        std::uninitialized_copy_n(data_.data(), size_, new_data.data());
      } else {
        std::uninitialized_move_n(data_.data(), size_, new_data.data());
      }
      std::destroy_n(data_.data(), size_);
      data_ = std::move(new_data);
    }
  }

  void Resize(size_t new_size) {
    if (new_size < size_) {
      std::destroy_n(data_.data() + new_size, size_ - new_size);
      size_ = new_size;
    } else if (new_size > size_) {
      if (new_size > data_.capacity()) {
        const size_t new_capacity = std::max(new_size, data_.capacity() * 2);
        RawMemory<T> new_data(new_capacity);

        if constexpr (std::is_nothrow_move_constructible_v<T>) {
          std::uninitialized_move_n(data_.data(), size_, new_data.data());
        } else if constexpr (std::is_copy_constructible_v<T>) {
          std::uninitialized_copy_n(data_.data(), size_, new_data.data());
        } else {
          std::uninitialized_move_n(data_.data(), size_, new_data.data());
        }

        size_t i = size_;
        try {
          for (; i < new_size; ++i) {
            new (new_data.data() + i) T();
          }
        } catch (...) {
          std::destroy_n(new_data.data() + size_, i - size_);
          std::destroy_n(new_data.data(), size_);
          throw;
        }

        std::destroy_n(data_.data(), size_);
        data_ = std::move(new_data);
        size_ = new_size;
      } else {
        size_t i = size_;
        try {
          for (; i < new_size; ++i) {
            new (data_.data() + i) T();
          }
        } catch (...) {
          std::destroy_n(data_.data() + size_, i - size_);
          throw;
        }
        size_ = new_size;
      }
    }
  }

  void PushBack(const T &value) {
    if (size_ == data_.capacity()) {
      const size_t new_capacity = size_ == 0 ? 1 : size_ * 2;
      RawMemory<T> new_data(new_capacity);

      if constexpr (std::is_nothrow_move_constructible_v<T>) {
        std::uninitialized_move_n(data_.data(), size_, new_data.data());
      } else if constexpr (std::is_copy_constructible_v<T>) {
        std::uninitialized_copy_n(data_.data(), size_, new_data.data());
      } else {
        std::uninitialized_move_n(data_.data(), size_, new_data.data());
      }

      try {
        new (new_data.data() + size_) T(value);
      } catch (...) {
        std::destroy_n(new_data.data(), size_);
        throw;
      }

      std::destroy_n(data_.data(), size_);
      data_ = std::move(new_data);
      ++size_;
    } else {
      new (data_.data() + size_) T(value);
      ++size_;
    }
  }

  void PushBack(T &&value) {
    if (size_ == data_.capacity()) {
      const size_t new_capacity = size_ == 0 ? 1 : size_ * 2;
      RawMemory<T> new_data(new_capacity);

      if constexpr (std::is_nothrow_move_constructible_v<T>) {
        std::uninitialized_move_n(data_.data(), size_, new_data.data());
      } else if constexpr (std::is_copy_constructible_v<T>) {
        std::uninitialized_copy_n(data_.data(), size_, new_data.data());
      } else {
        std::uninitialized_move_n(data_.data(), size_, new_data.data());
      }

      try {
        new (new_data.data() + size_) T(std::move(value));
      } catch (...) {
        std::destroy_n(new_data.data(), size_);
        throw;
      }

      std::destroy_n(data_.data(), size_);
      data_ = std::move(new_data);
      ++size_;
    } else {
      new (data_.data() + size_) T(std::move(value));
      ++size_;
    }
  }

  void PopBack() noexcept {
    --size_;
    std::destroy_at(data_.data() + size_);
  }

private:
  RawMemory<T> data_;
  size_t size_ = 0;
};