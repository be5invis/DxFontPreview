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

#include "Common.h"
#include "resource.h"
#include "WindowUtil.h"
#include "DxFontPreview.h"

////////////////////////////////////////
// Main entry.

const wchar_t* MainWindow::g_windowClassName = L"WC_DXFontPreviewMainWindow";

int APIENTRY wWinMain(
    HINSTANCE   hInstance,
    HINSTANCE   hPrevInstance,
    LPWSTR      commandLine,
    int         nCmdShow
) {
    // The Microsoft Security Development Lifecycle recommends that all
    // applications include the following call to ensure that heap corruptions
    // do not go unnoticed and therefore do not introduce opportunities
    // for security exploits.
    HeapSetInformation(NULL, HeapEnableTerminationOnCorruption, NULL, 0);

    MainWindow::RegisterWindowClass();

    HWND mainHwnd = CreateDialog(HINST_THISCOMPONENT, MAKEINTRESOURCE(IddMainWindow), nullptr, &MainWindow::StaticDialogProc);
    if (mainHwnd == nullptr) { return 1; }
    else { MainWindow::RunMessageLoop(); }

    return 0;
}

ATOM MainWindow::RegisterWindowClass() {
    // Registers the main window class.

    // Ensure that the common control DLL is loaded.
    // (probably not needed, but do it anyway)
    INITCOMMONCONTROLSEX icex;
    icex.dwSize = sizeof(INITCOMMONCONTROLSEX);
    icex.dwICC = ICC_STANDARD_CLASSES | ICC_LISTVIEW_CLASSES | ICC_LINK_CLASS;
    InitCommonControlsEx(&icex);

    return 0;
}

WPARAM MainWindow::RunMessageLoop() {
    MSG msg;
    while (GetMessage(&msg, nullptr, 0, 0) > 0) {
        bool messageHandled = false;

        const DWORD style = GetWindowStyle(msg.hwnd);
        HWND dialog = msg.hwnd;

        if (style & WS_CHILD)
            dialog = GetAncestor(msg.hwnd, GA_ROOT);

        // Send the Return key to the right control (one with focus) so that
        // we get a NM_RETURN from that control, not IDOK to the parent window.
        if (!messageHandled && msg.message == WM_KEYDOWN && msg.wParam == VK_RETURN) {
            messageHandled = !SendMessage(dialog, msg.message, msg.wParam, msg.lParam);
        }
        if (!messageHandled) {
            // Let the default dialog processing check it.
            messageHandled = !!IsDialogMessage(dialog, &msg);
        }
        if (!messageHandled) {
            // Not any of the above, so just handle it.
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
    }
    return msg.wParam;
}

void MainWindow::Initialize() {
    // Create the DWrite factory.
    THROW_IF_FAILED(DWriteCreateFactory(DWRITE_FACTORY_TYPE_SHARED, __uuidof(IDWriteFactory), m_dwriteFactory.put_unknown()));
    
    ShowWindow(m_hwnd, SW_SHOWNORMAL);
    UpdateWindow(m_hwnd);

    // Initialize the render target.
    {
        wil::com_ptr< IDWriteGdiInterop> gdiInterop;
        THROW_IF_FAILED(m_dwriteFactory->GetGdiInterop(&gdiInterop));

        RECT clientRect;
        GetClientRect(m_hwnd, &clientRect);

        HDC hdc = GetDC(m_hwnd);
        THROW_IF_FAILED(gdiInterop->CreateBitmapRenderTarget(hdc, clientRect.right, clientRect.bottom, &m_renderTarget));
        ReleaseDC(m_hwnd, hdc);
    }

    // Create our custom layout, source, and sink.

    m_fontSource = std::make_unique<FlowFontSource>(m_dwriteFactory);
    m_textLayout = std::make_unique<TextLayout>(m_dwriteFactory);
    
    THROW_IF_NULL_ALLOC(m_textLayout);
    THROW_IF_NULL_ALLOC(m_fontSource);

    auto scaffold = CreateUiScaffold();
    HFONT hFontEdit = CreateFont(scaffold.scaleDpi(16), 0, 0, 0, FW_DONTCARE, FALSE, FALSE, FALSE, ANSI_CHARSET,
        OUT_TT_PRECIS, CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY,
        DEFAULT_PITCH | FF_DONTCARE, TEXT("Consolas"));

    SendMessageW(GetDlgItem(m_hwnd, IdcEditText), WM_SETFONT, (WPARAM)hFontEdit, TRUE);

    SetLayoutText(true, CommandIdTextLatin);
    OnMove();
    OnSize();
    InvalidateRect(m_hwnd, NULL, FALSE);
}


INT_PTR CALLBACK MainWindow::StaticDialogProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam) {
    MainWindow* window = GetClassFromWindow<MainWindow>(hwnd);
    if (window == nullptr) {
        window = new(std::nothrow) MainWindow(hwnd);
        if (window == nullptr) {
            return -1; // failed creation
        }

        ::SetWindowLongPtr(hwnd, DWLP_USER, (LONG_PTR)window);
    }

    const DialogProcResult result = window->DialogProc(hwnd, message, wParam, lParam);
    if (result.handled) {
        ::SetWindowLongPtr(hwnd, DWLP_MSGRESULT, result.value);
    }
    return result.handled;
}

