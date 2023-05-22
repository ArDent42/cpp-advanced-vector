#pragma once
#include <cassert>
#include <cstdlib>
#include <new>
#include <utility>
#include <memory>

template<typename T>
class RawMemory {
public:
	RawMemory() = default;

	explicit RawMemory(size_t capacity) :
			buffer_(Allocate(capacity)), capacity_(capacity) {
	}
	RawMemory(const RawMemory&) = delete;
	RawMemory& operator=(const RawMemory &rhs) = delete;
	RawMemory(RawMemory &&other) noexcept {
		*this = std::move(other);
	}
	RawMemory& operator=(RawMemory &&rhs) noexcept {
		if (this != &rhs) {
			buffer_ = std::move(rhs.buffer_);
			capacity_ = std::move(rhs.Capacity());
			rhs.buffer_ = nullptr;
			rhs.capacity_ = 0;
		}
		return *this;
	}

	~RawMemory() {
		Deallocate(buffer_);
	}

	T* operator+(size_t offset) noexcept {
		// Разрешается получать адрес ячейки памяти, следующей за последним элементом массива
		assert(offset <= capacity_);
		return buffer_ + offset;
	}

	const T* operator+(size_t offset) const noexcept {
		return const_cast<RawMemory&>(*this) + offset;
	}

	const T& operator[](size_t index) const noexcept {
		return const_cast<RawMemory&>(*this)[index];
	}

	T& operator[](size_t index) noexcept {
		assert(index < capacity_);
		return buffer_[index];
	}

	void Swap(RawMemory &other) noexcept {
		std::swap(buffer_, other.buffer_);
		std::swap(capacity_, other.capacity_);
	}

	const T* GetAddress() const noexcept {
		return buffer_;
	}

	T* GetAddress() noexcept {
		return buffer_;
	}

	size_t Capacity() const {
		return capacity_;
	}

private:
	// Выделяет сырую память под n элементов и возвращает указатель на неё
	static T* Allocate(size_t n) {
		return n != 0 ? static_cast<T*>(operator new(n * sizeof(T))) : nullptr;
	}

	// Освобождает сырую память, выделенную ранее по адресу buf при помощи Allocate
	static void Deallocate(T *buf) noexcept {
		operator delete(buf);
	}

	T *buffer_ = nullptr;
	size_t capacity_ = 0;
};

template<typename T>
class Vector {
public:
	using iterator = T*;
	using const_iterator = const T*;

	Vector() = default;

	explicit Vector(size_t size) :
			data_(size), size_(size) {
		std::uninitialized_value_construct_n(data_.GetAddress(), size);
	}

	Vector(const Vector &other) :
			data_(other.size_), size_(other.size_) {
		std::uninitialized_copy_n(other.data_.GetAddress(), other.size_, data_.GetAddress());
	}

	Vector& operator=(const Vector &other) {
		if (this != &other) {
			if (other.size_ <= data_.Capacity()) {
				if (size_ <= other.size_) {
					std::copy(other.data_.GetAddress(), other.data_.GetAddress() + size_, data_.GetAddress());
					std::uninitialized_copy_n(other.data_.GetAddress() + size_, other.size_ - size_, data_.GetAddress() + size_);
				} else {
					std::copy(other.data_.GetAddress(), other.data_.GetAddress() + other.size_, data_.GetAddress());
					std::destroy_n(data_.GetAddress() + other.size_, size_ - other.size_);
				}
				size_ = other.size_;
			} else {
				Vector other_copy(other);
				Swap(other_copy);
			}
		}
		return *this;
	}

	Vector(Vector &&other) noexcept {
		*this = std::move(other);
	}

	Vector& operator=(Vector &&other) noexcept {
		data_ = std::move(other.data_);
		size_ = std::move(other.size_);
		other.size_ = 0;
		return *this;
	}

	void Reserve(size_t new_capacity) {
		if (new_capacity <= data_.Capacity()) {
			return;
		}
		RawMemory<T> new_data(new_capacity);
		if constexpr (std::is_nothrow_move_constructible_v<T> || !std::is_copy_constructible_v<T>) {
			std::uninitialized_move_n(data_.GetAddress(), size_, new_data.GetAddress());
		} else {
			std::uninitialized_copy_n(data_.GetAddress(), size_, new_data.GetAddress());
		}
		std::destroy_n(data_.GetAddress(), size_);
		data_.Swap(new_data);
	}

	void Swap(Vector &other) noexcept {
		data_.Swap(other.data_), std::swap(size_, other.size_);
	}

	void Resize(size_t new_size) {
		if (new_size == size_) {
			return;
		} else if (new_size < size_) {
			std::destroy_n(data_.GetAddress() + new_size, size_ - new_size);
			size_ = new_size;
		} else {
			Reserve(new_size);
			std::uninitialized_value_construct_n(data_.GetAddress() + size_, new_size - size_);
			size_ = new_size;
		}
	}

