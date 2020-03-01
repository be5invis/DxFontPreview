#pragma once

#include "FontSelector.h"

class FlowFontSource {
public:
    FlowFontSource(wil::com_ptr<IDWriteFactory> dwriteFactory)
        : m_dwriteFactory(dwriteFactory),
        m_memoryFontLoader() {
        UseSystem();
    }

    void UseSystem();
    void UseFiles(const std::vector<std::wstring> & filePaths);
    std::vector<std::wstring> GetCurrentFilePaths();
    void EnumerateFamilyNames(std::set<std::wstring>& familySet);
    void EnumerateStyleNames(const std::wstring& familyName, std::set<std::wstring>& styleNameSet);
    void GetDefaultSelector(FontSelector& fs);
    bool IsUsingSystemFontSet();

    wil::com_ptr<IDWriteFontSet> GetDWriteFontSet() const;

protected:
    std::wstring GetFamilyName(wil::com_ptr<IDWriteFontSet> fontSet, DWRITE_FONT_PROPERTY_ID prop, UINT32 index);

    std::vector<std::wstring> m_currentFilePaths;
    wil::com_ptr<IDWriteInMemoryFontFileLoader> m_memoryFontLoader;
    wil::com_ptr<IDWriteFactory> m_dwriteFactory;
    wil::com_ptr<IDWriteFontSet> m_fontSet;
};
