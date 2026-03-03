#include "glyph-run-dwrite.hh"
#include "font.hh"
#include "factories.hh"
#include "util/logging.hh"

#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <dwrite_1.h>
#include <d2d1.h>

//#################################################################################################
//
// Shaping
//
//#################################################################################################

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
struct ScriptAnalysisRecord {
	u32 position = 0u;
	u32 length = 0u;
	const DWRITE_SCRIPT_ANALYSIS* script = nullptr;
};

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
struct ShapingBuffer {
	
	//------------------------------------------
	// static
	//------------------------------------------	
	
	static constexpr u32 STATIC_BUFFER_SIZE = 256u;
	
	// size prediction of scriptAnalysisRecords = text length / this factor
	static constexpr u32 SCRIPT_ANALYSIS_SIZE_FACTOR = 8u;
	
	static WCHAR                          staticStringData[STATIC_BUFFER_SIZE];
	static u8                             staticStringUtfMapping[STATIC_BUFFER_SIZE];
	
	static ScriptAnalysisRecord staticScriptAnalysisRecords[STATIC_BUFFER_SIZE / SCRIPT_ANALYSIS_SIZE_FACTOR];
	
	static UINT16                         staticClusterMap[STATIC_BUFFER_SIZE];
	static DWRITE_SHAPING_TEXT_PROPERTIES staticTextProperties[STATIC_BUFFER_SIZE];
	
	static DWRITE_GLYPH_OFFSET             staticGlyphOffsets[STATIC_BUFFER_SIZE];
	static DWRITE_SHAPING_GLYPH_PROPERTIES staticGlyphProperties[STATIC_BUFFER_SIZE];
	
	static IDWriteTextAnalyzer* staticTextAnalyzer;
	
	//------------------------------------------
	// data
	//------------------------------------------	
	
	// string properties
	WCHAR* stringData         = nullptr;
	u8*    stringUtfMapping   = nullptr;
	u32    stringCapacity     = 0u;
	bool   stringDataIsStatic = false;
	
	// script records
	ScriptAnalysisRecord* scriptAnalysisRecords         = nullptr;
	u32                   scriptAnalysisRecordCapacity  = 0u;
	bool                  scriptAnalysisRecordsIsStatic = false;
	
	// shaping text properties
	UINT16*                         clusterMap                     = nullptr;
	DWRITE_SHAPING_TEXT_PROPERTIES* textProperties                 = nullptr;
	u32                             shapingTextDataCapacity  = 0u;
	bool                            shapingTextDataIsStatic = false;
	
	// shaping glyph properties
	DWRITE_GLYPH_OFFSET*             glyphOffsets                    = nullptr;
	DWRITE_SHAPING_GLYPH_PROPERTIES* glyphProperties                 = nullptr;
	u32                              shapingGlyphDataCapacity  = 0u;
	bool                             shapingGlyphDataIsStatic = false;
	
	// text analyzer
	IDWriteTextAnalyzer* textAnalyzer = nullptr;
	
	
	//------------------------------------------
	// functions
	//------------------------------------------	
	
	void InitStatic() {
		stringData         = staticStringData;
		stringUtfMapping   = staticStringUtfMapping;
		stringCapacity     = STATIC_BUFFER_SIZE;
		stringDataIsStatic = true;
		
		clusterMap              = staticClusterMap;
		textProperties          = staticTextProperties;
		shapingTextDataCapacity = STATIC_BUFFER_SIZE;
		shapingTextDataIsStatic = true;
	
		scriptAnalysisRecords         = staticScriptAnalysisRecords;
		scriptAnalysisRecordCapacity  = STATIC_BUFFER_SIZE / SCRIPT_ANALYSIS_SIZE_FACTOR;
		scriptAnalysisRecordsIsStatic = true;

		glyphOffsets             = staticGlyphOffsets;
		glyphProperties          = staticGlyphProperties;
		shapingGlyphDataCapacity = STATIC_BUFFER_SIZE;
		shapingGlyphDataIsStatic = true;
		
		textAnalyzer = staticTextAnalyzer;
	}
	
