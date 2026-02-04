#pragma once
#include "Sprite.h"
#include <memory>
#include <string>

class FadeManager {
public:
    enum class Status {
        None,
        FadeIn,      // フェードイン中（暗→明）
        FadeOut,     // フェードアウト中（明→暗）
        FadeInFinished,  // フェードイン完了（ここでゲーム開始！）
        FadeOutFinished  // フェードアウト完了（ここでシーン遷移！）
    };

    // --- シングルトン関連 ---
    static FadeManager* GetInstance();
    void Finalize();

    // --- 基本機能 ---
    void Initialize(SpriteManager* sm, const std::string& texturePath);
    void Update();
    void Draw();

    void StartFadeIn(float duration);
    void StartFadeOut(float duration);

    Status GetStatus() const { return status_; }

private:
    FadeManager() = default;
    ~FadeManager() = default;
    FadeManager(const FadeManager&) = delete;
    FadeManager& operator=(const FadeManager&) = delete;

    static FadeManager* instance;

private:
    std::unique_ptr<Sprite> fadeSprite_ = nullptr;
    Status status_ = Status::None;

    float timer_ = 0.0f;
    float duration_ = 1.0f;
    float alpha_ = 0.0f;
};