#include "Common.h"
#include "FontSelector.h"
#include "TextFormat.h"

std::wstring GetFamilyName(wil::com_ptr<IDWriteFontFace3> fontFace) {
    wil::com_ptr<IDWriteLocalizedStrings> localizedNames;
    THROW_IF_FAILED(fontFace->GetFamilyNames(&localizedNames));

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

wil::com_ptr<IDWriteFontSet> GetFilteredFontSet(const FlowFontSource& fontSource, const FontSelector& fs) {
    wil::com_ptr<IDWriteFontSet> fontSet = fontSource.GetDWriteFontSet();
    if (!fontSet) return nullptr;

    wil::com_ptr<IDWriteFontSet> filteredFontSet;

    DWRITE_FONT_PROPERTY filters[]{
        { DWRITE_FONT_PROPERTY_ID_TYPOGRAPHIC_FAMILY_NAME, fs.familyName.c_str() },
        { DWRITE_FONT_PROPERTY_ID_TYPOGRAPHIC_FACE_NAME, fs.styleName.c_str() },
    };

    HRESULT hr = fontSet->GetMatchingFonts(filters, _countof(filters), &filteredFontSet);
    if (FAILED(hr) || filteredFontSet->GetFontCount() == 0) return nullptr;

    return filteredFontSet;
}

wil::com_ptr<IDWriteFontFace3> SelectFirstFont(wil::com_ptr<IDWriteFontSet> fontSet) {
    wil::com_ptr<IDWriteFontFaceReference> fontFaceRef;
    THROW_IF_FAILED(fontSet->GetFontFaceReference(0, &fontFaceRef));

    wil::com_ptr<IDWriteFontFace3> fontFace3;
    HRESULT hr = fontFaceRef->CreateFontFace(&fontFace3);
    if (FAILED(hr)) return nullptr;

    return fontFace3;
}

void DisableFontFallback(wil::com_ptr<IDWriteFactory> factory, wil::com_ptr<IDWriteTextFormat3> fmt3) {
    auto factory2 = factory.try_query<IDWriteFactory2>();
    if (factory2) {
        wil::com_ptr<IDWriteFontFallbackBuilder> fallbackBuilder;
        THROW_IF_FAILED(factory2->CreateFontFallbackBuilder(&fallbackBuilder));
        wil::com_ptr<IDWriteFontFallback> fallback;
        THROW_IF_FAILED(fallbackBuilder->CreateFontFallback(&fallback));
        THROW_IF_FAILED(fmt3->SetFontFallback(fallback.get()));
    }
}

void ApplyVariation(wil::com_ptr<IDWriteFontFace3> fontFace3, wil::com_ptr<IDWriteTextFormat3> fmt3, const FontSelector& fs) {
    if (!fs.userVariationEnabled) return;
    auto fontFace5 = fontFace3.try_query<IDWriteFontFace5>();
    if (!fontFace5) return;
    UINT32 axesCount = fontFace5->GetFontAxisValueCount();
    std::vector<DWRITE_FONT_AXIS_VALUE> axisValues(axesCount);
    THROW_IF_FAILED(fontFace5->GetFontAxisValues(axisValues.data(), axesCount));
    THROW_IF_FAILED(fmt3->SetFontAxisValues(axisValues.data(), axisValues.size()));
}


wil::com_ptr<IDWriteTextFormat3> TextFormat::Create(wil::com_ptr<IDWriteFactory> factory, const FlowFontSource& fontSource, const FontSelector& fs) {
    wil::com_ptr<IDWriteFactory3> factory3 = factory.query<IDWriteFactory3>();

    wil::com_ptr<IDWriteFontSet> filteredFontSet = GetFilteredFontSet(fontSource, fs);
    if (!filteredFontSet) return nullptr;

    wil::com_ptr<IDWriteFontFace3> fontFace3 = SelectFirstFont(filteredFontSet);
    if (!fontFace3) return nullptr;

    std::wstring familyName = GetFamilyName(fontFace3);
    DWRITE_FONT_WEIGHT weight = fontFace3->GetWeight();
    DWRITE_FONT_STYLE style = fontFace3->GetStyle();
    DWRITE_FONT_STRETCH stretch = fontFace3->GetStretch();

    wil::com_ptr<IDWriteFontCollection1> collection;
    THROW_IF_FAILED(factory3->CreateFontCollectionFromFontSet(filteredFontSet.get(), &collection));

    wil::com_ptr<IDWriteTextFormat> fmt;
    THROW_IF_FAILED(factory3->CreateTextFormat(familyName.data(), collection.get(), weight, style, stretch, fs.fontEmSize, fs.localeName.data(), &fmt));

    wil::com_ptr<IDWriteTextFormat3> fmt3 = fmt.query<IDWriteTextFormat3>();

    ApplyVariation(fontFace3, fmt3, fs);
    if (!fs.doFontFallback) DisableFontFallback(factory, fmt3);

    THROW_IF_FAILED(fmt3->SetReadingDirection(g_dwriteReadingDirectionValues[fs.readingDirection]));
    THROW_IF_FAILED(fmt3->SetFlowDirection(g_dwriteFlowDirectionValues[fs.readingDirection]));
    if (fs.doJustify) THROW_IF_FAILED(fmt3->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_JUSTIFIED));

    return fmt3;
}
