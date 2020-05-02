#include "Common.h"
#include "DocParser.h"

class TextParser {
public:
    TextParser(const std::wstring_view& input): m_input(input), m_p(0) {}
    TextParser(const std::wstring_view& input, size_t p): m_input(input), m_p(p) {}
    TextParser(const TextParser& from): m_input(from.m_input), m_p(from.m_p) {}
    wchar_t current() {
        if (m_p >= m_input.size()) return 0;
        return m_input[m_p];
    }
    wchar_t ahead(size_t delta) {
        if (m_p + delta >= m_input.size()) return 0;
        return m_input[m_p + delta];
    }
    wchar_t advance() {
        if (m_p >= m_input.size()) return 0;
        return m_input[m_p++];
    }

    bool fWord() {
        wchar_t ch = current();
        return ch >= L'a' && ch <= L'z' || ch >= L'A' && ch <= L'Z' || ch >= L'0' && ch <= L'9' || ch == L'_';
    }
    bool fDigit() {
        wchar_t ch = current();
        return ch >= L'0' && ch <= L'9';
    }
    bool fEqual() {
        wchar_t ch = current();
        return ch == L'=';
    }
    bool fDot() {
        wchar_t ch = current();
        return ch == L'.';
    }
    bool fMinus() {
        wchar_t ch = current();
        return ch == L'-';
    }
    bool fPlus() {
        wchar_t ch = current();
        return ch == L'+';
    }
    bool fSpace() {
        wchar_t ch = current();
        return ch == L' ' || ch == L'\t';
    }
    bool fSeparator() {
        wchar_t ch = current();
        return ch == L',' || ch == L';';
    }
private:
    const std::wstring_view m_input;
    size_t m_p;
};

static void splitSurrogate(const uint32_t in, wchar_t &wchHigh, wchar_t &wchLow) noexcept {
    const uint32_t inMinus0x10000 = (in - 0x10000);
    wchHigh = static_cast<wchar_t>((inMinus0x10000 / 0x400) + 0xd800);
    wchLow = static_cast<wchar_t>((inMinus0x10000 % 0x400) + 0xdc00);
}

static inline int from_hex(wchar_t c) {
    if (c >= L'0' && c <= L'9')
        return c - L'0';
    else if (c >= L'A' && c <= L'F')
        return c - L'A' + 10;
    else if (c >= L'a' && c <= L'f')
        return c - L'a' + 10;
    else
        return -1;
}

static void swapByteOrder(uint32_t& ui) {
    ui = (ui >> 24) |
        ((ui << 8) & 0x00FF0000) |
        ((ui >> 8) & 0x0000FF00) |
        (ui << 24);
}

static bool Tag(TextParser& ps, uint32_t& result) {
    if (!ps.fWord()) return false;

    size_t digits = 0;
    while (ps.fWord()) {
        wchar_t ch = ps.current();
        result = (result << 8) | (ch & 0xFF);
        digits++;
        ps.advance();
    }
    if (digits > 4) return false;
    while (digits < 4) {
        result = (result << 8) | 0x20; // Pad spaces
        digits++;
    }
    swapByteOrder(result); // DWrite use little-endian :(
    return true;
}

static bool Fraction(TextParser& ps, double& value) {
    value = 0;

    // sign
    float sign = 1;
    if (ps.fMinus()) {
        sign = -1;
        ps.advance();
    } else if (ps.fPlus()) {
        ps.advance();
    }

    // integral
    if (!ps.fDigit()) return false;
    while (ps.fDigit()) {
        value = 10 * value + (ps.current() - L'0');
        ps.advance();
    }

    // fraction
    if (ps.fDot()) {
        ps.advance();
        double scalar = 1.0;
        while (ps.fDigit()) {
            scalar /= 10.0;
            value += scalar * (ps.current() - L'0');
            ps.advance();
        }
    }
    value *= sign;
    return true;
}

bool Expect(TextParser& ps, const wchar_t wch) {
    return ps.advance() == wch;
}

void SkipSpaces(TextParser& ps) {
    while (ps.fSpace()) ps.advance();
}

bool Separator(TextParser& ps) {
    SkipSpaces(ps);
    if (!ps.fSeparator()) return false;
    ps.advance();
    SkipSpaces(ps);
    return true;
}

static bool FeatureAssign(TextParser& ps, bool& fMinus, uint32_t& tag, double& parameter) {
    if (ps.fMinus()) {
        ps.advance();
        fMinus = true;
    } else {
        fMinus = false;
    }
    if (!Tag(ps, tag)) return false;
    if (!fMinus && ps.fEqual()) {
        ps.advance();
        if (!Fraction(ps, parameter)) return false;
    } else {
        parameter = 1;
    }
    return true;
}

static bool FeatureAssignmentSet(TextParser& ps, RunStyleType styleType, RunStyleState& style) {
    if (!Expect(ps, L'{')) return false;
    SkipSpaces(ps);
    if (ps.current() == L'-' && ps.ahead(1) == L'}') {
        ps.advance();
        style.ClearStyles(styleType);
    } else {
        bool started = false;
        for (;;) {
            if (!ps.current() || ps.current() == L'}') break;
            if (started && !Separator(ps)) return false;
            bool fMinus;
            uint32_t tag;
            double value;
            if (!FeatureAssign(ps, fMinus, tag, value)) return false;
            if (fMinus) {
                style.ClearStyles(styleType, tag);
            } else {
                style.SetStyle(styleType, tag, value);
            }
            started = true;
        }
    }
    SkipSpaces(ps);
    if (!Expect(ps, L'}')) return false;
    return true;
}


