#include "GamePlayScene.h"
#include "../Light/LightManager.h"
#include "ParticleManager.h"
#include "SphereObject.h"
#include <numbers>

#include "../externals/nlohmann/json.hpp"
#include "ResultScene.h"
#include <fstream>
#include <string>

#include "FadeManager.h"
#include "TitleScene.h"
static SoundData DronePropellerSound_;
static SoundData gateSound_;
#pragma region 関数
using json = nlohmann::json;

static inline json ToJsonVec3(const Vector3& v)
{
    return json { { "x", v.x }, { "y", v.y }, { "z", v.z } };
}
static inline Vector3 FromJsonVec3(const json& j)
{
    return Vector3 { j.at("x").get<float>(), j.at("y").get<float>(), j.at("z").get<float>() };
}

// ===== World -> Screen (row-vector行列想定) =====
static Vector4 MulRowVec4Mat4(const Vector4& v, const Matrix4x4& m)
{
    Vector4 o {};
    o.x = v.x * m.m[0][0] + v.y * m.m[1][0] + v.z * m.m[2][0] + v.w * m.m[3][0];
    o.y = v.x * m.m[0][1] + v.y * m.m[1][1] + v.z * m.m[2][1] + v.w * m.m[3][1];
    o.z = v.x * m.m[0][2] + v.y * m.m[1][2] + v.z * m.m[2][2] + v.w * m.m[3][2];
    o.w = v.x * m.m[0][3] + v.y * m.m[1][3] + v.z * m.m[2][3] + v.w * m.m[3][3];
    return o;
}

// ===== RowVec * Mat4 で w除算して Vector3 にする =====
static Vector3 TransformCoord_RowVector4(const Vector4& v, const Matrix4x4& m)
{
    Vector4 o = MulRowVec4Mat4(v, m);
    if (std::abs(o.w) > 1e-6f) {
        o.x /= o.w;
        o.y /= o.w;
        o.z /= o.w;
    }
    return { o.x, o.y, o.z };
}

// ===== Screen -> World のレイ（y=0平面に当てる用）=====
// D3DのNDC: x,y = [-1..1], z = [0..1] 想定
static bool ScreenRayToPlaneY0_RowVector(
    int mouseX, int mouseY,
    float screenW, float screenH,
    const Matrix4x4& viewProj,
    Vector3& outHit)
{
    // ViewProj の逆
    Matrix4x4 invVP = MatrixMath::Inverse(viewProj);

    // Screen -> NDC
    float ndcX = ((float)mouseX / screenW) * 2.0f - 1.0f;
    float ndcY = 1.0f - ((float)mouseY / screenH) * 2.0f;

    // near(z=0) と far(z=1) を unproject
    Vector3 pNear = TransformCoord_RowVector4({ ndcX, ndcY, 0.0f, 1.0f }, invVP);
    Vector3 pFar = TransformCoord_RowVector4({ ndcX, ndcY, 1.0f, 1.0f }, invVP);

    // ray
    Vector3 dir { pFar.x - pNear.x, pFar.y - pNear.y, pFar.z - pNear.z };
    float len = std::sqrt(dir.x * dir.x + dir.y * dir.y + dir.z * dir.z);
    if (len < 1e-6f)
        return false;
    dir.x /= len;
    dir.y /= len;
    dir.z /= len;

    // 平面 y=0 と交差
    if (std::abs(dir.y) < 1e-6f)
        return false; // 平面と平行
    float t = (0.0f - pNear.y) / dir.y;
    if (t <= 0.0f)
        return false; // カメラの後ろ側

    outHit = { pNear.x + dir.x * t, 0.0f, pNear.z + dir.z * t };
    return true;
}

#ifdef USE_IMGUI

static bool WorldToScreen_RowVector(
    const Vector3& worldPos,
    const Matrix4x4& viewProj,
    float screenW, float screenH,
    ImVec2& outScreen)
{
    Vector4 clip = MulRowVec4Mat4({ worldPos.x, worldPos.y, worldPos.z, 1.0f }, viewProj);

    // カメラ後ろ(またはwが小さい)は描かない
    if (clip.w <= 1e-6f)
        return false;

    // NDC化
    const float ndcX = clip.x / clip.w;
    const float ndcY = clip.y / clip.w;

    // 画面座標へ（左上原点）
    outScreen.x = (ndcX * 0.5f + 0.5f) * screenW;
    outScreen.y = (-ndcY * 0.5f + 0.5f) * screenH;

    return true;
}
#pragma endregion

#endif // USE_IMGUI

static bool WorldToScreen_RowVector(
    const Vector3& worldPos,
    const Matrix4x4& viewProj,
    float screenW, float screenH,
    Vector2& outScreen)
{
    Vector4 clip = MulRowVec4Mat4({ worldPos.x, worldPos.y, worldPos.z, 1.0f }, viewProj);

    // カメラ後ろ(またはwが小さい)は描かない
    if (clip.w <= 1e-6f)
        return false;

    // NDC化
    const float ndcX = clip.x / clip.w;
    const float ndcY = clip.y / clip.w;

    // 画面座標へ（左上原点）
    outScreen.x = (ndcX * 0.5f + 0.5f) * screenW;
    outScreen.y = (-ndcY * 0.5f + 0.5f) * screenH;

    return true;
}

