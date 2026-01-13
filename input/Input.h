#pragma once
#define DIRECTINPUT_VERSION 0x0800 // DirectInputのバージョン指定

#include "WinApp.h"
#include <dinput.h>
#include <wrl.h> // ComPtr 用

#pragma comment(lib, "dinput8.lib")
#pragma comment(lib, "dxguid.lib")

class Input {
public:
    // ================================
    // Singletonインスタンス取得
    // ================================
    static Input* GetInstance();

    bool Initialize(WinApp* winApp);
    void Finalize();
    void Update();

    // ================================
    // キー押下判定
    // ================================
    bool IsKeyPressed(BYTE keyCode) const;

    // ★追加：キーを押した瞬間（トリガー）
    bool IsKeyTrigger(BYTE keyCode) const;

    // ---- Mouse ----
    bool IsMousePressed(int button) const;   // 0=L,1=R,2=M
    bool IsMouseTrigger(int button) const;   // 押した瞬間
    POINT GetMousePos() const;               // クライアント座標

private:
    Input() = default;
    ~Input() = default;

    Input(const Input&) = delete;
    Input& operator=(const Input&) = delete;

private:
    Microsoft::WRL::ComPtr<IDirectInput8> directInput_ = nullptr;
    Microsoft::WRL::ComPtr<IDirectInputDevice8> keyboard_;

    Microsoft::WRL::ComPtr<IDirectInputDevice8> mouse_;

    DIMOUSESTATE mouseState_{};
    DIMOUSESTATE prevMouseState_{};
    POINT mousePos_{}; // 画面座標（client）

    BYTE keys_[256]{};      // 現在フレーム
    BYTE prevKeys_[256]{};  // ★前フレーム

    WinApp* winApp_ = nullptr;

    static Input* instance;
};
