#pragma once
#include "BaseScene.h"
#include "Camera.h"
#include "ModelManager.h"
#include "Object3d.h"
#include "Object3dManager.h"
#include "ParticleEmitter.h"
#include "ParticleManager.h"
#include "SoundManager.h"
#include "Sprite.h"
#include "SpriteManager.h"
#include "TextureManager.h"

#include "Input.h"


#include "Drone.h"
#include "GateVisual.h"
#include "Walls.h"
#include "GoalSystem.h"

#include <vector>
#include <string>

class StageEditorScene : public BaseScene {
public:
    ~StageEditorScene() noexcept override = default;

    void Initialize() override;
    void Finalize() override;
    void Update() override;

    void UpdateFreeCamera(float dt);

    void Draw2D() override;
    void Draw3D() override;
    void DrawImGui() override;

private:
    // --- editor runtime ---
    Camera* camera_ = nullptr;

    Object3d* droneObj_ = nullptr;
    Drone drone_;
    Vector3 droneHalf_ = { 0.1f, 0.1f, 0.1f };

    std::vector<GateVisual> gates_;
    int nextGate_ = 0;

    WallSystem wallSys_;
    bool drawWallDebug_ = true;

    GoalSystem goalSys_;
    bool stageCleared_ = false; // Editorでは基本使わないが残してOK

    Vector3 camPos_{ 0.0f, 3.0f, -10.0f };
    float camYaw_ = 0.0f;
    float camPitch_ = 0.0f;

    float camMoveSpeed_ = 8.0f;
    float camMouseSens_ = 0.0025f;

    // --- editor functions (GamePlaySceneから移植) ---
    void AddGate();
    void EditWallsImGui();

    bool SaveStageJson(const std::string& fileName);
    bool LoadStageJson(const std::string& fileName);
    void StageIOImGui();
    void GoalEditorImGui();

    Vector3 camPosInit_{ 0.0f, 3.0f, -10.0f };
    float camYawInit_ = 0.0f;
    float camPitchInit_ = 0.0f;

};
