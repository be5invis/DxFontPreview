#pragma once
#include "FontSelector.h"
#include "FontSource.h"

namespace TextFormat {
	wil::com_ptr<IDWriteTextFormat3> Create(wil::com_ptr<IDWriteFactory> factory, const FlowFontSource& fontSource,
		const FontSelector& fs);
}