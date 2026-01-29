#pragma once

#include <algorithm>
#include <cmath>
#include <dinput.h>

#include "MathStruct.h" // Vector3

#include <Xinput.h>
#pragma comment(lib, "Xinput.lib")
#include "../3D/Object3dManager.h"
#include "../Game/LandingEffect/LandingEffect.h"
#include "../Particle/ParticleManager.h"
#include "Camera.h"
class Input;

class Drone {
public:
    void Initialize(const Vector3& startPos = { 0, 0, 0 })
    {
        pos_ = startPos;
        yaw_ = 0.0f;
        vel_ = { 0, 0, 0 };

        pitch_ = 0.0f;
        roll_ = 0.0f;
        yawVel_ = 0.0f;
        pitchVel_ = 0.0f;
        rollVel_ = 0.0f;
        pitchI_ = 0.0f;
        rollI_ = 0.0f;
    }

    void UpdateMode1(const Input& input, float dt);

    // 追加：慣性なし（デバッグ用）
    void UpdateDebugNoInertia(const Input& input, float dt);

    const Vector3& GetPos() const { return pos_; }
    float GetYaw() const { return yaw_; }
    float GetPitch() const { return pitch_; }
    float GetRoll() const { return roll_; }

    void SetPos(const Vector3& p) { pos_ = p; }
    void SetVel(const Vector3& v) { vel_ = v; }
    const Vector3& GetVel() const { return vel_; }
    void SetYaw(float y) { yaw_ = y; }

    bool HasJustLanded() const;
    Vector3 GetForwardWithVisual(float yawOffset) const;

private:
    Vector3 pos_ { 0, 0, 0 };
    Vector3 vel_ { 0, 0, 0 };

    // 慣性なし用の移動速度
    float debugMoveSpeed_ = 8.0f; // units/sec
    float debugUpSpeed_ = 6.0f; // units/sec
    float debugYawSpeed_ = 2.5f; // rad/sec

    float yaw_ = 0.0f; // rad
    float pitch_ = 0.0f; // rad（+で上向き）
    float roll_ = 0.0f; // rad（+で右ロール想定）

    float yawVel_ = 0.0f;
    float pitchVel_ = 0.0f;
    float rollVel_ = 0.0f;

    // PID用（I項。PDだけでも十分だけど欲しいなら）
    float pitchI_ = 0.0f;
    float rollI_ = 0.0f;

    // ===== 入力→目標角度 =====
    float maxTiltRad_ = 0.50f; // 最大傾き（約28.6度）0.35?0.7くらいで調整

    // ===== 角度制御（PIDっぽい）=====
    float tiltKp_ = 18.0f; // P
    float tiltKd_ = 6.0f; // D（ダンピング）
    float tiltKi_ = 0.0f; // I（まず0でOK、必要なら0.5?2くらい）
    float tiltIMax_ = 0.25f; // Iの暴走防止

    // ===== 飛行感（加速度）=====
    float gravity_ = 9.8f; // ゲーム単位に合わせてOK
    float verticalAccel_ = 10.0f; // 上下入力の加速度
    float linearDrag_ = 0.25f; // 速度抵抗（大きいほど止まりやすい）

    // ===== ヨー（旋回）は今まで通り =====
    float turnAccel_ = 2.5f; // rad/s^2（入力1.0のとき）
    float yawDrag_ = 6.0f; // 角速度減衰

    float minY_ = -5.0f; // 地面の高さ

    Camera camera_;
    bool justLanded_ = false;

    bool wasOnGround_ = false;
};
