#include "FadeManager.h"
#include <algorithm>

FadeManager* FadeManager::instance = nullptr;

FadeManager* FadeManager::GetInstance() {
    if (instance == nullptr) {
        instance = new FadeManager();
    }
    return instance;
}

void FadeManager::Finalize() {
    delete instance;
    instance = nullptr;
}

void FadeManager::Initialize(SpriteManager* sm, const std::string& texturePath) {
    fadeSprite_ = std::make_unique<Sprite>();
    fadeSprite_->Initialize(sm, texturePath);

    // 画面全体サイズ（解像度に合わせて調整してほしいでやんす）
    fadeSprite_->SetSize({ 1280.0f, 720.0f });
    fadeSprite_->SetPosition({ 0.0f, 0.0f });
    fadeSprite_->SetAnchorPoint({ 0.0f, 0.0f });

    alpha_ = 0.0f;
    fadeSprite_->SetColor({ 0, 0, 0, alpha_ });
}

void FadeManager::Update() {
    // すでに終わっているなら何もしない
    if (status_ == Status::None ||
        status_ == Status::FadeInFinished ||
        status_ == Status::FadeOutFinished) return;

    timer_ += 1.0f / 60.0f;
    float t = std::clamp(timer_ / duration_, 0.0f, 1.0f);

    if (status_ == Status::FadeIn) {
        alpha_ = 1.0f - t; // 暗 -> 明
        if (t >= 1.0f) {
            status_ = Status::FadeInFinished; // フェードイン終了！
        }
    }
    else if (status_ == Status::FadeOut) {
        alpha_ = t;        // 明 -> 暗
        if (t >= 1.0f) {
            status_ = Status::FadeOutFinished; // フェードアウト終了！
        }
    }

    fadeSprite_->SetColor({ 0, 0, 0, alpha_ });
    fadeSprite_->Update();
}

void FadeManager::Draw() {
    if (!fadeSprite_ || (status_ == Status::None && alpha_ <= 0.0f)) return;
    fadeSprite_->Draw();
}

void FadeManager::StartFadeIn(float duration) {
    status_ = Status::FadeIn;
    duration_ = duration;
    timer_ = 0.0f;
    alpha_ = 1.0f;
}

void FadeManager::StartFadeOut(float duration) {
    status_ = Status::FadeOut;
    duration_ = duration;
    timer_ = 0.0f;
    alpha_ = 0.0f;
}