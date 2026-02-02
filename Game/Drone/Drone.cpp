#include "Drone.h"
#include "Input.h"
#include <Xinput.h>
#pragma comment(lib, "Xinput.lib")

static float NormalizeStick(short v, short deadZone) {
	const int av = std::abs((int)v);
	if (av <= deadZone) return 0.0f;
	const float sign = (v >= 0) ? 1.0f : -1.0f;
	const float norm = (av - deadZone) / float(32767 - deadZone);
	return sign * std::clamp(norm, 0.0f, 1.0f);
}

static float ClampAbs(float v, float a) { return std::clamp(v, -a, a); }

void Drone::UpdateMode1(const Input& input, float dt) {
	// -------------------------
	// 0) 入力を -1..+1 にまとめる（キーボード+パッド合算）
	// -------------------------
	float inForward = 0.0f; // 前後（左スティックY / W,S）
	float inYaw = 0.0f; // 旋回（左スティックX / A,D）
	float inStrafe = 0.0f; // 左右移動（右スティックX / ←→）
	float inUpDown = 0.0f; // 上下（右スティックY / ↑↓）

	// keyboard
	if (input.IsKeyPressed(DIK_W)) inForward += 1.0f;
	if (input.IsKeyPressed(DIK_S)) inForward -= 1.0f;
	if (input.IsKeyPressed(DIK_A)) inYaw -= 1.0f;
	if (input.IsKeyPressed(DIK_D)) inYaw += 1.0f;
	if (input.IsKeyPressed(DIK_RIGHT)) inStrafe += 1.0f;
	if (input.IsKeyPressed(DIK_LEFT))  inStrafe -= 1.0f;
	if (input.IsKeyPressed(DIK_UP))    inUpDown += 1.0f;
	if (input.IsKeyPressed(DIK_DOWN))  inUpDown -= 1.0f;

	// gamepad (Mode1)
	XINPUT_STATE st{};
	if (XInputGetState(0, &st) == ERROR_SUCCESS) {
		const float lx = NormalizeStick(st.Gamepad.sThumbLX, XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE);
		const float ly = NormalizeStick(st.Gamepad.sThumbLY, XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE);
		const float rx = NormalizeStick(st.Gamepad.sThumbRX, XINPUT_GAMEPAD_RIGHT_THUMB_DEADZONE);
		const float ry = NormalizeStick(st.Gamepad.sThumbRY, XINPUT_GAMEPAD_RIGHT_THUMB_DEADZONE);

		inYaw += lx;
		inForward += ly;
		inStrafe += rx;
		inUpDown += ry;
	}

	inForward = std::clamp(inForward, -1.0f, 1.0f);
	inYaw = std::clamp(inYaw, -1.0f, 1.0f);
	inStrafe = std::clamp(inStrafe, -1.0f, 1.0f);
	inUpDown = std::clamp(inUpDown, -1.0f, 1.0f);

	// -------------------------
	// 1) Yaw（旋回）は角速度で
	// -------------------------
	yawVel_ += (-inYaw) * turnAccel_ * dt;
	yawVel_ -= yawVel_ * yawDrag_ * dt; // 減衰
	yaw_ += yawVel_ * dt;

	// -pi..pi
	constexpr float kPi = 3.14159265358979323846f;
	if (yaw_ > kPi) yaw_ -= 2.0f * kPi;
	if (yaw_ < -kPi) yaw_ += 2.0f * kPi;

	// -------------------------
	// 2) 目標Pitch/Roll を作る（ここが “自動で傾く” 部分）
	//    前進したい → 前傾（pitchはマイナス）
	//    右に移動したい → 右ロール（rollはプラス）
	// -------------------------
	const float targetPitch = -inForward * maxTiltRad_;
	const float targetRoll = inStrafe * maxTiltRad_;

	// -------------------------
	// 3) PIDっぽい安定化（まずは PD が超おすすめ）
	//    angle を target に追従させる
	// -------------------------
	auto UpdateTiltAxis = [&](float target, float& angle, float& angVel, float& I) {
		const float err = target - angle;

		// I（使うなら）
		if (tiltKi_ > 0.0f) {
			I += err * dt;
			I = ClampAbs(I, tiltIMax_);
		}
		else {
			I = 0.0f;
		}

		// PID: acc = Kp*err + Ki*I - Kd*vel
		const float angAcc = tiltKp_ * err + tiltKi_ * I - tiltKd_ * angVel;

		angVel += angAcc * dt;
		angle += angVel * dt;

		// 安全に最大傾きでクランプ
		angle = std::clamp(angle, -maxTiltRad_, maxTiltRad_);
		};

	UpdateTiltAxis(targetPitch, pitch_, pitchVel_, pitchI_);
	UpdateTiltAxis(targetRoll, roll_, rollVel_, rollI_);

	// -------------------------
	// 4) 傾き → 水平方向の加速度に変換（これが“前傾で滑る”）
	//
	//   実機近似：水平加速度 ≒ g * tan(傾き)
	// -------------------------
	// あなたの環境は forward/right を作るのに -yaw を使ってたのでここも合わせる
	const float yawMove = -yaw_;
	const float s = std::sinf(yawMove);
	const float c = std::cosf(yawMove);
	const Vector3 forward{ s, 0.0f, c };
	const Vector3 right{ c, 0.0f, -s };

	const float aF = gravity_ * std::tanf(-pitch_); // pitchがマイナス(前傾)で前に加速
	const float aR = gravity_ * std::tanf(roll_);  // rollプラスで右に加速

	Vector3 acc{};
	acc.x = forward.x * aF + right.x * aR;
	acc.z = forward.z * aF + right.z * aR;

	// 上下は入力で（ここも実機っぽくするなら後で“推力-重力”にできる）
	acc.y = inUpDown * verticalAccel_;

	// -------------------------
	// 5) 速度・位置更新（抵抗つき）
	// -------------------------
	vel_.x += acc.x * dt;
	vel_.y += acc.y * dt;
	vel_.z += acc.z * dt;

	// 線形抵抗（空気抵抗っぽく）
	vel_.x -= vel_.x * linearDrag_ * dt;
	vel_.y -= vel_.y * linearDrag_ * dt;
	vel_.z -= vel_.z * linearDrag_ * dt;

	pos_.x += vel_.x * dt;
	pos_.y += vel_.y * dt;
	pos_.z += vel_.z * dt;

	// -------------------------
// 6) 下降制限（地面）

// -------------------------
// 毎フレーム最初に
	justLanded_ = false;

	// -------------------------
	// 6) 下降制限（地面）
	// -------------------------
	bool onGroundNow = false;

	if (pos_.y < minY_) {
		pos_.y = minY_;
		onGroundNow = true;

		if (vel_.y < 0.0f) {
			vel_.y = 0.0f;
		}
	}

	// 着地した瞬間だけ
	if (onGroundNow && !wasOnGround_) {
		justLanded_ = true;
	}

	// 状態更新
	wasOnGround_ = onGroundNow;






}

