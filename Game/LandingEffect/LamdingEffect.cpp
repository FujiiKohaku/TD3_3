#include "../Game/LandingEffect/LamdingEffect.h"
#include "Object3d.h"
#include "Object3dManager.h"
#include "Camera.h"

LandingEffect::LandingEffect()
    : effectObj_(nullptr)
    , active_(false)
    , time_(0.0f)
    , duration_(0.25f)
    , startScale_(0.05f)
    , endScale_(1.0f)
    , yOffset_(0.02f) {
}

LandingEffect::~LandingEffect() {
    delete effectObj_;
    effectObj_ = nullptr;
}

void LandingEffect::Initialize(Object3dManager* objManager, Camera* camera) {
    effectObj_ = new Object3d();
    effectObj_->Initialize(objManager);

    effectObj_->SetModel("cube.obj");
    effectObj_->SetCamera(camera);

    effectObj_->SetScale({ startScale_, 0.02f, startScale_ });
    effectObj_->SetTranslate({ 0.0f, 0.0f, 0.0f });

    effectObj_->Update();
}

void LandingEffect::Play(const Vector3& pos) {
    if (!effectObj_) {
        return;
    }

    active_ = true;
    time_ = 0.0f;

    Vector3 p = pos;
    p.y += yOffset_;

    effectObj_->SetTranslate(p);
    effectObj_->SetScale({ startScale_, 0.02f, startScale_ });
    effectObj_->Update();
    active_ = true;

}

void LandingEffect::Update(float dt) {
    if (!active_) {
        return;
    }

    time_ += dt;

    float t = time_ / duration_;
    if (t >= 1.0f) {
        active_ = false;
        return;
    }

    float scale = startScale_ + (endScale_ - startScale_) * t;

    effectObj_->SetScale({ scale, 0.02f, scale });
    effectObj_->Update();
}

void LandingEffect::Draw() {
    if (!active_) {
        return;
    }
    if (!effectObj_) {
        return;
    }

    effectObj_->Draw();
}
