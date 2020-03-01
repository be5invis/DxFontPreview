#pragma once

#include "Common.h"
#include "Render.h"

union DX_MATRIX_3X2F {
	// Explicity named fields for clarity.
	struct { // Win8
		FLOAT xx; // x affects x (horizontal scaling / cosine of rotation)
		FLOAT xy; // x affects y (vertical shear     / sine of rotation)
		FLOAT yx; // y affects x (horizontal shear   / negative sine of rotation)
		FLOAT yy; // y affects y (vertical scaling   / cosine of rotation)
		FLOAT dx; // displacement of x, always orthogonal regardless of rotation
		FLOAT dy; // displacement of y, always orthogonal regardless of rotation
	};
	struct { // D2D Win7
		FLOAT _11;
		FLOAT _12;
		FLOAT _21;
		FLOAT _22;
		FLOAT _31;
		FLOAT _32;
	};
	struct { // DWrite Win7
		FLOAT m11;
		FLOAT m12;
		FLOAT m21;
		FLOAT m22;
		FLOAT m31;
		FLOAT m32;
	};
	float m[6]; // Would [3][2] be better, more useful?

	DWRITE_MATRIX dwrite;
	D2D1_MATRIX_3X2_F d2d;
	XFORM gdi;
};

const static DX_MATRIX_3X2F g_identityTransform = { 1,0,0,1,0,0 };

DWRITE_GLYPH_IMAGE_FORMATS g_allMonochromaticOutlineGlyphImageFormats = DWRITE_GLYPH_IMAGE_FORMATS_TRUETYPE | DWRITE_GLYPH_IMAGE_FORMATS_CFF | DWRITE_GLYPH_IMAGE_FORMATS_COLR;

