#pragma once
#include <string>
#include <Windows.h>

// IME入力を受け取って「確定文字列」「変換中文字列」を保持するだけのクラス
class TextInput {
public:
    static TextInput* GetInstance();

    // WndProcから呼ぶ
    // 戻り値 true: ここで処理したので DefWindowProc に渡さない（任意）
    bool HandleWndProc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam);

    void Clear();

    // 確定済み（Enterなどで確定した文字列が入る）
    const std::wstring& GetCommitted() const { return committed_; }

    // 変換中（下線付きの途中状態）
    const std::wstring& GetComposing() const { return composing_; }

    // 表示用（committed + composing）
    std::wstring GetDisplayString() const { return committed_ + composing_; }

    // 文字が更新されたか（スプライト更新のトリガー用）
    bool ConsumeDirty();

private:
    TextInput() = default;

private:
    std::wstring committed_;
    std::wstring composing_;
    bool dirty_ = false;
};