MainWindow::DialogProcResult CALLBACK MainWindow::DialogProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam) {
    if (m_isRecursing)
        return false;

    switch (message) {
    case WM_INITDIALOG:
        Initialize();
        return DialogProcResult(true, 1); // Let focus be set to first control.

    case WM_COMMAND:
        return OnCommand(hwnd, wParam, lParam);

    case WM_MOVE:
        OnMove();
        break;

    case WM_SIZE:
        OnSize();
        break;

    case WM_TIMER:
        if (wParam == IdcUpdateUi) {
            KillTimer(hwnd, wParam);
            UpdateUi();
        } else {
            return false;
        }
        break;

    case WM_NCDESTROY:
        delete this; // do NOT reference class after this
        PostQuitMessage(0);
        return true;

    case WM_KEYDOWN:
        return DialogProcResult(true, SendMessage(GetFocus(), message, wParam, lParam));

    case WM_DROPFILES:
        OnDropFiles((HDROP)wParam);
        return true;

    default:
        return false; // unhandled.
    }

    return true;
}

MainWindow::DialogProcResult CALLBACK MainWindow::OnCommand(HWND hwnd, WPARAM wParam, LPARAM lParam) {

    uint32_t wmId = LOWORD(wParam);
    uint32_t wmEvent = HIWORD(wParam);
    UNREFERENCED_PARAMETER(wmEvent);

    // Handles menu commands.

    switch (wmId) {
    case IDCANCEL:
    case IDCLOSE:
    {
        DestroyWindow(hwnd); // Do not reference class after this
        PostQuitMessage(0);
    }
    break;

    case CommandIdTextLatin:
    case CommandIdTextArabic:
    case CommandIdTextJapanese:
        SetLayoutText(false, wmId);
        break;

    case CommandIdDirectionLeftToRightTopToBottom:
    case CommandIdDirectionRightToLeftTopToBottom:
    case CommandIdDirectionLeftToRightBottomToTop:
    case CommandIdDirectionRightToLeftBottomToTop:
    case CommandIdDirectionTopToBottomLeftToRight:
    case CommandIdDirectionBottomToTopLeftToRight:
    case CommandIdDirectionTopToBottomRightToLeft:
    case CommandIdDirectionBottomToTopRightToLeft:
        OnFontDirectionChange(wmId);
        break;

    case CommandIdToggleFontFallback:
        ToggleFontFallback();
        break;

    case CommandIdToggleJustify:
        ToggleJustify();
        break;

    case CommandIdToggleParseEscape:
        ToggleEsacpe();
        break;

    case IdcEditFontFamilyName:
        switch(wmEvent) {
        case CBN_SELCHANGE:
        case CBN_EDITCHANGE:
            OnFontFamilyChange(wmEvent);
            break;
        }
        break;
                
    case IdcSelectStyle:
        switch (wmEvent) {
        case CBN_SELCHANGE:
        case CBN_EDITCHANGE:
            OnFontStyleChange(wmEvent);
            break;
        }
        break;

    case IdcCheckFeatureEnabled:
        switch (wmEvent) {
        case BN_CLICKED:
            OnFeaturesEnabledChange();
            break;
        }
        break;

    case IdcEditFeatureSettings:
        switch (wmEvent) {
        case EN_CHANGE:
            OnFontFeatureSettingsChange();
            break;
        }
        break;

    case IdcCheckVariationEnabled:
        switch (wmEvent) {
        case BN_CLICKED:
            OnVariationEnabledChange();
            break;
        }
        break;

    case IdcEditVariationSettings:
        switch (wmEvent) {
        case EN_CHANGE:
            OnFontVariationSettingsChange();
            break;
        }
        break;
   
    case IdcSelectLocale:
        switch (wmEvent) {
        case CBN_SELCHANGE:
        case CBN_EDITCHANGE:
            OnLocaleChange(wmEvent);
            break;
        }
        break;

    case IdcSelectFontSize:
        switch (wmEvent) {
        case CBN_SELCHANGE:
        case CBN_EDITCHANGE:
            OnFontSizeChange(wmEvent);
            break;
        }
        break;

    case IdcEditText:
        switch (wmEvent) {
        case EN_CHANGE:
            OnTextChange();
        }
        break;

    case IdcBtnReload:
        switch (wmEvent) {
        case BN_CLICKED:        
            ReloadFontSource();
            break;        
        }
        break;

    case IdcBtnUnload:
        switch (wmEvent) {
        case BN_CLICKED:
            UnloadCustomFonts();
            break;
        }
        break;

    case IdcSampleTextButton:
        switch (wmEvent) {
        case BN_CLICKED:
            HMENU hMenu = LoadMenu(NULL, MAKEINTRESOURCE(IdmLoadSampleText));
            hMenu = GetSubMenu(hMenu, 0);
            RECT buttonRect;
            GetWindowRect(GetDlgItem(m_hwnd, IdcSampleTextButton), &buttonRect);
            CheckMenuItem(hMenu, CommandIdDirectionLeftToRightTopToBottom + m_fontSelector.readingDirection, MF_CHECKED);
            CheckMenuItem(hMenu, CommandIdToggleFontFallback, m_fontSelector.doFontFallback ? MF_CHECKED : MF_UNCHECKED);
            CheckMenuItem(hMenu, CommandIdToggleJustify, m_fontSelector.doJustify ? MF_CHECKED : MF_UNCHECKED);
            CheckMenuItem(hMenu, CommandIdToggleParseEscape, m_fontSelector.parseEscapes ? MF_CHECKED : MF_UNCHECKED);
            TrackPopupMenu(hMenu, TPM_LEFTALIGN, buttonRect.left, buttonRect.bottom, 0, m_hwnd, nullptr);            
            break;
        }
        break;

    default:
        return DialogProcResult(false, -1); // unhandled
    }

    return DialogProcResult(true, 0); // handled
}