	template<typename M>
	void PushBack(M &&value) {
		if (data_.Capacity() == size_) {
			RawMemory<T> new_data(size_ == 0 ? 1 : size_ * 2);
			new (new_data.GetAddress() + size_) T(std::forward<M>(value));
			if constexpr (std::is_nothrow_move_constructible_v<T> || !std::is_copy_constructible_v<T>) {
				std::uninitialized_move_n(data_.GetAddress(), size_, new_data.GetAddress());
			} else {
				std::uninitialized_copy_n(data_.GetAddress(), size_, new_data.GetAddress());
			}
			std::destroy_n(data_.GetAddress(), size_);
			data_.Swap(new_data);
		} else {
			new (data_.GetAddress() + size_) T(std::forward<M>(value));
		}
		++size_;
	}

	template<typename ... Args>
	T& EmplaceBack(Args &&... args) {
		if (data_.Capacity() == size_) {
			RawMemory<T> new_data(size_ == 0 ? 1 : size_ * 2);
			new (new_data.GetAddress() + size_) T(std::forward<Args>(args) ...);
			if constexpr (std::is_nothrow_move_constructible_v<T> || !std::is_copy_constructible_v<T>) {
				std::uninitialized_move_n(data_.GetAddress(), size_, new_data.GetAddress());
			} else {
				std::uninitialized_copy_n(data_.GetAddress(), size_, new_data.GetAddress());
			}
			std::destroy_n(data_.GetAddress(), size_);
			data_.Swap(new_data);
		} else {
			new (data_.GetAddress() + size_) T(std::forward<Args>(args)...);
		}
		return data_[size_++];
	}

	template<typename ... Args>
	iterator Emplace(const_iterator pos, Args &&... args) {
		int pos_index = pos - begin();
		if (data_.Capacity() == size_) {
			RawMemory<T> new_data(size_ == 0 ? 1 : size_ * 2);
			new (new_data.GetAddress() + pos_index) T(std::forward<Args>(args) ...);
			if constexpr (std::is_nothrow_move_constructible_v<T> || !std::is_copy_constructible_v<T>) {
				std::uninitialized_move_n(data_.GetAddress(), pos_index, new_data.GetAddress());
				std::uninitialized_move_n(data_.GetAddress() + pos_index, size_ - pos_index, new_data.GetAddress() + pos_index + 1);
			} else {
				std::uninitialized_copy_n(data_.GetAddress(), pos_index, new_data.GetAddress());
				std::uninitialized_copy_n(data_.GetAddress() + pos_index, size_ - pos_index, new_data.GetAddress() + pos_index + 1);
			}
			std::destroy_n(data_.GetAddress(), size_);
			data_.Swap(new_data);
		} else {
			try {
				if (pos == end()) {
					new (end()) T(std::forward<Args>(args)...);
				} else {
					T temp(std::forward<Args>(args)...);
					new (end()) T(std::forward<T>(data_[size_ - 1]));
					std::move_backward(begin() + pos_index, end() - 1, end());
					*(begin() + pos_index) = std::forward<T>(temp);
				}
			} catch (...) {
				operator delete(end());
				throw;
			}
		}
		++size_;
		return begin() + pos_index;
	}

	iterator Insert(const_iterator pos, const T &item) {
		return Emplace(pos, item);
	}
	iterator Insert(const_iterator pos, T &&item) {
		return Emplace(pos, std::move(item));
	}

	iterator Erase(const_iterator pos) {
		int pos_index = pos - begin();
		std::move(begin() + pos_index + 1, end(), begin() + pos_index);
		std::destroy_at(end() - 1);
		--size_;
		return begin() + pos_index;
	}

	void PopBack() noexcept {
		std::destroy_at(data_.GetAddress() + size_ - 1);
		--size_;
	}

	size_t Size() const noexcept {
		return size_;
	}

	size_t Capacity() const noexcept {
		return data_.Capacity();
	}

	const T& operator[](size_t index) const noexcept {
		return data_[index];
	}

	T& operator[](size_t index) noexcept {
		return data_[index];
	}

	iterator begin() noexcept {
		return data_.GetAddress();
	}
	iterator end() noexcept {
		return data_.GetAddress() + size_;
	}
	const_iterator begin() const noexcept {
		return data_.GetAddress();
	}
	const_iterator end() const noexcept {
		return data_.GetAddress() + size_;
	}
	const_iterator cbegin() const noexcept {
		return begin();
	}
	const_iterator cend() const noexcept {
		return end();
	}

	~Vector() {
		std::destroy_n(data_.GetAddress(), size_);
	}

private:
	RawMemory<T> data_;
	size_t size_ = 0;

};
