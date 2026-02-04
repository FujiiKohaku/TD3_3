#include "TitleScene.h"
#include "../Light/LightManager.h"
#include "../input/Input.h"
#include "FadeManager.h"
#include "GamePlayScene.h"
#include "ResultScene.h"
#include "StageEditorScene.h"
#include "StageSelectScene.h"
#include <numbers>
static SoundData bgmTitle_;
static bool prevAButton_ = false;

static SoundData se_;
void TitleScene::Initialize()
{
    float deltaTime;
    // カメラ生成
    camera_ = new Camera();
    camera_->Initialize();
    camera_->SetTranslate({ -19.8f, 4.7f, 7.0f });
    camera_->SetRotate({ -0.07f, -2.128f, 0.1f });

    //  Object3dManager にセット
    Object3dManager::GetInstance()->SetDefaultCamera(camera_);

    // モデル読み込み
    ModelManager::GetInstance()->LoadModel("terrain.obj");

    // Object3d 生成 & 初期化

    // Light
    LightManager::GetInstance()->Reset();
    LightManager::GetInstance()->SetIntensity(0.1f);
    // pointLight
    LightManager::GetInstance()->SetPointPosition({ -22.40f, 3.100f, 11.400f });
    LightManager::GetInstance()->SetPointIntensity(1.5f);
    LightManager::GetInstance()->SetPointRadius(14.600f);
    LightManager::GetInstance()->SetPointRadius(33.0f);
    LightManager::GetInstance()->SetPointIntensity(1.0f);
    // spotLight
    LightManager::GetInstance()->SetSpotLightPosition({ 0.1f, 12.6f, -4.6f });
    LightManager::GetInstance()->SetSpotLightDirection({ 0.0f, -1.0f, 0.0f });
    LightManager::GetInstance()->SetPointIntensity(10.0f);
    LightManager::GetInstance()->SetSpotLightDistance(13.5f);
    LightManager::GetInstance()->SetPointDecay(2.0f);
    // LightManager::GetInstance()->SetSpotLightIntensity(0.0f);

    // ホーム外殻
    ModelManager::GetInstance()->LoadModel("outShell.obj");
    outShellModel_ = new Object3d();
    outShellModel_->Initialize(Object3dManager::GetInstance());
    outShellModel_->SetModel("outShell.obj");
    outShellModel_->SetTranslate({ 0.0f, 0.0f, 0.0f });
    outShellModel_->Update();

    // ホーム
    ModelManager::GetInstance()->LoadModel("home.obj");
    homeModel_ = new Object3d();
    homeModel_->Initialize(Object3dManager::GetInstance());
    homeModel_->SetModel("home.obj");
    homeModel_->SetTranslate({ 0.0f, 0.0f, 0.0f });
    homeModel_->Update();

    // ドローソ
    ModelManager::GetInstance()->LoadModel("doroso.obj");
    doroso_ = std::make_unique<Object3d>();
    doroso_->Initialize(Object3dManager::GetInstance());
    doroso_->SetModel("doroso.obj");
    scale1 = { 0.6f, 0.6f, 0.5f };
    rotate1 = { 0.0f, -0.54f, 0.0f };
    pos1 = { -11.0f, 4.0f, 1.5f };

    doroso_->SetTranslate(pos1);
    doroso_->SetRotate(rotate1);
    doroso_->SetScale(scale1);
    // 線路
    ModelManager::GetInstance()->LoadModel("rail.obj");
    railModel_ = new Object3d();
    railModel_->Initialize(Object3dManager::GetInstance());
    railModel_->SetModel("rail.obj");
    railModel_->SetTranslate({ 0.0f, 0.0f, 0.0f });
    railModel_->Update();

    ModelManager::GetInstance()->LoadModel("Drone/dolone.obj");
    drone_ = new Object3d();
    drone_->Initialize(Object3dManager::GetInstance());
    drone_->SetModel("Drone/dolone.obj");

    bgmTitle_ = SoundManager::GetInstance()->SoundLoadFile("resources/titleSceneBGM.mp3");
    se_ = SoundManager::GetInstance()->SoundLoadFile("resources/maou_se_system49.mp3");
    SoundManager::GetInstance()->PlayBGM(bgmTitle_, 0.5f);

    AAA = new Sprite();
    AAA->Initialize(SpriteManager::GetInstance(), "resources/SAPCEOSE.png");
    AAA->SetPosition({ 0.0f, 0.0f });
}

