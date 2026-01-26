#pragma once
#include "MathStruct.h"
#include <vector>

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
    struct Piece {
        Object3d* obj = nullptr;
        Vector3 velocity{};
        float life = 0.0f;
    };

    std::vector<Piece> pieces_;

    Object3dManager* objManager_ = nullptr;
    Camera* camera_ = nullptr;

    bool isPlaying_ = false;

    float yOffset_ = 0.08f;
};
