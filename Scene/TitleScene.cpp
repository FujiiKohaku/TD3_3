#include "TitleScene.h"
#include "../input/Input.h"
#include "GamePlayScene.h"
#include "StageEditorScene.h"   // ★これを追加

void TitleScene::Initialize()
{
 
}

void TitleScene::Update()
{
    if (Input::GetInstance()->IsKeyPressed(DIK_SPACE)) {
        SceneManager::GetInstance()->SetNextScene(new GamePlayScene());
    }

    // 例：Eキーでエディタへ
    if (Input::GetInstance()->IsKeyPressed(DIK_E)) {
        SceneManager::GetInstance()->SetNextScene(new StageEditorScene());
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