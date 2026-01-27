#pragma once
#include "BaseScene.h"
#include <string> 

class SceneManager {
public:

    struct ThumbnailRequest {
        bool pending = false;
        std::wstring path;
        uint32_t w = 0;
        uint32_t h = 0;
    };

    BaseScene* GetCurrentScene() const { return scene_; } // ★追加

    void RequestThumbnail(const std::wstring& path, uint32_t w, uint32_t h) {
        thumb_.pending = true;
        thumb_.path = path;
        thumb_.w = w;
        thumb_.h = h;
    }

    bool ConsumeThumbnailRequest(ThumbnailRequest& out) {
        if (!thumb_.pending) return false;
        out = thumb_;
        thumb_.pending = false;
        return true;
    }

    // --------- Singleton取得 ---------
    static SceneManager* GetInstance()
    {
        static SceneManager instance;
        return &instance;
    }

    // --------- 次シーン予約 ---------
    void SetNextScene(BaseScene* nextScene)
    {
        nextScene_ = nextScene;
    }

    // --------- 実行 ---------
    void Update();
    void Draw();
    void Finalize();

    void DrawImGui();

    void SetSelectedStageFile(const std::string& file) { selectedStageFile_ = file; }
    const std::string& GetSelectedStageFile() const { return selectedStageFile_; }

private:
    // --------- Singleton基本処理 ---------
    SceneManager() = default;
    ~SceneManager();

    SceneManager(const SceneManager&) = delete;
    SceneManager& operator=(const SceneManager&) = delete;

private:
    BaseScene* scene_ = nullptr;
    BaseScene* nextScene_ = nullptr;
    std::string selectedStageFile_;
    ThumbnailRequest thumb_;

};
