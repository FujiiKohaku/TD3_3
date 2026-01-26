#include "TitleScene.h"
#include "../input/Input.h"
#include "GamePlayScene.h"
#include "StageEditorScene.h"  
#include "StageSelectScene.h"
#include "ResultScene.h"

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

    //Tキーでリザルトシーン
    if (Input::GetInstance ()->IsKeyTrigger (DIK_T)) {
        SceneManager::GetInstance ()->SetNextScene (new ResultScene ());
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