#pragma once
// #pragma message("GoalSystem.h included from: " __FILE__) // デバッグ用。動いたら消してOK

#include <vector>
#include <cmath>
#include "MathStruct.h"   // Vector3 が入っている想定
#include "Object3d.h"     // Object3d を使う

class Object3dManager;
class Camera;
struct GateVisual;

class GoalSystem {
public:
    // modelName は "cube.obj" など
    void Initialize(Object3dManager* objMgr, Camera* camera, const char* modelName = "cube.obj");
    void Finalize();

    void Reset(); // ゲートやステージをやり直す時に呼ぶ

    // gates / nextGate を見て、全通過したら出す。出てるなら接触判定。
    void Update(const std::vector<GateVisual>& gates, int nextGate, const Vector3& dronePos);

    void Draw();

    bool IsGoalActive() const { return active_; }
    bool IsCleared()   const { return cleared_; }

    // ---- 調整用 ----
    void SetSpawnOffset(const Vector3& ofs) { spawnOffset_ = ofs; }   // 最後ゲートからの出現オフセット
    void SetGoalRadius(float r) { goalRadius_ = r; }                  // ゴール判定半径
    void SetDroneRadius(float r) { droneRadius_ = r; }                // ドローン半径（当たり判定用）
    void SetGoalScale(const Vector3& s) { goalScale_ = s; if (goalObj_) goalObj_->SetScale(goalScale_); }

    const Vector3& GetGoalPos() const { return goalPos_; }

    const Vector3& GetSpawnOffset() const { return spawnOffset_; }

    void SetGoalPos(const Vector3& p);

    // 薄さ（0..1）
    void SetGoalAlpha(float a);

    // 編集用：ゲート未達でも強制表示（posを渡さないなら現在位置のまま）
    void ForceSpawn(const Vector3* pos = nullptr);

    // 強制表示を解除（通常挙動に戻す）
    void ClearForceSpawn();

private:
    // --- 内部 ---
    void SpawnAt_(const Vector3& pos);
    bool CheckTouch_(const Vector3& dronePos) const;

    void ApplyVisual_(); // 色/スケールなどまとめて反映（追加）

private:
    Object3dManager* objMgr_ = nullptr;
    Camera* camera_ = nullptr;

    Object3d* goalObj_ = nullptr;

    bool spawned_ = false; // 一度出したか
    bool active_ = false; // 表示/判定が有効か
    bool cleared_ = false; // 触れてクリアしたか

    Vector3 goalPos_{ 0,0,0 };
    Vector3 goalScale_{ 0.6f, 0.6f, 0.6f };

    // 最後のゲートからの出現位置オフセット（デフォルト：少し前）
    Vector3 spawnOffset_{ 0.0f, 0.0f, 6.0f };

    // 当たり判定（球）
    float goalRadius_ = 0.9f;
    float droneRadius_ = 0.15f;

    //デバッグ用
    bool forceActive_ = false; // ★追加
    float goalAlpha_ = 0.5f;  // ★薄い（デフォルト）

};
