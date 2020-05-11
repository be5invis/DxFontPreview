#pragma once

constexpr uint32_t PADDING = 8;
constexpr float GB_PADDING_H = 2;
constexpr float GB_PADDING_V = 8;

enum RenderMarkings {
    None = 0,
    Advance = 1,
    Positioning = 2
};

ENABLE_BITMASK_OPERATORS(RenderMarkings);

wil::com_ptr<IDWriteTextRenderer1> CreateTextRenderer(
    wil::com_ptr<IDWriteFactory> dwriteFactory,
    wil::com_ptr<IDWriteBitmapRenderTarget> renderTarget,
    wil::com_ptr<IDWriteRenderingParams> renderingParams);

wil::com_ptr<IDWriteTextRenderer1> CreateMarkingsRenderer(
    wil::com_ptr<IDWriteFactory> dwriteFactory,
    wil::com_ptr<IDWriteBitmapRenderTarget> renderTarget,
    wil::com_ptr<IDWriteRenderingParams> renderingParams,
    RenderMarkings options);