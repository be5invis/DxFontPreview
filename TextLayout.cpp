#include "Common.h"
#include "TextLayout.h"
#include "TextFormat.h"
#include "Render.h"
#include "DocParser.h"

void TextLayout::SetFont(const FlowFontSource& fontSource, const FontSelector& fs) {
	m_textFormat = TextFormat::Create(m_dwriteFactory, fontSource, fs);
    m_parsedText = ParseInputDoc(m_text, fs);
    m_fontState = fs;
    UpdateLayout();
}

void TextLayout::SetText(const wchar_t* text, UINT32 textLength) {
    m_text.assign(text, textLength);
    m_parsedText = ParseInputDoc(m_text, m_fontState);
    UpdateLayout();
}

void TextLayout::GetText(_Out_ const wchar_t** text, _Out_ UINT32* textLength) {
    *text = &m_text[0];
    *textLength = static_cast<UINT32>(m_text.size());
};

void TextLayout::SetSize(float width, float height) {
    m_width = std::max(PADDING * 2.0, width - PADDING * 2.0);
    m_height = std::max(PADDING * 2.0, height - PADDING * 2.0);
    if (m_layout) {
        THROW_IF_FAILED(m_layout->SetMaxWidth(m_width));
        THROW_IF_FAILED(m_layout->SetMaxHeight(m_height));
    } else {
        UpdateLayout();
    }
}

void TextLayout::Render(wil::com_ptr<IDWriteBitmapRenderTarget> target, wil::com_ptr<IDWriteRenderingParams> renderingParams, RenderMarkings options) {
    if (!m_layout) return;

    if (options) {
        wil::com_ptr<IDWriteTextRenderer1> markingsRenderer = CreateMarkingsRenderer(m_dwriteFactory, target, renderingParams, options);
        m_layout->Draw(nullptr, markingsRenderer.get(), 0, 0);
    }

    wil::com_ptr<IDWriteTextRenderer1> textRenderer = CreateTextRenderer(m_dwriteFactory, target, renderingParams);
    m_layout->Draw(nullptr, textRenderer.get(), 0, 0);
}

void TextLayout::UpdateLayout() {
    if (!m_textFormat) return;

    THROW_IF_FAILED(m_dwriteFactory->CreateTextLayout(m_parsedText.text.data(), m_parsedText.text.size(), m_textFormat.get(), m_width, m_height, &m_layout));
    (void) m_layout->SetLocaleName(m_fontState.localeName.data(), { 0, static_cast<UINT32>(m_parsedText.text.size()) });
    for (const auto& runStyle : m_parsedText.styles) {
        ApplyFeatures(m_layout, runStyle);
    }
    if (auto layout4 = m_layout.try_query<IDWriteTextLayout4>()) {
        UINT32 axesCount = m_textFormat->GetFontAxisValueCount();
        std::vector<DWRITE_FONT_AXIS_VALUE> defaultVariation(axesCount);
        THROW_IF_FAILED(m_textFormat->GetFontAxisValues(defaultVariation.data(), axesCount));
        for (const auto& runStyle : m_parsedText.styles) {
            ApplyVariation(defaultVariation, layout4, runStyle);
        }
    }
}

void TextLayout::ApplyFeatures(wil::com_ptr<IDWriteTextLayout> layout, const RunStyle& rg) {
    if (!m_fontState.userFeaturesEnabled) return;
    wil::com_ptr<IDWriteTypography> typography;
    THROW_IF_FAILED(m_dwriteFactory->CreateTypography(&typography));
    for (const auto& st : rg.style) {
        if (st.type != RunStyleType::Feature) continue;
        THROW_IF_FAILED(typography->AddFontFeature({ DWRITE_FONT_FEATURE_TAG(st.tag), static_cast<UINT32>(st.value) }));
    };
    THROW_IF_FAILED(layout->SetTypography(typography.get(), { rg.cpBegin, rg.cpEnd }));
}

void TextLayout::ApplyVariation(const std::vector<DWRITE_FONT_AXIS_VALUE>& defaultVariation, wil::com_ptr<IDWriteTextLayout4> layout, const RunStyle& rg) {
    if (!m_fontState.userVariationEnabled) return;
    std::vector<DWRITE_FONT_AXIS_VALUE> axisValues(defaultVariation);
    for (auto& st : rg.style) {
        if (st.type != RunStyleType::Variation) continue;
        bool found = false;
        for (auto& item : axisValues) {
            if (item.axisTag != st.tag) continue;
            item.value = st.value;
            found = true;
        }
        if (!found) {
            axisValues.push_back({ DWRITE_FONT_AXIS_TAG(st.tag), static_cast<float>(st.value) });
        }
    }
    THROW_IF_FAILED(layout->SetFontAxisValues(axisValues.data(), axisValues.size(), { rg.cpBegin, rg.cpEnd }));
}
