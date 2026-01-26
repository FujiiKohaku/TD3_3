#include"../LandingEffect/LamdingEffect.h"

void LandingEffect::Initialize(Object3dManager* objManager, Camera* camera) {
    effectObj_ = new Object3d();
    effectObj_->Initialize(objManager);
    effectObj_->SetModel("cube.obj"); // ← 平たい円 or cube でOK
    effectObj_->SetScale({ 0.01f, 0.01f, 0.01f });
    effectObj_->SetCamera(camera);
}

void LandingEffect::Play(const Vector3& pos) {
    if (!effectObj_) {
        return;
    }

    effectObj_->SetTranslate(pos);
    effectObj_->SetScale({ 0.01f, 0.01f, 0.01f });

    lifeTime_ = 0.0f;
    active_ = true;
}

void LandingEffect::Update(float dt) {
    if (!active_) {
        return;
    }

    lifeTime_ += dt;

    float t = lifeTime_ / duration_;
    if (t >= 1.0f) {
        active_ = false;
        return;
    }

    float scale = 0.5f * t;
    effectObj_->SetScale({ scale, 0.01f, scale });
    effectObj_->Update();
}

void LandingEffect::Draw() {
    if (!active_) {
        return;
    }

    effectObj_->Draw();
}
