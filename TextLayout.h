#pragma once

#include "FontSource.h"
#include "FontSelector.h"
#include "DocParser.h"
#include "Render.h"

class TextLayout {
public:
    TextLayout(wil::com_ptr<IDWriteFactory> dwriteFactory)
        : m_dwriteFactory(dwriteFactory), m_width(300), m_height(300) {}

    void SetFont(const FlowFontSource& fontSource, const FontSelector& fs);

    void SetText(const wchar_t* text, UINT32 textLength);
    void GetText(_Out_ const wchar_t** text, _Out_ UINT32* textLength);
    void SetSize(float width, float height);

    void Render(wil::com_ptr<IDWriteBitmapRenderTarget> target, wil::com_ptr<IDWriteRenderingParams> renderingParams, RenderMarkings options);

private:
    void UpdateLayout();
    void ApplylFeatures(wil::com_ptr<IDWriteTextLayout> layout, const RunStyle& rg);
    void ApplyVariation(const std::vector<DWRITE_FONT_AXIS_VALUE>& defaultVariation, wil::com_ptr<IDWriteTextLayout4> layout, const RunStyle& rg);

    wil::com_ptr<IDWriteFactory> m_dwriteFactory;
    FontSelector m_fontState;
    std::wstring m_text;
    ParsedDocument m_parsedText;
    wil::com_ptr<IDWriteTextFormat3> m_textFormat;
    wil::com_ptr<IDWriteTextLayout> m_layout;

    float m_width;
    float m_height;
};