void GamePlayScene::Initialize()
{
    camera_ = new Camera();
    camera_->Initialize();
    camera_->SetTranslate({ 0, 0, 0 });

    // シーン開始時に、1秒かけて明るくするでやんす！
    FadeManager::GetInstance()->StartFadeIn(1.0f);

    Object3dManager::GetInstance()->SetDefaultCamera(camera_);

    ParticleManager::GetInstance()->Initialize(DirectXCommon::GetInstance(), SrvManager::GetInstance(), camera_);

    Object3dManager::GetInstance()->SetDefaultCamera(camera_);

    TextureManager::GetInstance()->LoadTexture(compassPath_);
    InitCompass_();

    InitAltimeter_();

    sprite_ = new Sprite();
    sprite_->Initialize(SpriteManager::GetInstance(), "resources/uvChecker.png");
    sprite_->SetPosition({ 100.0f, 100.0f });

    // 説明画像
    setumei_ = new Sprite();
    setumei_->Initialize(SpriteManager::GetInstance(), "resources/setumei.png");
    setumei_->SetPosition(setumeiPos_);
    // 最終位置はそのまま使う
    setumeiEndPos_ = setumeiPos_;

    // 最初は画面外（上）
    setumeiStartPos_.x = setumeiPos_.x;
    setumeiStartPos_.y = -720.0f;

    // 起動時は画面外に置く
    setumei_->SetPosition(setumeiStartPos_);
    // Enter説明画像
    EnterSetumei_ = new Sprite();
    EnterSetumei_->Initialize(SpriteManager::GetInstance(), "resources/Enter.png");
    EnterSetumei_->SetPosition(EnterSetumeiPos_);
    // サウンド関連===============================

    //  SoundManager::GetInstance()->ResetSE(gateSound_);
    // ドローンのプロペラ音
    DronePropellerSound_ = SoundManager::GetInstance()->SoundLoadFile("Resources/DroneBGM.mp3");
    // ゲート通過時キラキラ
    gateSound_ = SoundManager::GetInstance()->SoundLoadFile("Resources/GateCollision.mp3");
    //==========================================
    player2_ = new Object3d();
    player2_->Initialize(Object3dManager::GetInstance());
    player2_->SetModel("cube.obj");
    // player2_->SetModel("terrain.obj");
    player2_->SetTranslate({ 3.0f, 0.0f, 0.0f });
    // player2_->SetRotate({ std::numbers::pi_v<float> / 2.0f, std::numbers::pi_v<float>, 0.0f });

    // 床―
    terraranan_ = new Object3d();
    terraranan_->Initialize(Object3dManager::GetInstance());
    terraranan_->SetModel("terrain.obj");
    terraranan_->SetTranslate({ 0.0f, 0.0f, 0.0f });
    terraranan_->SetEnableLighting(true);
    ParticleManager::GetInstance()->CreateParticleGroup("circle", "resources/circle.png");
    Transform t {};
    t.translate = { 0.0f, 0.0f, 0.0f };

    emitter_.Init("circle", t, 30, 0.1f);

    sphere_ = new SphereObject();
    sphere_->Initialize(DirectXCommon::GetInstance(), 16, 1.0f);

    // Transform
    sphere_->SetTranslate({ 0, 0, 0 }); // 消える？
    sphere_->SetScale({ 1.5f, 1.5f, 1.5f });

    // Material
    sphere_->SetColor({ 1, 1, 1, 1 });
    LightManager::GetInstance()->SetDirectional({ 1, 1, 1, 1 }, { 0, -1, 0 }, 1.0f);
    // ---- Drone Object ----
    droneObj_ = new Object3d();
    droneObj_->Initialize(Object3dManager::GetInstance());

    // 存在するモデル名にしてね（無ければ cube.obj とか）
    //  droneObj_->SetModel("cube.obj");
    droneObj_->SetModel("Drone/dolone.obj"); // ←まずこれで見えるかテスト
    droneObj_->SetScale({ 0.1f, 0.1f, 0.1f });

    // ---- Stage Load (from StageSelect) ----
    StageData stage {};
    {
        const std::string& fileUtf8 = SceneManager::GetInstance()->GetSelectedStageFile();

        const bool ok = StageIO::Load(fileUtf8, stage);
        if (!ok) {
            // ここで fall back したいなら、今までのハードコード配置にする
            // ひとまず「最低限」置いておく
            stage = StageData {};
            stage.gates.clear();
            // 例：最低限1つゲート置く
            Gate g {};
            g.pos = { 0, 2, 5 };
            g.rot = { 0, 0, 0 };
            g.scale = { 2, 2, 2 };
            g.perfectRadius = 1.0f;
            g.gateRadius = 2.5f;
            g.thickness = 0.8f;
            stage.gates.push_back(g);

            stage.droneSpawnPos = { 0, 1, 0 };
            stage.droneSpawnYaw = 0.0f;
            stage.hasGoalPos = false;
            stage.hasGoalSpawnOffset = false;
        }
    }

    drone_.Initialize(stage.droneSpawnPos);

    // yaw も使うなら（Drone に SetYaw がある想定。無ければ Initialize に含めるか、メンバへ直接）
    drone_.SetYaw(stage.droneSpawnYaw); // 無いならコメントアウト

    droneObj_->SetTranslate(stage.droneSpawnPos);

    // ---- gates build ----
    gates_.clear();
    gates_.resize((int)stage.gates.size());

    ModelManager::GetInstance()->LoadModel("ro.obj");

    for (int i = 0; i < (int)stage.gates.size(); ++i) {

        // StageData::gates は Gate 型（あなたの StageIO と同じ Gate）
        // ただし Visual 側 gates_[i].gate に代入する
        gates_[i].gate = stage.gates[i];

        // 見た目モデル（今は cube 固定でOK）
        gates_[i].Initialize(Object3dManager::GetInstance(), "ro.obj", camera_);
    }

    nextGate_ = 0;
    perfectCount_ = 0;
    goodCount_ = 0;

    gateNum_.Initialize(SpriteManager::GetInstance(),
        "resources/ui/ascii_font_16x6_cell32_first32.png",
        16, 6, 32, 32, 32);

    gateNum_.SetColor({ 1, 1, 1, 1 }); // 見やすい色（好きでOK）

    // ---- walls build ----
    wallSys_.Clear(); // ★無いなら追加（後述）
    wallSys_.BuildDebug(Object3dManager::GetInstance(), "cube2.obj");

    for (const auto& w : stage.walls) {
        if (w.type == WallSystem::Type::AABB) {
            wallSys_.AddAABB(w.center, w.half);
        } else {
            wallSys_.AddOBB(w.center, w.half, w.rot);
        }
    }

    ModelManager::GetInstance()->LoadModel("goal.obj");
    goalSys_.Initialize(Object3dManager::GetInstance(), camera_);
    goalSys_.Reset();
    stageCleared_ = false;

    if (stage.hasGoalPos) {
        goalSys_.SetFixedGoalPos(stage.goalPos);
    } else {
        goalSys_.ClearFixedGoalPos();
    }

    ModelManager::GetInstance()->LoadModel("skydome.obj");
    TextureManager::GetInstance()->LoadTexture("resources/skydome.png");
    skydome_ = std::make_unique<Object3d>();
    skydome_->Initialize(Object3dManager::GetInstance());
    skydome_->SetModel("skydome.obj");
    skydome_->SetCamera(camera_);
    skydome_->SetEnableLighting(false);

    skydome_->SetScale(Vector3 { 2, 2, 2 });

    skydome_->SetTranslate({ 0.0f, 0.01f, 0.0f });

    ModelManager::GetInstance()->LoadModel("ground.obj");
    ground_ = std::make_unique<Object3d>();
    ground_->Initialize(Object3dManager::GetInstance());
    ground_->SetModel("ground.obj");
    ground_->SetCamera(camera_);
    ground_->SetEnableLighting(false);
    ground_->SetTranslate({ 0.0f, -5.5f, 0.0f });
    ground_->SetScale(Vector3 { 2, 1, 2 });

    landingEffect_.Initialize(Object3dManager::GetInstance(), camera_);

    skydome_->SetTranslate({ 0.0f, 0.01f, 0.0f });

    skydome_->SetTranslate({ 0.0f, 0.01f, 0.0f });

    particleGate_.Initialize(Object3dManager::GetInstance(), camera_);

    // spotLight
    LightManager::GetInstance()->SetSpotLightDistance(10.0f);
    LightManager::GetInstance()->SetSpotLightIntensity(0.6f);

    // ===================
    // Markers (split)
    // ===================

    // 1) Compass: now heading (fixed)
    TextureManager::GetInstance()->LoadTexture(compassMarkerPath_);
    compassMarker_ = new Sprite();
    compassMarker_->Initialize(SpriteManager::GetInstance(), compassMarkerPath_);
    compassMarker_->SetAnchorPoint({ 0.5f, 0.5f });
    compassMarker_->SetSize(compassMarkerSize_);
    compassMarker_->SetRotation(0.0f);

    // 2) Compass: gate direction (moves)
    TextureManager::GetInstance()->LoadTexture(gateMarkerCompassPath_);
    gateMarkerCompass_ = new Sprite();
    gateMarkerCompass_->Initialize(SpriteManager::GetInstance(), gateMarkerCompassPath_);
    gateMarkerCompass_->SetAnchorPoint({ 0.5f, 0.5f });
    gateMarkerCompass_->SetSize(gateMarkerCompassSize_);
    gateMarkerCompass_->SetRotation(0.0f);

    // 3) Altimeter: now altitude (fixed)
    TextureManager::GetInstance()->LoadTexture(altMarkerNowPath_);
    altMarkerNow_ = new Sprite();
    altMarkerNow_->Initialize(SpriteManager::GetInstance(), altMarkerNowPath_);
    altMarkerNow_->SetAnchorPoint({ 0.5f, 0.5f });
    altMarkerNow_->SetSize(altMarkerNowSize_);
    altMarkerNow_->SetRotation(0.0f);

    // 4) Altimeter: gate altitude diff (moves)
    TextureManager::GetInstance()->LoadTexture(gateMarkerAltPath_);
    gateMarkerAlt_ = new Sprite();
    gateMarkerAlt_->Initialize(SpriteManager::GetInstance(), gateMarkerAltPath_);
    gateMarkerAlt_->SetAnchorPoint({ 0.5f, 0.5f });
    gateMarkerAlt_->SetSize(gateMarkerAltSize_);
    gateMarkerAlt_->SetRotation(0.0f);

    // ボタンは中心基準にしておくと計算が楽でやんす

    // Initializeの末尾付近に追加
    pauseBg_ = std::make_unique<Sprite>();
    pauseBg_->Initialize(SpriteManager::GetInstance(), "resources/white.png"); // 白塗り画像
    pauseBg_->SetSize({ 1280.0f, 720.0f });
    pauseBg_->SetColor({ 0.7f, 0.7f, 0.7f, 0.5f }); // 半透明
    btnToSelect_ = std::make_unique<Sprite>();
    btnToSelect_->Initialize(SpriteManager::GetInstance(), "resources/select.png");
    btnClose_ = std::make_unique<Sprite>();
    btnClose_->Initialize(SpriteManager::GetInstance(), "resources/tojiru.png");

    btnToSelect_->SetAnchorPoint({ 0.5f, 0.5f });
    btnClose_->SetAnchorPoint({ 0.5f, 0.5f });

    requestBackToSelect_ = false;
    requestBackToTitle_ = false;
    stageCleared_ = false;

    //// 3) Altimeter: now altitude (fixed)
    // TextureManager::GetInstance()->LoadTexture(altMarkerNowPath_);
    // altMarkerNow_ = new Sprite();
    // altMarkerNow_->Initialize(SpriteManager::GetInstance(), altMarkerNowPath_);
    // altMarkerNow_->SetAnchorPoint({ 0.5f, 0.5f });
    // altMarkerNow_->SetSize(altMarkerNowSize_);
    // altMarkerNow_->SetRotation(0.0f);

    // 4) Altimeter: gate altitude diff (moves)
    TextureManager::GetInstance()->LoadTexture(gateMarkerAltPath_);
    gateMarkerAlt_ = new Sprite();
    gateMarkerAlt_->Initialize(SpriteManager::GetInstance(), gateMarkerAltPath_);
    gateMarkerAlt_->SetAnchorPoint({ 0.5f, 0.5f });
    gateMarkerAlt_->SetSize(gateMarkerAltSize_);
    gateMarkerAlt_->SetRotation(0.0f);

    mojyuro = new Sprite();
    mojyuro->Initialize(SpriteManager::GetInstance(), "resources/aaaaaa.png"); // 白塗り画像
    mojyuro->SetPosition({ 0.0f, 0.0f });
}

