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
#include "BitmapFont.h"

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

    void UpdateEditorInput(float dt);

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

    void DrawEditorParamHud_();

    void DrawGateIndices_();

    Vector3 camPosInit_{ 0.0f, 3.0f, -10.0f };
    float camYawInit_ = 0.0f;
    float camPitchInit_ = 0.0f;

    enum class EditMode { Gate, Wall, Goal };

    EditMode editMode_ = EditMode::Gate;
    int selectedGate_ = 0;
    int selectedWall_ = 0;

    // 操作量
    float moveStep_ = 0.2f;
    float rotStep_ = 0.05f;
    float sizeStep_ = 0.1f;

    // 保存/ロード用（最初は固定でもOK）
    std::string stageFile_ = "stage01.json";

    float goalAlpha_ = 0.25f;


    //2D
    Sprite* modeHud_ = nullptr;
    BitmapFont font_;
    BitmapFont gateNum;

    //操作方法
    bool showHelp_ = true;
    Vector2 helpPosLeft_ = { 16.0f, 48.0f };
    Vector2 helpPosRight_ = { 800.0f, 48.0f }; // 右カラム（画面幅に応じて調整）
    float   helpScale_ = 0.5f;



};
