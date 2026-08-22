#pragma once

// Wrapper for reference counting objects

template<typename T>
struct Rc {
	struct Object : public T {
		std::atomic_int refCount = 0;
	};

	Object* ptr = nullptr;

	void Unreference();
	void ForceDelete();

	static Rc<T> New(int initialRefCount = 1);
	static Rc<T> AdoptVoidPtr(void* newPtr);

	Rc() = default;
	Rc(const Rc&) = delete;
	Rc(Rc&& other) noexcept;
	Rc(Object* ptr) noexcept;

	~Rc() noexcept;

	Rc<T>& operator=(Rc<T>&& other);
	T* operator->() noexcept;
	operator T*() noexcept;
	operator bool() const noexcept;
};

template<typename T>
void Rc<T>::Unreference() {
	ASSERT(ptr);
	if (--ptr->refCount <= 0) {
		delete ptr;
		ptr = nullptr;
	}
	ptr = nullptr;
}

template<typename T>
void Rc<T>::ForceDelete() {
	ASSERT(ptr);
	delete ptr;
	ptr = nullptr;
}

template<typename T>
Rc<T> Rc<T>::New(int initialRefCount) {
	Rc<T> Rc;
	Rc.ptr = new Object {.refCount = initialRefCount};
	return std::move(Rc);
}

template<typename T>
Rc<T> Rc<T>::AdoptVoidPtr(void* newPtr) {
	Rc<T> Rc;
	Rc.ptr = static_cast<Object*>(newPtr);
	return std::move(Rc);
}

template<typename T>
Rc<T>::Rc(Rc&& other) noexcept
	: ptr(other.ptr) {
	other.ptr = nullptr;
}

template<typename T>
Rc<T>::Rc(Object* ptr) noexcept
	: ptr(ptr) {
	if (ptr) ++ptr->refCount;
}

template<typename T>
Rc<T>::~Rc() noexcept {
	if (ptr) Unreference();
}

template<typename T>
Rc<T>& Rc<T>::operator=(Rc<T>&& other) {
	ASSERT(!ptr); // could make this work but currently there is no need for it

	ptr = other.ptr;
	other.ptr = nullptr;
	return *this;
}

template<typename T>
T* Rc<T>::operator->() noexcept {
	ASSERT(ptr);
	return ptr;
}

template<typename T>
Rc<T>::operator T*() noexcept {
	return ptr;
}

template<typename T>
Rc<T>::operator bool() const noexcept {
	return ptr != nullptr;
}
