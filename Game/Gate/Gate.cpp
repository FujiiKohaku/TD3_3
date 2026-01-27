#include "Gate.h"

// row-vector 座標変換（あなたの行列配置に合わせる）
static Vector3 TransformCoord_RowVector(const Vector3& v, const Matrix4x4& m)
{
    Vector3 out;
    out.x = v.x * m.m[0][0] + v.y * m.m[1][0] + v.z * m.m[2][0] + 1.0f * m.m[3][0];
    out.y = v.x * m.m[0][1] + v.y * m.m[1][1] + v.z * m.m[2][1] + 1.0f * m.m[3][1];
    out.z = v.x * m.m[0][2] + v.y * m.m[1][2] + v.z * m.m[2][2] + 1.0f * m.m[3][2];

    const float w = v.x * m.m[0][3] + v.y * m.m[1][3] + v.z * m.m[2][3] + 1.0f * m.m[3][3];
    if (std::abs(w) > 1e-6f) {
        out.x /= w; out.y /= w; out.z /= w;
    }
    return out;
}

void Gate::UpdateMatrices()
{
    world = MatrixMath::MakeAffineMatrix({ 1,1,1 }, rot, pos);
    invWorld = MatrixMath::Inverse(world);
}

void Gate::Tick(float dt)
{
    if (feedbackTimer > 0.0f) {
        feedbackTimer -= dt;
        if (feedbackTimer <= 0.0f) {
            feedbackTimer = 0.0f;
            lastResult = GateResult::None;
        }
    }
}

Color4 Gate::GetDrawColor() const
{
    if (feedbackTimer > 0.0f) {
        switch (lastResult) {
        case GateResult::Perfect: return colorPerfect;
        case GateResult::Good:    return colorGood;
        case GateResult::Miss:    return colorMiss;
        default: break;
        }
    }
    return baseColor;
}

bool Gate::GetIsHitGate() const
{
    return isHitGate_;
}

bool Gate::TryPass(const Vector3& droneWorldPos, GateResult& outResult)
{
    outResult = GateResult::None;

    // ゲートローカルへ（回転対応のキモ）
    const Vector3 pLocal = TransformCoord_RowVector(droneWorldPos, invWorld);

    // ---- デバッグ用（常に最新値を保持）----
    dbgLocalPos = pLocal;
    dbgPrevZ = prevLocalZ; // ★更新前の prev を入れる

    // 初回は前フレが無いので通過判定しない
    if (!hasPrev) {
        prevLocalZ = pLocal.z;
        hasPrev = true;

        dbgCrossed = false;
        dbgInThickness = false;
        dbgRadius = 0.0f;
        return false;
    }

    // 1) 面を横切った瞬間だけ判定（両方向）
    const bool crossed =
        (prevLocalZ > 0.0f && pLocal.z <= 0.0f) ||
        (prevLocalZ < 0.0f && pLocal.z >= 0.0f);

    dbgCrossed = crossed;

    // 次フレ用更新（★ここで1回だけ）
    prevLocalZ = pLocal.z;

    if (!crossed) {
        dbgInThickness = false;
        dbgRadius = std::sqrt(pLocal.x * pLocal.x + pLocal.y * pLocal.y); // 参考で更新してもOK
        return false;
    }

    // 2) 厚み内か
    const float halfT = thickness * 0.5f;
    const bool inThickness = (std::abs(pLocal.z) <= halfT);
    dbgInThickness = inThickness;

    // 3) 半径評価（ローカルXY距離）
    const float r = std::sqrt(pLocal.x * pLocal.x + pLocal.y * pLocal.y);
    dbgRadius = r;

    if (!inThickness) {
        outResult = GateResult::Miss;
    } else if (r <= perfectRadius) {
        outResult = GateResult::Perfect;
    } else if (r <= gateRadius) {
        outResult = GateResult::Good;
    } else {
        outResult = GateResult::Miss;
    }

    // 4) 色フィードバック
    lastResult = outResult;
    feedbackTimer = feedbackDuration;

    isHitGate_ = true;
    return true; // “通過イベント”が起きた
}
