// THIS CODE AND INFORMATION IS PROVIDED "AS IS" WITHOUT WARRANTY OF
// ANY KIND, EITHER EXPRESSED OR IMPLIED, INCLUDING BUT NOT LIMITED TO
// THE IMPLIED WARRANTIES OF MERCHANTABILITY AND/OR FITNESS FOR A
// PARTICULAR PURPOSE.
//
// Copyright (c) Microsoft Corporation. All rights reserved
//
// Contents:    Main user interface window.
//
//----------------------------------------------------------------------------

#pragma once

#include "TextLayout.h"

enum class NeedUpdateUi : uint32_t {
    None = 0,
    FontFamily = 1,
    FontStyle = 2,
    FontFeatureSettings = 4,
    FontVariationSettings = 8,
    FontSize = 16,
    FontLocale = 32,
    FontFeaturesEnabled = 64,
    FontVariationEnabled = 128,
    FontSelector = FontFamily | FontStyle | FontFeatureSettings | FontFeaturesEnabled
    | FontVariationEnabled | FontVariationSettings | FontSize | FontLocale,

    FontSource = 512,

    Text =  1024,
};
ENABLE_BITMASK_OPERATORS(NeedUpdateUi);

class MainWindow
{
public:
    struct DialogProcResult {
        INT_PTR handled; // whether it was handled or not
        LRESULT value;   // the return value

        DialogProcResult(INT_PTR newHandled, LRESULT newValue) : handled(newHandled), value(newValue) {}

        DialogProcResult(bool newHandled) : handled(newHandled), value() {}
    };

public:
    MainWindow(HWND hwnd)
    :   m_hwnd(hwnd),
        m_hMonitor(NULL),
        m_dwriteFactory(),
        m_renderingParams(),
        m_renderTarget(),
        m_textLayout()
    { }

    static ATOM RegisterWindowClass();
    static WPARAM RunMessageLoop();
    static INT_PTR CALLBACK StaticDialogProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam);

    void Initialize();

public:
    const static wchar_t* g_windowClassName;

protected:
    class UiScaffold {
    public:
        UiScaffold(HWND window) {
            if (window != NULL)
                m_dpi = GetDpiForWindow(window);
            else
                m_dpi = m_defaultDpi;
        }

        template<typename N>
        N scaleDpi(N pixels) {
            return pixels * static_cast<N>(m_dpi) / static_cast<N>(m_defaultDpi);
        }

        template<typename N>
        N unScaleDpi(N pixels) {
            return pixels * static_cast<N>(m_defaultDpi) / static_cast<N>(m_dpi);
        }
    private:
        UINT32 m_dpi = 96;
        static constexpr UINT32 m_defaultDpi = 96;
    };

protected:
    MainWindow::DialogProcResult CALLBACK DialogProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam);

    void OnSize();
    void OnMove();
    DialogProcResult CALLBACK OnCommand(HWND hwnd, WPARAM wParam, LPARAM lParam);
    void UpdateRenderingMode();

    void UnloadCustomFonts();
    void ReloadFontSource();
    void OnDropFiles(HDROP drop);

    void OnTextChange();
	void OnFontFamilyChange(const uint32_t& wmEvent);
    void OnFontStyleChange(const uint32_t& wmEvent);
    void OnLocaleChange(const uint32_t& wmEvent);
    void OnFontSizeChange(const uint32_t& wmEvent);
    void OnFontDirectionChange(const uint32_t& wmEvent);
    void OnFeaturesEnabledChange();
    void OnFontFeatureSettingsChange();
    void OnVariationEnabledChange();
    void OnFontVariationSettingsChange();
    void ToggleFontFallback();
    void ToggleJustify();

    void OnSelectNumber(uint32_t idc, const uint32_t& wmEvent, std::function<uint32_t(const FontSelector&)> fnGet, std::function<void(FontSelector&, uint32_t)> fnSet);
    void OnSelectIndex(uint32_t idc, uint32_t min, uint32_t max, std::function<void(FontSelector&, uint32_t)> fn);
    
    void ReflowLayout();
    void SetLayoutText(bool fInitialize, const UINT& commandId);

    void DeferUpdateUi(NeedUpdateUi neededUiUpdate = NeedUpdateUi::None, uint32_t timeOut = 50);
    void UpdateUi();
    void UpdateNumberSelect(uint32_t idc, const uint32_t* grades, const size_t size, const uint32_t currentValue);
    void UpdateIndexSelect(uint32_t idc, const WCHAR* const* grades, const size_t size, const uint32_t currentValue);
	void UpdateEditFontFamily();
	void UpdateSelectStyle();
    void UpdateCheckFeaturesEnabled();
	void UpdateEditFeatureSettings();
    void UpdateCheckVariationEnabled();
    void UpdateEditVariation();
    void UpdateSelectFontSize();
    void UpdateSelectLocale();
    void UpdateFontSource();
    void UpdateEditText();

    wil::com_ptr<IDWriteFactory> m_dwriteFactory;
    wil::com_ptr<IDWriteRenderingParams> m_renderingParams;
    wil::com_ptr<IDWriteBitmapRenderTarget> m_renderTarget;

    FontSelector m_fontSelector;
    std::unique_ptr<TextLayout> m_textLayout;
    std::unique_ptr<FlowFontSource> m_fontSource;

protected:
    HWND m_hwnd;
    HMONITOR m_hMonitor;

    UiScaffold CreateUiScaffold() {
        return UiScaffold(m_hwnd);
    }

private:
    // No copy construction allowed.
    MainWindow(const MainWindow& b);
    MainWindow& operator=(const MainWindow&);

    bool m_isRecursing = false;
    NeedUpdateUi m_needUpdateUi = NeedUpdateUi::None;
};

// Util functions

template<typename T>
T* GetClassFromWindow(HWND hwnd) {
    return reinterpret_cast<T*>(::GetWindowLongPtr(hwnd, DWLP_USER));
}