class DECLSPEC_UUID("1413a625-a27e-4886-9c33-5ab75370ee35") BitmapRenderTargetTextRenderer
	: public ComBase<QiListSelf<BitmapRenderTargetTextRenderer,
	QiList<IDWriteTextRenderer1, QiList<IUnknown>>>> {
public:
	BitmapRenderTargetTextRenderer(
		wil::com_ptr<IDWriteFactory> dwriteFactory,
		wil::com_ptr<IDWriteBitmapRenderTarget> renderTarget,
		wil::com_ptr<IDWriteRenderingParams> renderingParams,
		COLORREF textColor = 0x00000000,
		uint32_t colorPaletteIndex = 0xFFFFFFFF,
		bool enablePixelSnapping = true
	) : m_dwriteFactory(dwriteFactory),
		m_renderTarget(renderTarget),
		m_renderingParams(renderingParams),
		m_textColor(textColor),
		m_colorPaletteIndex(colorPaletteIndex),
		m_enablePixelSnapping(enablePixelSnapping) {}

	class TransformSetter {
	public:
		TransformSetter(
			wil::com_ptr<IDWriteBitmapRenderTarget> renderTarget,
			DWRITE_GLYPH_ORIENTATION_ANGLE orientationAngle,
			float originX,
			float originY,
			bool isSideways = false
		) {
			m_renderTarget = renderTarget;
			m_needToSetTransform = (orientationAngle != DWRITE_GLYPH_ORIENTATION_ANGLE_0_DEGREES || isSideways);
			m_renderTarget->GetCurrentTransform(OUT & m_previousTransform.dwrite);
			if (m_needToSetTransform) {
				DX_MATRIX_3X2F runTransform;
				GetGlyphOrientationTransform(
					orientationAngle,
					isSideways,
					originX,
					originY,
					OUT & runTransform.dwrite
				);
				CombineMatrix(runTransform, m_previousTransform, OUT m_currentTransform);
				m_renderTarget->SetCurrentTransform(&m_currentTransform.dwrite);
			} else {
				m_currentTransform = m_previousTransform;
			}
		}

		~TransformSetter() {
			if (m_needToSetTransform)
				m_renderTarget->SetCurrentTransform(&m_previousTransform.dwrite);
		}

	public:
		DX_MATRIX_3X2F m_currentTransform;
		DX_MATRIX_3X2F m_previousTransform;

	protected:
		wil::com_ptr<IDWriteBitmapRenderTarget> m_renderTarget; // Weak pointers because class is stack local.
		bool m_needToSetTransform;
	};

	HRESULT STDMETHODCALLTYPE DrawGlyphRun(
		_In_ void* clientDrawingContext,
		_In_ float baselineOriginX,
		_In_ float baselineOriginY,
		DWRITE_MEASURING_MODE measuringMode,
		_In_ DWRITE_GLYPH_RUN const* glyphRun,
		_In_ DWRITE_GLYPH_RUN_DESCRIPTION const* glyphRunDescription,
		_In_ IUnknown* clientDrawingEffect
	) noexcept override {
		// Forward to newer overload.
		return DrawGlyphRun(
			clientDrawingContext,
			baselineOriginX,
			baselineOriginY,
			DWRITE_GLYPH_ORIENTATION_ANGLE_0_DEGREES,
			measuringMode,
			glyphRun,
			glyphRunDescription,
			clientDrawingEffect
		);
	}

	HRESULT STDMETHODCALLTYPE DrawGlyphRun(
		_In_ void* clientDrawingContext,
		_In_ FLOAT baselineOriginX,
		_In_ FLOAT baselineOriginY,
		_In_ DWRITE_GLYPH_ORIENTATION_ANGLE orientationAngle,
		DWRITE_MEASURING_MODE measuringMode,
		_In_ DWRITE_GLYPH_RUN const* glyphRun,
		_In_ DWRITE_GLYPH_RUN_DESCRIPTION const* glyphRunDescription,
		_In_ IUnknown* clientDrawingEffect
	) noexcept override {
		if (glyphRun->glyphCount <= 0)
			return S_OK;

		TransformSetter transformSetter(m_renderTarget, orientationAngle, baselineOriginX, baselineOriginY, !!glyphRun->isSideways);

		return DrawColorGlyphRun(
			m_dwriteFactory,
			m_renderTarget,
			*glyphRun,
			transformSetter.m_currentTransform.dwrite,
			measuringMode,
			baselineOriginX,
			baselineOriginY,
			m_renderingParams,
			m_textColor,
			m_colorPaletteIndex
		);
	}

	HRESULT STDMETHODCALLTYPE DrawUnderline(
		_In_ void* clientDrawingContext,
		_In_ FLOAT baselineOriginX,
		_In_ FLOAT baselineOriginY,
		_In_ DWRITE_GLYPH_ORIENTATION_ANGLE orientationAngle,
		_In_ DWRITE_UNDERLINE const* underline,
		_In_ IUnknown* clientDrawingEffect
	) noexcept override {
		DrawLine(
			clientDrawingContext,
			baselineOriginX,
			baselineOriginY,
			clientDrawingEffect,
			m_textColor,
			underline->width,
			underline->offset,
			underline->thickness,
			orientationAngle
		);

		return S_OK;
	}

	HRESULT STDMETHODCALLTYPE DrawUnderline(
		_In_ void* clientDrawingContext,
		_In_ float baselineOriginX,
		_In_ float baselineOriginY,
		_In_ DWRITE_UNDERLINE const* underline,
		_In_ IUnknown* clientDrawingEffect
	) noexcept override {
		return DrawUnderline(
			clientDrawingContext,
			baselineOriginX,
			baselineOriginY,
			DWRITE_GLYPH_ORIENTATION_ANGLE_0_DEGREES,
			underline,
			clientDrawingEffect
		);
	}

	HRESULT STDMETHODCALLTYPE DrawStrikethrough(
		_In_ void* clientDrawingContext,
		_In_ float baselineOriginX,
		_In_ float baselineOriginY,
		_In_ DWRITE_GLYPH_ORIENTATION_ANGLE orientationAngle,
		_In_ DWRITE_STRIKETHROUGH const* strikethrough,
		_In_ IUnknown* clientDrawingEffect
	) noexcept override {
		DrawLine(
			clientDrawingContext,
			baselineOriginX,
			baselineOriginY,
			clientDrawingEffect,
			m_textColor,
			strikethrough->width,
			strikethrough->offset,
			strikethrough->thickness,
			orientationAngle
		);
		return S_OK;
	}

	HRESULT STDMETHODCALLTYPE DrawStrikethrough(
		_In_ void* clientDrawingContext,
		_In_ float baselineOriginX,
		_In_ float baselineOriginY,
		_In_ DWRITE_STRIKETHROUGH const* strikethrough,
		_In_ IUnknown* clientDrawingEffect
	) noexcept override {
		return DrawStrikethrough(
			clientDrawingContext,
			baselineOriginX,
			baselineOriginY,
			DWRITE_GLYPH_ORIENTATION_ANGLE_0_DEGREES,
			strikethrough,
			clientDrawingEffect
		);
	}

	HRESULT STDMETHODCALLTYPE DrawInlineObject(
		_In_ void* clientDrawingContext,
		_In_ FLOAT originX,
		_In_ FLOAT originY,
		_In_ DWRITE_GLYPH_ORIENTATION_ANGLE orientationAngle,
		_In_ IDWriteInlineObject* inlineObject,
		_In_ BOOL isSideways,
		_In_ BOOL isRightToLeft,
		_In_ IUnknown* clientDrawingEffect
	) noexcept override {
		return inlineObject->Draw(clientDrawingContext, this, originX, originY, isSideways, isRightToLeft, clientDrawingEffect);
	}

	HRESULT STDMETHODCALLTYPE DrawInlineObject(
		_In_ void* clientDrawingContext,
		_In_ float originX,
		_In_ float originY,
		_In_ IDWriteInlineObject* inlineObject,
		_In_ BOOL isSideways,
		_In_ BOOL isRightToLeft,
		_In_ IUnknown* clientDrawingEffect
	) noexcept override {
		return inlineObject->Draw(clientDrawingContext, this, originX, originY, isSideways, isRightToLeft, clientDrawingEffect);
	}

	void DrawLine(
		_In_ void* clientDrawingContext,
		_In_ FLOAT baselineOriginX,
		_In_ FLOAT baselineOriginY,
		_In_ IUnknown* clientDrawingEffects,
		_In_ COLORREF defaultColor,
		_In_ float width,
		_In_ float offset,
		_In_ float thickness,
		_In_ DWRITE_GLYPH_ORIENTATION_ANGLE orientationAngle
	) {
		TransformSetter transformSetter(m_renderTarget, orientationAngle, baselineOriginX, baselineOriginY);

		// We will always get a strikethrough or underline as a LTR rectangle with the baseline origin snapped.
		D2D1_RECT_F rectangle = { baselineOriginX + 0, baselineOriginY + offset, baselineOriginX + width, baselineOriginY + offset + thickness };

		// use GDI to draw line.
		RECT rect = { int(rectangle.left), int(rectangle.top), int(rectangle.right), int(rectangle.bottom), };
		if (rect.bottom <= rect.top) {
			rect.bottom = rect.top + 1;
		}

		// Draw the line
		HDC hdc = m_renderTarget->GetMemoryDC();
		XFORM previousTransform;
		GetWorldTransform(hdc, OUT & previousTransform);
		SetWorldTransform(hdc, &transformSetter.m_currentTransform.gdi);
		SetBkColor(hdc, defaultColor);
		ExtTextOut(hdc, 0, 0, ETO_OPAQUE, &rect, L"", 0, nullptr);
		SetWorldTransform(hdc, &previousTransform);
	}

	HRESULT STDMETHODCALLTYPE IsPixelSnappingDisabled(
		_In_opt_ void* clientDrawingContext,
		_Out_ BOOL* isDisabled
	) noexcept override {
		*isDisabled = !m_enablePixelSnapping;
		return S_OK;
	}

	HRESULT STDMETHODCALLTYPE GetCurrentTransform(
		_In_opt_ void* clientDrawingContext,
		_Out_ DWRITE_MATRIX* transform
	) noexcept override {
		*transform = g_identityTransform.dwrite;
		return S_OK;
	}

	HRESULT STDMETHODCALLTYPE GetPixelsPerDip(
		_In_opt_ void* clientDrawingContext,
		_Out_ float* pixelsPerDip
	) noexcept override {
		*pixelsPerDip = 1.0;
		return S_OK;
	}

private:
	static HRESULT GetColorGlyphRunEnumerator(
		wil::com_ptr<IDWriteFactory> dwriteFactory,
		DWRITE_GLYPH_RUN const& glyphRun,
		DWRITE_MATRIX const& transform,
		float baselineOriginX,
		float baselineOriginY,
		uint32_t colorPalette,
		_COM_Outptr_ IDWriteColorGlyphRunEnumerator** colorEnumerator
	) noexcept {
		*colorEnumerator = nullptr;
		wil::com_ptr<IDWriteFontFace2> fontFace2;
		if (colorPalette != 0xFFFFFFFF && SUCCEEDED(glyphRun.fontFace->QueryInterface(&fontFace2))) {
			uint32_t colorPaletteCount = fontFace2->GetColorPaletteCount();
			if (colorPalette >= colorPaletteCount)
				colorPalette = 0;

			wil::com_ptr<IDWriteFactory4> factory4 = dwriteFactory.try_query<IDWriteFactory4>();
			if (factory4) {
				return (factory4->TranslateColorGlyphRun(
					{ baselineOriginX, baselineOriginY },
					&glyphRun,
					nullptr,
					g_allMonochromaticOutlineGlyphImageFormats,
					DWRITE_MEASURING_MODE_NATURAL,
					&transform,
					colorPalette,
					OUT reinterpret_cast<IDWriteColorGlyphRunEnumerator1**>(colorEnumerator)
				));
			} else {
				wil::com_ptr<IDWriteFactory2> factory2 = dwriteFactory.try_query<IDWriteFactory2>();
				if (!factory2) return DWRITE_E_NOCOLOR;

				// Perform color translation.
				// Fall back to the default palette if the current palette index is out of range.
				return factory2->TranslateColorGlyphRun(
					baselineOriginX,
					baselineOriginY,
					&glyphRun,
					nullptr,
					DWRITE_MEASURING_MODE_NATURAL,
					&transform,
					colorPalette,
					OUT colorEnumerator
				);
			}
		}

		return DWRITE_E_NOCOLOR;
	}

	static HRESULT DrawColorGlyphRun(
		wil::com_ptr<IDWriteFactory> dwriteFactory,
		wil::com_ptr<IDWriteBitmapRenderTarget> renderTarget,
		DWRITE_GLYPH_RUN const& glyphRun,
		DWRITE_MATRIX const& transform,
		DWRITE_MEASURING_MODE measuringMode,
		float baselineOriginX,
		float baselineOriginY,
		wil::com_ptr<IDWriteRenderingParams> renderingParams,
		COLORREF textColor,
		uint32_t colorPalette // 0xFFFFFFFF if none
	) noexcept {
		textColor &= 0x00FFFFFF; // GDI may render nothing in outline mode if alpha byte is set.

		wil::com_ptr<IDWriteColorGlyphRunEnumerator> colorEnumerator;
		HRESULT hr = GetColorGlyphRunEnumerator(
			dwriteFactory,
			glyphRun,
			transform,
			baselineOriginX,
			baselineOriginY,
			colorPalette,
			OUT & colorEnumerator
		);

		if (hr == DWRITE_E_NOCOLOR) {
			// No color information; draw the top line with no color translation.
			RETURN_IF_FAILED(renderTarget->DrawGlyphRun(baselineOriginX, baselineOriginY, measuringMode, &glyphRun, renderingParams.get(), textColor, nullptr));
		} else {
			wil::com_ptr<IDWriteColorGlyphRunEnumerator1> colorEnumerator1 = colorEnumerator.try_query<IDWriteColorGlyphRunEnumerator1>();
			DWRITE_GLYPH_IMAGE_FORMATS glyphImageFormat = DWRITE_GLYPH_IMAGE_FORMATS_TRUETYPE | DWRITE_GLYPH_IMAGE_FORMATS_CFF;

			for(;;) {
				BOOL haveRun;
				RETURN_IF_FAILED(colorEnumerator->MoveNext(OUT & haveRun));

				if (!haveRun) break;

				// Get the old run or new one.
				DWRITE_COLOR_GLYPH_RUN const* colorRun = nullptr;
				if (colorEnumerator1 != nullptr) {
					DWRITE_COLOR_GLYPH_RUN1 const* colorRun1 = nullptr;
					RETURN_IF_FAILED(colorEnumerator1->GetCurrentRun(OUT & colorRun1));
					colorRun = colorRun1;
					glyphImageFormat = colorRun1->glyphImageFormat;
				} else {
					RETURN_IF_FAILED(colorEnumerator->GetCurrentRun(OUT & colorRun));
				}

				COLORREF runColor = (colorRun->paletteIndex == 0xFFFF) ? textColor : ToCOLORREF(colorRun->runColor);

				wil::com_ptr<IDWriteColorGlyphRunEnumerator> colorLayers;

				switch (glyphImageFormat) {
				case DWRITE_GLYPH_IMAGE_FORMATS_TRUETYPE:
				case uint32_t(DWRITE_GLYPH_IMAGE_FORMATS_TRUETYPE) | uint32_t(DWRITE_GLYPH_IMAGE_FORMATS_COLR):
				case uint32_t(DWRITE_GLYPH_IMAGE_FORMATS_CFF) | uint32_t(DWRITE_GLYPH_IMAGE_FORMATS_COLR):
				case uint32_t(DWRITE_GLYPH_IMAGE_FORMATS_TRUETYPE) | uint32_t(DWRITE_GLYPH_IMAGE_FORMATS_CFF):
				case DWRITE_GLYPH_IMAGE_FORMATS_CFF:
					RETURN_IF_FAILED(renderTarget->DrawGlyphRun(
						colorRun->baselineOriginX,
						colorRun->baselineOriginY,
						measuringMode,
						&colorRun->glyphRun,
						renderingParams.get(),
						runColor,
						nullptr // don't need blackBoxRect
					));
				case DWRITE_GLYPH_IMAGE_FORMATS_PNG:
					// todo:::
					break;
				}
			}
		}

		return S_OK;
	}

	// Matrix utility
	static void GetGlyphOrientationTransform(
		DWRITE_GLYPH_ORIENTATION_ANGLE glyphOrientationAngle,
		bool isSideways,
		float originX,
		float originY,
		_Out_ DWRITE_MATRIX* transform
	) noexcept {
		uint32_t quadrant = glyphOrientationAngle;
		if (isSideways) {
			// The world transform is an additional 90 degrees clockwise from the
			// glyph orientation when the sideways flag is set.
			++quadrant;
		}

		const static DWRITE_MATRIX quadrantMatrices[4] = {
			/* I0   */{  1, 0, 0, 1, 0, 0 }, // translation is always zero
			/* C90  */{  0, 1,-1, 0, 0, 0 },
			/* C180 */{ -1, 0, 0,-1, 0, 0 },
			/* C270 */{  0,-1, 1, 0, 0, 0 }
		};

		auto const& matrix = quadrantMatrices[quadrant & 3];
		*transform = matrix;

		if (quadrant != 0) {
			// Compute the translation necessary to rotate around the given origin.
			// (if the angle is zero, the equation would produce zero translation
			// values anyway, so skip it in that case).
			transform->dx = originX * (1 - matrix.m11) - originY * matrix.m21;
			transform->dy = originY * (1 - matrix.m22) - originX * matrix.m12;
		}
	}

	static void CombineMatrix(
		DX_MATRIX_3X2F const& a,
		DX_MATRIX_3X2F const& b,
		DX_MATRIX_3X2F& result
	) {
		// Common transposed dot product (as opposed to any of the other numerous
		// forms of matrix 'multiplication' like the element-wise Hadamard product)
		// such that:
		//
		//  If you transpose <10,0> and rotate 45 degrees, you'll be at <.7,.7>
		//  If you rotate 45 degrees and transpose <10,0>, you'll be at <10,0>
		//
		// This is similar to XNA and opposite of OpenGL (so, easier to understand).

		result.xx = a.xx * b.xx + a.xy * b.yx;
		result.xy = a.xx * b.xy + a.xy * b.yy;
		result.yx = a.yx * b.xx + a.yy * b.yx;
		result.yy = a.yx * b.xy + a.yy * b.yy;
		result.dx = a.dx * b.xx + a.dy * b.yx + b.dx;
		result.dy = a.dx * b.xy + a.dy * b.yy + b.dy;
	}

	static DX_MATRIX_3X2F CombineMatrix(
		DX_MATRIX_3X2F const& a,
		DX_MATRIX_3X2F const& b
	) {
		DX_MATRIX_3X2F result;
		CombineMatrix(a, b, OUT result);
		return result;
	}

	// Color utility
	static inline uint8_t FloatToColorByte(float c) {
		return static_cast<uint8_t>(floorf(c * 255 + 0.5f));
	}

	static COLORREF ToCOLORREF(DWRITE_COLOR_F const& color) {
		return RGB(
			FloatToColorByte(color.r),
			FloatToColorByte(color.g),
			FloatToColorByte(color.b)
		);
	}

private:
	wil::com_ptr<IDWriteFactory> m_dwriteFactory;
	wil::com_ptr<IDWriteBitmapRenderTarget> m_renderTarget;
	wil::com_ptr<IDWriteRenderingParams> m_renderingParams;
	COLORREF m_textColor;
	uint32_t m_colorPaletteIndex;
	bool m_enablePixelSnapping;
};

wil::com_ptr<IDWriteTextRenderer1> CreateTextRenderer(
	wil::com_ptr<IDWriteFactory> dwriteFactory,
	wil::com_ptr<IDWriteBitmapRenderTarget> renderTarget,
	wil::com_ptr<IDWriteRenderingParams> renderingParams) {
	auto renderer = new(std::nothrow) BitmapRenderTargetTextRenderer(dwriteFactory, renderTarget, renderingParams, 0x0, 0x0);
	THROW_IF_NULL_ALLOC(renderer);
	return wil::com_ptr<IDWriteTextRenderer1>(renderer);
}