void MainWindow::UnloadCustomFonts() {
    if (m_fontSource->IsUsingSystemFontSet()) return;
    m_fontSource->UseSystem();
    m_fontSelector.familyName = L"Calibri";
    m_fontSelector.styleName = L"Regular";
    m_textLayout->SetFont(*m_fontSource, m_fontSelector);
    DeferUpdateUi(NeedUpdateUi::FontSource | NeedUpdateUi::FontSelector);
    ReflowLayout();
}

void MainWindow::ReloadFontSource() {
    auto filePaths = m_fontSource->GetCurrentFilePaths();
    if (filePaths.size()) m_fontSource->UseFiles(filePaths);
    m_textLayout->SetFont(*m_fontSource, m_fontSelector);
    DeferUpdateUi(NeedUpdateUi::FontSource | NeedUpdateUi::FontSelector);
    ReflowLayout();
}

void MainWindow::OnDropFiles(HDROP drop) {
    auto const dropped_files_count = DragQueryFileW(drop, 0xFFFFFFFF, nullptr, 0);
    std::vector<std::wstring> filePaths;

    for (UINT i{ 0 }; dropped_files_count != i; ++i) {
        std::vector<WCHAR> buffer;
        auto const filePathCount{ DragQueryFileW(drop, i, nullptr, 0) };
        if (0 < filePathCount) {
            auto const buffer_size{ filePathCount + 1 };
            buffer.resize(buffer_size);
            auto const copiedSymbolsCount{ DragQueryFileW(drop, i, buffer.data(), buffer_size) };
            if (copiedSymbolsCount == filePathCount) { buffer.back() = L'\0'; }
        }
        if (buffer.size()) filePaths.push_back(std::wstring(&buffer[0]));
    }

    if (!m_textLayout) return;
    
    m_fontSource->UseFiles(filePaths);
    m_fontSource->GetDefaultSelector(m_fontSelector);
    m_textLayout->SetFont(*m_fontSource, m_fontSelector);
        

    DeferUpdateUi(NeedUpdateUi::FontSource | NeedUpdateUi::FontSelector);
    ReflowLayout();
}

void MainWindow::OnTextChange() {
    std::wstring userText = WinUtil::GetValueOfTextBox(GetDlgItem(m_hwnd, IdcEditText));
    m_textLayout->SetText(userText.c_str(), userText.size());
    ReflowLayout();
}

