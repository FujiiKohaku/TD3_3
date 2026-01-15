#pragma once
#include <cmath>
#include <algorithm>
#include <cstdint>

#include "MathStruct.h"
#include "MatrixMath.h"

enum class GateResult : uint8_t {
    None,
    Perfect,
    Good,
    Miss
};

struct Color4 {
    float r = 1, g = 1, b = 1, a = 1;
};

struct Gate {
    // --- 編集対象 ---
    Vector3 pos{ 0,0,0 };
    Vector3 rot{ 0,0,0 };    // rad
    Vector3 scale{ 1,1,1 };

    float perfectRadius = 1.0f;
    float gateRadius = 2.5f;
    float thickness = 0.8f;   // 仮デフォ（ImGuiで要調整想定）

    // --- 行列 ---
    Matrix4x4 world{};
    Matrix4x4 invWorld{};

    // --- 通過イベント用（前フレ値） ---
    float prevLocalZ = 0.0f;
    bool hasPrev = false;

    // --- 色フィードバック ---
    GateResult lastResult = GateResult::None;
    float feedbackTimer = 0.0f;
    float feedbackDuration = 0.35f;

    Color4 baseColor{ 1,1,1,1 };
    Color4 colorPerfect{ 0.2f, 0.7f, 1.0f, 1.0f };
    Color4 colorGood{ 0.2f, 1.0f, 0.3f, 1.0f };
    Color4 colorMiss{ 1.0f, 0.2f, 0.2f, 1.0f };


    // --- デバッグ用 ---
    Vector3 dbgLocalPos{};
    float   dbgPrevZ = 0.0f;
    bool    dbgCrossed = false;
    bool    dbgInThickness = false;
    float   dbgRadius = 0.0f;

    void UpdateMatrices();
    void Tick(float dt);

    // 通過“イベント”が発生したら true（Perfect/Good/Miss のどれかが返る）
    bool TryPass(const Vector3& droneWorldPos, GateResult& outResult);

    Color4 GetDrawColor() const;
};
