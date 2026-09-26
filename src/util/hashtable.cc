#include "hashtable.hh"

u64 HashString(std::string_view key) {
    u64 hash = 14695981039346656037u;
    
    for (char ch : key) {
        hash ^= static_cast<u8>(ch);
        hash *= 1099511628211u;
    }
    
    return hash;
}

std::unique_ptr<char[]> Hashtable_CloneKey(std::string_view key) {
	auto keyBuffer = std::make_unique<char[]>(key.size() + 1u);
	memcpy_s(keyBuffer.get(), key.size() + 1u, key.data(), key.size());
	keyBuffer[key.size()] = '\0';
	return keyBuffer;
}