void GamePlayScene::Update()
{

    mojyuro->Update();
    float dt = 1.0f / 60.0f;
    Input& input = *Input::GetInstance();

    // ==========================================
    // 【重要】フェード更新を一番最初に持ってくる
    // これでポーズ中もフェードが止まらなくなるでやんす！
    // ==========================================
    FadeManager::GetInstance()->Update();

    // ==========================================
    // 1. ポーズの開始・解除トリガー
    // ==========================================
    if (input.IsKeyTrigger(DIK_TAB)) {
        if (!isPaused_) {
            SoundManager::GetInstance()->StopBGM(DronePropellerSound_);
            isPaused_ = true;
            isPauseClosing_ = false;
            pauseIndex_ = PauseMenuIndex::Close; // 開いたときは「閉じる」にリセット
        } else if (!requestBackToSelect_ && !requestBackToTitle_) {
            // すでに遷移が始まっていないときだけ閉じる
            isPauseClosing_ = true;
        }
    }

    // ==========================================
    // 2. ポーズ中の処理
    // ==========================================
    if (isPaused_) {
        // --- 2a. 入力判定（遷移中や閉じている最中は無視） ---
        if (!isPauseClosing_ && !requestBackToSelect_ && !requestBackToTitle_) {
            if (input.IsKeyTrigger(DIK_W)) {
                int idx = static_cast<int>(pauseIndex_);
                idx = (idx - 1 + static_cast<int>(PauseMenuIndex::COUNT)) % static_cast<int>(PauseMenuIndex::COUNT);
                pauseIndex_ = static_cast<PauseMenuIndex>(idx);
            }
            if (input.IsKeyTrigger(DIK_S)) {
                int idx = static_cast<int>(pauseIndex_);
                idx = (idx + 1) % static_cast<int>(PauseMenuIndex::COUNT);
                pauseIndex_ = static_cast<PauseMenuIndex>(idx);
            }

            // EnterまたはSpaceで決定
            if (input.IsKeyTrigger(DIK_RETURN) || input.IsKeyTrigger(DIK_SPACE)) {
                switch (pauseIndex_) {
                case PauseMenuIndex::Close:
                    isPauseClosing_ = true;
                    break;
                case PauseMenuIndex::ToSelect:
                    requestBackToSelect_ = true;
                    break;
                }
            }
        }

        // --- 2b. アニメーション・遷移ロジック ---
        if (requestBackToSelect_ || requestBackToTitle_) {
            pauseAnimTimer_ = 1.0f; // 遷移中は出しっぱなし

            if (FadeManager::GetInstance()->GetStatus() == FadeManager::Status::None || FadeManager::GetInstance()->GetStatus() == FadeManager::Status::FadeInFinished) {
                FadeManager::GetInstance()->StartFadeOut(1.0f);
            }
        } else if (isPauseClosing_) {
            // 【重要】閉じるアニメーション：タイマーを減らす
            pauseAnimTimer_ -= dt * 4.0f;
            if (pauseAnimTimer_ <= 0.0f) {
                pauseAnimTimer_ = 0.0f;
                isPaused_ = false;
                isPauseClosing_ = false;
            }
        } else {
            // 【重要】開くアニメーション：タイマーを増やす
            // これがないと画面外で止まったままになっちゃうでやんす！
            pauseAnimTimer_ += dt * 3.0f;
            if (pauseAnimTimer_ >= 1.0f)
                pauseAnimTimer_ = 1.0f;
        }

        // --- 2c. イージング計算 (ラムダ式を使わない版) ---
        float tInv = 1.0f - pauseAnimTimer_;
        float tEase = 1.0f - (tInv * tInv * tInv * tInv * tInv); // OutQuint

        // 背景
        pauseBg_->SetPosition({ 0.0f, -720.0f * (1.0f - tEase) });

        // 各ボタンの個別イージング（時間差）
        float t1 = std::clamp((pauseAnimTimer_ - 0.1f) / 0.8f, 0.0f, 1.0f);
        float t2 = std::clamp((pauseAnimTimer_ - 0.2f) / 0.8f, 0.0f, 1.0f);
        float t3 = std::clamp((pauseAnimTimer_ - 0.3f) / 0.8f, 0.0f, 1.0f);

        t1 = 1.0f - std::pow(1.0f - t1, 5.0f);
        t2 = 1.0f - std::pow(1.0f - t2, 5.0f);
        t3 = 1.0f - std::pow(1.0f - t3, 5.0f);

        btnClose_->SetPosition({ kPauseCenter.x, -100.0f + (kPauseCenter.y - 100.0f + 100.0f) * t1 });
        btnToSelect_->SetPosition({ kPauseCenter.x, -100.0f + (kPauseCenter.y + 100.0f) * t2 });

        // --- 2d. 色と透明度の更新 ---
        float bA = 1.0f; // 選択中
        float dA = 0.4f; // 非選択
        btnClose_->SetColor({ 1, 1, 1, (pauseIndex_ == PauseMenuIndex::Close ? bA : dA) });
        btnToSelect_->SetColor({ 1, 1, 1, (pauseIndex_ == PauseMenuIndex::ToSelect ? bA : dA) });

        pauseBg_->Update();
        btnClose_->Update();
        btnToSelect_->Update();

        // ==========================================
        // 【修正箇所】遷移リクエストがないときだけ return するでやんす！
        // ==========================================
        if (!requestBackToSelect_ && !requestBackToTitle_) {
            return; // ポーズ中はゲーム本体の更新を止める
        }
    }

    if (drone_.isMove()) {
        SoundManager::GetInstance()->PlayBGM(DronePropellerSound_, 0.4f);
    }

    FadeManager::GetInstance()->Update();

    // =========================
    // BackSpace : エディターへ戻る
    // =========================
    auto* sm = SceneManager::GetInstance();

    // =========================
    // BackSpace : エディターへ戻る（テスト時のみ）
    // =========================
    if (sm->IsTestPlay() && input.IsKeyTrigger(DIK_BACKSPACE)) {
        sm->SetTestPlay(false);

        sm->RequestOpenEditorFile("_test/__test_play.json"); // ★戻ったらテストファイルを開く
        sm->SetNextScene(new StageEditorScene());
        return;
    }

    // ドローン更新（※これが無いとカメラも動かない）
    if (isDebug_) {
        drone_.UpdateDebugNoInertia(input, dt);
    } else {
        drone_.UpdateMode1(input, dt);
    }
    {
        Vector3 pos = drone_.GetPos();
        Vector3 vel = drone_.GetVel();

        wallSys_.ResolveDroneAABB(pos, vel, droneHalf_, dt, 4);

        drone_.SetPos(pos);
        drone_.SetVel(vel);
    }

    if (drone_.HasJustLanded()) {
        landingEffect_.Play(drone_.GetPos());
    }
    landingEffect_.Update(dt);
    // ドローン実体 → 描画Object3dへ反映（毎フレーム必須）
    if (droneObj_) {
        droneObj_->SetTranslate(drone_.GetPos());

        // ★傾きだけ修正：roll と pitch を交換
        droneObj_->SetRotate({
            -drone_.GetPitch(),                 // X = pitch
            drone_.GetYaw() + droneYawOffset,  // Y = yaw
            drone_.GetRoll()                   // Z = roll
            });

        droneObj_->Update();
        UpdateDroneSpotLight();
    }


    // これを毎フレーム呼ぶ
    camera_->FollowDroneRigid(drone_,
        20.0f,   // ← 少し遠く
        5.0f,    // ← 少し高く
        -0.5f,  // ← 見下ろし強め
        droneYawOffset
    );
    if (setumei_) {
        setumei_->Update();
    }
    if (EnterSetumei_) {
        EnterSetumei_->Update();
    }

    if (input.IsKeyTrigger(DIK_RETURN)) {
        setumeiVisible_ = !setumeiVisible_;
        showSetumei_ = true;
        setumeiAnimT_ = 0.0f;
    }
    if (showSetumei_) {
        setumeiAnimT_ += dt;
        if (setumeiAnimT_ > 1.0f) {
            setumeiAnimT_ = 1.0f;
            showSetumei_ = false; // アニメ終了
        }

        // EaseOutCubic
        float t = setumeiAnimT_;
        float ease = 1.0f - (1.0f - t) * (1.0f - t) * (1.0f - t);

        Vector2 from;
        Vector2 to;

        if (setumeiVisible_) {
            // 下に出る
            from = setumeiStartPos_;
            to = setumeiEndPos_;
        } else {
            // 上に戻る
            from = setumeiEndPos_;
            to = setumeiStartPos_;
        }

        Vector2 pos;
        pos.x = from.x;
        pos.y = from.y + (to.y - from.y) * ease;

        setumei_->SetPosition(pos);
    }

    // 更新系
    emitter_.Update();
    ParticleManager::GetInstance()->Update();
    player2_->Update();
    sprite_->Update();
    UpdateCompass_();
    sphere_->Update(camera_);
    droneObj_->Update();
    skydome_->Update();
    ground_->Update();

    if (input.IsKeyTrigger(DIK_O)) {

        isDebug_ = !isDebug_;
    }

    //  camera_->DebugUpdate();

    particleGate_.Update(dt);

    camera_->Update();

    // ゲート

    // 1) 全ゲートの見た目更新（色タイマーもここで進む）
    for (auto& g : gates_) {
        g.Tick(dt);

        if (g.gate.GetIsHitGate() && !g.gate.playedEffect) {

            g.gate.playedEffect = true;
        }

        if (!g.gate.GetIsHitGate()) {
            g.gate.playedEffect = false;
        }
    }

    // 1. ゲート通過判定 (ゲートが残っている時だけ実行)
    if (nextGate_ < (int)gates_.size()) {
        GateResult res;
        const Vector3 dronePos = drone_.GetPos();

        if (gates_[nextGate_].TryPass(dronePos, res)) {
            if (res == GateResult::Perfect) {
                SoundManager::GetInstance()->PlaySE(gateSound_, 1.0f);
                //  SoundManager::GetInstance()->ResetSE(gateSound_);
                particleGate_.Play(drone_.GetPos());
                perfectCount_++;
                nextGate_++;
            } else if (res == GateResult::Good) {
                SoundManager::GetInstance()->PlaySE(gateSound_, 1.0f);
                // SoundManager::GetInstance()->ResetSE(gateSound_);
                particleGate_.Play(drone_.GetPos());
                goodCount_++;
                nextGate_++;
            }
        }
    }
    // 2. ゴール出現判定 (全ゲート通過後のみ実行)
    else {
        goalSys_.Update(gates_, nextGate_, drone_.GetPos());
        if (goalSys_.IsCleared()) {
            if (FadeManager::GetInstance()->GetStatus() == FadeManager::Status::FadeInFinished || FadeManager::GetInstance()->GetStatus() == FadeManager::Status::None) {
                FadeManager::GetInstance()->StartFadeOut(1.0f);
                stageCleared_ = true; // ゴールによるクリア
            }
        }
    }

    if (FadeManager::GetInstance()->GetStatus() == FadeManager::Status::FadeOutFinished) {
        if (isPaused_) {
            SoundManager::GetInstance()->StopBGM(DronePropellerSound_);
            LightManager::GetInstance()->Reset();

            if (requestBackToTitle_) {
                SceneManager::GetInstance()->SetNextScene(new TitleScene());
                requestBackToTitle_ = false; // 個別にリセット
                return;
            }

            if (requestBackToSelect_) { // else if にせず独立させる
                SceneManager::GetInstance()->SetNextScene(new StageSelectScene());
                requestBackToSelect_ = false; // 個別にリセット
                return;
            }
        }

        // ステージクリア（ゴール）による遷移
        if (stageCleared_) {
            stageCleared_ = false;

            stageCleared_ = true;
            SoundManager::GetInstance()->PlaySE(gateSound_, 1.0f);
            // ここで「リザルトへ遷移」「SE」「フェード」等を入れる
            // 例：次シーンへ
            // SceneManager::GetInstance()->SetNextScene(new ResultScene(perfectCount_, goodCount_));
            auto* sm = SceneManager::GetInstance();
            if (!sm->IsTestPlay()) {
                sm->SetNextScene(new ResultScene(perfectCount_, goodCount_));
                return;
            }
        }
    }

#ifdef USE_IMGUI

    // USE_IMGU

    // ==================================
    // Lighting Panel（ライト操作パネル）
    // ==================================
    ImGui::Begin("Lighting Control");

    // ---- ライトの ON / OFF ----
    static bool lightEnabled = true;
    ImGui::Checkbox("Enable Light", &lightEnabled);

    // ---- ライトの色 ----
    static Vector4 lightColor = Vector4(1.0f, 1.0f, 1.0f, 1.0f);
    ImGui::ColorEdit3("Light Color", (float*)&lightColor);

    // ---- 明るさ（強さ） ----
    static float lightIntensity = 1.0f;
    ImGui::SliderFloat("Intensity", &lightIntensity, 0.0f, 5.0f);

    // ---- 光の向き ----
    static Vector3 lightDir = { 0.0f, -1.0f, 0.0f };
    ImGui::SliderFloat3("Direction", &lightDir.x, -1.0f, 1.0f);

    // ---- 正規化 ----
    Vector3 normalizedDir = Normalize(lightDir);

    float intensity = lightIntensity;
    if (!lightEnabled) {
        intensity = 0.0f;
    }

    LightManager::GetInstance()->SetDirectional(
        { lightColor.x, lightColor.y, lightColor.z, 1.0f },
        normalizedDir,
        intensity);

    // ---- リセット ----
    if (ImGui::Button("Reset Direction")) {
        lightDir = { 0.0f, -1.0f, 0.0f };
    }

    ImGui::SameLine();

    if (ImGui::Button("Reset Light")) {
        lightEnabled = true;
        lightColor = Vector4(1.0f, 1.0f, 1.0f, 1.0f);
        lightIntensity = 1.0f;
        lightDir = { 0.0f, -1.0f, 0.0f };
    }

    ImGui::Separator();
    ImGui::Text("Point Light Control");

    static bool pointEnabled = true;
    ImGui::Checkbox("Enable Point Light", &pointEnabled);

    static Vector4 pointColor = { 1.0f, 1.0f, 1.0f, 1.0f };
    ImGui::ColorEdit3("Point Color", (float*)&pointColor);

    static float pointIntensity = 1.0f;
    ImGui::SliderFloat("Point Intensity", &pointIntensity, 0.0f, 5.0f);

    static float pointRadius = 10.0f;
    static float pointDecay = 1.0f;

    ImGui::SliderFloat("Point Radius", &pointRadius, 0.1f, 30.0f);
    ImGui::SliderFloat("Point Decay", &pointDecay, 0.1f, 5.0f);

    // Point 反映（位置は UpdateDronePointLight() 側で毎フレーム更新する前提）
    LightManager::GetInstance()->SetPointRadius(pointRadius);
    LightManager::GetInstance()->SetPointDecay(pointDecay);

    float pI = pointIntensity;
    if (!pointEnabled) {
        pI = 0.0f;
    }
    LightManager::GetInstance()->SetPointIntensity(pI);

    ImGui::Separator();
    ImGui::Text("Spot Light Control");

    // --------------------
    // Enable
    // --------------------
    static bool spotEnabled = true;
    ImGui::Checkbox("Enable Spot Light", &spotEnabled);

    // --------------------
    // Color
    // --------------------
    static Vector4 spotColor = { 1.0f, 1.0f, 1.0f, 1.0f };
    ImGui::ColorEdit3("Spot Color", &spotColor.x);

    // --------------------
    // Intensity
    // --------------------

    // --------------------
    // Distance / Decay
    // --------------------

    static float spotDecay = 2.0f;

    ImGui::SliderFloat("Spot Decay", &spotDecay, 0.1f, 10.0f);

    // --------------------
    // Position
    // --------------------
    /*   static Vector3 spotPos = { 0.0f, 2.0f, 0.0f };
       ImGui::SliderFloat3("Spot Position", &spotPos.x, -20.0f, 20.0f);*/

    // --------------------
    // Angle
    // --------------------
    static float spotAngleDeg = 60.0f;
    static float spotFalloffDeg = 30.0f;

    ImGui::SliderFloat("Spot Angle (deg)", &spotAngleDeg, 1.0f, 90.0f);
    ImGui::SliderFloat("Spot Falloff (deg)", &spotFalloffDeg, 0.0f, 89.0f);

    if (spotFalloffDeg > spotAngleDeg) {
        spotFalloffDeg = spotAngleDeg;
    }

    float cosAngle = std::cos(spotAngleDeg * std::numbers::pi_v<float> / 180.0f);
    float cosFalloffStart = std::cos(spotFalloffDeg * std::numbers::pi_v<float> / 180.0f);

    // --------------------
    // Direction（直接ベクトル）
    // --------------------

    // static Vector3 spotDir = {};

    // ImGui::SliderFloat3("SpotDir", &spotDir.x, -1.0f, 1.0f);
    // ImGui::Text("SpotDir = %.2f %.2f %.2f", spotDir.x, spotDir.y, spotDir.z);

    // spotDir = Normalize(spotDir);

    // --------------------
    // Apply
    // --------------------

    LightManager* lm = LightManager::GetInstance();
    lm->SetSpotLightColor(spotColor);

    lm->SetSpotLightDecay(spotDecay);
    lm->SetSpotLightCosAngle(cosAngle);

    ImGui::End();
    ImGui::Begin("Camera Debug");

    const Vector3& camPos = camera_->GetTranslate();
    const Vector3& camRot = camera_->GetRotate();

    ImGui::Text("Pos : %.2f  %.2f  %.2f", camPos.x, camPos.y, camPos.z);
    ImGui::Text("Rot(rad): %.3f  %.3f  %.3f", camRot.x, camRot.y, camRot.z);

    // 見やすいように度数も
    const float rad2deg = 180.0f / std::numbers::pi_v<float>;
    ImGui::Text("Rot(deg): %.1f  %.1f  %.1f",
        camRot.x * rad2deg, camRot.y * rad2deg, camRot.z * rad2deg);

    ImGui::Text("FovY     : %.3f", camera_->GetFovY());
    ImGui::Text("Near/Far : %.2f / %.2f", camera_->GetNearClip(), camera_->GetFarClip());

    ImGui::End();

    // ================================
    // GOAL overlay (ImGui)
    // ================================
    if (stageCleared_) {

        // 【重要】ポーズ中じゃない時だけ、Enterでのセレクト戻りを受け付ける
        if (!isPaused_ && input.IsKeyTrigger(DIK_RETURN)) {
            requestBackToSelect_ = true;
        }

        // 画面中央に出す
        ImGuiIO& io = ImGui::GetIO();
        const float W = io.DisplaySize.x;
        const float H = io.DisplaySize.y;

        const char* msg = "GOAL!!";
        ImVec2 textSize = ImGui::CalcTextSize(msg);

        // 少し上に出す
        ImVec2 pos((W - textSize.x) * 0.5f, (H * 0.35f) - textSize.y * 0.5f);

        auto* dl = ImGui::GetForegroundDrawList();

        // 影（見やすく）
        dl->AddText(ImVec2(pos.x + 2, pos.y + 2), IM_COL32(0, 0, 0, 200), msg);

        // 本体
        dl->AddText(pos, IM_COL32(255, 255, 0, 255), msg);

        // ついでに小さく案内（任意）
        const char* sub = "Press Enter to continue";
        ImVec2 subSize = ImGui::CalcTextSize(sub);
        ImVec2 subPos((W - subSize.x) * 0.5f, pos.y + 40.0f);
        dl->AddText(ImVec2(subPos.x + 1, subPos.y + 1), IM_COL32(0, 0, 0, 180), sub);
        dl->AddText(subPos, IM_COL32(255, 255, 255, 230), sub);
    }

    ImGui::Begin("Gate Debug");

    if (nextGate_ < (int)gates_.size()) {
        const Gate& g = gates_[nextGate_].gate;

        ImGui::Text("=== Next Gate ===");
        ImGui::Text("Local Pos : x=%.2f y=%.2f z=%.2f",
            g.dbgLocalPos.x, g.dbgLocalPos.y, g.dbgLocalPos.z);

        ImGui::Text("PrevZ     : %.2f", g.dbgPrevZ);

        ImGui::Separator();

        ImGui::Text("Crossed   : %s", g.dbgCrossed ? "YES" : "NO");
        ImGui::Text("Thickness : %s", g.dbgInThickness ? "IN" : "OUT");

        ImGui::Text("Radius    : %.2f", g.dbgRadius);
        ImGui::Text("Perfect R : %.2f", g.perfectRadius);
        ImGui::Text("Good R    : %.2f", g.gateRadius);

        if (g.dbgRadius <= g.perfectRadius)
            ImGui::TextColored(ImVec4(0, 1, 1, 1), "=> PERFECT ZONE");
        else if (g.dbgRadius <= g.gateRadius)
            ImGui::TextColored(ImVec4(0, 1, 0, 1), "=> GOOD ZONE");
        else
            ImGui::TextColored(ImVec4(1, 0, 0, 1), "=> MISS ZONE");
    }

    ImGui::End();

#endif

    // 反映
    sphere_->SetEnableLighting(sphereLighting);
    sphere_->SetTranslate(spherePos);
    sphere_->SetRotate(sphereRotate);
    sphere_->SetScale(sphereScale);
    // here_->SetShininess(shininess);

    if (drawWallDebug_) {
        wallSys_.UpdateDebug();
    }

    // マーカー
    //  例：コンパス中心 = compassCenter_ を持ってるならそれに合わせる
    compassMarker_->SetPosition(Vector2 { compassPos_.x, compassPos_.y + 35.0f });
    compassMarker_->SetSize(compassMarkerSize_);
    compassMarker_->Update();
}

