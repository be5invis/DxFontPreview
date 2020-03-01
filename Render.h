#pragma once

wil::com_ptr<IDWriteTextRenderer1> CreateTextRenderer(
    wil::com_ptr<IDWriteFactory> dwriteFactory,
    wil::com_ptr<IDWriteBitmapRenderTarget> renderTarget,
    wil::com_ptr<IDWriteRenderingParams> renderingParams);