#include "ResultScene.h"
#include "ModelManager.h"
#include "Object3dManager.h"

ResultScene::ResultScene (int perfectCount, int goodCount) {
	camera_ = std::make_unique<Camera> ();
	skydome_ = std::make_unique<Object3d> ();
}

void ResultScene::Initialize () {
	ModelManager::GetInstance ()->LoadModel ("skydome.obj");
	TextureManager::GetInstance ()->LoadTexture ("resources/skydome.png");
	skydome_->Initialize (Object3dManager::GetInstance ());
	skydome_->SetModel ("skydome.obj");
	skydome_->SetCamera (camera_.get());
	skydome_->SetEnableLighting (false);
}

void ResultScene::Finalize () {
}

void ResultScene::Update () {
	camera_->Update ();
	skydome_->Update ();
}

void ResultScene::Draw2D () {
}

void ResultScene::Draw3D () {

}

void ResultScene::DrawImGui () {
}
