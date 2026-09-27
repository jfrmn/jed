#pragma once
#include "basic.hh"
#include <string_view>
#include <memory>

// Fowler-Noll-Vo 1a hash
// see: http://en.wikipedia.org/wiki/Fowler-Noll-Vo_hash_function
u64 HashString(std::string_view key);

template<class T>
struct Hashtable {
	
	struct Slot {
		std::unique_ptr<char[]> keyBuffer = nullptr;
		u64   keySize = 0u;
		T     value   = {};
		u64   hash    = 0u;
		
		bool IsOccupied() const { return keyBuffer != nullptr; }
		std::string_view Key() const { return std::string_view {keyBuffer.get(), keySize}; }
	};
	
	static constexpr f32 MAX_LOAD_FACTOR = 0.75f;
	static constexpr u64 GROW_FACTOR = 2u;
	
	std::unique_ptr<Slot[]> slots = nullptr;
	u64 size     = 0u;
	u64 occupied = 0u;
	
	void Reserve(u64 expectedItems);
	void Grow(u64 newSize);
	
	// returns true if the key didn't exist and the value was inserted
	// otherwise returns false
	bool InsertWithHash(std::string_view key, u64 hash, T value);
	// returns true if the key didn't exists and the value was inserted
	// otherwise the existing value gets updated and the function returns false
	bool UpsertWithHash(std::string_view key, u64 hash, T value);
	T* LookupWithHash(std::string_view key, u64 hash);
	
	bool Insert(std::string_view key, T value) { return this->InsertWithHash(key, HashString(key), value); }
	bool Upsert(std::string_view key, T value) { return this->UpsertWithHash(key, HashString(key), value); }
  
	      T* Lookup(std::string_view key)                         { return this->LookupWithHash(key, HashString(key)); }
	const T* Lookup(std::string_view key) const                   { return const_cast<Hashtable<T>*>(this)->LookupWithHash(key, HashString(key)); }
	const T* LookupWithHash(std::string_view key, u64 hash) const { return const_cast<Hashtable<T>*>(this)->LookupWithHash(key, hash); }
	      T* operator[](std::string_view key)                     { return this->LookupWithHash(key, HashString(key)); }
	const T* operator[](std::string_view key) const               { return const_cast<Hashtable<T>*>(this)->LookupWithHash(key, HashString(key)); }
};

template<class T>
void Hashtable<T>::Reserve(u64 expectedItems) {
	const u64 neededSize = static_cast<u64>((static_cast<f32>(expectedItems) / MAX_LOAD_FACTOR) + 0.5f);
	if (size < neededSize) Grow(neededSize);
}

template<class T>
void Hashtable<T>::Grow(u64 newSize) {
	auto newSlots = std::make_unique<Slot[]>(newSize);
	
	for (u64 i = 0u; i < occupied; i++) {
		Slot& oldSlot = slots[i];
		
		const u64 startIndex = oldSlot.hash % newSize;
		for (u64 j = 0; j < newSize; j++) {
			const u64 index = (startIndex + j) % newSize;
			Slot& targetSlot = newSlots[index];
			if (!targetSlot.IsOccupied()) {
				targetSlot = std::move(oldSlot);
				goto found_slot;
			}
		}
		// found no slot
		ASSERT_UNREACHABLE;
		
	found_slot: __noop;
	}
	
	slots = std::move(newSlots);
	size = newSize;	
}

std::unique_ptr<char[]> Hashtable_CloneKey(std::string_view key);

template<class T>
static void Hashtable_MaybeGrow(Hashtable<T>* self) {
	if (self->size == 0u) {
		self->Grow(4u);
		return;
	}
	
	const f32 loadFactor = static_cast<f32>(self->occupied) / self->size;
	if (loadFactor >= Hashtable<T>::MAX_LOAD_FACTOR)
		self->Grow(self->size * Hashtable<T>::GROW_FACTOR);
}

template<class T>
bool Hashtable<T>::InsertWithHash(std::string_view key, u64 hash, T value) {
	Hashtable_MaybeGrow(this);	
		
	const u64 startIndex = hash % size;
	for (u64 i = 0; i < size; i++) {
		const u64 index = (startIndex + i) % size;
		Slot& targetSlot = slots[index];
		
		if (!targetSlot.IsOccupied()) {
			auto keyBuffer = Hashtable_CloneKey(key);
			
			targetSlot = Slot {
				.keyBuffer = std::move(keyBuffer),
				.keySize = key.size(),
				.value = std::move(value),
				.hash = hash};
			occupied++;
			return true;
		}
		
		if (targetSlot.hash == hash && targetSlot.Key() == key)
			return false;
	}
	
	// found no slot
	ASSERT_UNREACHABLE;
	return false;
}

template<class T>
bool Hashtable<T>::UpsertWithHash(std::string_view key, u64 hash, T value) {
	Hashtable_MaybeGrow(this);
	
	const u64 startIndex = hash % size;
	for (u64 i = 0; i < size; i++) {
		const u64 index = (startIndex + i) % size;
		Slot& targetSlot = slots[index];
		
		if (!targetSlot.IsOccupied()) {
			auto keyBuffer = Hashtable_CloneKey(key);
			
			targetSlot = Slot {
				.keyBuffer = std::move(keyBuffer),
				.keySize = key.size(),
				.value = std::move(value),
				.hash = hash};
			occupied++;
			return true;
		}
		
		if (targetSlot.hash == hash && targetSlot.Key() == key) {
			targetSlot.value = value;
			return false;
		}
	}
	
	// found no slot
	ASSERT_UNREACHABLE;
	return false;
}

template<class T>
T* Hashtable<T>::LookupWithHash(std::string_view key, u64 hash) {
	if (size == 0u) return nullptr;
	const u64 startIndex = hash % size;
	for (u64 i = 0; i < size; i++) {
		const u64 index = (startIndex + i) % size;
		Slot& targetSlot = slots[index];
		
		if (targetSlot.hash == hash && targetSlot.Key() == key)
			return &targetSlot.value;
	}

	return nullptr;
}