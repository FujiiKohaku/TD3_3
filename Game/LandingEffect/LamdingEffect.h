#pragma once
#include "Object3d.h"
#include "Camera.h"

class LandingEffect {
public:
    void Initialize(Object3dManager* objManager, Camera* camera);
    void Play(const Vector3& pos);
    void Update(float dt);
    void Draw();

private:
    Object3d* effectObj_ = nullptr;

    float lifeTime_ = 0.0f;
    float duration_ = 0.25f;
    bool active_ = false;
};
