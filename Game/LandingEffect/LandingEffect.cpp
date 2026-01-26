#include "LandingEffect.h"
#include "Object3d.h"
#include "Object3dManager.h"
#include "Camera.h"
#include <cmath>
#include <algorithm>

LandingEffect::LandingEffect() {
}

LandingEffect::~LandingEffect() {
    for (auto& p : pieces_) {
        delete p.obj;
        p.obj = nullptr;
    }
    pieces_.clear();
}

void LandingEffect::Initialize(Object3dManager* objManager, Camera* camera) {
    objManager_ = objManager;
    camera_ = camera;

    const int pieceCount = 8;
    pieces_.resize(pieceCount);

    for (auto& p : pieces_) {
        p.obj = new Object3d();
        p.obj->Initialize(objManager_);
        p.obj->SetModel("cube.obj");
        p.obj->SetCamera(camera_);
    }
}

void LandingEffect::Play(const Vector3& pos) {
    if (isPlaying_) {
        return;
    }

    isPlaying_ = true;

    const float speed = 3.0f;
    const float upSpeed = 2.0f;
    const float lifeTime = 0.5f;

    const int pieceCount = static_cast<int>(pieces_.size());

    for (int i = 0; i < pieceCount; i++) {
        auto& piece = pieces_[i];

        Vector3 start = pos;
        start.y += yOffset_;
        piece.obj->SetTranslate(start);
        piece.obj->SetScale({ 0.05f, 0.05f, 0.05f });

        float angle = (2.0f * 3.14159265f / pieceCount) * i;
        piece.velocity.x = std::cosf(angle) * speed;
        piece.velocity.z = std::sinf(angle) * speed;
        piece.velocity.y = upSpeed;

        piece.life = lifeTime;

        piece.obj->Update();
    }
}

void LandingEffect::Update(float dt) {
    const float gravity = 9.8f;
    bool anyAlive = false;

    for (auto& p : pieces_) {
        if (p.life <= 0.0f) {
            continue;
        }

        anyAlive = true;

        p.velocity.y -= gravity * dt;

        Vector3 pos = p.obj->GetTranslate();
        pos.x += p.velocity.x * dt;
        pos.y += p.velocity.y * dt;
        pos.z += p.velocity.z * dt;

        p.obj->SetTranslate(pos);
        p.obj->Update();

        p.life -= dt;
    }

    if (!anyAlive) {
        isPlaying_ = false;
    }
}

void LandingEffect::Draw() {
    for (auto& p : pieces_) {
        if (p.life > 0.0f) {
            p.obj->Draw();
        }
    }
}
