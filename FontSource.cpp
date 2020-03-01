#include "Common.h"
#include "FontSource.h"

void FlowFontSource::UseSystem() {
    m_currentFilePaths.clear();

    if (!!m_memoryFontLoader) {
        THROW_IF_FAILED(m_dwriteFactory->UnregisterFontFileLoader(m_memoryFontLoader.get()));
        m_memoryFontLoader.reset();
    }

    wil::com_ptr<IDWriteFactory3> factory3 = m_dwriteFactory.query<IDWriteFactory3>();
    factory3->GetSystemFontSet(&m_fontSet);
}


class MemoryFontInfo {
public:
    MemoryFontInfo() : m_size(0), m_buffer(nullptr) {}

    void Swap(UINT32 size, const uint8_t* buffer) {
        if (m_buffer) {
            delete[] m_buffer;
        }
        m_size = size;
        m_buffer = buffer;
    }
    ~MemoryFontInfo() {
        if (m_buffer) {
            delete[] m_buffer;
        }
    }
    UINT32 m_size;
    const uint8_t* m_buffer;

private:
    MemoryFontInfo(const MemoryFontInfo&) = delete;
};

class FOpenGuard {
public:
    FOpenGuard(const WCHAR* wzPath, const WCHAR* wzMode)
        : filePtr(_wfopen(wzPath, wzMode)) {}

    HRESULT ReadBytes(OUT MemoryFontInfo& result) {
        if (!filePtr) return E_POINTER;

        long filelen;

        fseek(filePtr, 0, SEEK_END);          // Jump to the end of the file
        filelen = ftell(filePtr);             // Get the current byte offset in the file
        rewind(filePtr);                      // Jump back to the beginning of the file

        uint8_t* buffer = new(std::nothrow) uint8_t[filelen]; // Enough memory for the file
        if (buffer == nullptr) { return E_POINTER; }
        fread(buffer, filelen, 1, filePtr); // Read in the entire file

        result.Swap(filelen, buffer);
        return S_OK;
    }

    ~FOpenGuard() {
        if (filePtr != nullptr) {
            fclose(filePtr);
        }
    }

    FILE* filePtr;
};

void FlowFontSource::UseFiles(const std::vector<std::wstring>& filePaths) {
    m_currentFilePaths = filePaths;

    if (!!m_memoryFontLoader) {
        THROW_IF_FAILED(m_dwriteFactory->UnregisterFontFileLoader(m_memoryFontLoader.get()));
        m_memoryFontLoader.reset();
    }

    wil::com_ptr<IDWriteFactory5> factory5 = m_dwriteFactory.query<IDWriteFactory5>();

    if (!m_memoryFontLoader) {
        THROW_IF_FAILED(factory5->CreateInMemoryFontFileLoader(&m_memoryFontLoader));
        THROW_IF_FAILED(factory5->RegisterFontFileLoader(m_memoryFontLoader.get()));
    }

    wil::com_ptr<IDWriteFontSetBuilder1> fontSetBuilder;
    THROW_IF_FAILED(factory5->CreateFontSetBuilder(&fontSetBuilder));

    for (const auto& path : filePaths) {
        std::wostringstream s;
        s << L"Read " << path << L"\n";
        OutputDebugString(s.str().c_str());

        FOpenGuard file(path.c_str(), L"rb");
        MemoryFontInfo mf;
        if(FAILED(file.ReadBytes(mf))) continue;
        wil::com_ptr<IDWriteFontFile> fontFileReference;

        if(FAILED(m_memoryFontLoader->CreateInMemoryFontFileReference(factory5.get(), mf.m_buffer, mf.m_size, nullptr, &fontFileReference))) continue;
        if(FAILED(fontSetBuilder->AddFontFile(fontFileReference.get()))) continue;
    }

    THROW_IF_FAILED(fontSetBuilder->CreateFontSet(&m_fontSet));
}

std::vector<std::wstring> FlowFontSource::GetCurrentFilePaths() {
    return std::vector<std::wstring>(m_currentFilePaths);
}

