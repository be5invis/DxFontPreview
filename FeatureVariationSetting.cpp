#include "Common.h"
#include "FeatureVariationSetting.h"

class ParsingState {
public:
	ParsingState(const std::wstring& userInput)
		: m_str(userInput), m_index(0) {
		m_length = m_str.size();
	}
	bool Eof() {
		return m_index >= m_length;
	}
	wchar_t Current() {
		return m_str[m_index];
	}
	void Next(){
		m_index++;
	}

	// Char Classes
	bool FSpace() {
		if (Eof()) return false;
		wchar_t ch = Current();
		return ch == L' ' || ch == L'\t';
	}
	bool FAlpha() {
		if (Eof()) return false;
		wchar_t ch = Current();
		return ch >= L'a' && ch <= L'z' || ch >= L'A' && ch <= L'Z';
	}
	bool FDigit() {
		if (Eof()) return false;
		wchar_t ch = Current();
		return ch >= L'0' && ch <= L'9';
	}
	bool FWord() {
		if (Eof()) return false;
		wchar_t ch = Current();
		return ch >= L'a' && ch <= L'z' || ch >= L'A' && ch <= L'Z' || ch >= L'0' && ch <= L'9' || ch == L'_';
	}
	bool FSeparator() {
		if (Eof()) return false;
		wchar_t ch = Current();
		return ch == L',' || ch == L';';
	}
	bool FEqual() {
		if (Eof()) return false;
		wchar_t ch = Current();
		return ch == L'=';
	}
	bool FDot() {
		if (Eof()) return false;
		wchar_t ch = Current();
		return ch == L'.';
	}
	bool FMinus() {
		if (Eof()) return false;
		wchar_t ch = Current();
		return ch == L'-';
	}
	bool FPlus() {
		if (Eof()) return false;
		wchar_t ch = Current();
		return ch == L'+';
	}
private:
	std::wstring m_str;
	size_t m_index;
	size_t m_length;

	// No copying
	ParsingState(const ParsingState&) = delete;
};

void SkipSpaces(ParsingState& ps) {
	while (ps.FSpace()) ps.Next();
}

bool Separator(ParsingState& ps) {
	if (!ps.FSeparator()) return false;
	ps.Next();
	SkipSpaces(ps);
	return true;
}

void swapByteOrder(uint32_t& ui) {
	ui = (ui >> 24) |
		((ui << 8) & 0x00FF0000) |
		((ui >> 8) & 0x0000FF00) |
		(ui << 24);
}

bool Tag(ParsingState& ps, uint32_t& result) {
	if (!ps.FWord()) return false;

	size_t digits = 0;
	while (ps.FWord()) {
		wchar_t ch = ps.Current();
		result = (result << 8) | (ch & 0xFF);
		digits++;
		ps.Next();
	}
	if (digits > 4) return false;
	while (digits < 4) {
		result = (result << 8) | 0x20; // Pad spaces
		digits++;
	}
	swapByteOrder(result); // DWrite use little-endian :(
	return true;
}

bool Integer(ParsingState& ps, uint32_t& value) {
	value = 0;
	if (!ps.FDigit()) return false;
	while (ps.FDigit()) {
		value = 10 * value + (ps.Current() - L'0');
		ps.Next();
	}
	return true;
}

bool FeatureAssign(ParsingState& ps, uint32_t& tag, uint32_t& parameter) {
	if (!Tag(ps, tag)) return false;
	SkipSpaces(ps);
	if (ps.FEqual()) {
		ps.Next();
		SkipSpaces(ps);
		if (!Integer(ps, parameter)) return false;
	} else {
		parameter = 1;
	}
	return true;
}

bool Fraction(ParsingState& ps, float& value) {
	value = 0;

	// sign
	float sign = 1;
	if (ps.FMinus()) {
		sign = -1;
		ps.Next();
	} else if (ps.FPlus()) {
		ps.Next();
	}

	// integral
	if (!ps.FDigit()) return false;
	while (ps.FDigit()) {
		value = 10 * value + (ps.Current() - L'0');
		ps.Next();
	}

	// fraction
	if (ps.FDot()) {
		ps.Next();
		float scalar = 1.0;
		while (ps.FDigit()) {
			scalar /= 10.0;
			value += scalar * (ps.Current() - L'0');
			ps.Next();
		}
	}
	value *= sign;
	return true;
}


bool VariationAssign(ParsingState& ps, uint32_t& tag, float& parameter) {
	if (!Tag(ps, tag)) return false;
	SkipSpaces(ps);
	if (ps.FEqual()) {
		ps.Next();
		SkipSpaces(ps);
		if (!Fraction(ps, parameter)) return false;
	}
	return true;
}

FeatureSettings ParseFeatures(const std::wstring& userInput){
	ParsingState ps(userInput);
	FeatureSettings fs;
	size_t tagIndex = 0;
	while (!ps.Eof()) {
		SkipSpaces(ps);
		if (tagIndex > 0 && !Separator(ps)) break;

		uint32_t tag = 0;
		uint32_t parameter = 0;
		if (!FeatureAssign(ps, tag, parameter)) break;

		fs.push_back({ DWRITE_FONT_FEATURE_TAG(tag), parameter });
		tagIndex++;
	}
	return fs;
}

VariationSettings ParseVariations(const std::wstring& userInput) {
	ParsingState ps(userInput);
	VariationSettings fs;
	size_t tagIndex = 0;
	while (!ps.Eof()) {
		SkipSpaces(ps);
		if (tagIndex > 0 && !Separator(ps)) break;

		uint32_t tag = 0;
		float parameter = 0;
		if (!VariationAssign(ps, tag, parameter)) break;

		fs.push_back({ DWRITE_FONT_AXIS_TAG(tag), parameter });
		tagIndex++;
	}
	return fs;
}

void AmendFeatureSettings(FeatureSettings& fs, DWRITE_FONT_FEATURE_TAG tag, uint32_t parameter) {
	bool hasTag = false;
	for (auto& item : fs) {
		if (item.nameTag == tag) {
			item.parameter = parameter;
			hasTag = true;
		}
	}
	if (!hasTag) {
		fs.push_back({ tag, parameter });
	}
}
