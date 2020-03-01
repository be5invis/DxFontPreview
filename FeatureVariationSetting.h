#pragma once

using FeatureSettings = std::vector<DWRITE_FONT_FEATURE>;
using VariationSettings = std::vector<DWRITE_FONT_AXIS_VALUE>;

FeatureSettings ParseFeatures(const std::wstring& userInput);
VariationSettings ParseVariations(const std::wstring& userInput);

void AmendFeatureSettings(FeatureSettings& fs, DWRITE_FONT_FEATURE_TAG tag, uint32_t parameter);