	void InitDynamic(u32 initialSize, IDWriteTextAnalyzer* textAnalyz) {
		stringData         = new WCHAR[initialSize];
		stringUtfMapping   = new u8[initialSize];
		stringCapacity     = initialSize;
		stringDataIsStatic = false;				
		
		clusterMap              = new UINT16[initialSize];
		textProperties          = new DWRITE_SHAPING_TEXT_PROPERTIES[initialSize];
		shapingTextDataCapacity = initialSize;
		shapingTextDataIsStatic = false;
	
		scriptAnalysisRecords         = new ScriptAnalysisRecord[initialSize / SCRIPT_ANALYSIS_SIZE_FACTOR];
		scriptAnalysisRecordCapacity  = initialSize / SCRIPT_ANALYSIS_SIZE_FACTOR;
		scriptAnalysisRecordsIsStatic = false;

		glyphOffsets             = new DWRITE_GLYPH_OFFSET[initialSize];
		glyphProperties          = new DWRITE_SHAPING_GLYPH_PROPERTIES[initialSize];
		shapingGlyphDataCapacity = initialSize;
		shapingGlyphDataIsStatic = false;
		
		textAnalyzer = textAnalyz;
	}
	
	void PrepareStringCapacity(u32 capacity) {
		if (capacity <= stringCapacity) {
			memset(stringData,       0, sizeof(*stringData)       * stringCapacity);
			memset(stringUtfMapping, 0, sizeof(*stringUtfMapping) * stringCapacity);
		} else {
			if (!stringDataIsStatic) {
				delete[] stringData;
				delete[] stringUtfMapping;
			}
			stringData = new WCHAR[capacity];
			stringUtfMapping = new u8[capacity];
			stringCapacity = capacity;
			stringDataIsStatic = false;
		}
	}
	
	void ClearScriptAnalysisRecord() {
		memset(scriptAnalysisRecords, 0, sizeof(*scriptAnalysisRecords) * scriptAnalysisRecordCapacity);
	}
	
	void ReallocateScriptAnalysisRecords(u32 newCapacity) {
		if (newCapacity <= scriptAnalysisRecordCapacity) return;
	
		auto newScriptAnalysisRecords = new ScriptAnalysisRecord[newCapacity];
		memcpy(newScriptAnalysisRecords, scriptAnalysisRecords, sizeof(*scriptAnalysisRecords) * scriptAnalysisRecordCapacity);
			
		if (!scriptAnalysisRecordsIsStatic) {
			delete[] scriptAnalysisRecords;
		}
		
		scriptAnalysisRecords = newScriptAnalysisRecords;
		scriptAnalysisRecordCapacity = newCapacity;
		scriptAnalysisRecordsIsStatic = false;
	}
	
	void PrepareShapingTextData(u32 capacity) {
		if (capacity <= shapingTextDataCapacity) return;
			
		if (!shapingTextDataIsStatic) {
			delete[] clusterMap;
			delete[] textProperties;
		}
		clusterMap = new UINT16[capacity];
		textProperties = new DWRITE_SHAPING_TEXT_PROPERTIES[capacity];
		shapingTextDataCapacity = capacity;
		shapingTextDataIsStatic = false;
	}
		
	void ReallocateShapingGlyphData(u32 newCapacity) {
		if (newCapacity <= shapingGlyphDataCapacity) return;
		
		auto newGlyphOffsets = new DWRITE_GLYPH_OFFSET[newCapacity];
		auto newGlyphProperties  = new DWRITE_SHAPING_GLYPH_PROPERTIES[newCapacity];
		
		memcpy(newGlyphOffsets, glyphOffsets, sizeof(*glyphOffsets) * shapingGlyphDataCapacity);
		memcpy(newGlyphProperties, glyphProperties, sizeof(*glyphProperties) * shapingGlyphDataCapacity);
		
		if (!shapingGlyphDataIsStatic) {
			delete[] glyphOffsets;
			delete[] glyphProperties;
		}
		
		glyphOffsets = newGlyphOffsets;
		glyphProperties = newGlyphProperties;
		
		shapingGlyphDataCapacity = newCapacity;
		shapingGlyphDataIsStatic = false;
	}

	void ClearShapingData() {
		memset(clusterMap,      0, sizeof(*clusterMap)      * shapingTextDataCapacity);
		memset(textProperties,  0, sizeof(*textProperties)  * shapingTextDataCapacity);
		memset(glyphOffsets,    0, sizeof(*glyphOffsets)    * shapingGlyphDataCapacity);
		memset(glyphProperties, 0, sizeof(*glyphProperties) * shapingGlyphDataCapacity);
	}
	
