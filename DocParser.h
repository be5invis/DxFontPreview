#pragma once
#include "FontSelector.h"

enum RunStyleType {
	Feature = 1,
	Variation = 2
};
struct RunStyleEntry {
	RunStyleType type;
	uint32_t     tag;
	double       value;
};
struct RunStyleState {
	uint32_t                   cpBegin;
	std::vector<RunStyleEntry> style;
	void ClearStyles(RunStyleType type);
	void ClearStyles(RunStyleType type, uint32_t tag);
	void SetStyle(RunStyleType type, uint32_t tag, double value);
};
struct RunStyle {
	uint32_t                   cpBegin;
	uint32_t                   cpEnd;
	std::vector<RunStyleEntry> style;
};
struct ParsedDocument {
	std::wstring text;
	std::vector<RunStyle> styles;
};

class DocumentBuilder {
public:
	void Add(wchar_t wch);
	void Flush();
	void BeginSubrun();
	void EndSubrun();
	RunStyleState GetCurrentStyle();
	void Update(const RunStyleState& newStyle);
	std::wstring GetText();
	std::vector<RunStyle> GetStyles();

private:
	std::stack<RunStyleState> m_runStyleStack;

	std::wstringstream m_stream;
	std::vector<RunStyle> m_style;
	uint32_t m_current = 0;
};

ParsedDocument ParseInputDoc(const std::wstring_view& input, const FontSelector& fs);
