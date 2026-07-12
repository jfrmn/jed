#include "glyph-run.hh"
#include "font.hh"
#include "factories.hh"
#include "util/logging.hh"
#include "text/text-buffer.hh"

#include <atomic>

#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <dwrite_1.h>
#include <d2d1.h>

///////////////////////////////////////////////////////////////////////////////////////////////////
//
// Shaping
//
///////////////////////////////////////////////////////////////////////////////////////////////////

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
struct ScriptAnalysisRecord {
	u32 position = 0u;
	u32 length = 0u;
	const DWRITE_SCRIPT_ANALYSIS* script = nullptr;
};

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
struct ShapingBuffer {
	
	static constexpr u64 SCRIPT_ANALYSIS_SIZE_FACTOR = 8;
	
	//------------------------------------------
	// data
	//------------------------------------------	
	
	// string properties
	std::unique_ptr<WCHAR[]> stringData        = nullptr;
	std::unique_ptr<u8[]>    stringUtfMapping  = nullptr;
	u32                      stringCapacity    = 0u;
	
	// script records
	std::unique_ptr<ScriptAnalysisRecord[]> scriptAnalysisRecords        = nullptr;
	u32                                     scriptAnalysisRecordCapacity = 0u;
	
	// shaping text properties
	std::unique_ptr<UINT16[]>                         clusterMap              = nullptr;
	std::unique_ptr<DWRITE_SHAPING_TEXT_PROPERTIES[]> textProperties          = nullptr;
	u32                                               shapingTextDataCapacity = 0u;
	
	// shaping glyph properties
	std::unique_ptr<DWRITE_GLYPH_OFFSET[]>             glyphOffsets             = nullptr;
	std::unique_ptr<DWRITE_SHAPING_GLYPH_PROPERTIES[]> glyphProperties          = nullptr;
	u32                                                shapingGlyphDataCapacity = 0u;
	
	// text analyzer
	IDWriteTextAnalyzer* textAnalyzer = nullptr;
	
	//------------------------------------------
	// functions
	//------------------------------------------	
	
	bool Init(u32 initialSize) {
		stringData.reset(      new WCHAR[initialSize]);
		stringUtfMapping.reset(new u8[initialSize]);
		stringCapacity = initialSize;
		
		clusterMap.reset(    new UINT16[initialSize]);
		textProperties.reset(new DWRITE_SHAPING_TEXT_PROPERTIES[initialSize]);
		shapingTextDataCapacity = initialSize;
	
		scriptAnalysisRecords.reset(new ScriptAnalysisRecord[initialSize / ShapingBuffer::SCRIPT_ANALYSIS_SIZE_FACTOR]);
		scriptAnalysisRecordCapacity = initialSize / ShapingBuffer::SCRIPT_ANALYSIS_SIZE_FACTOR;
	
		glyphOffsets.reset(   new DWRITE_GLYPH_OFFSET[initialSize]);
		glyphProperties.reset(new DWRITE_SHAPING_GLYPH_PROPERTIES[initialSize]);
		shapingGlyphDataCapacity = initialSize;
		
		if (HRESULT hr = dwFactory->CreateTextAnalyzer(&textAnalyzer); hr != S_OK) {
			LogError("CreateTextAnalyzer() failed. HRESULT: %", FHr(hr));
			return false;
		}
		
		return true;
	}
	
	void PrepareStringCapacity(u32 capacity) {
		if (capacity <= stringCapacity) {
			memset(stringData.get(),       0, sizeof(stringData[0])       * stringCapacity);
			memset(stringUtfMapping.get(), 0, sizeof(stringUtfMapping[0]) * stringCapacity);
		
		} else {
			stringData.reset(new WCHAR[capacity]);
			stringUtfMapping.reset(new u8[capacity]);
			stringCapacity = capacity;
		}
	}
	
	void ClearScriptAnalysisRecord() {
		memset(scriptAnalysisRecords.get(), 0, sizeof(scriptAnalysisRecords[0]) * scriptAnalysisRecordCapacity);
	}
	
	void ReallocateScriptAnalysisRecords(u32 newCapacity) {
		if (newCapacity <= scriptAnalysisRecordCapacity) return;
	
		auto newScriptAnalysisRecords = new ScriptAnalysisRecord[newCapacity];
		memcpy(newScriptAnalysisRecords, scriptAnalysisRecords.get(), sizeof(scriptAnalysisRecords[0]) * scriptAnalysisRecordCapacity);
			
		scriptAnalysisRecords.reset(newScriptAnalysisRecords);
		scriptAnalysisRecordCapacity = newCapacity;
	}
	