void GamePlayScene::Draw3D()
{
    Object3dManager::GetInstance()->PreDraw();
    LightManager::GetInstance()->Bind(DirectXCommon::GetInstance()->GetCommandList());
    //	player2_->Draw();

    Object3dManager::GetInstance()->SetBlendMode(kBlendModeNone);
    Object3dManager::GetInstance()->SetNormalPSO();
    if (droneObj_)
        droneObj_->Draw();
    if (skydome_)
        skydome_->Draw();
    if (ground_)
        ground_->Draw();

    for (auto& g : gates_) {
        g.Draw();
    }

    landingEffect_.Draw();

    if (drawWallDebug_) {
        wallSys_.DrawDebug();
    }
    Object3dManager::GetInstance()->SetBlendMode(kBlendModeAdd);
    Object3dManager::GetInstance()->SetGlowPSO();
    goalSys_.Draw();

    particleGate_.Draw();
    // sphere_->Draw(DirectXCommon::GetInstance()->GetCommandList());
    Object3dManager::GetInstance()->SetBlendMode(kBlendModeNone);

    // ParticleManager::GetInstance()->PreDraw();
    // ParticleManager::GetInstance()->Draw();
}

void GamePlayScene::Draw2D()
{
    SpriteManager::GetInstance()->PreDraw();

    // フォント使うときこれしないとすっごい固まる
    font_.BeginFrame();
    gateNum_.BeginFrame();

    DrawAltimeter_();
    DrawSpeedSimple_();
    mojyuro->Draw();
    if (compassA_)
        compassA_->Draw();
    if (compassB_)
        compassB_->Draw();

    // 固定▲（現在方位）
    if (compassMarker_)
        compassMarker_->Draw();

    // 次ゲート方位マーカー
    if (gateMarkerCompass_)
        gateMarkerCompass_->Draw();

    if (setumei_) {
        setumei_->Draw();
    }
    if (EnterSetumei_) {
        EnterSetumei_->Draw();
    }
    // Altimeter

    DrawGateIndices2D_();

    // sprite_->SetColor(Vector4{ 0, 1, 0, 1.0f});

    // sprite_->Draw();

    if (isPaused_ || pauseAnimTimer_ > 0.0f) {
        pauseBg_->Draw();
        btnClose_->Draw();
        btnToSelect_->Draw();
    }

    FadeManager::GetInstance()->Draw();
}

