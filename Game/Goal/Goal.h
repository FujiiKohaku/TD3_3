#pragma once
#include "Object3d.h"
#include "Object3dManager.h"
#include "MathStruct.h"

// 目的：
// - 非表示/表示の切り替え
// - 3Dモデルの表示
// - 当たり判定（簡易：球 or AABB）
//
// 今回は「球判定」で実装（軽い＆回転関係ない）
class Goal {
public:
    void Initialize(Object3dManager* objMgr, Camera* camera, const char* modelName = "cube.obj");
    void Finalize();

    void Spawn(const Vector3& pos);
    void Hide();

    void Update();
    void Draw();

    bool IsActive() const { return active_; }

    // ドローンが触れたか？（dronePos と半径で判定）
    bool CheckClear(const Vector3& dronePos, float droneRadius = 0.15f) const;

    void SetRadius(float r) { radius_ = r; }
    float GetRadius() const { return radius_; }

    void SetPos(const Vector3& p) { pos_ = p; if (obj_) obj_->SetTranslate(pos_); }
    const Vector3& GetPos() const { return pos_; }

private:
    Object3d* obj_ = nullptr;
    bool active_ = false;

    Vector3 pos_{ 0,0,0 };
    Vector3 scale_{ 0.6f, 0.6f, 0.6f };
    float radius_ = 0.8f; // ゴール判定半径
};