void TitleScene::Update()
{
    AAA->Update();

    bool aButtonTrigger = false;

    // ===== gamepad (Aボタン) =====
    XINPUT_STATE st {};
    if (XInputGetState(0, &st) == ERROR_SUCCESS) {

        bool nowAButton = (st.Gamepad.wButtons & XINPUT_GAMEPAD_A) != 0;

        if (nowAButton && !prevAButton_) {
            aButtonTrigger = true; // 押した瞬間
        }

        prevAButton_ = nowAButton;
    } else {
        prevAButton_ = false;
    }
    // ===== 決定（Aボタン or SPACE） =====
    if (aButtonTrigger || Input::GetInstance()->IsKeyTrigger(DIK_SPACE)) {

        FadeManager::GetInstance()->StartFadeOut(1.0f);
        SoundManager::GetInstance()->PlaySE(se_, 1.0f);
    }

    FadeManager::GetInstance()->Update();

    // フェードアウトが終わったらシーン切り替え
    if (FadeManager::GetInstance()->GetStatus() == FadeManager::Status::FadeOutFinished) {
        SceneManager::GetInstance()->SetNextScene(new StageSelectScene());
        SoundManager::GetInstance()->StopBGMAll();
    }

    camera_->Update();

    outShellModel_->Update();
    homeModel_->Update();
    railModel_->Update();
    doroso_->Update();

    if (drone_) {
        drone_->SetTranslate(dronePos);
        drone_->SetRotate(droneRot);
        drone_->SetScale(droneScale);
        drone_->Update();
    }
}

void TitleScene::Draw2D()
{
    SpriteManager::GetInstance()->PreDraw();
    FadeManager::GetInstance()->Draw();
    AAA->Draw();
}

void TitleScene::Draw3D()
{
    Object3dManager::GetInstance()->PreDraw();
    LightManager::GetInstance()->Bind(DirectXCommon::GetInstance()->GetCommandList());

    if (outShellModel_) {
        outShellModel_->Draw();
    }
    if (homeModel_) {
        homeModel_->Draw();
    }
    if (railModel_) {
        railModel_->Draw();
    }
    doroso_->Draw();
    if (outShellModel_) {
        outShellModel_->Draw();
    }
    if (homeModel_) {
        homeModel_->Draw();
    }
    if (railModel_) {
        railModel_->Draw();
    }
    if (drone_) {
        drone_->Draw();
    }
}