	void PrepareShapingTextData(u32 capacity) {
		if (capacity <= shapingTextDataCapacity) {
			memset(clusterMap.get(), 0, sizeof(clusterMap[0]) * shapingTextDataCapacity);
			memset(textProperties.get(), 0, sizeof(textProperties[0]) * shapingTextDataCapacity);
		} else {
			clusterMap.reset(new UINT16[capacity]);
			textProperties.reset(new DWRITE_SHAPING_TEXT_PROPERTIES[capacity]);
			shapingTextDataCapacity = capacity;
		}
	}
		
	void ReallocateShapingGlyphData(u32 newCapacity) {
		if (newCapacity <= shapingGlyphDataCapacity) return;
		
		auto newGlyphOffsets = new DWRITE_GLYPH_OFFSET[newCapacity];
		auto newGlyphProperties  = new DWRITE_SHAPING_GLYPH_PROPERTIES[newCapacity];
		
		memcpy(newGlyphOffsets, glyphOffsets.get(), sizeof(glyphOffsets[0]) * shapingGlyphDataCapacity);
		memcpy(newGlyphProperties, glyphProperties.get(), sizeof(glyphProperties[0]) * shapingGlyphDataCapacity);
		
		glyphOffsets.reset(newGlyphOffsets);
		glyphProperties.reset(newGlyphProperties);
		
		shapingGlyphDataCapacity = newCapacity;
	}

	void ClearShapingGlyphData() {
		memset(glyphOffsets.get(),    0, sizeof(glyphOffsets[0])    * shapingGlyphDataCapacity);
		memset(glyphProperties.get(), 0, sizeof(glyphProperties[0]) * shapingGlyphDataCapacity);
	}
	
	~ShapingBuffer() noexcept {
		if (textAnalyzer) {
			textAnalyzer->Release();
			textAnalyzer = nullptr;
		}
	}
};

static ShapingBuffer staticShapingBuffer {};