void Drone::UpdateDebugNoInertia(const Input& input, float dt) {
	// -------------------------
	// 入力を -1..+1 にまとめる（Mode1と同じ）
	// -------------------------
	float inForward = 0.0f;
	float inYaw = 0.0f;
	float inStrafe = 0.0f;
	float inUpDown = 0.0f;

	// keyboard
	if (input.IsKeyPressed(DIK_W)) inForward += 1.0f;
	if (input.IsKeyPressed(DIK_S)) inForward -= 1.0f;
	if (input.IsKeyPressed(DIK_A)) inYaw -= 1.0f;
	if (input.IsKeyPressed(DIK_D)) inYaw += 1.0f;
	if (input.IsKeyPressed(DIK_RIGHT)) inStrafe += 1.0f;
	if (input.IsKeyPressed(DIK_LEFT))  inStrafe -= 1.0f;
	if (input.IsKeyPressed(DIK_UP))    inUpDown += 1.0f;
	if (input.IsKeyPressed(DIK_DOWN))  inUpDown -= 1.0f;

	// gamepad
	XINPUT_STATE st{};
	if (XInputGetState(0, &st) == ERROR_SUCCESS) {
		const float lx = NormalizeStick(st.Gamepad.sThumbLX, XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE);
		const float ly = NormalizeStick(st.Gamepad.sThumbLY, XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE);
		const float rx = NormalizeStick(st.Gamepad.sThumbRX, XINPUT_GAMEPAD_RIGHT_THUMB_DEADZONE);
		const float ry = NormalizeStick(st.Gamepad.sThumbRY, XINPUT_GAMEPAD_RIGHT_THUMB_DEADZONE);

		inYaw += lx;
		inForward += ly;
		inStrafe += rx;
		inUpDown += ry;
	}

	inForward = std::clamp(inForward, -1.0f, 1.0f);
	inYaw = std::clamp(inYaw, -1.0f, 1.0f);
	inStrafe = std::clamp(inStrafe, -1.0f, 1.0f);
	inUpDown = std::clamp(inUpDown, -1.0f, 1.0f);

	// -------------------------
	// 1) yaw は「慣性なし」でそのまま回す（角速度固定）
	// -------------------------
	yaw_ += (-inYaw) * debugYawSpeed_ * dt;

	constexpr float kPi = 3.14159265358979323846f;
	if (yaw_ > kPi) yaw_ -= 2.0f * kPi;
	if (yaw_ < -kPi) yaw_ += 2.0f * kPi;

	// -------------------------
	// 2) pitch/roll はデバッグなので 0 に戻す（見た目が安定する）
	//    ※「傾きも手で操作したい」ならここを変える
	// -------------------------
	pitch_ = 0.0f;
	roll_ = 0.0f;
	pitchVel_ = rollVel_ = 0.0f;
	pitchI_ = rollI_ = 0.0f;

	// -------------------------
	// 3) yaw に合わせて前/右方向を作り、入力のまま移動（慣性なし）
	// -------------------------
	const float yawMove = -yaw_;
	const float s = std::sinf(yawMove);
	const float c = std::cosf(yawMove);
	const Vector3 forward{ s, 0.0f, c };
	const Vector3 right{ c, 0.0f,-s };

	Vector3 move{};
	move.x = (forward.x * inForward + right.x * inStrafe) * debugMoveSpeed_;
	move.z = (forward.z * inForward + right.z * inStrafe) * debugMoveSpeed_;
	move.y = inUpDown * debugUpSpeed_;

	pos_.x += move.x * dt;
	pos_.y += move.y * dt;
	pos_.z += move.z * dt;

	// -------------------------
	// 4) vel はデバッグなので 0 にしておく（壁押し戻し等が楽）
	// -------------------------
	vel_ = { 0.0f, 0.0f, 0.0f };
}
bool Drone::HasJustLanded() const {
	return justLanded_;
}

