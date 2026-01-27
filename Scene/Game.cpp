#include "Game.h"
#include <numbers>

void Game::Initialize()
{
    // -------------------------------
    // 例外キャッチ設定（アプリ起動時に1回だけ）
    // -------------------------------
    SetUnhandledExceptionFilter(Utility::ExportDump);
    std::filesystem::create_directory("logs");
    winApp_ = new WinApp();
    winApp_->initialize();
    DirectXCommon::GetInstance()->Initialize(winApp_);
    SrvManager::GetInstance()->Initialize(DirectXCommon::GetInstance());
    TextureManager::GetInstance()->Initialize(DirectXCommon::GetInstance(), SrvManager::GetInstance());
    ImGuiManager::GetInstance()->Initialize(winApp_, DirectXCommon::GetInstance(), SrvManager::GetInstance());
    SpriteManager::GetInstance()->Initialize(DirectXCommon::GetInstance());
    // TextureManager::GetInstance()->LoadTexture("resources/circle.png")
    ModelManager::GetInstance()->Initialize(DirectXCommon::GetInstance());
    Object3dManager::GetInstance()->Initialize(DirectXCommon::GetInstance());
    // camera_ = new Camera();
    // camera_->SetTranslate({ 0.0f, 0.0f, 2.0f });
    // Object3dManager::GetInstance()->SetDefaultCamera(camera_);
    modelCommon_.Initialize(DirectXCommon::GetInstance());
    // 入力関連
    Input::GetInstance()->Initialize(winApp_);
    // パーティクル関連
    // ParticleManager::GetInstance()->Initialize(DirectXCommon::GetInstance(), SrvManager::GetInstance(), camera_);
    ModelManager::GetInstance()->LoadModel("plane.obj");
    ModelManager::GetInstance()->LoadModel("axis.obj");
    ModelManager::GetInstance()->LoadModel("titleTex.obj");
    ModelManager::GetInstance()->LoadModel("fence.obj");
    ModelManager::GetInstance()->LoadModel("terrain.obj");
    ModelManager::GetInstance()->LoadModel("cube.obj");
    TextureManager::GetInstance()->LoadTexture("resources/uvChecker.png");
    TextureManager::GetInstance()->LoadTexture("resources/uvChecker.png");
    TextureManager::GetInstance()->LoadTexture("resources/fence.png");
    TextureManager::GetInstance()->LoadTexture("resources/ui/bitMap.png");
    TextureManager::GetInstance()->LoadTexture("resources/ui/ascii_font_16x6_cell32_first32.png");
    TextureManager::GetInstance()->LoadTexture("resources/ui/no_thumb.png");


    BaseScene* scene = new TitleScene();
    // シーンマネージャーに最初のシーンをセット
    SceneManager::GetInstance()->SetNextScene(scene);
}

void Game::Update()
{
    // ======== ImGui begin ========
    ImGuiManager::GetInstance()->Begin();

    // --- ゲーム更新 ---
    Input::GetInstance()->Update();
    // camera_->Update();

    // camera_->DebugUpdate();

    if (Input::GetInstance()->IsKeyTrigger(DIK_F6)) {

        BaseScene* scene = SceneManager::GetInstance()->GetCurrentScene();

        if (scene && scene->AllowThumbnailCapture()) {
            requestCapture_ = true;

            if (scene->HideUIForThumbnail()) {
                hideUIThisFrame_ = true;
            }
        }
    }


    // エスケープで離脱
    if (Input::GetInstance()->IsKeyPressed(DIK_ESCAPE)) {
        endRequest_ = true;
    }

    SceneManager::GetInstance()->Update();

    SceneManager::GetInstance()->DrawImGui();

    // ======== ImGui end ========
    ImGuiManager::GetInstance()->End();
}

void Game::Draw() {
    SrvManager::GetInstance()->PreDraw();
    DirectXCommon::GetInstance()->PreDraw();

    BaseScene* scene = SceneManager::GetInstance()->GetCurrentScene();

    bool skipUI = false;

    // ★ここで「保存したい」ことだけ予約する
    SceneManager::ThumbnailRequest req;
    if (scene &&
        scene->AllowThumbnailCapture() &&
        SceneManager::GetInstance()->ConsumeThumbnailRequest(req)) {

        DirectXCommon::GetInstance()->RequestBackBufferCapture(req.path, req.w, req.h);

        // ★UI非表示で撮りたいなら、このフレームは2D/ImGui描かない
        if (scene->HideUIForThumbnail()) {
            skipUI = true;
        }
    }

    if (scene) scene->Draw3D();
    if (scene && !skipUI) scene->Draw2D();
    if (!skipUI) ImGuiManager::GetInstance()->Draw();

    DirectXCommon::GetInstance()->PostDraw(); // ★ここで実際の保存をやる
}



void Game::Finalize()
{
    // シーンマネージャーも singleton
    SceneManager::GetInstance()->Finalize();
    ParticleManager::GetInstance()->Finalize();
    Object3dManager::GetInstance()->Finalize();
    SpriteManager::GetInstance()->Finalize();
    ModelManager::GetInstance()->Finalize();
    TextureManager::GetInstance()->Finalize();
    ImGuiManager::GetInstance()->Finalize();
    SrvManager::GetInstance()->Finalize();
    // DirectXCommonはFinalizeしてもデバイス破棄処理だけ。deleteは不要
    DirectXCommon::GetInstance()->Finalize();

    winApp_->Finalize();
    delete winApp_;
    delete camera_;
}
