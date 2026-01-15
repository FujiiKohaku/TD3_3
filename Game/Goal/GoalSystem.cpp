#include "GoalSystem.h"
#include "GateVisual.h"
#include "Object3dManager.h"

static float LengthSq3(const Vector3& v) {
    return v.x * v.x + v.y * v.y + v.z * v.z;
}

void GoalSystem::Initialize(Object3dManager* objMgr, Camera* camera, const char* modelName) {
    objMgr_ = objMgr;
    camera_ = camera;

    if (!goalObj_) {
        goalObj_ = new Object3d();
        goalObj_->Initialize(objMgr_);
        goalObj_->SetModel(modelName);
        goalObj_->SetScale(goalScale_);
        goalObj_->SetTranslate(goalPos_);
        goalObj_->Update();
    }

    Reset();
}

void GoalSystem::Finalize() {
    delete goalObj_;
    goalObj_ = nullptr;

    objMgr_ = nullptr;
    camera_ = nullptr;
}

void GoalSystem::SpawnAt_(const Vector3& pos) {
    spawned_ = true;
    active_ = true;
    goalPos_ = pos;

    if (goalObj_) {
        goalObj_->SetTranslate(goalPos_);
        goalObj_->SetScale(goalScale_);
        goalObj_->Update();
    }
}

bool GoalSystem::CheckTouch_(const Vector3& dronePos) const {
    if (!active_) return false;

    Vector3 d{
        dronePos.x - goalPos_.x,
        dronePos.y - goalPos_.y,
        dronePos.z - goalPos_.z
    };

    const float r = (goalRadius_ + droneRadius_);
    return LengthSq3(d) <= (r * r);
}

void GoalSystem::Update(const std::vector<GateVisual>& gates, int nextGate, const Vector3& dronePos) {
    //if (cleared_) {
    //    // クリア後も表示したいなら active_ を維持
    //    // 消したいなら：active_=false;
    //    return;
    //}

    if (forceActive_) {
        // 強制表示中もクリア判定したいならここだけやる
        if (active_ && !cleared_ && CheckTouch_(dronePos)) {
            cleared_ = true;
        }
        ApplyVisual_();
        return;
    }

    // 1) まだ出してない → 全ゲート通過で出す
    if (!spawned_) {
        if (!gates.empty() && nextGate >= (int)gates.size()) {
            // 最後のゲート位置 + オフセット
            Vector3 p = gates.back().gate.pos;
            p.x += spawnOffset_.x;
            p.y += spawnOffset_.y;
            p.z += spawnOffset_.z;

            SpawnAt_(p);
        }
    }

    // 2) 出ているなら触れたか判定
    if (active_) {
        // 見た目更新（回転させたいならここで）
        if (goalObj_) {
            // 例：くるくる回す（好みで）
            // Vector3 r = goalObj_->GetRotate();  // GetRotateがあるなら
            // r.y += 0.02f;
            // goalObj_->SetRotate(r);
            goalObj_->Update();
        }

        if (CheckTouch_(dronePos)) {
            cleared_ = true;

            // 触れたら消すなら
            // active_ = false;
        }
    }
}

void GoalSystem::Draw() {
    if (!active_) return;
    if (goalObj_) goalObj_->Draw();
}

void GoalSystem::ApplyVisual_()
{
    if (!goalObj_) return;

    goalObj_->SetTranslate(goalPos_);
    goalObj_->SetScale(goalScale_);
    goalObj_->SetColor({ 1.0f, 1.0f, 1.0f, goalAlpha_ });

    goalObj_->Update(); // ★これを追加
}

void GoalSystem::SetGoalAlpha(float a)
{
    // 0..1にクランプ
    if (a < 0.0f) a = 0.0f;
    if (a > 1.0f) a = 1.0f;
    goalAlpha_ = a;
    ApplyVisual_();
}

void GoalSystem::SetGoalPos(const Vector3& p)
{
    goalPos_ = p;

    // 位置はいつでも見た目に反映してOK（active_でDrawするかどうかは別）
    ApplyVisual_();
    goalObj_->Update(); // ← ApplyVisual_の中にUpdate入れないならここで
}

void GoalSystem::ForceSpawn(const Vector3* pos)
{
    forceActive_ = true;
    spawned_ = true;
    active_ = true;
    cleared_ = false;

    if (pos) {
        goalPos_ = *pos;
    }

    ApplyVisual_();
}

void GoalSystem::ClearForceSpawn()
{
    forceActive_ = false;

    // 表示は消す（好みで）
    active_ = false;
    spawned_ = false;
    cleared_ = false;
}

void GoalSystem::Reset()
{
    spawned_ = false;
    active_ = false;
    cleared_ = false;

    // ★強制表示は維持したいなら「falseにしない」
    // forceActive_ を Reset で解除したいなら下を有効に
    // forceActive_ = false;

    // ここで goalPos_ を初期化してるなら注意！
    // 位置を保持したいなら goalPos_ は触らない方がいい
}
