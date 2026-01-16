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
#include "SceneManager.h"
#include "StageIO.h"
#include "StageSelectScene.h"

//ゲームプレイ用
#include "Input.h"
#include "Drone.h"
#include "Gate.h"
#include "GateVisual.h"
#include "Walls.h"
#include "GoalSystem.h"


class SphereObject;
class GamePlayScene : public BaseScene {
public:
    void Initialize() override;

    void Finalize() override;

    void Update() override;

    void Draw2D() override;
    void Draw3D() override;
    void DrawImGui() override;

  /*  void AddGate();

    void EditWallsImGui();

    bool LoadStageJson(const std::string& fileName);

    bool SaveStageJson(const std::string& fileName);

    void StageIOImGui();*/

private:
    // ------------------------------
    // サウンド
    // ------------------------------
    SoundData bgm;
    Sprite* sprite_ = nullptr;
    std::vector<Sprite*> sprites_;
    Object3d* player2_;

    ParticleEmitter emitter_;

    // ------------------------------
    // メッシュ
    // ------------------------------
    SphereObject* sphere_ = nullptr;

    Camera* camera_;

    bool sphereLighting = true;
    Vector3 spherePos = { 0.0f, 0.0f, 0.0f };
    Vector3 sphereRotate = { 0.0f, 0.0f, 0.0f }; // ラジアン想定
    Vector3 sphereScale = { 1.0f, 1.0f, 1.0f };
    float lightIntensity = 1.0f;
    Vector3 lightDir = { 0.0f, -1.0f, 0.0f };

    // --- Drone ---
    Object3d* droneObj_ = nullptr;
    Drone drone_;

    // --- 追従カメラ（後ろから見る） ---
    float camDist_ = 8.0f;
    float camHeight_ = 3.0f;
    float camPitch_ = -0.20f;

    int isDebug_ = false;

    float droneYawOffset = 0.0f; // rad

    //ゲート(リング)

    std::vector<GateVisual> gates_;
    int nextGate_ = 0;
    int perfectCount_ = 0;
    int goodCount_ = 0;

    //壁
    WallSystem wallSys_;
    Vector3 droneHalf_ = { 0.1f, 0.1f, 0.1f }; // ドローン当たり判定（半サイズ）
    bool drawWallDebug_ = true;

    //ゴール
    GoalSystem goalSys_;
    bool stageCleared_ = false; // クリア後の演出用（任意）

    bool requestBackToSelect_ = false;

};