bool InitStaticShapingBuffer() {
	return staticShapingBuffer.Init(128);
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
struct TextAnalysisSink : public IDWriteTextAnalysisSink {
	
	//------------------------------------------
	// data
	//------------------------------------------	
	
	ShapingBuffer* shapingBuffer = nullptr;
	u32 recordCount = 0u;
	
	//------------------------------------------
	// functions
	//------------------------------------------
	
	TextAnalysisSink(ShapingBuffer* shapingBuffer)
		: shapingBuffer(shapingBuffer) {}
		
	const ScriptAnalysisRecord* begin() const { return shapingBuffer->scriptAnalysisRecords.get(); }
	const ScriptAnalysisRecord* end()   const { return shapingBuffer->scriptAnalysisRecords.get() + recordCount; }
	
	virtual HRESULT SetScriptAnalysis(
			UINT32 textPosition,
			UINT32 textLength,
			DWRITE_SCRIPT_ANALYSIS const* scriptAnalysis) noexcept override {
		
		ASSERT(recordCount <= shapingBuffer->scriptAnalysisRecordCapacity)
		
		if (recordCount == shapingBuffer->scriptAnalysisRecordCapacity) {
			const u32 newCapacity = static_cast<u32>(1.5f * shapingBuffer->scriptAnalysisRecordCapacity);
			shapingBuffer->ReallocateScriptAnalysisRecords(newCapacity);
		}
		
		shapingBuffer->scriptAnalysisRecords[recordCount] = ScriptAnalysisRecord {
			.position = textPosition,
			.length = textLength,
			.script = scriptAnalysis};
		recordCount++;
		
		return S_OK;
	}
	
	virtual HRESULT SetLineBreakpoints(
			UINT32 textPosition,
        	UINT32 textLength,
        	DWRITE_LINE_BREAKPOINT const* lineBreakpoints) noexcept override {
		return S_OK;
    }
    
    virtual HRESULT SetBidiLevel(
        	UINT32 textPosition,
        	UINT32 textLength,
        	UINT8 explicitLevel,
        	UINT8 resolvedLevel)  noexcept override {
		ASSERT_UNREACHABLE;
		return E_NOTIMPL;
    }   
	
	virtual HRESULT SetNumberSubstitution(
        	UINT32 textPosition,
        	UINT32 textLength,
        	IDWriteNumberSubstitution* numberSubstitution) noexcept override {
		ASSERT_UNREACHABLE;
		return E_NOTIMPL;
    }
    
    virtual ULONG AddRef() noexcept override  { return 0L; }
    virtual ULONG Release() noexcept override { return 0L; }
    virtual HRESULT QueryInterface(REFIID riid, void** ptr) noexcept override { return E_NOINTERFACE; }
};

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
struct TextAnalysisSource : IDWriteTextAnalysisSource {
	
	//------------------------------------------
	// data
	//------------------------------------------	
	
	ShapingBuffer* shapingBuffer = nullptr;
	u32 length = 0;
	
	//------------------------------------------
	// functions
	//------------------------------------------	

	TextAnalysisSource(ShapingBuffer* shapingBuffer, u32 length)
		: shapingBuffer(shapingBuffer)
		, length(length) {}

	virtual HRESULT GetTextAtPosition(
			UINT32 textPosition,
			WCHAR const** textString,
			UINT32* remainingLength) noexcept override {
		ASSERT(textPosition < length);
		*textString = shapingBuffer->stringData.get() + textPosition;
		*remainingLength = length - textPosition;
		return S_OK;
	}
	        
	virtual HRESULT GetTextBeforePosition(
			UINT32 textPosition,
			WCHAR const** textString,
			UINT32* textLength) noexcept override {
		ASSERT(textPosition < length);
		*textString = shapingBuffer->stringData.get();
		*textLength = textPosition;
		return S_OK;
	}        
      
	virtual DWRITE_READING_DIRECTION GetParagraphReadingDirection() noexcept override {
		return DWRITE_READING_DIRECTION_LEFT_TO_RIGHT;
	}
       
	virtual HRESULT GetLocaleName(
        	UINT32 textPosition,
        	UINT32* textLength,
        	WCHAR const** localeName) noexcept override {
		*localeName = L"en-US"; // @TODO - get actual locale
		return S_OK;
	}
    
    virtual HRESULT GetNumberSubstitution(
        	UINT32 textPosition,
        	UINT32* textLength,
        	IDWriteNumberSubstitution** numberSubstitution) noexcept override {
		ASSERT_UNREACHABLE;
		return E_NOTIMPL;
    }
        
    virtual ULONG AddRef() noexcept override  { return 0L; }
    virtual ULONG Release() noexcept override { return 0L; }
    virtual HRESULT QueryInterface(REFIID riid, void** ptr) noexcept override { return E_NOINTERFACE; }
};

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
static u32 ConvertToUtf16(std::string_view utf8, ShapingBuffer* shapingBuffer) {
	ASSERT(utf8.size() <= U32_MAX);
	
	// Constants for UTF-16 encoding
	constexpr u32    UTF16_MAX_UNICODE        = 0x10FFFF;
	constexpr u32    UTF16_SURROGATE_OFFSET   = 0x10000;
	constexpr u16    UTF16_HIGH_SURROGATE_MIN = 0xD800;
	constexpr u16    UTF16_LOW_SURROGATE_MIN  = 0xDC00;
	constexpr char16 UTF16_REPLACEMENT_CHAR   = 0xFFFD;
	
	u32 size = 0;
	
	for (u64 i = 0; i < utf8.size(); /**/) {
    	u32 cp = 0;
    	u8 seqLen = 0u;
    				
		// Decode UTF-8
    	
    	// 1 byte (0-127)
    	if (utf8[i] >= 0 && utf8[i] <= 0x7F) {
        	cp = utf8[i];
        	seqLen = 1u;
    	
    	// 2 bytes
    	} else if ((utf8[i] & 0xE0) == 0xC0 && i < utf8.size()-1) {
        	cp  = (utf8[i  ] & 0x1F) << 6;
        	cp |= (utf8[i+1] & 0x3F);
        	seqLen = 2u;
    	
    	// 3 bytes
    	} else if ((utf8[i] & 0xF0) == 0xE0 && i < utf8.size()-2) {
        	cp =  (utf8[i  ] & 0x0F) << 12;
        	cp |= (utf8[i+1] & 0x3F) << 6;
        	cp |= (utf8[i+2] & 0x3F);
        	seqLen = 3u;
    	
    	// 4 bytes
    	} else if ((utf8[i] & 0xF8) == 0xF0 && i < utf8.size()-3) {
        	cp  = (utf8[i  ] & 0x07) << 18;
        	cp |= (utf8[i+1] & 0x3F) << 12;
        	cp |= (utf8[i+2] & 0x3F) << 6;
        	cp |= (utf8[i+3] & 0x3F);
        	seqLen = 4u;
    	
    	// 5 bytes (Legacy)
    	} else if ((utf8[i] & 0xFC) == 0xF8 && i < utf8.size()-4) {
        	cp  = (utf8[i  ] & 0x03) << 24;
        	cp |= (utf8[i+1] & 0x3F) << 18;
        	cp |= (utf8[i+2] & 0x3F) << 12;
        	cp |= (utf8[i+3] & 0x3F) << 6;
        	cp |= (utf8[i+4] & 0x3F);
        	seqLen = 5u;
        
    	// 6 bytes (Legacy)
    	} else if ((utf8[i] & 0xFE) == 0xFC && i < utf8.size()-5) {
        	cp  = (utf8[i  ] & 0x01) << 30;
        	cp |= (utf8[i+1] & 0x3F) << 24;
        	cp |= (utf8[i+2] & 0x3F) << 18;
        	cp |= (utf8[i+3] & 0x3F) << 12;
        	cp |= (utf8[i+4] & 0x3F) << 6;
        	cp |= (utf8[i+5] & 0x3F);
        	seqLen = 6u;
		
		// can only happen with invalid utf8 sequences. we can do whatever
		} else {
			cp = utf8[i];
			seqLen = 1;
		}
	
		// Encode to UTF-16
    	if (cp < UTF16_SURROGATE_OFFSET) {
        	// Fits in a single UTF-16 code unit (BMP)
        	shapingBuffer->stringData[size] = static_cast<wchar>(cp);
        	shapingBuffer->stringUtfMapping[size] = seqLen;
        	
        	size += 1;
        	
    	} else if (cp <= UTF16_MAX_UNICODE) {
        	// Requires a Surrogate Pair
        	cp -= UTF16_SURROGATE_OFFSET;
        	
        	shapingBuffer->stringData[size] = static_cast<wchar>((cp >> 10) + UTF16_HIGH_SURROGATE_MIN);
        	shapingBuffer->stringData[size+1] = static_cast<wchar>((cp & 0x3FF) + UTF16_LOW_SURROGATE_MIN);
        	
        	shapingBuffer->stringUtfMapping[size] = seqLen;
        	shapingBuffer->stringUtfMapping[size+1] = 0;
        	
        	size += 2;
        	
    	} else {
        	// Code point is out of UTF-16 range (from 5-6 byte UTF-8)
        	shapingBuffer->stringData[size] = UTF16_REPLACEMENT_CHAR;
        	shapingBuffer->stringUtfMapping[size] = seqLen;

        	size += 1;
    	}

		i += seqLen;
	}
	
	return size;
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
static u64 CalculateGlyphArraySize(u32 capacity) {
	const u64 capacityForIndicies = static_cast<u64>((capacity / 2.0f) + 0.5f);
	return capacity + capacityForIndicies;
}

static void ReallocateGlyphs(GlyphRun* self, u32 newCapacity) {
	if (newCapacity <= self->glyphCapacity) return;
	
	const u64 newArraySize = CalculateGlyphArraySize(newCapacity);
	f32* newMemory = new f32[newArraySize];
	memcpy(newMemory,               self->glyphAdvances.get(), self->glyphCount * sizeof(f32));
	memcpy(newMemory + newCapacity, self->glyphIndicies,       self->glyphCount * sizeof(u16));
	
	self->glyphAdvances.reset(newMemory);
	self->glyphIndicies = reinterpret_cast<u16*>(self->glyphAdvances.get() + newCapacity);
	self->glyphCapacity = newCapacity;
}

static void ClearGlyphs(GlyphRun* self) {
	const u64 size = CalculateGlyphArraySize(self->glyphCapacity);
	memset(self->glyphAdvances.get(), 0, size * sizeof(f32));
	self->glyphCount = 0u;
	self->width = 0.0f;
}

// @IMPROVE would be better to reuse old memory but we need to store the 
// capacity somewhere. One thing we could do is make the char mapping
// "U32_MAX-terminated" and store the length this way.
static void PrepareMapping(GlyphRun* self, u32 count) {
	if (self->charCount == count) {
		memset(self->charMapping.get(), 0, count * sizeof(*self->charMapping.get()));
	} else {
		self->charMapping.reset(new u32[count]);
		self->charCount = count;
	}
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
static bool ShapeInternal(GlyphRun* self, std::string_view text, const Font& font, ShapingBuffer* shapingBuffer, std::vector<u32>* lineEnds) {
	ASSERT(text.size() <= U32_MAX);
	
	const u32 textSize = static_cast<u32>(text.size());
	ReallocateGlyphs(self, textSize);
	ClearGlyphs(self);
	PrepareMapping(self, textSize);
	
	shapingBuffer->PrepareStringCapacity(textSize);
	const u32 utf16Length = ConvertToUtf16(text, shapingBuffer);
	
	shapingBuffer->ClearScriptAnalysisRecord();	
	TextAnalysisSource textAnalysisSource {shapingBuffer, utf16Length};
	TextAnalysisSink textAnalysisSink {shapingBuffer};
	
	if (HRESULT hr = shapingBuffer->textAnalyzer->AnalyzeScript(&textAnalysisSource, 0, utf16Length, &textAnalysisSink); hr != S_OK) {
		LogError("AnalyzeScript() failed. HRESULT: %", FHr(hr));
		return false;
	}
	
	u32 charMappingWritten = 0u;
	for (const ScriptAnalysisRecord& scriptAnalysisRecord : textAnalysisSink) {		
		shapingBuffer->PrepareShapingTextData(scriptAnalysisRecord.length);
		shapingBuffer->ClearShapingGlyphData();
		
		u32 numGlyphsInChunk = 0u;
		while (true) {
			const u32 effectiveCapacity = std::min(shapingBuffer->shapingGlyphDataCapacity, self->glyphCapacity - self->glyphCount);
			
			HRESULT hr = shapingBuffer->textAnalyzer->GetGlyphs(
  				/* textString */          shapingBuffer->stringData.get() + scriptAnalysisRecord.position,
  				/* textLength */          scriptAnalysisRecord.length,
  				/* fontFace */            font.fontFace,
  				/* isSideways */          FALSE,
  				/* isRightToLeft */       FALSE,
  				/* scriptAnalysis */      scriptAnalysisRecord.script,
  				/* localeName */          L"en-US", // @TODO get actual locale
  				/* numberSubstitution */  nullptr,
  				/* features */            nullptr,
  				/* featureRangeLengths */ nullptr,
  				/* featureRanges */       0,
  				/* maxGlyphCount */       effectiveCapacity,
  				/* clusterMap */          shapingBuffer->clusterMap.get(),
  				/* textProps */           shapingBuffer->textProperties.get(),
  				/* glyphIndices */        self->glyphIndicies + self->glyphCount,
  				/* glyphProps */          shapingBuffer->glyphProperties.get(),
  				/* actualGlyphCount */    &numGlyphsInChunk);
	  		
	  		if (hr == S_OK) {
		  		break;
	  			
	  		} else if (hr == HRESULT_FROM_WIN32(ERROR_INSUFFICIENT_BUFFER)) {
				
				const u64 newCapacity = static_cast<u64>(
					std::min(shapingBuffer->shapingGlyphDataCapacity, self->glyphCapacity) * 1.5f) + 1u;
				
				if (newCapacity > U32_MAX) {
					LogFatal("GetGlyphs() failed: exceeding maximum capacity. Aborting...");
					return false;
				}
				
				LogDetail("Shaping buffer too small. Retrying with %", newCapacity);
				ReallocateGlyphs(self, static_cast<u32>(newCapacity));
				shapingBuffer->ReallocateShapingGlyphData(static_cast<u32>(newCapacity));
				continue; // try again
	  		
	  		} else {
		  		LogError("GetGlpyhs() failed. HRESULT: %", FHr(hr));
		  		return false;
	  		}
  		}
	  		
  		HRESULT hr = shapingBuffer->textAnalyzer->GetGlyphPlacements(
			/* textString */          shapingBuffer->stringData.get() + scriptAnalysisRecord.position,
			/* clusterMap */          shapingBuffer->clusterMap.get(),
			/* textProps */           shapingBuffer->textProperties.get(),
			/* textLength */          scriptAnalysisRecord.length,
			/* glyphIndices */        self->glyphIndicies + self->glyphCount,
			/* glyphProps */          shapingBuffer->glyphProperties.get(),
			/* glyphCount */          numGlyphsInChunk,
			/* fontFace */            font.fontFace,
			/* fontEmSize */          font.size,
			/* isSideways */          FALSE,
			/* isRightToLeft */       FALSE,
			/* scriptAnalysis */      scriptAnalysisRecord.script,
			/* localeName */          L"en-US",
			/* features */            nullptr,
			/* featureRangeLengths */ nullptr,
			/* featureRanges */       0,
			/* glyphAdvances */       self->glyphAdvances.get() + self->glyphCount,
			/* glyphOffset */         shapingBuffer->glyphOffsets.get());
			
		if (hr != S_OK) {
			LogError("GetGlyphPlacements() failed. HRESULT: %", FHr(hr));
	  		return false;
		}
		
		// transfer cluster map and correct tabs
		{
			for (u32 icharInChunk = 0; icharInChunk < scriptAnalysisRecord.length; icharInChunk++) {
				
				const u16 iglyphInChunk = shapingBuffer->clusterMap[icharInChunk];
				const u32 iglyphInRun   = self->glyphCount + iglyphInChunk;
								
				const u8 utf8SequenceLenght = shapingBuffer->stringUtfMapping[scriptAnalysisRecord.position + icharInChunk];
				
				for (u8 iseq = 0; iseq < utf8SequenceLenght; iseq++) {
					u32* charMapEntry = self->charMapping.get() + charMappingWritten + iseq;
					*charMapEntry = iglyphInRun;
				}
				
				charMappingWritten += utf8SequenceLenght;
				
				const wchar ch = shapingBuffer->stringData[scriptAnalysisRecord.position + icharInChunk];
				if (ch == L'\t') {
					self->glyphIndicies[iglyphInRun] = font.glyphIndexSpace;
					
					const f32 tabStopWidth = font.GetSpaceAdvance() * 4;
					
					f32 currentAdvance = self->width;
					for (u32 iadv = 0; iadv < icharInChunk; iadv++)
						currentAdvance += self->glyphAdvances[self->glyphCount + iadv];
					
					const u32 currentStopIndex = static_cast<u32>(currentAdvance / tabStopWidth);
					const f32 nextStopPosition = (currentStopIndex + 1) * tabStopWidth;
					
					const f32 advance = nextStopPosition - currentAdvance;
					self->glyphAdvances[iglyphInRun] = advance;
				
				} else if (ch == L'\r' || ch == L'\n') {
					self->glyphIndicies[iglyphInRun] = font.glyphIndexSpace;
					self->glyphAdvances[iglyphInRun] = 0.0f;
					
					if (lineEnds) {
						const bool isLineFeed = ch == '\n';
						const bool nextIsLineFeed = ((icharInChunk+1) < scriptAnalysisRecord.length)
							&& (shapingBuffer->stringData[scriptAnalysisRecord.position + icharInChunk + 1] == L'\n');
						
						if (isLineFeed || !nextIsLineFeed)	
							lineEnds->push_back(scriptAnalysisRecord.position + icharInChunk);
					}
				}
			}
		}
		
		// sum up advances		
		for (u32 igly = 0; igly < numGlyphsInChunk; igly++)
			self->width += self->glyphAdvances[self->glyphCount + igly];
		
		// add to glyph count
		self->glyphCount += numGlyphsInChunk;
	}
	
	return true;
}

bool GlyphRun::Shape(std::string_view text, const Font& font) {
	return ShapeInternal(this, text, font, &staticShapingBuffer, nullptr);
}

///////////////////////////////////////////////////////////////////////////////////////////////////
//
// Multithreaded shaping
//
///////////////////////////////////////////////////////////////////////////////////////////////////

struct ShapeBatchThreadData {
	std::atomic_uint64_t currentLine = 0u;
	u64 lineCount = 0u;
	
	const void* lineSource = nullptr;
	std::string_view (*funcGetLine)(const void*, u64);
	
	GlyphRun* output = nullptr;
	const Font* font = nullptr;
	
	std::string_view GetLine(u64 l) const {
		return funcGetLine(lineSource, l);
	}
};

static DWORD ShapeBatchThreadProc(LPVOID param) {
	auto threadData = static_cast<ShapeBatchThreadData*>(param);
	
	ShapingBuffer shapingBuffer {};
	if (!shapingBuffer.Init(128u)) {
		LogError("init shaping buffer failed");
		return FALSE;
	}
	
	bool allOk = true;
	while (true) {
		const u64 line = threadData->currentLine.fetch_add(1, std::memory_order_relaxed);
		if (line >= threadData->lineCount) break;
		
		allOk &= ShapeInternal(&threadData->output[line], threadData->GetLine(line), *threadData->font, &shapingBuffer, nullptr);
	}
	
	return (allOk ? TRUE : FALSE);
}

static bool ShapeBatchInternal(const void* lineSource, std::string_view (*funcGetLine)(const void*, u64), u64 totalLineCount, const Font& font, /*out*/ std::span<GlyphRun> output) {
	ASSERT(output.size() >= totalLineCount);
	
	SYSTEM_INFO systemInfo;
	GetSystemInfo(&systemInfo);
	
	if (totalLineCount >= systemInfo.dwNumberOfProcessors) {
	//if (false) {
		ASSERT(systemInfo.dwNumberOfProcessors > 0);
		
		const u32 threadCount = systemInfo.dwNumberOfProcessors;
		
		auto handles = new HANDLE[threadCount];
		DEFER({
			for (u64 i = 0; i < threadCount; i++)
				CloseHandle(handles[i]);
			delete[] handles; });
					
		ShapeBatchThreadData threadData {
			.currentLine = 0u,
			.lineCount = totalLineCount,
			.lineSource = lineSource,
			.funcGetLine = funcGetLine,
			.output = output.data(),
			.font = &font};
			
		for (u64 i = 0; i < threadCount; i++)
			handles[i] = CreateThread(nullptr, 0, ShapeBatchThreadProc, &threadData, 0, nullptr);
		
		DWORD result = WaitForMultipleObjects(threadCount, handles, TRUE, INFINITE);
		if (result != WAIT_OBJECT_0) {
			LogError("ShapeBatch() failed. WaitForMultipleObjects() failed: %", FWaitRes(result));
			return false;
		}
		
	} else {
		for (u64 i = 0u; i < totalLineCount; i++) {
			const std::string_view line = funcGetLine(lineSource, i);
			output[i].Shape(line, font);
		}
	}
	
	return true;	
}

bool GlyphRun::ShapeBatch(std::span<const std::string_view> batch, const Font& font, /*out*/ std::span<GlyphRun> output) {
	return ShapeBatchInternal(
		batch.data(),
		[] (const void* ls, u64 ln) { return static_cast<const std::string_view*>(ls)[ln]; },
		batch.size(),
		font,
		output);
}

bool GlyphRun::ShapeBatch(const TextBuffer& textBuffer , const Font& font, /*out*/ std::span<GlyphRun> output) {
	return ShapeBatchInternal(
		&textBuffer,
		[] (const void* ls, u64 ln) { return static_cast<const TextBuffer*>(ls)->GetLineAt(ln).GetText(); },
		textBuffer.LineCount(),
		font,
		output);
}

///////////////////////////////////////////////////////////////////////////////////////////////////
//
// GlyphRun functionality
//
///////////////////////////////////////////////////////////////////////////////////////////////////

GlyphRun staticGlyphRun {};

void GlyphRun::Draw(ID2D1RenderTarget* renderTarget, f32 x, f32 y, const Font& font, ID2D1SolidColorBrush* brush) const {
	if (glyphCount == 0u) return;
	
	const DWRITE_GLYPH_RUN glyphRun {
		.fontFace      = font.fontFace,
		.fontEmSize    = font.size,
		.glyphCount    = glyphCount,
		.glyphIndices  = glyphIndicies,
		.glyphAdvances = glyphAdvances.get(),
		.glyphOffsets  = nullptr,
		.isSideways    = FALSE,
		.bidiLevel     = 0};

	renderTarget->DrawGlyphRun(
		D2D_POINT_2F {
			.x = x,
			.y = y + font.baselineOffset},
		&glyphRun,
		brush);
}

void GlyphRun::DrawPartial(ID2D1RenderTarget* renderTarget, f32 x, f32 y, u64 startChar, u64 charCount, const Font& font, ID2D1SolidColorBrush* brush, /*out*/ f32* drawWidth /*= nullptr*/) const {
	if (glyphCount == 0u) return;
	if (charCount == 0u) return;
	
	ASSERT(startChar <  this->charCount);
	ASSERT(startChar <= U32_MAX);
	
	const u32 startGlyphIndex = charMapping[startChar];
	
	if (charCount > this->charCount - startChar - 1u)
		charCount = this->charCount - startChar - 1u;

	const u64 endChar = startChar + charCount;
	const u32 endGlyphIndex = charMapping[endChar];
	ASSERT(startGlyphIndex < endGlyphIndex);
	
	const DWRITE_GLYPH_RUN glyphRun {
		.fontFace      = font.fontFace,
		.fontEmSize    = font.size,
		.glyphCount    = endGlyphIndex - startGlyphIndex + 1u,
		.glyphIndices  = glyphIndicies + startGlyphIndex,
		.glyphAdvances = glyphAdvances.get() + startGlyphIndex,
		.glyphOffsets  = nullptr,
		.isSideways    = FALSE,
		.bidiLevel     = 0};
	
	renderTarget->DrawGlyphRun(
		D2D_POINT_2F {
			.x = x,
			.y = y + font.baselineOffset},
		&glyphRun,
		brush);
	
	if (drawWidth) {	
		for (u32 i = startGlyphIndex; i < endGlyphIndex; i++)
			*drawWidth += glyphAdvances[i];
	}
}

void GlyphRun::DrawCenter(ID2D1RenderTarget* renderTarget, f32 x, f32 y, f32 availableW, const Font& font, ID2D1SolidColorBrush* brush) const {
	Draw(renderTarget,
		(x + availableW / 2.0f) - (width / 2.0f),
		y,
		font,
		brush);

}

bool GlyphRun::ShapeAndDraw(ID2D1RenderTarget* renderTarget, std::string_view text, f32 x, f32 y, const Font& font, ID2D1SolidColorBrush* brush) {
	if (!ShapeInternal(this, text, font, &staticShapingBuffer, nullptr))
		return false;
		
	Draw(renderTarget, x, y, font, brush);
	return true;
}

u64 GlyphRun::HitTest(f32 offset) const {
	
	// find the glyph index first
	f32 totalAdv = .0f;
	u64 glyphIndex = 0u;
	for (; glyphIndex < glyphCount; glyphIndex++) {
		totalAdv += glyphAdvances[glyphIndex];
		if (totalAdv > offset)
			break;
	}
	
	// find the char index of the glyph
	u64 charIndex = 0u;
	for (; charIndex < charCount; charIndex++) {
		const u32 currentGly = charMapping[charIndex];
		if (currentGly >= glyphIndex)
			break;
	}
	
	return charIndex;
}

f32 GlyphRun::MeasureOffset(u64 pos) const {
	ASSERT(pos <= charCount);
	
	if (charCount == 0) return 0.0f;
	
	const u32 glyphIndex = pos < charCount
		? charMapping[pos]
		: glyphCount;
	
	f32 totalAdv = 0.0f;
	for (u32 i = 0; i < glyphIndex; i++)
		totalAdv += glyphAdvances[i];
		
	return totalAdv;
}

void GlyphRun::MeasureOffsetRange(u64 startChar, u64 endChar, /*out*/ f32* offStart, /*out*/ f32* offEnd) const {
	ASSERT(startChar <= endChar);
	
	if (charCount == 0) {
		*offStart = *offEnd = 0.0f;
		return;
	}

	const u32 startGlyphIndex = (startChar < charCount)
		? charMapping[startChar]
		: glyphCount;
	const u32 endGlyphIndex = (endChar < charCount)
		? charMapping[endChar]
		: glyphCount;
	
	f32 totalAdv = 0.0f;
	u32 i = 0;
	for (; i < startGlyphIndex; i++)
		totalAdv += glyphAdvances[i];
	*offStart = totalAdv;
	
	for (; i < endGlyphIndex; i++)
		totalAdv += glyphAdvances[i];
	*offEnd = totalAdv;
}

bool GlyphRunMultiline::Shape(std::string_view text, const Font& font) {
	return ShapeInternal(&glyphRun, text, font, &staticShapingBuffer, &lineEnds);
}

void GlyphRunMultiline::Draw(ID2D1RenderTarget* renderTarget, f32 x, f32 y, const Font& font, ID2D1SolidColorBrush* brush) const {
	
	u32 start = 0u;
	for (u32 end : lineEnds) {
		const u64 amount = (end - start);
		
		glyphRun.DrawPartial(renderTarget, x, y, start, amount, font, brush);
		
		y += font.lineHeight;
		start = end + 1;
	}
	
	glyphRun.DrawPartial(renderTarget, x, y, start, U64_MAX, font, brush);
}

u64 GlyphRunMultiline::LineCount() const {
	return lineEnds.size() + 1u;
}

f32 GlyphRunMultiline::GetWidth() const {
	
	f32 maxWidth = 0.0f;
	
	u32 start = 0u;
	for (u32 end : lineEnds) {
		f32 lineFrom = 0.0f, lineTo = 0.0f;
		glyphRun.MeasureOffsetRange(start, end, &lineFrom, &lineTo);
		const f32 currWidth = (lineTo - lineFrom);
		if (maxWidth < currWidth)
			maxWidth = currWidth;
		start = end + 1;
	}
	
	f32 lineFrom = 0.0f, lineTo = 0.0f;
	glyphRun.MeasureOffsetRange(start, U64_MAX, &lineFrom, &lineTo);
	const f32 currWidth = (lineTo - lineFrom);
	if (maxWidth < currWidth)
		maxWidth = currWidth;
		
	return maxWidth;
}
