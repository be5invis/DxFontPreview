#include "Common.h"
#include "TextLayout.h"
#include "TextFormat.h"
#include "FeatureVariationSetting.h"
#include "Render.h"

void TextLayout::SetFont(const FlowFontSource& fontSource, const FontSelector& fs) {
	m_textFormat = TextFormat::Create(m_dwriteFactory, fontSource, fs);
	m_fontState = fs;
}

void TextLayout::SetText(const wchar_t* text, UINT32 textLength) {
    m_text.assign(text, textLength);
}

void TextLayout::GetText(_Out_ const wchar_t** text, _Out_ UINT32* textLength) {
    *text = &m_text[0];
    *textLength = static_cast<UINT32>(m_text.size());
};

void TextLayout::SetSize(float width, float height) {
    m_width = width;
    m_height = height;
}

void TextLayout::Render(wil::com_ptr<IDWriteBitmapRenderTarget> target, wil::com_ptr<IDWriteRenderingParams> renderingParams) {
    if (!m_text.size() || !m_textFormat) return;

    wil::com_ptr<IDWriteTextLayout> layout;
    THROW_IF_FAILED(m_dwriteFactory->CreateTextLayout(m_text.data(), m_text.size(), m_textFormat.get(), m_width, m_height, &layout));
    ApplylFeatures(layout);
    wil::com_ptr<IDWriteTextRenderer1> renderer = CreateTextRenderer(m_dwriteFactory, target, renderingParams);
    layout->Draw(nullptr, renderer.get(), 0, 0);
}

void TextLayout::ApplylFeatures(wil::com_ptr<IDWriteTextLayout> layout) {
    FeatureSettings featureSettings = m_fontState.userFeaturesEnabled
        ? ParseFeatures(m_fontState.userFeatureSettings)
        : FeatureSettings();
    wil::com_ptr<IDWriteTypography> typography;
    THROW_IF_FAILED(m_dwriteFactory->CreateTypography(&typography));
    for (const auto& feature : featureSettings) {
        THROW_IF_FAILED(typography->AddFontFeature(feature));
    };
    THROW_IF_FAILED(layout->SetTypography(typography.get(), { 0, static_cast<UINT32>(m_text.size()) }));
}
