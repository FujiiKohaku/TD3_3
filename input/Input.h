#pragma once
#define DIRECTINPUT_VERSION 0x0800

#include "WinApp.h"
#include <dinput.h>
#include <wrl.h>

#pragma comment(lib, "dinput8.lib")
#pragma comment(lib, "dxguid.lib")

class Input {
public:
    static Input* GetInstance();

    bool Initialize(WinApp* winApp);
    void Finalize();
    void Update();

    // ---- Keyboard ----
    bool IsKeyPressed(BYTE keyCode) const;
    bool IsKeyTrigger(BYTE keyCode) const;

    // ---- Mouse ----
    bool IsMousePressed(int button) const;   // 0=L,1=R,2=M
    bool IsMouseTrigger(int button) const;   // 押した瞬間
    POINT GetMousePos() const;               // クライアント座標
    POINT GetMouseDelta() const;             // ★追加：差分

private:
    Input() = default;
    ~Input() = default;

    Input(const Input&) = delete;
    Input& operator=(const Input&) = delete;

private:
    Microsoft::WRL::ComPtr<IDirectInput8> directInput_;
    Microsoft::WRL::ComPtr<IDirectInputDevice8> keyboard_;
    Microsoft::WRL::ComPtr<IDirectInputDevice8> mouse_;

    DIMOUSESTATE mouseState_{};
    DIMOUSESTATE prevMouseState_{};

    POINT mousePos_{};        // 現在(クライアント)
    POINT prevMousePos_{};    // ★前フレ
    POINT mouseDelta_{};      // ★差分

    BYTE keys_[256]{};
    BYTE prevKeys_[256]{};

    WinApp* winApp_ = nullptr;

    static Input* instance;
};
