#include "ResultScene.h"

ResultScene::ResultScene () {
	camera_ = std::make_unique<MainCamera> ();
}

void ResultScene::Initialize () {
	camera_->Initialize ({ {1.0f, 1.0f, 1.0f}, {0.0f, 0.0f, 0.0f},{0.0f, 0.0f, 0.0f} });
}

void ResultScene::Finalize () {
}

void ResultScene::Update () {
	camera_->Update ();
	camera_->ImGui ();
}

void ResultScene::Draw2D () {
}

void ResultScene::Draw3D () {
}

void ResultScene::DrawImGui () {
}
