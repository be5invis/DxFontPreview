#pragma once
#pragma once

#include "FontSource.h"
#include "FontSelector.h"
#include "FeatureVariationSetting.h"

class TextLayout {
public:
    TextLayout(wil::com_ptr<IDWriteFactory> dwriteFactory)
        : m_dwriteFactory(dwriteFactory), m_width(300), m_height(300) {}

    void SetFont(const FlowFontSource& fontSource, const FontSelector& fs);

    void SetText(const wchar_t* text, UINT32 textLength);
    void GetText(_Out_ const wchar_t** text, _Out_ UINT32* textLength);
    void SetSize(float width, float height);

    void Render(wil::com_ptr<IDWriteBitmapRenderTarget> target, wil::com_ptr<IDWriteRenderingParams> renderingParams);

private:
    void ApplylFeatures(wil::com_ptr<IDWriteTextLayout> layout, std::wstring& text);

    wil::com_ptr<IDWriteFactory> m_dwriteFactory;
    FontSelector m_fontState;
    std::wstring m_text;
    std::wstring m_parsedText;
    wil::com_ptr<IDWriteTextFormat3> m_textFormat;

    float m_width;
    float m_height;

};