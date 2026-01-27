#include "Camera.h"
#include "DirectXCommon.h"
#include "../Game/Drone/Drone.h"

Camera::Camera()
    : transform_({ { 1.0f, 1.0f, 1.0f }, { 0.0f, 0.0f, 0.0f }, { 0.0f, 0.0f, 0.0f } })
    , fovY_(0.45f)
    , aspectRatio_(static_cast<float>(WinApp::kClientWidth) / static_cast<float>(WinApp::kClientHeight))
    , nearClip_(0.1f)
    , farClip_(100.0f)
    , worldMatrix_(MatrixMath::MakeAffineMatrix(transform_.scale, transform_.rotate, transform_.translate))
    , viewMatrix_(MatrixMath::Inverse(worldMatrix_))
    , projectionMatrix_(MatrixMath::MakePerspectiveFovMatrix(fovY_, aspectRatio_, nearClip_, farClip_))
    , viewProjectionMatrix_(MatrixMath::Multiply(viewMatrix_, projectionMatrix_)) {
}

void Camera::Initialize() {
    cameraResource_ = DirectXCommon::GetInstance()->CreateBufferResource(sizeof(CameraForGPU));

    cameraResource_->Map(0, nullptr, reinterpret_cast<void**>(&cameraData_));

}

void Camera::Update() {
    assert(cameraData_ && "Camera::Initialize() is not called");
    cameraData_->worldPosition = transform_.translate;

    worldMatrix_ = MatrixMath::MakeAffineMatrix(transform_.scale, transform_.rotate, transform_.translate);

    if (useCustomView_) {
        viewMatrix_ = customView_;                 // ★LookAtの結果を使う
    }
    else {
        viewMatrix_ = MatrixMath::Inverse(worldMatrix_);
    }

    projectionMatrix_ = MatrixMath::MakePerspectiveFovMatrix(
        0.45f,
        static_cast<float>(WinApp::kClientWidth) / static_cast<float>(WinApp::kClientHeight),
        0.1f, 100.0f
    );

    viewProjectionMatrix_ = MatrixMath::Multiply(viewMatrix_, projectionMatrix_);
}


void Camera::DebugUpdate() {
#ifdef USE_IMGUI

    ImGui::Begin("Settings");

    // 位置（移動）
    ImGui::DragFloat3("CameraTranslate", &transform_.translate.x, 0.01f, -50.0f, 50.0f);

    // 回転（角度）
    ImGui::SliderAngle("CameraRotateX", &transform_.rotate.x);
    ImGui::SliderAngle("CameraRotateY", &transform_.rotate.y);
    ImGui::SliderAngle("CameraRotateZ", &transform_.rotate.z);

    ImGui::End();

#endif
}

void Camera::FollowDroneRigid(const Drone& drone, float backDist, float height, float pitchRad, float yawOffset) {
    const Vector3 target{ drone.GetPos().x, drone.GetPos().y, drone.GetPos().z };

    const float yawBase = -drone.GetYaw() + yawOffset; // ここがポイント
    const float s = std::sinf(yawBase);
    const float c = std::cosf(yawBase);
    const Vector3 forward{ s, 0.0f, c };

    Vector3 eye{
        target.x - forward.x * backDist,
        target.y + height,
        target.z - forward.z * backDist
    };

    transform_.translate = eye;

    // いまのあなたの計算（符号調整済み）
    Vector3 dir{ target.x - eye.x, target.y - eye.y, target.z - eye.z };
    float len = std::sqrt(dir.x * dir.x + dir.y * dir.y + dir.z * dir.z);
    if (len > 1e-6f) { dir.x /= len; dir.y /= len; dir.z /= len; }

    const float camYaw = std::atan2f(-dir.x, dir.z);
    const float camPitch = -std::asinf(std::clamp(dir.y, -1.0f, 1.0f));
    transform_.rotate = { camPitch, camYaw, 0.0f };
}

void Camera::SetViewLookAt(const Vector3& eye, const Vector3& target, const Vector3& up) {
    useCustomView_ = true;
    customView_ = MatrixMath::MakeLookAtMatrix(eye, target, up); // ←あなたのLookAt関数名に合わせて
}
void Camera::ClearCustomView() {
    useCustomView_ = false;
}

void Camera::SetCustomView(const Matrix4x4& v) {
    useCustomView_ = true;
    customView_ = v;
}