void MainWindow::OnFontFamilyChange(const uint32_t& wmEvent) {
    if (wmEvent == CBN_SELCHANGE) {
        m_fontSelector.familyName = WinUtil::GetValueOfComboBox(GetDlgItem(m_hwnd, IdcEditFontFamilyName));
    } else {
        m_fontSelector.familyName = WinUtil::GetValueOfTextBox(GetDlgItem(m_hwnd, IdcEditFontFamilyName));
    }

    std::set<std::wstring> styleSet;
    m_fontSource->EnumerateStyleNames(m_fontSelector.familyName, styleSet);
    if (styleSet.find(m_fontSelector.styleName) == styleSet.end()) {
        if (styleSet.find(L"Regular") != styleSet.end()) {
            m_fontSelector.styleName = L"Regular";
        } else if (styleSet.cbegin() != styleSet.cend()) {
            m_fontSelector.styleName = *styleSet.cbegin();
        }
    }

    m_textLayout->SetFont(*m_fontSource, m_fontSelector);
    ReflowLayout();
    DeferUpdateUi(NeedUpdateUi::FontStyle);
}

void MainWindow::OnLocaleChange(const uint32_t& wmEvent) {
    if (wmEvent == CBN_SELCHANGE) {
        m_fontSelector.localeName = WinUtil::GetValueOfComboBox(GetDlgItem(m_hwnd, IdcSelectLocale));
    } else {
        m_fontSelector.localeName = WinUtil::GetValueOfTextBox(GetDlgItem(m_hwnd, IdcSelectLocale));
    }
    m_textLayout->SetFont(*m_fontSource, m_fontSelector);
    ReflowLayout();
}

void MainWindow::OnFontStyleChange(const uint32_t& wmEvent) {
    if (wmEvent == CBN_SELCHANGE) {
        m_fontSelector.styleName = WinUtil::GetValueOfComboBox(GetDlgItem(m_hwnd, IdcSelectStyle));
    } else {
        m_fontSelector.styleName = WinUtil::GetValueOfTextBox(GetDlgItem(m_hwnd, IdcSelectStyle));
    }
    m_textLayout->SetFont(*m_fontSource, m_fontSelector);
    ReflowLayout();
}

void MainWindow::OnFontSizeChange(const uint32_t& wmEvent) {
    OnSelectNumber(IdcSelectFontSize, wmEvent,
        [](const FontSelector& fs) {return fs.fontEmSize; },
        [](FontSelector& fs, uint32_t value) {fs.fontEmSize = value; });
    ReflowLayout();
}


void MainWindow::OnFontDirectionChange(const uint32_t& wmEvent) {
    m_fontSelector.readingDirection = ReadingDirection(wmEvent - CommandIdDirectionLeftToRightTopToBottom); 
    m_textLayout->SetFont(*m_fontSource, m_fontSelector);
    ReflowLayout();
}

void MainWindow::OnFeaturesEnabledChange() {
    m_fontSelector.userFeaturesEnabled = !!IsDlgButtonChecked(m_hwnd, IdcCheckFeatureEnabled);
    m_textLayout->SetFont(*m_fontSource, m_fontSelector);
    DeferUpdateUi(NeedUpdateUi::FontFeatureSettings);
    ReflowLayout();
}

void MainWindow::OnFontFeatureSettingsChange() {
    m_fontSelector.userFeatureSettings = WinUtil::GetValueOfTextBox(GetDlgItem(m_hwnd, IdcEditFeatureSettings));
    m_textLayout->SetFont(*m_fontSource, m_fontSelector);
    ReflowLayout();
}

void MainWindow::OnVariationEnabledChange() {
    m_fontSelector.userVariationEnabled = !!IsDlgButtonChecked(m_hwnd, IdcCheckVariationEnabled);
    m_textLayout->SetFont(*m_fontSource, m_fontSelector);
    DeferUpdateUi(NeedUpdateUi::FontVariationSettings);
    ReflowLayout();
}

void MainWindow::OnFontVariationSettingsChange() {
    m_fontSelector.userVariationSettings = WinUtil::GetValueOfTextBox(GetDlgItem(m_hwnd, IdcEditVariationSettings));
    m_textLayout->SetFont(*m_fontSource, m_fontSelector);
    ReflowLayout();
}

void MainWindow::ToggleFontFallback() {
    m_fontSelector.doFontFallback = !m_fontSelector.doFontFallback;
    m_textLayout->SetFont(*m_fontSource, m_fontSelector);
    ReflowLayout();
}

void MainWindow::ToggleJustify() {
    m_fontSelector.doJustify = !m_fontSelector.doJustify;
    m_textLayout->SetFont(*m_fontSource, m_fontSelector);
    ReflowLayout();
}

void MainWindow::ToggleEsacpe() {
    m_fontSelector.parseEscapes = !m_fontSelector.parseEscapes;
    m_textLayout->SetFont(*m_fontSource, m_fontSelector);
    ReflowLayout();
}

