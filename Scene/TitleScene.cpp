#include "TitleScene.h"
#include "../input/Input.h"
#include "GamePlayScene.h"
#include "StageEditorScene.h"  
#include "StageSelectScene.h"
#include "../Light/LightManager.h"
void TitleScene::Initialize()
{
    // 1. カメラ生成
    camera_ = new Camera();
    camera_->Initialize();
    camera_->SetTranslate({ 0.0f, 0.0f, -10.0f });
    camera_->Update();

    // 2. Object3dManager にセット
    Object3dManager::GetInstance()->SetDefaultCamera(camera_);

    // 3. モデル読み込み
    ModelManager::GetInstance()->LoadModel("cube.obj");
    LightManager::GetInstance()->Initialize(DirectXCommon::GetInstance());
    // 4. Object3d 生成 & 初期化
    titleModel_ = new Object3d();
    titleModel_->Initialize(Object3dManager::GetInstance());
    titleModel_->SetModel("cube.obj");

    titleModel_->SetTranslate({ 0, 0, 0 });
    titleModel_->Update();
    LightManager::GetInstance()->SetIntensity(0.0f);
    LightManager::GetInstance()->SetPointIntensity(0.0f);
    LightManager::GetInstance()->SetSpotLightIntensity(1.0f);
   
}


void TitleScene::Update()
{
    if (Input::GetInstance()->IsKeyPressed(DIK_SPACE)) {
        SceneManager::GetInstance()->SetNextScene(new StageSelectScene());
    }

    if (Input::GetInstance()->IsKeyPressed(DIK_E)) {
        SceneManager::GetInstance()->SetNextScene(new StageEditorScene());
    }

  // titleModel->Update();
    camera_->Update();

 
    i.y += 0.01f;
     
    titleModel_->SetRotate(i);
    titleModel_->Update();

}


void TitleScene::Draw2D()
{
}

void TitleScene::Draw3D()
{
    Object3dManager::GetInstance()->PreDraw();
    LightManager::GetInstance()->Bind(DirectXCommon::GetInstance()->GetCommandList());
    if (titleModel_) {
        titleModel_->Draw();
    }
}

void TitleScene::DrawImGui()
{
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
}

void TitleScene::Finalize()
{
    delete camera_;
    delete titleModel_;
    LightManager::GetInstance()->Finalize();
}