void GamePlayScene::DrawImGui()
{
#ifdef USE_IMGUI

#endif
}

void GamePlayScene::Finalize()
{
    ParticleManager::GetInstance()->Finalize();

    SoundManager::GetInstance()->StopBGMAll();

    delete droneObj_;
    droneObj_ = nullptr;

    delete sprite_;
    sprite_ = nullptr;

    delete sphere_;
    sphere_ = nullptr;

    delete player2_;
    player2_ = nullptr;

    delete camera_;
    camera_ = nullptr;

    delete terraranan_;
    terraranan_ = nullptr;
    goalSys_.Finalize();

    if (compassA_) {
        delete compassA_;
        compassA_ = nullptr;
    }
    if (compassB_) {
        delete compassB_;
        compassB_ = nullptr;
    }

    delete altMarkerNow_;
    altMarkerNow_ = nullptr;
    delete gateMarkerAlt_;
    gateMarkerAlt_ = nullptr;
    delete compassMarker_;
    compassMarker_ = nullptr;
    delete gateMarkerCompass_;
    gateMarkerCompass_ = nullptr;

    SoundManager::GetInstance()->StopBGMAll();
}
void GamePlayScene::UpdateDronePointLight()
{
    float yaw = drone_.GetYaw();
    float pitch = drone_.GetPitch();

    Vector3 forward;
    forward.x = std::sinf(yaw) * std::cosf(pitch);
    forward.y = std::sinf(pitch);
    forward.z = std::cosf(yaw) * std::cosf(pitch);

    const float offset = 1.2f;

    Vector3 pos = drone_.GetPos();
    pos.x += forward.x * offset;
    pos.y += forward.y * offset;
    pos.z += forward.z * offset;

    LightManager::GetInstance()->SetPointPosition(pos);
}

