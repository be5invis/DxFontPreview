#include "Common.h"
#include "StringParser.h"

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

int parseEscape(TextParser& parser) {
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

std::wstring ParseStringLiteralImpl(TextParser& parser) {
    std::wstringstream ret;
    for (;;) {
        wchar_t c = parser.advance();
        if (!c) break;
        if (c == L'\\') {
            c = parser.current();
            switch (c) {
            case L'\0':
                break;
            case L'\\':
                ret << c;
                parser.advance();
                break;
            case L'\r':
                if (parser.ahead(1) == L'\n') parser.advance();
                // fall through
            case L'\n':
                parser.advance();
                continue;
            default:
                int lch = parseEscape(parser);
                if (lch < 0) {
                    ret << L'\xFFFD';
                } else if (lch < 0x10000) {
                    ret << static_cast<wchar_t>(lch);
                } else {
                    wchar_t high, low;
                    splitSurrogate(lch, high, low);
                    ret << high << low;
                }
                break;
            }
        } else {
            ret << c;
        }
    }
    return ret.str();
}


std::wstring ParseStringLiteral(const std::wstring_view& input) {
    return ParseStringLiteralImpl(TextParser(input));
}
