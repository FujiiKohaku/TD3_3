#pragma once
#include "BaseScene.h"

#include "../3D/Object3d.h"
#include "SceneManager.h"
#include "Camera.h"
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
};
