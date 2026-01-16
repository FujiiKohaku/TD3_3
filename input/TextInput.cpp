#include "TextInput.h"
#include <imm.h>
#pragma comment(lib, "imm32.lib")

TextInput* TextInput::GetInstance()
{
    static TextInput inst;
    return &inst;
}

void TextInput::Clear()
{
    committed_.clear();
    composing_.clear();
    dirty_ = true;
}

bool TextInput::ConsumeDirty()
{
    const bool d = dirty_;
    dirty_ = false;
    return d;
}

static std::wstring GetImeStringW(HWND hwnd, DWORD type)
{
    std::wstring out;

    HIMC hImc = ImmGetContext(hwnd);
    if (!hImc) return out;

    const LONG bytes = ImmGetCompositionStringW(hImc, type, nullptr, 0);
    if (bytes > 0) {
        out.resize(bytes / sizeof(wchar_t));
        ImmGetCompositionStringW(hImc, type, out.data(), bytes);
    }

    ImmReleaseContext(hwnd, hImc);
    return out;
}

bool TextInput::HandleWndProc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam)
{
    switch (msg) {
    case WM_IME_STARTCOMPOSITION:
        composing_.clear();
        dirty_ = true;
        return false; // DefWindowProcに流してOK

    case WM_IME_ENDCOMPOSITION:
        composing_.clear();
        dirty_ = true;
        return false;

    case WM_IME_COMPOSITION:
        // 変換中文字
        if (lparam & GCS_COMPSTR) {
            composing_ = GetImeStringW(hwnd, GCS_COMPSTR);
            dirty_ = true;
        }
        // 確定文字（ここで committed に追加する）
        if (lparam & GCS_RESULTSTR) {
            const std::wstring result = GetImeStringW(hwnd, GCS_RESULTSTR);
            if (!result.empty()) {
                committed_ += result;
                composing_.clear();
                dirty_ = true;
            }
        }
        return true; // ここで処理した、という扱いでOK（好み）

    case WM_CHAR:
        // IMEを使わない英数字などは WM_CHAR で来ることがある
        // Enter(13) はここでは無視（確定/保存トリガーは別キーでやるのが安全）
        if (wparam == VK_BACK) {
            if (!composing_.empty()) {
                composing_.pop_back();
                dirty_ = true;
            } else if (!committed_.empty()) {
                committed_.pop_back();
                dirty_ = true;
            }
            return true;
        }
        if (wparam >= 0x20) { // 制御文字除外
            committed_.push_back((wchar_t)wparam);
            dirty_ = true;
            return true;
        }
        return false;
    }

    return false;
}
