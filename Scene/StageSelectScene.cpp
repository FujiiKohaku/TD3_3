#include "StageSelectScene.h"
#include "SceneManager.h"
#include "../input/Input.h"
#include "GamePlayScene.h"
#include "StageEditorScene.h" // タイトルへ戻す等で使ってもOK
#include "TitleScene.h"

#include <algorithm>
#include <Windows.h>

std::string StageSelectScene::WideToUtf8_(const std::wstring& ws)
{
    if (ws.empty()) return {};
    int size = WideCharToMultiByte(CP_UTF8, 0, ws.data(), (int)ws.size(),
        nullptr, 0, nullptr, nullptr);
    std::string out(size, '\0');
    WideCharToMultiByte(CP_UTF8, 0, ws.data(), (int)ws.size(),
        out.data(), size, nullptr, nullptr);
    return out;
}
void StageSelectScene::Initialize()
{
    // ★最低限のカメラを用意（ParticleManager がどこかで Update されても落ちないように）
    camera_ = new Camera();
    camera_->Initialize();
    camera_->SetTranslate({ 0, 0, 0 });
    Object3dManager::GetInstance()->SetDefaultCamera(camera_);

    // ★ParticleManager が “常に Update されうる” なら、ここで camera を差し替える
    ParticleManager::GetInstance()->SetCamera(camera_);
    // もし ParticleManager が未Initializeの可能性があるなら、Initializeしてしまうのが確実
    // ParticleManager::GetInstance()->Initialize(DirectXCommon::GetInstance(), SrvManager::GetInstance(), camera_);

    Rescan_();
}

void StageSelectScene::Finalize()
{
    delete camera_;
    camera_ = nullptr;
}

void StageSelectScene::Rescan_()
{
    entries_.clear();

    const std::filesystem::path dir = std::filesystem::path(L"resources") / L"stage";
    if (!std::filesystem::exists(dir)) {
        selected_ = -1;
        return;
    }

    for (const auto& e : std::filesystem::directory_iterator(dir)) {
        if (!e.is_regular_file()) continue;

        const auto& p = e.path();
        if (p.extension() != L".json") continue;

        StageEntry se{};
        se.path = p;
        se.fileW = p.filename().wstring();      // "テスト01.json"
        se.fileUtf8 = WideToUtf8_(se.fileW);    // "テスト01.json"
        se.titleUtf8 = WideToUtf8_(p.stem().wstring()); // "テスト01"

        entries_.push_back(std::move(se));
    }

    std::sort(entries_.begin(), entries_.end(),
        [](const StageEntry& a, const StageEntry& b) { return a.fileW < b.fileW; });

    selected_ = entries_.empty() ? -1 : 0;
}

void StageSelectScene::Decide_()
{
    if (selected_ < 0 || selected_ >= (int)entries_.size()) return;

    // ★次シーンへ渡す
    SceneManager::GetInstance()->SetSelectedStageFile(entries_[selected_].fileUtf8);

    // ★ゲームへ
    SceneManager::GetInstance()->SetNextScene(new GamePlayScene());
}

void StageSelectScene::Update()
{
    Input& input = *Input::GetInstance();

    // リスキャン
    if (input.IsKeyTrigger(DIK_F5)) {
        Rescan_();
    }

    if (entries_.empty()) return;

    // 選択（↑↓）
    if (input.IsKeyTrigger(DIK_UP)) {
        selected_ = std::max<int>(0, selected_ - 1);
    }
    if (input.IsKeyTrigger(DIK_DOWN)) {
        selected_ = std::min<int>((int)entries_.size() - 1, selected_ + 1);
    }

    // 決定（Enter）
    if (input.IsKeyTrigger(DIK_RETURN)) {
        Decide_();
    }

    // 戻る（Escでタイトルへ、など）
    if (input.IsKeyTrigger(DIK_ESCAPE)) {
        SceneManager::GetInstance()->SetNextScene(new TitleScene()); // TitleScene使うならincludeしてね
    }


    DrawImGui();

}

void StageSelectScene::Draw2D()
{
}

void StageSelectScene::Draw3D()
{
}

void StageSelectScene::DrawImGui()
{
    ImGui::Begin("Stage Select (A: ImGui)");

    if (ImGui::Button("Rescan (F5)")) {
        Rescan_();
    }

    ImGui::Separator();

    if (entries_.empty()) {
        ImGui::Text("No .json in resources/stage/");
        ImGui::End();
        return;
    }

    ImGui::Text("Enter : Decide");
    ImGui::Text("Up/Down : Select");
    ImGui::Separator();

    for (int i = 0; i < (int)entries_.size(); ++i) {
        bool sel = (i == selected_);
        if (ImGui::Selectable(entries_[i].titleUtf8.c_str(), sel)) {
            selected_ = i;
        }
    }

    ImGui::Separator();
    ImGui::Text("Selected file: %s", entries_[selected_].fileUtf8.c_str());

    if (ImGui::Button("Decide")) {
        Decide_();
    }

    ImGui::End();
}
