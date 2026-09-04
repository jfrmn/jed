#include "json-helper.hh"
#include "logging.hh"
#include "settings.hh"

///////////////////////////////////////////////////////////////////////////////////////////////////
//
// J S O N   W R I T E R   B U F F E R
//
///////////////////////////////////////////////////////////////////////////////////////////////////

JsonWriteBuffer::JsonWriteBuffer(std::span<char> staticBuffer) noexcept{
	begin = head = staticBuffer.data();
	end = staticBuffer.data() + staticBuffer.size();
}

JsonWriteBuffer::~JsonWriteBuffer() noexcept {
	if (reallocated) delete[] begin;
}


static void Reallocate(JsonWriteBuffer* self) {
	const u64 currentSize = (self->end - self->begin);
	const u64 currentOffset = (self->head - self->begin);
	const u64 newSize = static_cast<u64>(currentSize * 1.5);
	
	char* newBuffer = new char[newSize];
	memcpy(newBuffer, self->begin, currentSize);
	
	if (self->reallocated) delete[] self->begin;
	self->begin = newBuffer;
	self->head = newBuffer + currentOffset;
	self->end = newBuffer + newSize;
	self->reallocated = true;
}

static void Reserve(JsonWriteBuffer* self, u64 size) {
	while ((self->head + size) > self->end)
		Reallocate(self);
}

void JsonWriteBuffer::Put(char ch) {
	if (head == end) Reallocate(this);
	*head++ = ch;
}

void JsonWriteBuffer::WriteObjectStart() {	
	Put('{');
}

void JsonWriteBuffer::WriteObjectEnd() {
	ASSERT(head != begin)
	if (head[-1] == ',') {
		head[-1] = '}';
		Put(',');
	} else {
		Reserve(this, 2u);
		*head++ = '}';
		*head++ = ',';
	}
}

void JsonWriteBuffer::WriteArrayStart() {
	Put('[');
}

void JsonWriteBuffer::WriteArrayEnd() {
	ASSERT(head != begin)
	if (head[-1] == ',') {
		head[-1] = ']';
		Put(',');
	} else {
		Reserve(this, 2u);
		*head++ = ']';
		*head++ = ',';
	}
}

JsonWriteBuffer* JsonWriteBuffer::WriteProperty(std::string_view property) {
	Reserve(this, property.size() + 3u);
	
	*head++ = '"';
	memcpy(head, property.data(), property.size());
	head += property.size();
	*head++ = '"';
	*head++ = ':';
	return this;
}

static void WriteLiteral(JsonWriteBuffer* self, std::string_view literal) {
	Reserve(self, literal.size() + 1u);
	memcpy(self->head, literal.data(), literal.size());
	self->head += literal.size();
	*self->head++ = ',';
}

void JsonWriteBuffer::WriteBoolean(bool value) {
	WriteLiteral(this, value ? "true" : "false");
}

void JsonWriteBuffer::WriteNull() {
	WriteLiteral(this, "null");
}

template<class T>
static void WriteNumber(JsonWriteBuffer* self, T value) {
	const auto tcr = std::to_chars(self->head, self->end, value);
	if (tcr.ec == std::errc::value_too_large) {
		Reallocate(self);
		WriteNumber(self, value);	
		return;
	}
	if (tcr.ec != std::errc()) {
		LogError("to_chars() failed for value %. Error: %", value, Str(tcr));
		return;
	}
	
	self->head = tcr.ptr;
	self->Put(',');	
}

void JsonWriteBuffer::WriteUnsigned(u64 value) {
	WriteNumber(this, value);	
}

void JsonWriteBuffer::WriteInteger(int value) {
	WriteNumber(this, value);	
}

void JsonWriteBuffer::WriteFloat(f64 value) {
	WriteNumber(this, value);	
}

