#pragma once
#include "basic.hh"

template<class T>
struct RingBuffer {

	T* data = nullptr;	
	usize capacity = 0u;
	u64 written = 0u;

	bool Init(usize capa);

	T* Push();
	void Reset();
	usize UsedSize() const;

	DISALLOW_COPY_AND_ASSING(RingBuffer)
};


template <class T>
inline bool RingBuffer<T>::Init(usize size) {
	ASSERT(!this->data);
	ASSERT(this->capacity == 0u);

	data = new T[size];
	capacity = size;
	return true;
}

template <class T>
inline T* RingBuffer<T>::Push() {

	T* item = &data[written % capacity];
	written++;
	
	return item;
}

template <class T>
inline void RingBuffer<T>::Reset() {
	written = 0u;
}

template <class T>
inline usize RingBuffer<T>::UsedSize() const {
	return std::min(written, capacity);
}

template <class T>
inline RingBuffer<T>::~RingBuffer() noexcept {
	delete[] data;
}
