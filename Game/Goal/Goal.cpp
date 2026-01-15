#include "Goal.h"
#include <cmath>

static float LengthSq(const Vector3& v) {
    return v.x * v.x + v.y * v.y + v.z * v.z;
}

void Goal::Initialize(Object3dManager* objMgr, Camera* /*camera*/, const char* modelName) {
    if (!obj_) {
        obj_ = new Object3d();
        obj_->Initialize(objMgr);
        obj_->SetModel(modelName);
        obj_->SetScale(scale_);
        obj_->SetTranslate(pos_);
        obj_->Update();
    }
    active_ = false; // 最初は非表示
}

void Goal::Finalize() {
    delete obj_;
    obj_ = nullptr;
}

void Goal::Spawn(const Vector3& pos) {
    active_ = true;
    pos_ = pos;
    if (obj_) {
        obj_->SetTranslate(pos_);
        obj_->SetScale(scale_);
        obj_->Update();
    }
}

void Goal::Hide() {
    active_ = false;
}

void Goal::Update() {
    if (!active_) return;
    if (obj_) obj_->Update();
}

void Goal::Draw() {
    if (!active_) return;
    if (obj_) obj_->Draw();
}

bool Goal::CheckClear(const Vector3& dronePos, float droneRadius) const {
    if (!active_) return false;
    Vector3 d{ dronePos.x - pos_.x, dronePos.y - pos_.y, dronePos.z - pos_.z };
    const float rr = (radius_ + droneRadius);
    return LengthSq(d) <= rr * rr;
}
