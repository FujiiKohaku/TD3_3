#include "ResultScene.h"
#include "../Light/LightManager.h"
#include "ImGuiManager.h"
#include "ModelManager.h"
#include "Object3dManager.h"
#include "SceneManager.h"
#include "SpriteManager.h"
#include "StageSelectScene.h"

ResultScene::ResultScene(int perfectCount, int goodCount)
{
    perfectCount_ = perfectCount;
    goodCount_ = goodCount;

    perfect_ = std::make_unique<Sprite>();
    good_ = std::make_unique<Sprite>();

    camera_ = std::make_unique<Camera>();
}

void ResultScene::Initialize()
{
    ModelManager::GetInstance()->LoadModel("skydome.obj");
    TextureManager::GetInstance()->LoadTexture("resources/skydome.png");

    perfect_->Initialize(SpriteManager::GetInstance(), "resources/perfect.png");
    perfect_->SetPosition({ 400.0f, 200.0f }); // 画面中央など
    perfect_->SetAnchorPoint({ 0.5f, 0.5f }); // 中心点合わせ
    good_->Initialize(SpriteManager::GetInstance(), "resources/good.png");
    good_->SetPosition({ 400.0f, 400.0f }); // 画面中央など
    good_->SetAnchorPoint({ 0.5f, 0.5f }); // 中心点合わせ

    camera_->Initialize();
    camera_->SetTranslate({});
    camera_->SetRotate({});

    skydome_ = std::make_unique<Object3d>();
    skydome_->Initialize(Object3dManager::GetInstance());
    skydome_->SetModel("skydome.obj");
    skydome_->SetCamera(camera_.get());
    skydome_->SetEnableLighting(false);

    LightManager::GetInstance()->Initialize(DirectXCommon::GetInstance());
    LightManager::GetInstance()->SetDirectional({ 1, 1, 1, 1 }, { 0, -1, 0 }, 1.0f);

    font_ = std::make_unique<BitmapFont>();
    font_->Initialize(SpriteManager::GetInstance(),
        "resources/ui/bitmapFont.png",
        16, 6, 128, 128, 32);

    font_->SetColor({ 1, 1, 1, 1 }); // 見やすい色（好きに）

    bgmData_ = SoundManager::GetInstance()->SoundLoadFile("resources/result.mp3");
    SoundManager::GetInstance()->PlaySE(bgmData_, 1.0);

    fanfareCount_ = 1;
    fanfareTimer_ = 0.0f;
}

void ResultScene::Finalize()
{

    SoundManager::GetInstance()->StopBGMAll();
    LightManager::GetInstance()->Finalize();
}

void ResultScene::Update()
{

    if (fanfareCount_ <= 2) {
        fanfareTimer_ += 1.0f / 60.0f;

        if (fanfareTimer_ >= 2.0f) { // 1秒後にもう一回
            SoundManager::GetInstance()->PlaySE(bgmData_, 1.0f);
            fanfareCount_++;
        }
    }

    // 入出力取得
    Input& input = *Input::GetInstance();
    if (input.IsKeyTrigger(DIK_F1)) {
        SceneManager::GetInstance()->SetNextScene(new StageSelectScene());
    }

    camera_->Update();
    skydome_->Update();

    // タイマーを 0.0 から 1.0 まで進める
    if (animationTimer_ < 1.0f) {
        animationTimer_ += 1.0f / 60.0f * kAnimSpeed;
        if (animationTimer_ > 1.0f)
            animationTimer_ = 1.0f;
    }

    perfect_->Update();
    good_->Update();
}

void ResultScene::Draw2D()
{
    // 描画準備
    SpriteManager::GetInstance()->PreDraw();

    // 1. Perfectロゴ (0.00 - 0.25)
    float tP_Logo = std::clamp((animationTimer_ - 0.00f) * 4.0f, 0.0f, 1.0f);
    // 2. Perfect数字 (0.25 - 0.50)
    float tP_Count = std::clamp((animationTimer_ - 0.25f) * 4.0f, 0.0f, 1.0f);
    // 3. Goodロゴ    (0.50 - 0.75)
    float tG_Logo = std::clamp((animationTimer_ - 0.50f) * 4.0f, 0.0f, 1.0f);
    // 4. Good数字    (0.75 - 1.00)
    float tG_Count = std::clamp((animationTimer_ - 0.75f) * 4.0f, 0.0f, 1.0f);

    // 全員にイージング（EaseOutCubic）を適用
    auto EaseOut = [](float t) { return 1.0f - std::pow(1.0f - t, 3.0f); };
    float eP_Logo = EaseOut(tP_Logo);
    float eP_Count = EaseOut(tP_Count);
    float eG_Logo = EaseOut(tG_Logo);
    float eG_Count = EaseOut(tG_Count);

    // --- 座標計算 (画面外 1300 -> 目標位置) ---
    // ロゴの目標X: 400, 数字の目標X: 600 (重ならないように少しずらすのがコツでやんす)
    float xP_Logo = 1500.0f + (400.0f - 1500.0f) * eP_Logo;
    float xP_Count = 1500.0f + (800.0f - 1500.0f) * eP_Count;
    float xG_Logo = 1500.0f + (400.0f - 1500.0f) * eG_Logo;
    float xG_Count = 1500.0f + (800.0f - 1500.0f) * eG_Count;

    perfect_->SetPosition({ xP_Logo, 200.0f });
    good_->SetPosition({ xG_Logo, 400.0f });

    perfect_->Draw();
    good_->Draw();

    // フォントの描画準備（内部カウンタのリセット）
    font_->BeginFrame();

    // 文字列を作って表示
    std::string perfectStr = std::to_string(perfectCount_);
    std::string goodStr = std::to_string(goodCount_);

    // 座標は適宜調整
    font_->DrawString(xP_Count, 125.0f, perfectStr, 1.0f);
    font_->DrawString(xG_Count, 325.0f, goodStr, 1.0f);
}

void ResultScene::Draw3D()
{
    Object3dManager::GetInstance()->PreDraw();
    LightManager::GetInstance()->Bind(DirectXCommon::GetInstance()->GetCommandList());
    skydome_->Draw();
}

void ResultScene::DrawImGui()
{
    ImGui::Text("Perfect : %d", perfectCount_);
    ImGui::Text("Good : %d", goodCount_);
}