	~ShapingBuffer() noexcept {
		if (!stringDataIsStatic) {
			delete[] stringData;
			delete[] stringUtfMapping;
		}
		
		if (!scriptAnalysisRecordsIsStatic) {
			delete[] scriptAnalysisRecords;
		}
		
		if (!shapingTextDataIsStatic) {
			delete[] clusterMap;
			delete[] textProperties;
		}
		
		if (!shapingGlyphDataIsStatic) {
			delete[] glyphOffsets;
			delete[] glyphProperties;
		}
	}
};

WCHAR ShapingBuffer::staticStringData[ShapingBuffer::STATIC_BUFFER_SIZE];
u8 ShapingBuffer::staticStringUtfMapping[ShapingBuffer::STATIC_BUFFER_SIZE];
ScriptAnalysisRecord ShapingBuffer::staticScriptAnalysisRecords[ShapingBuffer::STATIC_BUFFER_SIZE / ShapingBuffer::SCRIPT_ANALYSIS_SIZE_FACTOR];
DWRITE_SHAPING_TEXT_PROPERTIES ShapingBuffer::staticTextProperties[ShapingBuffer::STATIC_BUFFER_SIZE];
UINT16 ShapingBuffer::staticClusterMap[ShapingBuffer::STATIC_BUFFER_SIZE];
DWRITE_GLYPH_OFFSET ShapingBuffer::staticGlyphOffsets[ShapingBuffer::STATIC_BUFFER_SIZE];
DWRITE_SHAPING_GLYPH_PROPERTIES ShapingBuffer::staticGlyphProperties[ShapingBuffer::STATIC_BUFFER_SIZE];
IDWriteTextAnalyzer* ShapingBuffer::staticTextAnalyzer = nullptr;

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
bool InitStaticTextAnalyzer() {
	if (HRESULT hr = dwFactory->CreateTextAnalyzer(&ShapingBuffer::staticTextAnalyzer); hr != S_OK) {
		LogError("CreateTextAnalyzer() failed. HRESULT: %", FHr(hr));
		return false;
	}
	
	return true;
}