static bool parseStyle(DocumentBuilder& db, TextParser& parser) {
    wchar_t commandType = parser.advance();
    RunStyleType styleType;
    if (commandType == L'f') styleType = RunStyleType::Feature;
    else if (commandType == L'v') styleType = RunStyleType::Variation;
    else return false;

    RunStyleState rs = db.GetCurrentStyle();

    if (!FeatureAssignmentSet(parser, styleType, rs)) return false;
    db.Update(rs);

    return true;
}

int parseEscapeChar(TextParser& parser) {
    uint32_t c;

    c = parser.advance();
    switch (c) {
    case L't':
        c = L'\t';
        break;
    case L'x':
    case L'u':
    {
        int h, n, i;
        uint32_t c1;

        if (parser.current() == L'{') {
            parser.advance();
            c = 0;
            for(;;) {
                h = from_hex(parser.advance());
                if (h < 0)
                    return -1;
                c = (c << 4) | h;
                if (c > 0x10FFFF)
                    return -1;
                if (parser.current() == L'}') {
                    parser.advance();
                    break;
                }
            }
        } else {
            if (c == 'x') {
                n = 2;
            } else {
                n = 4;
            }

            c = 0;
            for (i = 0; i < n; i++) {
                h = from_hex(parser.advance());
                if (h < 0) return -1;                
                c = (c << 4) | h;
            }
        }
    }
    break;
    default:
        return -2;
    }
    return c;
}

void ParseInput(DocumentBuilder& db, TextParser& parser) {
    for (;;) {
        wchar_t c = parser.advance();
        if (!c) break;
        if (c == L'\\') {
            c = parser.current();
            switch (c) {
            case L'\0':
                break;
            case L'\\':
                db.Add(c);
                parser.advance();
                break;
            case L'\r':
                if (parser.ahead(1) == L'\n') parser.advance();
                // fall through
            case L'\n':
                parser.advance();
                continue;
            case L'{':
                db.BeginSubrun();
                parser.advance();
                break;
            case L'}':
                db.EndSubrun();
                parser.advance();
                break;
            case L'f':
            case L'v':
                if (!parseStyle(db, parser)) {
                    db.Add(L'\xFFFD');
                }
                break;
            default:
                int lch = parseEscapeChar(parser);
                if (lch < 0) {
                    db.Add(L'\xFFFD');
                } else if (lch < 0x10000) {
                    db.Add(static_cast<wchar_t>(lch));
                } else {
                    wchar_t high, low;
                    splitSurrogate(lch, high, low);
                    db.Add(high);
                    db.Add(low);
                }
                break;
            }
        } else {
            db.Add(c);
        }
    }
}

ParsedDocument ParseInputDoc(const std::wstring_view& input, const FontSelector& fs) {
    DocumentBuilder db;
    db.BeginSubrun();
    parseStyle(db, TextParser(L"f{" + fs.userFeatureSettings + L"}"));
    parseStyle(db, TextParser(L"v{" + fs.userVariationSettings + L"}"));
    if (fs.parseEscapes) ParseInput(db, TextParser(input));
    else for (const auto& wch : input) db.Add(wch);
    db.EndSubrun();
    return { db.GetText(), db.GetStyles() };
}

void DocumentBuilder::Add(wchar_t wch) {
    m_stream << wch;
    m_current += 1;
}

void DocumentBuilder::Flush() {
    if (m_runStyleStack.empty()) return;
    RunStyleState& top = m_runStyleStack.top();
    if (m_current > top.cpBegin) {
        m_style.push_back(RunStyle{ top.cpBegin, m_current, top.style });
    }
    top.cpBegin = m_current;
}

void DocumentBuilder::BeginSubrun() {
    Flush();
    if (m_runStyleStack.empty()) {
        m_runStyleStack.push(RunStyleState{ 0, {} });
    } else {
        m_runStyleStack.push(RunStyleState(m_runStyleStack.top()));
    }
    m_runStyleStack.top().cpBegin = m_current;
}

void DocumentBuilder::EndSubrun() {
    Flush();
    if (m_runStyleStack.empty()) return;
    m_runStyleStack.pop();
    if (m_runStyleStack.empty()) return;
    m_runStyleStack.top().cpBegin = m_current;
}

RunStyleState DocumentBuilder::GetCurrentStyle() {
    if (m_runStyleStack.empty())
        return RunStyleState();
    else
        return RunStyleState(m_runStyleStack.top());
}

void DocumentBuilder::Update(const RunStyleState& newStyle) {
    Flush();
    if (m_runStyleStack.empty()) return;
    m_runStyleStack.top().cpBegin = m_current;
    m_runStyleStack.top().style = newStyle.style;
}

std::wstring DocumentBuilder::GetText() {
    return m_stream.str();
}

std::vector<RunStyle> DocumentBuilder::GetStyles() {
    return std::vector<RunStyle>(m_style);
}

void RunStyleState::ClearStyles(RunStyleType type) {
    std::vector<RunStyleEntry> newStyles;
    std::copy_if(style.begin(), style.end(), std::back_inserter(newStyles),
        [type](const RunStyleEntry& rs) {return rs.type != type; });
    style = newStyles;
}

void RunStyleState::ClearStyles(RunStyleType type, uint32_t tag) {
    std::vector<RunStyleEntry> newStyles;
    std::copy_if(style.begin(), style.end(), std::back_inserter(newStyles),
        [type, tag](const RunStyleEntry& rs) {return rs.type != type || rs.tag != tag; });
    style = newStyles;
}

void RunStyleState::SetStyle(RunStyleType type, uint32_t tag, double value) {
    bool found = false;
    for (auto& item : style) {
        if (item.type == type && item.tag == tag) {
            item.value = value;
            found = true;
        }
    }
    if (!found) style.push_back({ type, tag, value });
}
