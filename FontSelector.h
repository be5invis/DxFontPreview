#pragma once

enum ReadingDirection {
    ReadingDirectionLeftToRightTopToBottom = 0,
    ReadingDirectionRightToLeftTopToBottom = 1,
    ReadingDirectionLeftToRightBottomToTop = 2,
    ReadingDirectionRightToLeftBottomToTop = 3,
    ReadingDirectionTopToBottomLeftToRight = 4,
    ReadingDirectionBottomToTopLeftToRight = 5,
    ReadingDirectionTopToBottomRightToLeft = 6,
    ReadingDirectionBottomToTopRightToLeft = 7,

    // Contributing bits
    ReadingDirectionPrimaryProgression = 1, // False = Ltr/Ttb,    True = Rtl/Btt
    ReadingDirectionSecondaryProgression = 2, // False = Ttb/Ltr,    True = Btt/Rtl
    ReadingDirectionPrimaryAxis = 4, // False = Horizontal, True = Vertical

    // Shorter Aliases
    ReadingDirectionEs = ReadingDirectionLeftToRightTopToBottom,
    ReadingDirectionSw = ReadingDirectionTopToBottomRightToLeft,
    ReadingDirectionWn = ReadingDirectionRightToLeftBottomToTop,
    ReadingDirectionNe = ReadingDirectionBottomToTopLeftToRight,

    // A single direction
    ReadingDirectionE = ReadingDirectionLeftToRightTopToBottom,
    ReadingDirectionS = ReadingDirectionTopToBottomLeftToRight,
    ReadingDirectionW = ReadingDirectionRightToLeftTopToBottom,
    ReadingDirectionN = ReadingDirectionBottomToTopLeftToRight,
};

const static DWRITE_READING_DIRECTION g_dwriteReadingDirectionValues[8] = {
    DWRITE_READING_DIRECTION_LEFT_TO_RIGHT,
    DWRITE_READING_DIRECTION_RIGHT_TO_LEFT,
    DWRITE_READING_DIRECTION_LEFT_TO_RIGHT,
    DWRITE_READING_DIRECTION_RIGHT_TO_LEFT,
    DWRITE_READING_DIRECTION_TOP_TO_BOTTOM,
    DWRITE_READING_DIRECTION_BOTTOM_TO_TOP,
    DWRITE_READING_DIRECTION_TOP_TO_BOTTOM,
    DWRITE_READING_DIRECTION_BOTTOM_TO_TOP,
};

const static DWRITE_FLOW_DIRECTION g_dwriteFlowDirectionValues[8] = {
    DWRITE_FLOW_DIRECTION_TOP_TO_BOTTOM,
    DWRITE_FLOW_DIRECTION_TOP_TO_BOTTOM,
    DWRITE_FLOW_DIRECTION_BOTTOM_TO_TOP,
    DWRITE_FLOW_DIRECTION_BOTTOM_TO_TOP,
    DWRITE_FLOW_DIRECTION_LEFT_TO_RIGHT,
    DWRITE_FLOW_DIRECTION_LEFT_TO_RIGHT,
    DWRITE_FLOW_DIRECTION_RIGHT_TO_LEFT,
    DWRITE_FLOW_DIRECTION_RIGHT_TO_LEFT,
};

class FontSelector {
public:
    std::wstring familyName;
    std::wstring styleName;
    uint32_t fontEmSize = 12;
    
    bool userFeaturesEnabled = false;
    std::wstring userFeatureSettings;
    bool userVariationEnabled = false;
    std::wstring userVariationSettings;

    ReadingDirection readingDirection = ReadingDirectionLeftToRightTopToBottom;

    bool doFontFallback = false;
    bool doJustify = false;
    bool parseEscapes = true;
    std::wstring localeName;
};