void MainWindow::OnSelectNumber(uint32_t idc, const uint32_t& wmEvent,
    std::function<uint32_t(const FontSelector&)> fnGet,
    std::function<void(FontSelector&, uint32_t)> fnSet) {
    uint32_t current = fnGet(m_fontSelector);
    if (wmEvent == CBN_SELCHANGE) {
        fnSet(m_fontSelector, int(WinUtil::GetNumOfComboBox(GetDlgItem(m_hwnd, idc), current)));
    } else {
        fnSet(m_fontSelector, int(WinUtil::GetNumOfTextBox(GetDlgItem(m_hwnd, idc), current)));
    }
    m_textLayout->SetFont(*m_fontSource, m_fontSelector);
}

void MainWindow::OnSelectIndex(uint32_t idc, uint32_t min, uint32_t max, std::function<void(FontSelector&, uint32_t)> fn) {
    LRESULT currentIndex = SendMessage(GetDlgItem(m_hwnd, idc), CB_GETCURSEL, (WPARAM)0, (LPARAM)0);
    if (currentIndex >= min && currentIndex <= max) {
        fn(m_fontSelector, currentIndex);
    }

    m_textLayout->SetFont(*m_fontSource, m_fontSelector);
};


void MainWindow::OnSize()
{
    // Update control position
    HWND hwnd = m_hwnd;
    long const spacing = 4;
    RECT clientRect;

    GetClientRect(hwnd, OUT & clientRect);
    RECT paddedClientRect = clientRect;
    InflateRect(IN OUT & paddedClientRect, -spacing, -spacing);

    WindowPosition windowPositions[] = {
        WindowPosition(GetDlgItem(hwnd, IdcEditFontFamilyName)),
        WindowPosition(GetDlgItem(hwnd, IdcSelectStyle)),
        WindowPosition(GetDlgItem(hwnd, IdcSelectFontSize)),
        WindowPosition(GetDlgItem(hwnd, IdcSelectLocale)),
        WindowPosition(GetDlgItem(hwnd, IdcFilePathInfo), PositionOptionsFillWidth),
        WindowPosition(GetDlgItem(hwnd, IdcBtnReload)),
        WindowPosition(GetDlgItem(hwnd, IdcBtnUnload)),
        WindowPosition(GetDlgItem(hwnd, IdcCheckFeatureEnabled), PositionOptionsNewLine),
        WindowPosition(GetDlgItem(hwnd, IdcEditFeatureSettings), PositionOptionsFillWidth),
        WindowPosition(GetDlgItem(hwnd, IdcCheckVariationEnabled)),
        WindowPosition(GetDlgItem(hwnd, IdcEditVariationSettings), PositionOptionsFillWidth),
        WindowPosition(GetDlgItem(hwnd, IdcSampleTextButton)),
        WindowPosition(GetDlgItem(hwnd, IdcEditText), PositionOptionsNewLine | PositionOptionsFillWidth),
        WindowPosition(GetDlgItem(hwnd, IdcDrawingCanvas), PositionOptionsFillWidth | PositionOptionsFillHeight | PositionOptionsNewLine)
    };

    // Assign width
    auto scaffold = CreateUiScaffold();
    windowPositions[0].SetWidth(scaffold.scaleDpi(192));
    windowPositions[1].SetWidth(scaffold.scaleDpi(96));
    windowPositions[2].SetWidth(scaffold.scaleDpi(48));
    windowPositions[3].SetWidth(scaffold.scaleDpi(72));
    windowPositions[4].SetWidth(scaffold.scaleDpi(192));

    WindowPosition::ReflowGrid(windowPositions, _countof(windowPositions), paddedClientRect, spacing, 0, PositionOptionsFlowHorizontal | PositionOptionsUnwrapped);
    WindowPosition::Update(windowPositions, _countof(windowPositions));

    WinUtil::AmendComboBoxItemWidth(GetDlgItem(hwnd, IdcSelectStyle));

    // Resizes the render target and flow source.
    RECT canvasRect;
    GetClientRect(GetDlgItem(hwnd, IdcDrawingCanvas), &canvasRect);
    if (m_renderTarget != nullptr) {
        m_renderTarget->Resize(canvasRect.right, canvasRect.bottom);
    }
    if (m_textLayout != nullptr) {
        float pixelsPerDip = m_renderTarget->GetPixelsPerDip();
        m_textLayout->SetSize(float(canvasRect.right) / pixelsPerDip, float(canvasRect.bottom) / pixelsPerDip);
    }

    ReflowLayout();
}


void MainWindow::OnMove()
{
    // Updates rendering parameters according to current monitor.

    if (m_dwriteFactory == NULL) return; // Not initialized yet.
    HMONITOR monitor = MonitorFromWindow(m_hwnd, MONITOR_DEFAULTTONEAREST);
    if (monitor == m_hMonitor) return;
    UpdateRenderingMode();
    if (m_renderingParams == NULL) return;
    m_hMonitor = monitor;
}

