#pragma once
#include "MathStruct.h"

class Object3d;
class Object3dManager;
class Camera;

class LandingEffect {
public:
    LandingEffect();
    ~LandingEffect();

    void Initialize(Object3dManager* objManager, Camera* camera);

    void Play(const Vector3& pos);

    void Update(float dt);
    void Draw();

private:
    Object3d* effectObj_;

    bool active_;
    float time_;

    float startScale_;
    float endScale_;
    float yOffset_;
    float duration_ = 0.3f;

};
