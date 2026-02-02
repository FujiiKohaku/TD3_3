#include "ResultScene.h"
#include "ModelManager.h"
#include "SpriteManager.h"
#include "Object3dManager.h"
#include "ImGuiManager.h"
#include "../Light/LightManager.h"

ResultScene::ResultScene(int perfectCount, int goodCount) {
	perfectCount_ = perfectCount;
	goodCount_ = goodCount;

	perfect_ = std::make_unique<Sprite>();
	good_ = std::make_unique<Sprite>();

	camera_ = std::make_unique<Camera>();
}

void ResultScene::Initialize() {
	ModelManager::GetInstance()->LoadModel("skydome.obj");
	TextureManager::GetInstance()->LoadTexture("resources/skydome.png");

	perfect_->Initialize(SpriteManager::GetInstance(), "resources/perfect.png");
	perfect_->SetPosition({ 200.0f, 200.0f }); // 画面中央など
	perfect_->SetAnchorPoint({ 0.5f, 0.5f });  // 中心点合わせ
	good_->Initialize(SpriteManager::GetInstance(), "resources/good.png");
	good_->SetPosition({ 200.0f, 400.0f }); // 画面中央など
	good_->SetAnchorPoint({ 0.5f, 0.5f });  // 中心点合わせ

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
}

void ResultScene::Finalize() {
	LightManager::GetInstance()->Finalize();
}

void ResultScene::Update() {
	camera_->Update();
	skydome_->Update();

	perfect_->Update();
	good_->Update();
}

void ResultScene::Draw2D() {
	// 描画準備
	SpriteManager::GetInstance()->PreDraw();

	perfect_->Draw();
	good_->Draw();
}

void ResultScene::Draw3D() {
	Object3dManager::GetInstance()->PreDraw();
	LightManager::GetInstance()->Bind(DirectXCommon::GetInstance()->GetCommandList());
	skydome_->Draw();
}

void ResultScene::DrawImGui() {
	ImGui::Text("Perfect : %d", perfectCount_);
	ImGui::Text("Good : %d", goodCount_);
}