void MainWindow::UpdateRenderingMode() {
    if (m_dwriteFactory == NULL) return; // Not initialized yet.
    wil::com_ptr<IDWriteRenderingParams> screenMode;
    m_dwriteFactory->CreateMonitorRenderingParams(
        MonitorFromWindow(m_hwnd, MONITOR_DEFAULTTONEAREST),
        &screenMode
    );
    m_dwriteFactory->CreateCustomRenderingParams(
        screenMode->GetGamma(),
        screenMode->GetEnhancedContrast(),
        screenMode->GetClearTypeLevel(),
        screenMode->GetPixelGeometry(),
        DWRITE_RENDERING_MODE_NATURAL_SYMMETRIC,
        &m_renderingParams
    );
}


void MainWindow::ReflowLayout() {
    RECT canvasRect;
    HWND hwndStatic = GetDlgItem(m_hwnd, IdcDrawingCanvas);
    GetClientRect(hwndStatic, &canvasRect);
    HDC memoryHdc = m_renderTarget->GetMemoryDC();

    // Clear background.
    SetDCBrushColor(memoryHdc, GetSysColor(COLOR_WINDOW));
    SelectObject(memoryHdc, GetStockObject(NULL_PEN));
    SelectObject(memoryHdc, GetStockObject(DC_BRUSH));
    Rectangle(memoryHdc, 0, 0, canvasRect.right, canvasRect.bottom);

    // Draw all of the produced glyph runs.
    m_textLayout->Render(m_renderTarget, m_renderingParams);

    // Blit
    HBITMAP bitmapSrc = (HBITMAP) GetCurrentObject(memoryHdc, OBJ_BITMAP);
    HBITMAP bitmapCopy = (HBITMAP)CopyImage(bitmapSrc, IMAGE_BITMAP, 0, 0, LR_DEFAULTSIZE);
    HBITMAP bitmapOld = (HBITMAP) SendDlgItemMessage(m_hwnd, IdcDrawingCanvas, STM_SETIMAGE, IMAGE_BITMAP, (LPARAM)bitmapCopy);
    if (bitmapOld && bitmapOld != bitmapCopy) DeleteObject(bitmapOld);
}


void MainWindow::SetLayoutText(bool fInit, const UINT& commandId)
{    
    ReadingDirection readingDirection = ReadingDirection::ReadingDirectionLeftToRightTopToBottom;
    const wchar_t* text = L"";
    const wchar_t* fontName = L"";
    const wchar_t* styleName = L"Regular";
    const wchar_t* localeName = L"";

    switch (commandId) {
    case CommandIdTextLatin:
        fontName = L"Calibri";
        localeName = L"en-US";
        text = L"I can eat glass and it doesn't hurt me.";
        break;

    case CommandIdTextArabic:
        fontName = L"Calibri";
        localeName = L"ar-EG";
        text = L"أنا قادر على أكل الزجاج و هذا لا يؤلمني.";
        readingDirection = ReadingDirection::ReadingDirectionRightToLeftTopToBottom;
        break;

    case CommandIdTextJapanese:
        fontName = L"Yu Gothic";
        localeName = L"ja-JP";
        text = L"私はガラスを食べられます。それは私を傷つけません。";
        break;

    }


    if (m_fontSource->IsUsingSystemFontSet()) {
        m_fontSelector.familyName = fontName;
        m_fontSelector.styleName = styleName;
    }
    m_fontSelector.localeName = localeName;
    m_fontSelector.readingDirection = readingDirection;

    if (fInit) {
        m_fontSelector.fontEmSize = 72;
        m_fontSelector.userFeaturesEnabled = true;
        m_fontSelector.userFeatureSettings = L"calt, liga, clig, kern";
    }

    m_textLayout->SetFont(*m_fontSource, m_fontSelector);
    m_textLayout->SetText(text, static_cast<UINT32>(wcsnlen(text, UINT32_MAX)));
    
    if (!fInit) ReflowLayout();
    DeferUpdateUi(NeedUpdateUi::Text | NeedUpdateUi::FontSelector | NeedUpdateUi::FontSource);
}

void MainWindow::DeferUpdateUi(NeedUpdateUi neededUiUpdate, uint32_t timeOut) {
    m_needUpdateUi |= neededUiUpdate;
    if (m_needUpdateUi != NeedUpdateUi::None) {
        SetTimer(m_hwnd, IdcUpdateUi, timeOut, nullptr);
    }
}