//================================
// 位置表示
//================================

Vector3 GamePlayScene::GetNavTargetPos_() const
{
    if (nextGate_ < (int)gates_.size()) {
        return gates_[nextGate_].gate.pos;
    }
    // GoalSystem に GetGoalPos() が無いなら追加して返す
    return goalSys_.GetGoalPos();
}

static float Wrap01(float t)
{
    t = std::fmod(t, 1.0f);
    if (t < 0.0f)
        t += 1.0f;
    return t;
}

static float WrapDeg180(float d)
{
    while (d > 180.0f)
        d -= 360.0f;
    while (d < -180.0f)
        d += 360.0f;
    return d;
}

void GamePlayScene::InitCompass_()
{
    if (compassInit_)
        return;

    auto* sm = SpriteManager::GetInstance();

    compassA_ = new Sprite();
    compassB_ = new Sprite();
    compassA_->Initialize(sm, compassPath_);
    compassB_->Initialize(sm, compassPath_);

    compassA_->SetAnchorPoint({ 0.5f, 0.5f });
    compassB_->SetAnchorPoint({ 0.5f, 0.5f });

    compassA_->SetPosition(compassPos_);
    compassB_->SetPosition(compassPos_);

    compassA_->SetSize(compassSize_);
    compassB_->SetSize(compassSize_);

    compassA_->SetRotation(0.0f);
    compassB_->SetRotation(0.0f);

    compassInit_ = true;
}