void JsonWriteBuffer::WriteString(std::string_view value) {
	
	Reserve(this, 1u);
	*head++ = '"';
		
	for (char ch : value) {
		
		// quote and backslash
		if (ch == '"' || ch == '\\') {
			Reserve(this, 2);
			*head++ = '\\';
			*head++ = ch;
		
		// control characeter with escape sequence
		} else if (ch >= '\b' && ch <= '\r') {
			constexpr char escapeCharacters[] = "btnvfr";
			Reserve(this, 2);
			*head++ = '\\';
			*head++ = escapeCharacters[ch - '\b'];
			
		// printable ascii
		} else if (ch >= 32 && ch <= 127) {
			Reserve(this, 1);
			*head++ = ch;
		
		// 1-byte sequence but non-ascii
		} else if ((ch & 0b1000'0000) == 0) { 
			constexpr char hexDigits[] = "0123456789abcdef";
			
			Reserve(this, 6);
			memcpy(head, "\\u00", 4);
			head += 4;
			*head++ = hexDigits[(ch >> 4) & 0xf];
			*head++ = hexDigits[ch & 0xf];
		
		// anything else	
		} else {
			Reserve(this, 1);
			*head++ = ch;
		}
	}
	
	Reserve(this, 2u);
	*head++ = '"';
	*head++ = ',';	
}

void JsonWriteBuffer::WriteRawString(std::string_view value) {
	Reserve(this, value.size() + 3u);
	*head++ = '"';
	memcpy(head, value.data(), value.size());
	head += value.size();
	*head++ = '"';
	*head++ = ',';
}


void JsonWriteBuffer::WriteEnumString(std::string_view value) {
	Reserve(this, value.size() + 2u);
	*head++ = '"';
	memcpy(head, value.data(), value.size());
	head += value.size();
	*head++ = '"';
}

std::string_view JsonWriteBuffer::GetString() const {
	const char* stringEnd = head;
	if (head[-1] == ',') stringEnd = head-1;
	return std::string_view {begin, stringEnd};
}

///////////////////////////////////////////////////////////////////////////////////////////////////
//
// J S O N   O B J E C T   R E A D E R
//
///////////////////////////////////////////////////////////////////////////////////////////////////

static cJSON* LookupProperty(JsonObjectReader* self, std::string_view property, int type) {
	auto it = self->properties.find(property);
	if (it == self->properties.end()) return nullptr;
		
	if (std::string err; !JsonExpectType(it->second, type, &err)) {
		LogWarning("json: property '%s%s%s' %.*s", (self->object->string ? self->object->string : ""), (self->object->string ? "." : ""), property, SIZE_AND_DATA(err));
		return nullptr;
	}
	
	return it->second;
}

JsonObjectReader::JsonObjectReader(const cJSON* object) {
	this->object = object;
	for (cJSON* prop = object->child; prop != nullptr; prop = prop->next) {
		properties.insert({std::string_view {prop->string}, prop});
	}
}
	
bool JsonObjectReader::ReadUnsigned(std::string_view property, u64* value) {
	const cJSON* prop = LookupProperty(this, property, cJSON_Number);
	if (!prop) return false;
	*value = static_cast<u64>(prop->valuedouble);
	return true;
}

bool JsonObjectReader::ReadInteger(std::string_view property, int* value) {
	const cJSON* prop = LookupProperty(this, property, cJSON_Number);
	if (!prop) return false;
	*value = static_cast<int>(prop->valuedouble);
	return true;
}

bool JsonObjectReader::ReadFloat(std::string_view property, f32* value) {
	const cJSON* prop = LookupProperty(this, property, cJSON_Number);
	if (!prop) return false;
	*value = static_cast<f32>(prop->valuedouble);
	return true;
}

bool JsonObjectReader::ReadString(std::string_view property, std::string_view* value) {
	const cJSON* prop = LookupProperty(this, property, cJSON_String);
	if (!prop) return false;
	*value = prop->valuestring;
	return true;
}

bool JsonObjectReader::ReadBoolean(std::string_view property, bool* value) {
	const cJSON* prop = LookupProperty(this, property, cJSON_True | cJSON_False);
	if (!prop) return false;
	*value = prop->type == cJSON_True;
	return true;
}

bool JsonObjectReader::Contains(std::string_view property) const {
	return properties.contains(property);
}

const cJSON* JsonObjectReader::GetArray(std::string_view property) {
	const cJSON* json = LookupProperty(const_cast<JsonObjectReader*>(this), property, cJSON_Array);
	if (!json) return nullptr;
	return json->child;
}

const cJSON* JsonObjectReader::Get(std::string_view property) {
	auto it = properties.find(property);
	if (it == properties.end()) return nullptr;
	return it->second;
}

const cJSON* JsonObjectReader::GetObject(std::string_view property) {
	auto it = properties.find(property);
	if (it == properties.end()) return nullptr;
	if (!JsonExpectType(it->second, cJSON_Object)) return nullptr;
	return it->second;
}

///////////////////////////////////////////////////////////////////////////////////////////////////
//
// J S O N   A L L O C A T O R
//
///////////////////////////////////////////////////////////////////////////////////////////////////

void JsonAllocator::Init() {
	nodesCapacity = settings.jsonAllocatorNodeCapacity;
	nodePool = new cJSON[nodesCapacity];
	
	stringCapacity = settings.jsonAllocatorStringCapacity;
	stringPool = new char[stringCapacity];
}

cJSON* JsonAllocator::AllocateNode() {
	
	if (nodesOccupied < nodesCapacity) {
	
		cJSON* slot = &nodePool[nodesOccupied];
		nodesOccupied++;
		return slot;
	
	} else {
		nodesOccupied++; // increase anyway - for statistics
		return static_cast<cJSON*>(AllocateMemory(sizeof(cJSON)));
	}
}

char* JsonAllocator::AllocateString(u64 size) {
	
	if ((stringOccupied + size) <= stringCapacity) {
		char* str = &stringPool[stringOccupied];
		stringOccupied += size;
		return str;
	
	} else {
		// unlike in AllocateNode, we do not increase occupied here because maybe we can fit the next allocation
		return static_cast<char*>(AllocateMemory(size));
	}
}

void* JsonAllocator::AllocateMemory(u64 size) {
	const usize actualSize = sizeof(Memblock) + (size - 1);
	auto block = static_cast<Memblock*>(malloc(actualSize));
	block->prev = memblocks;
	memblocks = block;

	return &block->data;
}


static u64 ClearMemblocks(JsonAllocator* self) {
	
	u64 count = 0u;

	JsonAllocator::Memblock* block = self->memblocks;
	while (block) {
		JsonAllocator::Memblock* prev = block->prev;
		free(block);
		block = prev;
		count++;
	}
	self->memblocks = nullptr;
	return count;
}

void JsonAllocator::Shutdown() {
	ClearMemblocks(this);
	delete[] stringPool;
	delete[] nodePool;
}

JsonAllocator::~JsonAllocator() noexcept {
	Shutdown();
}

void JsonAllocator::Reset() {
	const u64 statMemblocks = ClearMemblocks(this);
	const u64 statNodes = nodesOccupied;
	const u64 statString = stringOccupied;
	
	memset(nodePool,   0, sizeof(cJSON) * nodesCapacity);
	memset(stringPool, 0, sizeof(char)  * stringCapacity);
	
	nodesOccupied = 0u;
	stringOccupied = 0u;
	
	LogTrace("allocator: node: %lu/%lu string: %lu/%lu  memblocks: %lu", statNodes, nodesCapacity, statString, stringCapacity, statMemblocks);
}

///////////////////////////////////////////////////////////////////////////////////////////////////
//
// F R E E   F U N C T I O N S
//
///////////////////////////////////////////////////////////////////////////////////////////////////

thread_local JsonAllocator* JsonAllocator::activeAllocator = nullptr;

std::string_view JsonTypeToString(int type) {
	switch (type & 0xFF) {
		case cJSON_Invalid: return "undefined";
		case cJSON_False:   return "false";
		case cJSON_True:    return "true";
		case cJSON_NULL:    return "null";
		case cJSON_Number:  return "number";
		case cJSON_String:  return "string";
		case cJSON_Array:   return "array";
		case cJSON_Object:  return "object";
		case cJSON_Raw:     return "raw";
		default:            return "unknown";
	}
}

bool JsonExpectType(const cJSON* json, int types, /*out*/ std::string* errorStr /*= nullptr*/) {
	
	// 256 = IsReference; 512 = StringIsConst - we don't care about these and need to filter them out
	const int actualType = json->type & 0xFF;
	
	if ((actualType & types) != 0)
		return true;
	
	std::string errorString = "expected ";
	for (int i = 0; i < 8; i++) {
		const int typeToTest = (1 << i);
		if ((types & typeToTest) == typeToTest) {
			
			if (errorString.size() > 9) errorString.append(" or ");
			errorString.push_back('[');
			errorString.append(JsonTypeToString(typeToTest));
			errorString.push_back(']');
		}
	}
	
	errorString.append(" - but was [")
		.append(JsonTypeToString(json->type))
		.push_back(']');
	
	if (errorStr) *errorStr = std::move(errorString);
	else LogError("json: '%s' %.*s", (json->string ? json->string : ""), SIZE_AND_DATA(errorString));
	return false;
}

static void* JsonMalloc(usize size) {
	if (JsonAllocator::activeAllocator) {
		return (size == sizeof(cJSON))
			? static_cast<void*>(JsonAllocator::activeAllocator->AllocateNode())
			: static_cast<void*>(JsonAllocator::activeAllocator->AllocateString(size));

	} else {
		return malloc(size);
	}
}

static void JsonFree(void* memblock) {
	if (!JsonAllocator::activeAllocator)
		free(memblock);
}

bool InitJsonLib() {
	cJSON_Hooks hooks {
		.malloc_fn = JsonMalloc,
		.free_fn = JsonFree };
	cJSON_InitHooks(&hooks);
	return true;
}

