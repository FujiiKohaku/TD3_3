#pragma once
#include "BaseScene.h"
#include <string> 

class SceneManager {
public:
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
};