void GamePlayScene::UpdateCompass_()
{
    if (!compassInit_)
        InitCompass_();

    const float texW = 4096.0f;
    const float texH = 64.0f;

    const float viewW = compassSize_.x; // 画面に見せたい幅(px)
    const float viewH = compassSize_.y; // 画面に見せたい高さ(px)

    // yaw(rad) -> deg
    float yawDeg = drone_.GetYaw() * 180.0f / 3.1415926535f;

    // 0..360
    yawDeg = std::fmod(yawDeg, 360.0f);
    if (yawDeg < 0.0f)
        yawDeg += 360.0f;

    // yaw をテクスチャの x に変換（0..texW）
    float xCenter = (yawDeg / 360.0f) * texW;

    // “中心に来てほしい”ので、表示窓の左端を求める
    float xLeft = xCenter - viewW * 0.5f;

    // 0..texW に畳み込み
    xLeft = std::fmod(xLeft, texW);
    if (xLeft < 0.0f)
        xLeft += texW;

    // 1枚目が表示する幅
    float wA = std::min(viewW, texW - xLeft);
    float wB = viewW - wA;

    // A: [xLeft .. xLeft+wA]
    compassA_->SetTextureLeftTop({ xLeft, 0.0f });
    compassA_->SetTextureSize({ wA, texH });
    compassA_->SetSize({ wA, viewH });
    compassA_->SetPosition({ compassPos_.x - (viewW * 0.5f) + (wA * 0.5f), compassPos_.y });

    // B: 残りを [0 .. wB]
    if (wB > 0.0f) {
        compassB_->SetTextureLeftTop({ 0.0f, 0.0f });
        compassB_->SetTextureSize({ wB, texH });
        compassB_->SetSize({ wB, viewH });
        compassB_->SetPosition({ compassPos_.x - (viewW * 0.5f) + wA + (wB * 0.5f), compassPos_.y });
        compassB_->SetColor({ 1, 1, 1, 1 });
    } else {
        // 使わないフレームは見えなく
        compassB_->SetColor({ 1, 1, 1, 0 });
    }

    compassA_->SetColor({ 1, 1, 1, 1 });

    compassA_->Update();
    compassB_->Update();
    UpdateGateMarkerScreenX_();
}

void GamePlayScene::UpdateGateMarkerScreenX_()
{
    if (!gateMarkerCompass_)
        return;

    // 「追うべきターゲットがあるか？」
    const bool hasNextGate = (nextGate_ < (int)gates_.size());
    const bool hasGoal = goalSys_.IsGoalActive(); // ★追加したやつ
    const bool cleared = goalSys_.IsCleared();

    // 何も追うものがない / クリア済みなら消す（お好み）
    if (!hasNextGate && (!hasGoal || cleared)) {
        // 消したいなら透明に
        gateMarkerCompass_->SetColor({ 1, 1, 1, 0 });
        gateMarkerCompass_->Update();
        return;
    }

    gateMarkerCompass_->SetColor({ 1, 1, 1, 1 }); // 表示

    const float W = (float)WinApp::kClientWidth;
    const Matrix4x4& vp = camera_->GetViewProjectionMatrix();

    // 次ゲートがあればゲート、無ければゴール（GetNavTargetPos_ がそうなってる）
    const Vector3 target = GetNavTargetPos_();

    Vector4 clip = MulRowVec4Mat4({ target.x, target.y, target.z, 1.0f }, vp);

    if (clip.w <= 1e-6f) {
        float x = (clip.x >= 0.0f) ? gateMarkerClampRightX_ : gateMarkerClampLeftX_;
        gateMarkerCompass_->SetPosition({ x, gateMarkerScreenY_ });
        gateMarkerCompass_->SetRotation(0.0f);
        gateMarkerCompass_->Update();
        return;
    }

    const float ndcX = clip.x / clip.w;
    float screenX = (ndcX * 0.5f + 0.5f) * W;

    if (ndcX < -1.0f)
        screenX = gateMarkerClampLeftX_;
    if (ndcX > 1.0f)
        screenX = gateMarkerClampRightX_;

    screenX = std::clamp(screenX, gateMarkerClampLeftX_, gateMarkerClampRightX_);

    gateMarkerCompass_->SetPosition({ screenX, gateMarkerScreenY_ });
    gateMarkerCompass_->SetRotation(0.0f);
    gateMarkerCompass_->Update();
}

///===================================
/// 高さ表示
///===================================

void GamePlayScene::InitAltimeter_()
{
    if (altInit_)
        return;

    auto* sm = SpriteManager::GetInstance();

    // 1x1白PNGが一番楽（線・矩形をスプライトで作れる）
    altBarBg_.Initialize(sm, altTickTex_);
    altBarFill_.Initialize(sm, altTickTex_);
    altTick_.Initialize(sm, altTickTex_);
    altPointer_.Initialize(sm, altTickTex_);

    // アンカーは左上
    altBarBg_.SetAnchorPoint({ 0.0f, 0.0f });
    altBarFill_.SetAnchorPoint({ 0.0f, 0.0f });
    altTick_.SetAnchorPoint({ 0.0f, 0.0f });
    altPointer_.SetAnchorPoint({ 0.0f, 0.0f });

    // フォント
    font_.Initialize(sm, altFontTex_, 16, 6, 32, 32, 32);
    font_.SetColor({ 1, 1, 1, 1 });

    altInit_ = true;
}

