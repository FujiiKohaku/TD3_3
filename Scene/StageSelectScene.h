#pragma once
#include "BaseScene.h"
#include <filesystem>
#include <vector>
#include <string>
#include "Camera.h"

class StageSelectScene : public BaseScene {
public:
    void Initialize() override;
    void Finalize() override;
    void Update() override;
    void Draw2D() override;
    void Draw3D() override;
    void DrawImGui() override;

private:
    struct StageEntry {
        std::filesystem::path path; // resources/stage/xxx.json
        std::wstring fileW;         // xxx.json（日本語含む）
        std::string  fileUtf8;      // xxx.json（UTF-8）
        std::string  titleUtf8;     // xxx（拡張子なし）
    };

    std::vector<StageEntry> entries_;
    int selected_ = -1;

private:
    void Rescan_();
    void Decide_();

    // あなたの StageEditorScene にある関数を共通化できるならそれを使うのがベスト
    static std::string WideToUtf8_(const std::wstring& ws);
    Camera* camera_ = nullptr;

};
