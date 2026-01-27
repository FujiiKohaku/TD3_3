#pragma once
#include "BaseScene.h"
#include <filesystem>
#include <vector>
#include <string>
#include "Camera.h"
#include "Sprite.h"

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

        // ★追加：thumb
        std::filesystem::path thumbPath; // resources/stage/thumbs/xxx.png
        std::string thumbKeyUtf8;        // TextureManagerに渡すキー（=パス文字列想定）
        Sprite* thumbSprite = nullptr;   // サムネ表示用
    };

    std::vector<StageEntry> entries_;
    int selected_ = -1;

private:
  

    // サムネ表示設定
    static constexpr int kThumbCols = 4;
    static constexpr float kThumbW = 256.0f;
    static constexpr float kThumbH = 144.0f; // 16:9
    static constexpr float kThumbPadX = 16.0f;
    static constexpr float kThumbPadY = 16.0f;
    static constexpr float kThumbStartX = 32.0f;
    static constexpr float kThumbStartY = 120.0f;

    // サムネが無い時のダミー
    static constexpr const char* kNoThumbPath = "resources/ui/no_thumb.png";


private:
    void Rescan_();
    void Decide_();

    // あなたの StageEditorScene にある関数を共通化できるならそれを使うのがベスト
    static std::string WideToUtf8_(const std::wstring& ws);
    Camera* camera_ = nullptr;

    // ===== 日本語描画（動的テクスチャ） =====
    static constexpr const char* kStageNameTexKey = "StageSelect_StageName";
    static constexpr uint32_t kStageNameTexW = 512;
    static constexpr uint32_t kStageNameTexH = 64;

    Sprite* stageNameSprite_ = nullptr;
    std::vector<uint8_t> stageNameRgba_;
    int lastSelected_ = -1;

    void UpdateStageNameTexture_();
    static void RenderTextToRGBA_GDI_(
        const std::wstring& text,
        uint32_t width,
        uint32_t height,
        std::vector<uint8_t>& outRgba,
        int fontSizePx = 32
    );


};
