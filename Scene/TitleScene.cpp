#include "TitleScene.h"
#include "../input/Input.h"
#include "GamePlayScene.h"
#include "StageEditorScene.h"  
#include "StageSelectScene.h"

void TitleScene::Initialize()
{
 
}

void TitleScene::Update()
{
    if (Input::GetInstance()->IsKeyPressed(DIK_SPACE)) {
        SceneManager::GetInstance()->SetNextScene(new StageSelectScene());
    }

    if (Input::GetInstance()->IsKeyPressed(DIK_E)) {
        SceneManager::GetInstance()->SetNextScene(new StageEditorScene());
    }

    // ★Sキーでステージセレクトへ
    if (Input::GetInstance()->IsKeyPressed(DIK_S)) {
     
    }
}


void TitleScene::Draw2D()
{
}

void TitleScene::Draw3D()
{
}

void TitleScene::DrawImGui()
{
}

void TitleScene::Finalize()
{
}