void ShutdownStaticTextAnalyzer() {
	if (ShapingBuffer::staticTextAnalyzer)
		ShapingBuffer::staticTextAnalyzer->Release();
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
		
	const ScriptAnalysisRecord* begin() const { return shapingBuffer->scriptAnalysisRecords; }
	const ScriptAnalysisRecord* end()   const { return shapingBuffer->scriptAnalysisRecords + recordCount; }
	
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
		*textString = shapingBuffer->stringData + textPosition;
		*remainingLength = length - textPosition;
		return S_OK;
	}
	        
	virtual HRESULT GetTextBeforePosition(
			UINT32 textPosition,
			WCHAR const** textString,
			UINT32* textLength) noexcept override {
		ASSERT(textPosition < length);
		*textString = shapingBuffer->stringData;
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
	
	for (u64 i = 0; i < utf8.size(); i++) {
    	u32 cp = 0;
    	u8 seqLen = 0u;
    				
		// Decode UTF-8
    	
    	// 1 byte (0-127)
    	if (utf8[i] <= 0x7F) {
        	cp = utf8[i];
        	seqLen = 1u;
    	
    	// 2 bytes
    	} else if ((utf8[i] & 0xE0) == 0xC0 && i < utf8.size()-1) {
        	cp  = (utf8[i  ] & 0x1F) << 6;
        	cp |= (utf8[i+1] & 0x3F);
        	seqLen = 2u;
    	
    	// 3 bytes
    	} else if ((utf8[i+1] & 0xF0) == 0xE0 && i < utf8.size()-2) {
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
	}
	
	return size;
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
static u64 CalculateGlyphArraySize(u32 capacity) {
	const u64 capacityForIndicies = static_cast<u64>((capacity / 2.0f) + 0.5f);
	return capacity + capacityForIndicies;
}

static void ReallocateGlyphs(GlyphRun_DWrite* self, u32 newCapacity) {
	if (newCapacity <= self->glyphCapacity) return;
	
	const u64 oldArraySize = CalculateGlyphArraySize(self->glyphCapacity);
	const u64 newArraySize = CalculateGlyphArraySize(newCapacity);
	
	f32* newMemory = new f32[newArraySize];
	memcpy(self->glyphAdvances.get(), newMemory, oldArraySize * sizeof(f32));
	
	self->glyphAdvances.reset(newMemory);
	self->glyphIndicies = reinterpret_cast<u16*>(self->glyphAdvances.get() + newCapacity);
	self->glyphCapacity = newCapacity;
}

static void ClearGlyphs(GlyphRun_DWrite* self) {
	const u64 size = CalculateGlyphArraySize(self->glyphCapacity);
	memset(self->glyphAdvances.get(), 0, size * sizeof(f32));
}

// @IMPROVE would be better to reuse old memory but we need to store the 
// capacity somewhere. One thing we could do is make the char mapping
// "U32_MAX-terminated" and store the length this way.
static void PrepareMapping(GlyphRun_DWrite* self, u32 count) {
	if (self->charCount== count) {
		memset(self->charMapping.get(), 0, count * sizeof(*self->charMapping.get()));
	} else {
		self->charMapping.reset(new u32[count]);
		self->charCount = count;
	}
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
static bool ShapeInternal(GlyphRun_DWrite* self, std::string_view text, const Font& font, ShapingBuffer* shapingBuffer) {
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
	
	shapingBuffer->PrepareShapingTextData(utf16Length);
	
	u32 charMappingWritten = 0u;
	for (const ScriptAnalysisRecord& scriptAnalysisRecord : textAnalysisSink) {		
		shapingBuffer->ClearShapingData();
		
		u32 numGlyphsInChunk = 0u;
		while (true) {
			const u32 currentCapacity = std::min(shapingBuffer->shapingGlyphDataCapacity, self->glyphCapacity - self->glyphCount);
			
			HRESULT hr = shapingBuffer->textAnalyzer->GetGlyphs(
  				/* textString */          shapingBuffer->stringData + scriptAnalysisRecord.position,
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
  				/* maxGlyphCount */       static_cast<u32>(currentCapacity),
  				/* clusterMap */          shapingBuffer->clusterMap,
  				/* textProps */           shapingBuffer->textProperties,
  				/* glyphIndices */        self->glyphIndicies + self->glyphCount,
  				/* glyphProps */          shapingBuffer->glyphProperties,
  				/* actualGlyphCount */    &numGlyphsInChunk);
	  		
	  		if (hr == S_OK) {
		  		break;
	  			
	  		} else if (hr == HRESULT_FROM_WIN32(ERROR_INSUFFICIENT_BUFFER)) {
				const u32 newCapacity = static_cast<u32>(currentCapacity * 1.5f);
				if (newCapacity > U32_MAX) {
					LogFatal("GetGlyphs() failed: exceeding maximum capacity. Aborting...");
					return false;
				}
				
				//LogDetail("Shaping buffer too small. Retrying with %", newCapacity);
				ReallocateGlyphs(self, newCapacity);
				shapingBuffer->ReallocateShapingGlyphData(newCapacity);
				continue; // try again
	  		
	  		} else {
		  		LogError("GetGlpyhs() failed. HRESULT: %", FHr(hr));
		  		return false;
	  		}
  		}
	  		
  		HRESULT hr = shapingBuffer->textAnalyzer->GetGlyphPlacements(
			/* textString */          shapingBuffer->stringData + scriptAnalysisRecord.position,
			/* clusterMap */          shapingBuffer->clusterMap,
			/* textProps */           shapingBuffer->textProperties,
			/* textLength */          scriptAnalysisRecord.length,
			/* glyphIndices */        self->glyphIndicies + self->glyphCount,
			/* glyphProps */          shapingBuffer->glyphProperties,
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
			/* glyphOffset */         shapingBuffer->glyphOffsets);
			
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
				if (ch == '\t') {
					self->glyphIndicies[iglyphInRun] = font.glyphIndexSpace;
					
					const f32 tabStopWidth = font.GetSpaceAdvance() * 4;
					
					f32 currentAdvance = self->width;
					for (u32 iadv = 0; iadv < iglyphInRun; iadv++)
						currentAdvance += self->glyphAdvances[self->glyphCount + iadv];
					
					const u32 currentStopIndex = static_cast<u32>(currentAdvance / tabStopWidth);
					const f32 nextStopPosition = (currentStopIndex + 1) * tabStopWidth;
					
					const f32 advance = nextStopPosition - self->width;
					self->glyphAdvances[iglyphInRun] = advance;
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

bool GlyphRun_DWrite::Shape(std::string_view text, const Font& font) {
	
	ShapingBuffer shapingBuffer {};
	shapingBuffer.InitStatic();
	
	return ShapeInternal(this, text, font, &shapingBuffer);	
}

struct ShapeBatchThreadData {
	const std::string_view* lines = nullptr;
	u64 lineCount = 0u;
	GlyphRun_DWrite* output = nullptr;
	const Font* font = nullptr;
};

static DWORD ShapeBatchThreadProc(LPVOID param) {
	ShapeBatchThreadData* threadData = static_cast<ShapeBatchThreadData*>(param);
	
	IDWriteTextAnalyzer* textAnalyzer = nullptr;
	if (HRESULT hr = dwFactory->CreateTextAnalyzer(&textAnalyzer); hr != S_OK) {
		LogError("CreateTextAnalyzer() failed. HRESULT: %", FHr(hr));
		return -1;
	}
	
	ShapingBuffer shapingBuffer {};
	shapingBuffer.InitDynamic(128u, textAnalyzer);
	
	bool allOk = true;
	for (u64 i = 0; i < threadData->lineCount; i++)
		allOk &= ShapeInternal(threadData->output + i, threadData->lines[i], *threadData->font, &shapingBuffer);

	textAnalyzer->Release();	
	return allOk;
}

bool GlyphRun_DWrite::ShapeBatch(std::span<const std::string_view> batch, const Font& font, /*out*/ std::vector<GlyphRun_DWrite>* runs) {
	
	SYSTEM_INFO systemInfo;
	GetSystemInfo(&systemInfo);
	
	if (batch.size() >= systemInfo.dwNumberOfProcessors) {
		ASSERT(systemInfo.dwNumberOfProcessors > 0);
		
		const u32 threadCount = 7u;//std::max<u32>(systemInfo.dwNumberOfProcessors - 1u, 2u);
		const u64 linesPerThread = static_cast<u64>(batch.size() / threadCount);
		
		auto threadData = new ShapeBatchThreadData[threadCount];
		auto handles = new HANDLE[threadCount];
		DEFER({
			for (u64 i = 0; i < threadCount; i++) CloseHandle(handles[i]);
			delete[] handles;
			delete[] threadData; });
		
		runs->resize(batch.size());
		
		const std::string_view* lineIter = batch.data();
		GlyphRun_DWrite* runIter = runs->data();
		
		for (u64 i = 0; i < threadCount - 1; i++) {
			threadData[i] = ShapeBatchThreadData {
				.lines = lineIter,
				.lineCount = linesPerThread,
				.output = runIter,
				.font = &font};
			handles[i] = CreateThread(nullptr, systemInfo.dwPageSize, ShapeBatchThreadProc, &threadData[i], STACK_SIZE_PARAM_IS_A_RESERVATION, nullptr);
			
			if (handles[i] == NULL)
				LogWarning("ShapeBatch() failed to start thread #%. Last Error: %", i, FLastErr(GetLastError()));
		
			lineIter += linesPerThread;		
			runIter  += linesPerThread;		
		}
		
		threadData[threadCount-1] = ShapeBatchThreadData {
			.lines = lineIter,
			.lineCount = static_cast<u64>(batch.data() + batch.size() - lineIter),
			.output = runIter,
			.font = &font};
		handles[threadCount-1] = CreateThread(nullptr, systemInfo.dwPageSize, ShapeBatchThreadProc, &threadData[threadCount-1], STACK_SIZE_PARAM_IS_A_RESERVATION, nullptr);
		
		// sanity check
		{
			u64 totalLines = 0;
			for (u64 i = 0; i < threadCount; i++)
				totalLines += threadData[i].lineCount;
			ASSERT(totalLines == batch.size());
		}
		
		DWORD result = WaitForMultipleObjects(threadCount, handles, TRUE, INFINITE);
		if (result != WAIT_OBJECT_0) {
			LogError("ShapeBatch() failed. WaitForMultipleObjects() failed: %", FWaitRes(result));
			return false;
		}
		
	} else {
		runs->resize(batch.size());
		for (u64 i = 0u; i < batch.size(); i++)
			runs->at(i).Shape(batch[i], font);
	}
	
	return true;
}

//#################################################################################################
//
// GlyphRun functionality
//
//#################################################################################################

void GlyphRun_DWrite::Draw(ID2D1RenderTarget* renderTarget, f32 x, f32 y, const Font& font, ID2D1SolidColorBrush* brush) const {
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