void TitleScene::DrawImGui()
{
    /*ImGui::Begin("Camera");

    Vector3 pos = camera_->GetTranslate();
    Vector3 rot = camera_->GetRotate();

    if (ImGui::DragFloat3("Position", &pos.x, 0.1f)) {
        camera_->SetTranslate(pos);
    }

    if (ImGui::DragFloat3("Rotate", &rot.x, 0.01f)) {
        camera_->SetRotate(rot);
    }

    camera_->Update();
    ImGui::End();

    ImGui::Begin("Drone Control");

    ImGui::DragFloat3("Position", &dronePos.x, 0.1f);
    ImGui::DragFloat3("Rotate", &droneRot.x, 0.01f);
    ImGui::DragFloat3("Scale", &droneScale.x, 0.1f, 0.01f, 10.0f);



    ImGui::End();

    if (ImGui::DragFloat3("Position", &pos1.x, 0.1f)) {
        doroso_->SetTranslate(pos1);
    }

    if (ImGui::DragFloat3("Rotate", &rotate1.x, 0.01f)) {
        doroso_->SetRotate(rotate1);
    }
    if (ImGui::DragFloat3("scale", &scale1.x, 0.01f)) {
        doroso_->SetScale(scale1);
    }*/
    //// ============================
    //// Light
    //// ============================
    // ImGui::Begin("Light");

    // static bool enableLight = true;
    // static Vector3 lightDir = { 0.0f, -1.0f, 0.0f };
    // static Vector4 lightColor = { 1.0f, 1.0f, 1.0f, 1.0f };
    // static float intensity = 1.0f;

    // ImGui::Checkbox("Enable", &enableLight);
    // ImGui::DragFloat3("Direction", &lightDir.x, 0.01f);
    // ImGui::ColorEdit3("Color", &lightColor.x);
    // ImGui::DragFloat("Intensity", &intensity, 0.1f, 0.0f, 10.0f);

    // Vector3 dir = Normalize(lightDir);
    // float i = enableLight ? intensity : 0.0f;

    // LightManager::GetInstance()->SetDirectional(lightColor, dir, i);
    // ImGui::Begin("Point Light");

    // static bool pointEnabled = true;
    // static Vector4 pointColor = { 1.0f, 1.0f, 1.0f, 1.0f };
    // static Vector3 pointPos = { -2.0f, 7.8f, 5.4f };
    // static float pointIntensity = 1.0f;
    // static float pointRadius = 18.0f;
    // static float pointDecay = 1.0f;

    // ImGui::Checkbox("Enable", &pointEnabled);
    // ImGui::ColorEdit3("Color", &pointColor.x);
    // ImGui::DragFloat3("Position", &pointPos.x, 0.1f);
    // ImGui::DragFloat("Intensity", &pointIntensity, 0.1f, 0.0f, 10.0f);
    // ImGui::DragFloat("Radius", &pointRadius, 0.1f, 0.1f, 50.0f);
    // ImGui::DragFloat("Decay", &pointDecay, 0.1f, 0.1f, 5.0f);

    // float pI = pointEnabled ? pointIntensity : 0.0f;

    // LightManager::GetInstance()->SetPointLight(pointColor, pointPos, pI);
    // LightManager::GetInstance()->SetPointRadius(pointRadius);
    // LightManager::GetInstance()->SetPointDecay(pointDecay);

    // ImGui::End();
    // ImGui::Begin("Spot Light");

    // static bool spotEnabled = true;
    // static Vector4 spotColor = { 1.0f, 1.0f, 1.0f, 1.0f };
    // static Vector3 spotPos = { 0.0f, 5.0f, 5.0f };
    // static Vector3 spotDir = { 0.0f, -1.0f, 0.0f };
    // static float spotIntensity = 4.0f;
    // static float spotDistance = 10.0f;
    // static float spotDecay = 2.0f;

    //// 角度（度）
    // static float spotAngleDeg = 45.0f;

    // ImGui::Checkbox("Enable", &spotEnabled);
    // ImGui::ColorEdit3("Color", &spotColor.x);
    // ImGui::DragFloat3("Position", &spotPos.x, 0.1f);
    // ImGui::DragFloat3("Direction", &spotDir.x, 0.01f);
    // ImGui::DragFloat("Intensity", &spotIntensity, 0.1f, 0.0f, 10.0f);
    // ImGui::DragFloat("Distance", &spotDistance, 0.1f, 0.1f, 50.0f);
    // ImGui::DragFloat("Decay", &spotDecay, 0.1f, 0.1f, 5.0f);
    // ImGui::SliderFloat("Angle (deg)", &spotAngleDeg, 1.0f, 90.0f);
    // ImGui::End();
    //// 正規化 & cos 変換
    // Vector3 normalizedDir = Normalize(spotDir);
    // float cosAngle = std::cos(spotAngleDeg * std::numbers::pi_v<float> / 180.0f);
    // float sI = spotEnabled ? spotIntensity : 0.0f;

    // auto* lm = LightManager::GetInstance();
    // lm->SetSpotLightColor(spotColor);
    // lm->SetSpotLightPosition(spotPos);
    // lm->SetSpotLightDirection(normalizedDir);
    // lm->SetSpotLightIntensity(sI);
    // lm->SetSpotLightDistance(spotDistance);
    // lm->SetSpotLightDecay(spotDecay);
    // lm->SetSpotLightCosAngle(cosAngle);

    /* ImGui::End();*/
}

void TitleScene::Finalize()
{
    delete camera_;
    delete outShellModel_;
    delete homeModel_;
    delete railModel_;
    delete drone_;
    delete AAA;
    SoundManager::GetInstance()->StopBGMAll();
}