void GamePlayScene::DrawAltimeter_()
{
    InitAltimeter_();

    // 地面からの相対高度
    const float alt = std::max<float>(0.0f, drone_.GetPos().y - (-5.0f)); // minY_ を使ってね
    // ↑minY_がDrone側なら drone_から取得できるようにするか、GamePlaySceneのminY_と同じ値を使う

    const float x = altPos_.x;
    const float y = altPos_.y;
    const float w = altSize_.x;
    const float h = altSize_.y;

    const float centerY = y + h * 0.5f;

    // 背景
    altBarBg_.SetPosition({ x, y });
    altBarBg_.SetSize({ w, h });
    altBarBg_.SetColor({ 0, 0, 0, 0.35f });
    altBarBg_.Update();
    altBarBg_.Draw();

    // レンジのpixel変換： altHalfRange_ が画面で h の半分に相当
    const float pxPerUnit = (h * 0.5f) / altHalfRange_;

    // ポインタ（中央の水平線）
    altPointer_.SetPosition({ x, centerY - 1.0f });
    altPointer_.SetSize({ w, 2.0f });
    altPointer_.SetColor({ 1, 1, 1, 0.9f });
    altPointer_.Update();
    altPointer_.Draw();

    // マーカー（高度メーターの横）
    if (altMarkerNow_) {
        const float mx = x + w + altMarkerOffsetX_;
        const float my = centerY;

        altMarkerNow_->SetPosition({ mx + 10.0f, my - 2.0f });
        altMarkerNow_->SetRotation(-0.5f);
        altMarkerNow_->SetSize(altMarkerNowSize_);
        altMarkerNow_->Update();
        altMarkerNow_->Draw();
    }

    // 目的地の高度差マーカー（NEW）
    if (gateMarkerAlt_) {
        const Vector3 target = GetNavTargetPos_();
        const float dy = target.y - drone_.GetPos().y;

        float my = centerY + (-dy * pxPerUnit);

        const float topY = y + 4.0f;
        const float botY = y + h - 4.0f;
        if (my < topY)
            my = topY;
        if (my > botY)
            my = botY;

        const float mx = x + w + altMarkerOffsetX_ - 26.0f;

        gateMarkerAlt_->SetPosition({ mx, my });
        gateMarkerAlt_->SetRotation(0.0f);
        gateMarkerAlt_->SetSize(gateMarkerAltSize_);
        gateMarkerAlt_->Update();
        gateMarkerAlt_->Draw();
    }

    // 目盛り描画範囲（表示する高度）
    const float minAlt = std::max<float>(0.0f, alt - altHalfRange_);
    const float maxAlt = alt + altHalfRange_;

    // 小目盛り（minor）
    for (float a = std::floor(minAlt / altMinorStep_) * altMinorStep_;
        a <= maxAlt + 0.0001f; a += altMinorStep_) {
        if (a < 0.0f)
            continue;

        const float dy = (a - alt) * pxPerUnit; // +なら上
        const float yy = centerY - dy; // 上が小さいのでマイナス

        // 範囲外はスキップ
        if (yy < y || yy > y + h)
            continue;

        const bool major = (std::fmod(a, altStep_) < 0.0001f);

        const float tickLen = major ? (w * 0.55f) : (w * 0.30f);
        const float tickX = x + (w - tickLen);

        altTick_.SetPosition({ tickX, yy });
        altTick_.SetSize({ tickLen, major ? 2.0f : 1.0f });
        altTick_.SetColor(major ? Vector4 { 1, 1, 1, 0.85f } : Vector4 { 1, 1, 1, 0.35f });
        altTick_.Update();
        altTick_.Draw();

        // 数字（majorのみ）
        if (major) {
            font_.BeginFrame(); // ★フォントはそのフレーム最初に1回でOK（後でまとめる）
            // ここだけだと毎回BeginFrameしちゃうので、実際はDraw2Dの先頭で1回呼ぶのが良い
        }
    }

    // 数字は「major刻みだけ」別ループで描く（BeginFrameを1回にするため）
    font_.BeginFrame();
    for (float a = std::floor(minAlt / altStep_) * altStep_;
        a <= maxAlt + 0.0001f; a += altStep_) {
        if (a < 0.0f)
            continue;

        const float dy = (a - alt) * pxPerUnit;
        const float yy = centerY - dy;
        if (yy < y || yy > y + h)
            continue;

        // 左に数字
        char buf[32];
        std::snprintf(buf, sizeof(buf), "%.0f", a);

        // 少し左寄せ & 中央揃え
        font_.DrawString(x + 6.0f, yy - 10.0f, buf, 0.6f);
    }

    // “現在高度” を太字っぽく（同じ文字を2回描いて影）
    {
        char buf[32];
        std::snprintf(buf, sizeof(buf), "HEIGHT%.1f", alt);
        font_.DrawString(x, y - 24.0f, buf, 0.7f);
    }

    // 次の場所
    {
    }
}

void GamePlayScene::DrawSpeedSimple_()
{
    const Vector3 v = drone_.GetVel();

    const float spd = std::sqrt(v.x * v.x + v.z * v.z); // 水平速度
    const float vspd = v.y; // 上下速度

    const float x = WinApp::kClientWidth - 180.0f;
    float y = 160.0f;

    // ---- SPD ----
    font_.DrawString(x, y, "SPD", 0.6f);
    font_.DrawString(x, y + 20.0f,
        std::format("{:.1f}", spd), 1.0f);

    y += 70.0f;

    // ---- VSPD ----
    font_.DrawString(x, y, "VSPD", 0.6f);
    font_.DrawString(x, y + 20.0f,
        std::format("{:+.1f}", vspd), 1.0f);
}

void GamePlayScene::DrawGateIndices2D_()
{
    const float W = (float)WinApp::kClientWidth;
    const float H = (float)WinApp::kClientHeight;
    const Matrix4x4& vp = camera_->GetViewProjectionMatrix();

    // デバッグ：ここは赤で出るはず
    /*gateNum_.SetColor({ 1,0,0,1 });
    gateNum_.DrawString(100, 100, "TEST", 2.0f);*/

    for (int i = 0; i < (int)gates_.size(); ++i) {

        Vector3 labelPos = gates_[i].gate.pos;

        Vector2 screen {};
        if (!WorldToScreen_RowVector(labelPos, vp, W, H, screen)) {
            continue;
        }

        // ★ここで「そのゲートの色」を決めて
        if (i == nextGate_)
            gateNum_.SetColor({ 1, 1, 0, 1 });
        else
            gateNum_.SetColor({ 1, 1, 1, 0.8f });

        const std::string txt = std::to_string(i + 1);
        float offsetX = (txt.size() == 1) ? 8.0f : 16.0f;

        // ★その直後に描く
        gateNum_.DrawString(screen.x - offsetX, screen.y - 10.0f, txt, 0.8f);
    }
}
void GamePlayScene::UpdateDroneSpotLight()
{
    float yaw = drone_.GetYaw();
    float pitch = drone_.GetPitch();

    // ドローンの前方向
    Vector3 forward;
    forward.x = std::sinf(yaw) * std::cosf(pitch);
    forward.y = std::sinf(pitch);
    forward.z = std::cosf(yaw) * std::cosf(pitch);

    // 正規化
    float len = std::sqrt(
        forward.x * forward.x + forward.y * forward.y + forward.z * forward.z);
    if (len > 0.0001f) {
        forward.x /= len;
        forward.y /= len;
        forward.z /= len;
    }

    // 後ろに下げる距離
    const float backOffset = 1.5f;
    const float upOffset = 0.3f;

    Vector3 lightPos = drone_.GetPos();
    lightPos.x -= forward.x * backOffset;
    lightPos.y -= forward.y * backOffset;
    lightPos.z -= forward.z * backOffset;
    lightPos.y += upOffset;

    LightManager* lm = LightManager::GetInstance();
    lm->SetSpotLightPosition(lightPos);

    // 向きは「前」f
    lm->SetSpotLightDirection(forward);
}
