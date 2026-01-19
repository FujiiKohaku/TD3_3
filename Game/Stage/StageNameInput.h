// StageNameInput.h
#pragma once
#include <string>
#include <windows.h>

struct StageNameInput
{
    std::wstring committed;   // 確定済み
    std::wstring composing;   // 変換中（下線付けたい）
    bool imeActive = false;
    int  compCursor = 0;      // composing内のカーソル位置（GCS_CURSORPOS）

    void Clear() {
        committed.clear();
        composing.clear();
        imeActive = false;
        compCursor = 0;
    }

    // 表示用（確定 + 変換中）
    std::wstring GetDisplayText() const {
        return committed + composing;
    }

    // 下線を付ける範囲（displayText 内で）
    // start = committed.size(), length = composing.size()
    UINT32 GetUnderlineStart() const { return (UINT32)committed.size(); }
    UINT32 GetUnderlineLength() const { return (UINT32)composing.size(); }
};