void FlowFontSource::EnumerateFamilyNames(std::set<std::wstring>& familySet) {
    if (!m_fontSet) return;
    UINT32 fontCount = m_fontSet->GetFontCount();
    for (UINT32 index = 0; index < fontCount; index++) {
        familySet.insert(GetFamilyName(m_fontSet, DWRITE_FONT_PROPERTY_ID_TYPOGRAPHIC_FAMILY_NAME, index));
    }
}

void FlowFontSource::EnumerateStyleNames(const std::wstring& familyName, std::set<std::wstring>& styleNameSet) {
    if (!m_fontSet) return;

    DWRITE_FONT_PROPERTY filters[]{ { DWRITE_FONT_PROPERTY_ID_TYPOGRAPHIC_FAMILY_NAME, familyName.c_str() }  };
    wil::com_ptr<IDWriteFontSet> filteredSet;
    if (FAILED(m_fontSet->GetMatchingFonts(filters, _countof(filters), &filteredSet))) return;

    UINT32 fontCount = filteredSet->GetFontCount();
    for (UINT32 index = 0; index < fontCount; index++) {
        styleNameSet.insert(GetFamilyName(filteredSet, DWRITE_FONT_PROPERTY_ID_TYPOGRAPHIC_FACE_NAME, index));
    }
}

void FlowFontSource::GetDefaultSelector(FontSelector& fs) {
    if (!m_fontSet) return;
    UINT32 fontCount = m_fontSet->GetFontCount();
    if (fontCount <= 0) return;

    // Find a font that is closest to existing
    // If not found, choose the one closest to regular
    int bestScore = -1, bestScoreIndex = 0;
    for (UINT32 index = 0; index < fontCount; index++) {
        int score = 0;
        if (fs.styleName == GetFamilyName(m_fontSet, DWRITE_FONT_PROPERTY_ID_TYPOGRAPHIC_FACE_NAME, index)) score += 100;
        if (L"5" == GetFamilyName(m_fontSet, DWRITE_FONT_PROPERTY_ID_STRETCH, index)) score += 20;
        if (L"400" == GetFamilyName(m_fontSet, DWRITE_FONT_PROPERTY_ID_WEIGHT, index)) score += 10;
        if (L"0" == GetFamilyName(m_fontSet, DWRITE_FONT_PROPERTY_ID_STYLE, index)) score += 1;
        if (score > bestScore) { bestScore = score; bestScoreIndex = index; }
    }

    fs.familyName = GetFamilyName(m_fontSet, DWRITE_FONT_PROPERTY_ID_TYPOGRAPHIC_FAMILY_NAME, bestScoreIndex);
    fs.styleName = GetFamilyName(m_fontSet, DWRITE_FONT_PROPERTY_ID_TYPOGRAPHIC_FACE_NAME, bestScoreIndex);
}

bool FlowFontSource::IsUsingSystemFontSet() {
	return !m_currentFilePaths.size();
}

wil::com_ptr<IDWriteFontSet> FlowFontSource::GetDWriteFontSet() const {
    return wil::com_ptr<IDWriteFontSet>(m_fontSet);
}

std::wstring FlowFontSource::GetFamilyName(wil::com_ptr<IDWriteFontSet> fontSet, DWRITE_FONT_PROPERTY_ID prop, UINT32 index) {
    BOOL dummyExists;
    wil::com_ptr<IDWriteLocalizedStrings> localizedNames;
    THROW_IF_FAILED(fontSet->GetPropertyValues(index, prop, OUT & dummyExists, OUT & localizedNames));

    UINT32 localeIndex = 0;
    BOOL found;
    THROW_IF_FAILED(localizedNames->FindLocaleName(L"en-US", &localeIndex, &found));

    if (!found) return std::wstring();

    UINT32 familyNameLength;
    THROW_IF_FAILED(localizedNames->GetStringLength(localeIndex, &familyNameLength));

    std::vector<WCHAR> familyNameRecv(1 + familyNameLength);
    THROW_IF_FAILED(localizedNames->GetString(localeIndex, &familyNameRecv[0], 1 + familyNameLength));
    return std::wstring(&familyNameRecv[0]);
}
