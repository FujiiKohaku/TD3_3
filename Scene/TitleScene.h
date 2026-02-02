#pragma once
#include "../3D/Object3d.h"
#include "BaseScene.h"
#include "Camera.h"
#include "SceneManager.h"
#include <memory>
class TitleScene : public BaseScene {
public:
    void Initialize() override;

    void Finalize() override;

    void Update() override;

    void Draw2D() override;
    void Draw3D() override;
    void DrawImGui() override;

private:
    Vector3 i = { 0.0f, 0.0f, 0.0f };
    Camera* camera_;
    Object3d* outShellModel_ = nullptr;
    Object3d* homeModel_ = nullptr;
    Object3d* railModel_ = nullptr;
    std::unique_ptr<Object3d> doroso_ = nullptr;
    Object3d* drone_ = nullptr;

    Vector3 dronePos = { 0.0f, 5.0f, 0.6f };
    Vector3 droneRot = { 0.0f, 0.0f, -5.0f };
    Vector3 droneScale = { 0.1f, 0.1f, 0.1f };
    Vector3 pos1 = {};
    Vector3 rotate1 = {};
    Vector3 scale1 = {};
};
