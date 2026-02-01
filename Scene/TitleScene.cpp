#include "TitleScene.h"
#include "../Light/LightManager.h"
#include "../input/Input.h"
#include "GamePlayScene.h"
#include "StageEditorScene.h"
#include "StageSelectScene.h"
#include "ResultScene.h"
void TitleScene::Initialize() {
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
	LightManager::GetInstance()->Initialize(DirectXCommon::GetInstance());
	// Object3d 生成 & 初期化

	LightManager::GetInstance()->SetIntensity(1.0f);
	LightManager::GetInstance()->SetPointRadius(18.0f);
	LightManager::GetInstance()->SetPointPosition({ 0.0f, 10.0f, 10.0f });
	LightManager::GetInstance()->SetPointPosition({ -2.0f, 7.8f, 5.4f });
	LightManager::GetInstance()->SetPointIntensity(1.8f);


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

	// 線路
	ModelManager::GetInstance()->LoadModel("rail.obj");
	railModel_ = new Object3d();
	railModel_->Initialize(Object3dManager::GetInstance());
	railModel_->SetModel("rail.obj");
	railModel_->SetTranslate({ 0.0f, 0.0f, 0.0f });
	railModel_->Update();
}

void TitleScene::Update() {
	if (Input::GetInstance()->IsKeyPressed(DIK_SPACE)) {
		SceneManager::GetInstance()->SetNextScene(new StageSelectScene());
	}

	if (Input::GetInstance()->IsKeyPressed(DIK_E)) {
		SceneManager::GetInstance()->SetNextScene(new StageEditorScene());
	}

    // ★Sキーでステージセレクトへ
    if (Input::GetInstance()->IsKeyPressed(DIK_S)) {
     
    }

    //Rキーでリザルトシーン
    if (Input::GetInstance ()->IsKeyTrigger (DIK_R)) {
        SceneManager::GetInstance ()->SetNextScene (new ResultScene (20, 5));
    }
	// titleModel->Update();
	camera_->Update();

	// 外殻
	outShellModel_->Update();
	homeModel_->Update();
	railModel_->Update();
}

void TitleScene::Draw2D() {
}

void TitleScene::Draw3D() {
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
}

void TitleScene::DrawImGui() {
	ImGui::Begin("Camera");

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

	// ============================
	// Light
	// ============================
	ImGui::Begin("Light");

	static bool enableLight = true;
	static Vector3 lightDir = { 0.0f, -1.0f, 0.0f };
	static Vector4 lightColor = { 1.0f, 1.0f, 1.0f, 1.0f };
	static float intensity = 1.0f;

	ImGui::Checkbox("Enable", &enableLight);
	ImGui::DragFloat3("Direction", &lightDir.x, 0.01f);
	ImGui::ColorEdit3("Color", &lightColor.x);
	ImGui::DragFloat("Intensity", &intensity, 0.1f, 0.0f, 10.0f);

	Vector3 dir = Normalize(lightDir);
	float i = enableLight ? intensity : 0.0f;

	LightManager::GetInstance()->SetDirectional(lightColor, dir, i);

	ImGui::End();

}


void TitleScene::Finalize() {
	delete camera_;
	delete outShellModel_;
	delete homeModel_;
	delete railModel_;
	LightManager::GetInstance()->Finalize();
}