void MainWindow::UpdateUi() {
    auto mask = m_needUpdateUi;
    m_needUpdateUi = NeedUpdateUi::None;

    m_isRecursing = true;
    if (NeedUpdateUi::None != (mask & NeedUpdateUi::FontFamily)) {
        UpdateEditFontFamily();
    }
    if (NeedUpdateUi::None != (mask & NeedUpdateUi::FontStyle)) {
        UpdateSelectStyle();
    }
    if (NeedUpdateUi::None != (mask & NeedUpdateUi::FontFeaturesEnabled)) {
        UpdateCheckFeaturesEnabled();
    }
    if (NeedUpdateUi::None != (mask & NeedUpdateUi::FontFeatureSettings)) {
        UpdateEditFeatureSettings();
    }
    if (NeedUpdateUi::None != (mask & NeedUpdateUi::FontVariationEnabled)) {
        UpdateCheckVariationEnabled();
    }
    if (NeedUpdateUi::None != (mask & NeedUpdateUi::FontVariationSettings)) {
        UpdateEditVariation();
    }
    if (NeedUpdateUi::None != (mask & NeedUpdateUi::FontSize)) {
        UpdateSelectFontSize();
    }
    if (NeedUpdateUi::None != (mask & NeedUpdateUi::FontLocale)) {
        UpdateSelectLocale();
    }
    if (NeedUpdateUi::None != (mask & NeedUpdateUi::FontSource)) {
        UpdateFontSource();
    }
    if (NeedUpdateUi::None != (mask & NeedUpdateUi::Text)) {
        UpdateEditText();
    }
    m_isRecursing = false;
}

void MainWindow::UpdateEditFontFamily() {
    std::set<std::wstring> fontFamilySet;
    m_fontSource->EnumerateFamilyNames(fontFamilySet);

    HWND hwnd = GetDlgItem(m_hwnd, IdcEditFontFamilyName);

    SendMessage(hwnd, CB_RESETCONTENT, 0, 0);

    for (const auto& item : fontFamilySet) {
        SendMessage(hwnd, CB_ADDSTRING, 0, LPARAM(item.c_str()));
    }

    WinUtil::SetComboBoxItem(hwnd, m_fontSelector.familyName);
}

void MainWindow::UpdateSelectStyle() {
    std::set<std::wstring> styleSet;
    m_fontSource->EnumerateStyleNames(m_fontSelector.familyName, styleSet);

    HWND hwnd = GetDlgItem(m_hwnd, IdcSelectStyle);

    SendMessage(hwnd, CB_RESETCONTENT, 0, 0);
    for (const auto& item : styleSet) {
        SendMessage(hwnd, CB_ADDSTRING, 0, LPARAM(item.c_str()));
    }

    WinUtil::SetComboBoxItem(hwnd, m_fontSelector.styleName);
}

void MainWindow::UpdateCheckFeaturesEnabled() {
    SendMessage(GetDlgItem(m_hwnd, IdcCheckFeatureEnabled), BM_SETCHECK, m_fontSelector.userFeaturesEnabled ? BST_CHECKED : BST_UNCHECKED, 0);
}

void MainWindow::UpdateEditFeatureSettings() {
    SendMessage(GetDlgItem(m_hwnd, IdcEditFeatureSettings), WM_SETTEXT, 0, LPARAM(m_fontSelector.userFeatureSettings.c_str()));
    EnableWindow(GetDlgItem(m_hwnd, IdcEditFeatureSettings), m_fontSelector.userFeaturesEnabled);
}

void MainWindow::UpdateCheckVariationEnabled() {
    SendMessage(GetDlgItem(m_hwnd, IdcCheckVariationEnabled), BM_SETCHECK, m_fontSelector.userVariationEnabled ? BST_CHECKED : BST_UNCHECKED, 0);
}

void MainWindow::UpdateEditVariation() {
    SendMessage(GetDlgItem(m_hwnd, IdcEditVariationSettings), WM_SETTEXT, 0, LPARAM(m_fontSelector.userVariationSettings.c_str()));
    EnableWindow(GetDlgItem(m_hwnd, IdcEditVariationSettings), m_fontSelector.userVariationEnabled);
}


void MainWindow::UpdateIndexSelect(uint32_t idc, const WCHAR* const* grades, const size_t size, const uint32_t currentValue){
    HWND hwnd = GetDlgItem(m_hwnd, idc);

    SendMessage(hwnd, CB_RESETCONTENT, 0, 0);
    for (int index = 0; index < size; index++) {
        SendMessage(hwnd, CB_ADDSTRING, 0, LPARAM(grades[index]));
    }

    if (currentValue < size) {
        WinUtil::SetComboBoxItem(hwnd, grades[currentValue]);
    }
}

void MainWindow::UpdateNumberSelect(uint32_t idc, const uint32_t* grades, const size_t size, const uint32_t currentValue) {
    HWND hwnd = GetDlgItem(m_hwnd, idc);
    SendMessage(hwnd, CB_RESETCONTENT, 0, 0);
    for (int index = 0; index < size; index++) {
        SendMessage(hwnd, CB_ADDSTRING, 0, LPARAM(std::to_wstring(grades[index]).c_str()));
    }
    WinUtil::SetComboBoxItem(hwnd, std::to_wstring(currentValue));
}

constexpr uint32_t SizeGrades[]{ 6,7,8,9,10,11,12,13,14,15,16,17,18,20,22,24,28,32,36,48,72,120,144,160 };
void MainWindow::UpdateSelectFontSize() {
    UpdateNumberSelect(IdcSelectFontSize, SizeGrades, _countof(SizeGrades), m_fontSelector.fontEmSize);
}

const static wchar_t* KnownLocales[][2] = {
    { L"English US", L"en-US"},
    { L"English UK", L"en-GB"},
    { L"الْعَرَبيّة Arabic Egypt", L"ar-EG"},
    { L"الْعَرَبيّة Arabic Iraq", L"ar-IQ"},
    { L"中文 Chinese PRC", L"zh-CN"},
    { L"中文 Chinese Taiwan", L"zh-TW"},
    { L"한글 Hangul Korea", L"ko-KR"},
    { L"עִבְרִית Hebrew Israel", L"he-IL"},
    { L"हिन्दी Hindi India", L"hi-IN"},
    { L"日本語 Japanese", L"ja-JP"},
    { L"Romania" , L"ro-RO"},
    { L"Русский язык Russian", L"ru-RU"},
    { L"ca-ES",        L"ca-ES"},
    { L"cs-CZ",        L"cs-CZ"},
    { L"da-DK",        L"da-DK"},
    { L"de-DE",        L"de-DE"},
    { L"el-GR",        L"el-GR"},
    { L"es-ES",        L"es-ES"},
    { L"es-ES_tradnl", L"es-ES_tradnl"},
    { L"es-MX",        L"es-MX"},
    { L"eu-ES",        L"eu-ES"},
    { L"fi-FI",        L"fi-FI"},
    { L"fr-CA",        L"fr-CA"},
    { L"fr-FR",        L"fr-FR"},
    { L"hu-HU",        L"hu-HU"},
    { L"it-IT",        L"it-IT"},
    { L"nb-NO",        L"nb-NO"},
    { L"nl-NL",        L"nl-NL"},
    { L"pl-PL",        L"pl-PL"},
    { L"pt-BR",        L"pt-BR"},
    { L"pt-PT",        L"pt-PT"},
    { L"ru-RU",        L"ru-RU"},
    { L"sk-SK",        L"sk-SK"},
    { L"sl-SI",        L"sl-SI"},
    { L"sv-SE",        L"sv-SE"},
    { L"tr-TR",        L"tr-TR"},
};
void MainWindow::UpdateSelectLocale() {
    HWND hwnd = GetDlgItem(m_hwnd, IdcSelectLocale);

    SendMessage(hwnd, CB_RESETCONTENT, 0, 0);
    for (int index = 0; index < _countof(KnownLocales); index++) {
        SendMessage(hwnd, CB_ADDSTRING, 0, LPARAM(KnownLocales[index][1]));
    }

    WinUtil::SetComboBoxItem(hwnd, m_fontSelector.localeName);
}

void MainWindow::UpdateFontSource() {
    std::wstringstream ss;
    std::vector<std::wstring> filePaths = m_fontSource->GetCurrentFilePaths();
    if (filePaths.size()) {
        ss << L"Using font set from ";
        ss << filePaths.size();
        ss << L" files: ";
        size_t n = 0;
        for (const auto& item : filePaths) {
            if (n) ss << L", ";
            ss << item;
            n++;
        }
    } else {
        ss << L"Using system font set.";
    }
    SendMessage(GetDlgItem(m_hwnd, IdcFilePathInfo), WM_SETTEXT, 0, LPARAM(ss.str().c_str()));

    bool fUsingCustomFonts = !m_fontSource->IsUsingSystemFontSet();
    EnableWindow(GetDlgItem(m_hwnd, IdcBtnReload), fUsingCustomFonts);
    EnableWindow(GetDlgItem(m_hwnd, IdcBtnUnload), fUsingCustomFonts);
}

void MainWindow::UpdateEditText() {
    const WCHAR* text;
    UINT32 size;
    m_textLayout->GetText(&text, &size);
    SendMessage(GetDlgItem(m_hwnd, IdcEditText), WM_SETTEXT, 0, LPARAM(text));
}
