#pragma once
#include <vector>
#include <algorithm>
#include <cmath>
#include <cfloat>
#include <memory>
#include "MathStruct.h" // Vector3
#include "Object3d.h"
#include "Object3dManager.h"

// ========================
// Minimal math helpers
// ========================
static inline Vector3 V3Add(const Vector3& a, const Vector3& b) { return { a.x + b.x, a.y + b.y, a.z + b.z }; }
static inline Vector3 V3Sub(const Vector3& a, const Vector3& b) { return { a.x - b.x, a.y - b.y, a.z - b.z }; }
static inline Vector3 V3Mul(const Vector3& a, float s) { return { a.x * s, a.y * s, a.z * s }; }
static inline float   V3Dot(const Vector3& a, const Vector3& b) { return a.x * b.x + a.y * b.y + a.z * b.z; }
static inline float   V3Len(const Vector3& v) { return std::sqrt(V3Dot(v, v)); }
static inline Vector3 V3Norm(const Vector3& v) {
    float l = V3Len(v);
    if (l < 1e-6f) return { 0,0,0 };
    return { v.x / l, v.y / l, v.z / l };
}
static inline Vector3 V3Abs(const Vector3& v) { return { std::abs(v.x), std::abs(v.y), std::abs(v.z) }; }
static inline float   Clamp(float v, float a, float b) { return std::clamp(v, a, b); }

// ========================
// Shapes
// ========================
struct AABB3 {
    Vector3 min;
    Vector3 max;
};

static inline AABB3 MakeAABB_CenterHalf(const Vector3& c, const Vector3& half) {
    return { {c.x - half.x, c.y - half.y, c.z - half.z},
             {c.x + half.x, c.y + half.y, c.z + half.z} };
}

static inline bool IntersectAABB(const AABB3& a, const AABB3& b) {
    return (a.min.x <= b.max.x && a.max.x >= b.min.x) &&
        (a.min.y <= b.max.y && a.max.y >= b.min.y) &&
        (a.min.z <= b.max.z && a.max.z >= b.min.z);
}

static inline Vector3 CenterOf(const AABB3& a) {
    return { (a.min.x + a.max.x) * 0.5f, (a.min.y + a.max.y) * 0.5f, (a.min.z + a.max.z) * 0.5f };
}

// ========================
// OBB (oriented box)
// rot: Euler(rad) (pitch, yaw, roll) として扱う
// basis: ローカル軸 (x,y,z) をワールドに向けた単位ベクトル
// ========================
struct OBB {
    Vector3 center{ 0,0,0 };
    Vector3 half{ 1,1,1 };
    Vector3 rot{ 0,0,0 }; // rad
};

// Euler -> basis (XYZ回転順で1つに固定)
// ※あなたのエンジン側で行列があるならそれに置き換えてOK
static inline void MakeBasisFromEulerXYZ(const Vector3& rot, Vector3& outX, Vector3& outY, Vector3& outZ)
{
    const float cx = std::cos(rot.x), sx = std::sin(rot.x);
    const float cy = std::cos(rot.y), sy = std::sin(rot.y);
    const float cz = std::cos(rot.z), sz = std::sin(rot.z);

    // 回転行列 R = Rz * Ry * Rx (よくある形)
    // 列ベクトルがローカル軸の向き、という扱いにします
    // Xaxis
    outX = { cz * cy,  sz * cy,  -sy };
    // Yaxis
    outY = { cz * sy * sx - sz * cx,  sz * sy * sx + cz * cx,  cy * sx };
    // Zaxis
    outZ = { cz * sy * cx + sz * sx,  sz * sy * cx - cz * sx,  cy * cx };

    outX = V3Norm(outX);
    outY = V3Norm(outY);
    outZ = V3Norm(outZ);
}

// AABB(ドローン) vs OBB(壁) を SAT で解決（最小押し戻しMTV）
// 返り値: ぶつかってたら true, outPush に押し戻しベクトル
static inline bool ResolveAABB_vs_OBB_MinPush(
    const Vector3& aabbCenter, const Vector3& aabbHalf,
    const OBB& obb,
    Vector3& outPush
) {
    // AABB を「OBB座標系」に持っていって、OBB(軸平行) vs AABB(軸平行) に近い形で解く
    // SAT：テスト軸は obbの3軸 + worldの3軸（+ crossは厳密には必要）
    // ここでは “壁”用途として安定を優先して「15軸SAT(交差軸も含む)」を入れます。

    Vector3 Ax, Ay, Az;
    MakeBasisFromEulerXYZ(obb.rot, Ax, Ay, Az);

    // ワールド軸
    const Vector3 Wx{ 1,0,0 };
    const Vector3 Wy{ 0,1,0 };
    const Vector3 Wz{ 0,0,1 };

    // AABB中心 -> OBB中心
    const Vector3 D = V3Sub(aabbCenter, obb.center);

    auto ProjectRadiusAABB_OnAxis = [&](const Vector3& axis)->float {
        // AABB half をワールド軸で持つので: r = sum(half_i * |dot(worldAxis_i, axis)|)
        return aabbHalf.x * std::abs(V3Dot(Wx, axis)) +
            aabbHalf.y * std::abs(V3Dot(Wy, axis)) +
            aabbHalf.z * std::abs(V3Dot(Wz, axis));
        };

    auto ProjectRadiusOBB_OnAxis = [&](const Vector3& axis)->float {
        // OBB half と basis から: r = sum(half_i * |dot(basis_i, axis)|)
        return obb.half.x * std::abs(V3Dot(Ax, axis)) +
            obb.half.y * std::abs(V3Dot(Ay, axis)) +
            obb.half.z * std::abs(V3Dot(Az, axis));
        };

    auto TestAxis = [&](const Vector3& axis, float& outMinOverlap, Vector3& outMinAxis)->bool {
        const float len = V3Len(axis);
        if (len < 1e-6f) return true; // 軸が無効ならスキップ
        const Vector3 n = V3Mul(axis, 1.0f / len);

        const float dist = std::abs(V3Dot(D, n));
        const float ra = ProjectRadiusAABB_OnAxis(n);
        const float rb = ProjectRadiusOBB_OnAxis(n);
        const float overlap = (ra + rb) - dist;

        if (overlap < 0.0f) return false; // 分離

        if (overlap < outMinOverlap) {
            outMinOverlap = overlap;
            outMinAxis = n;
        }
        return true;
        };

    float minOverlap = FLT_MAX;
    Vector3 minAxis{ 0,0,0 };

    // 15軸: 3(OBB) + 3(World) + 9(cross)
    Vector3 axes[15] = {
        Ax, Ay, Az,
        Wx, Wy, Wz,
        // cross
        {0,0,0},{0,0,0},{0,0,0},
        {0,0,0},{0,0,0},{0,0,0},
        {0,0,0},{0,0,0},{0,0,0},
    };
    int idx = 6;
    auto Cross = [](const Vector3& a, const Vector3& b)->Vector3 {
        return { a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x };
        };
    const Vector3 A[3] = { Ax, Ay, Az };
    const Vector3 W[3] = { Wx, Wy, Wz };
    for (int i = 0; i < 3; ++i) {
        for (int j = 0; j < 3; ++j) {
            axes[idx++] = Cross(A[i], W[j]);
        }
    }

    for (int i = 0; i < 15; ++i) {
        if (!TestAxis(axes[i], minOverlap, minAxis)) {
            return false;
        }
    }

    // 押す向きを決める（Dと逆なら反転）
    if (V3Dot(D, minAxis) < 0.0f) {
        minAxis = V3Mul(minAxis, -1.0f);
    }

    outPush = V3Mul(minAxis, minOverlap);
    return true;
}

// AABB vs AABB の最小押し戻し
static inline bool ResolveAABB_vs_AABB_MinPush(const AABB3& moving, const AABB3& solid, Vector3& outPush)
{
    if (!IntersectAABB(moving, solid)) return false;

    const Vector3 ca = CenterOf(moving);
    const Vector3 cb = CenterOf(solid);

    const float ox = std::min<float>(moving.max.x, solid.max.x) - std::max<float>(moving.min.x, solid.min.x);
    const float oy = std::min<float>(moving.max.y, solid.max.y) - std::max<float>(moving.min.y, solid.min.y);
    const float oz = std::min<float>(moving.max.z, solid.max.z) - std::max<float>(moving.min.z, solid.min.z);

    outPush = { 0,0,0 };
    if (ox <= oy && ox <= oz) outPush.x = (ca.x < cb.x) ? -ox : +ox;
    else if (oy <= ox && oy <= oz) outPush.y = (ca.y < cb.y) ? -oy : +oy;
    else outPush.z = (ca.z < cb.z) ? -oz : +oz;

    return true;
}

// ========================
// Wall System (class)
// ========================
class WallSystem {
public:
    enum class Type { AABB, OBB };

    struct Wall {
        Type type = Type::AABB;
        Vector3 center{ 0,0,0 };
        Vector3 half{ 1,1,1 };
        Vector3 rot{ 0,0,0 }; // OBBのみ使用
    };

    void Clear() {
        walls_.clear();
        ClearDebug();
    }

    int AddAABB(const Vector3& center, const Vector3& half) {
        Wall w;
        w.type = Type::AABB;
        w.center = center;
        w.half = half;
        walls_.push_back(w);
        dirtyDebug_ = true;
        return (int)walls_.size() - 1;
    }

    int AddOBB(const Vector3& center, const Vector3& half, const Vector3& rotRad) {
        Wall w;
        w.type = Type::OBB;
        w.center = center;
        w.half = half;
        w.rot = rotRad;
        walls_.push_back(w);
        dirtyDebug_ = true;
        return (int)walls_.size() - 1;
    }

    std::vector<Wall>& Walls() { return walls_; }
    const std::vector<Wall>& Walls() const { return walls_; }

    // ドローン(AABB)を壁と衝突解決
    // pos/vel を参照更新する
    void ResolveDroneAABB(Vector3& pos, Vector3& vel, const Vector3& droneHalf, float /*dt*/, int iterations = 4)
    {
        for (int iter = 0; iter < iterations; ++iter) {
            bool hitAny = false;

            const Vector3 aabbCenter = pos;
            AABB3 droneBox = MakeAABB_CenterHalf(pos, droneHalf);

            for (const auto& w : walls_) {
                Vector3 push{ 0,0,0 };

                if (w.type == Type::AABB) {
                    const AABB3 wallBox = MakeAABB_CenterHalf(w.center, w.half);
                    if (ResolveAABB_vs_AABB_MinPush(droneBox, wallBox, push)) {
                        hitAny = true;
                    }
                } else {
                    OBB obb;
                    obb.center = w.center;
                    obb.half = w.half;
                    obb.rot = w.rot;
                    if (ResolveAABB_vs_OBB_MinPush(aabbCenter, droneHalf, obb, push)) {
                        hitAny = true;
                    }
                }

                if (hitAny && (std::abs(push.x) + std::abs(push.y) + std::abs(push.z) > 0.0f)) {
                    pos = V3Add(pos, push);

                    // 押した軸の速度を殺す（滑り）
                    if (std::abs(push.x) > 0.0f) vel.x = 0.0f;
                    if (std::abs(push.y) > 0.0f) vel.y = 0.0f;
                    if (std::abs(push.z) > 0.0f) vel.z = 0.0f;

                    // 更新
                    droneBox = MakeAABB_CenterHalf(pos, droneHalf);
                }
            }

            if (!hitAny) break;
        }
    }

    // -------------- Debug draw (cube.obj で可視化) --------------
    void BuildDebug(Object3dManager* mgr, const char* modelName = "cube.obj")
    {
        mgr_ = mgr;
        modelName_ = modelName ? modelName : "cube.obj";
        dirtyDebug_ = true;
        RebuildDebugIfNeeded_();
    }

    void UpdateDebug()
    {
        RebuildDebugIfNeeded_();
        for (size_t i = 0; i < debugObjs_.size(); ++i) {
            auto* o = debugObjs_[i].get();
            const Wall& w = walls_[i];

            o->SetTranslate(w.center);
            o->SetScale(w.half); // ★cube.objは半サイズ1なので half をそのまま
            if (w.type == Type::OBB) {
                o->SetRotate(w.rot);
            } else {
                o->SetRotate({ 0,0,0 });
            }
            o->Update();
        }
    }

    void DrawDebug()
    {
        for (auto& o : debugObjs_) {
            if (o) o->Draw();
        }
    }

    void ClearDebug()
    {
        debugObjs_.clear();
        mgr_ = nullptr;
        dirtyDebug_ = true;
    }

private:
    void RebuildDebugIfNeeded_()
    {
        if (!mgr_) return;
        if (!dirtyDebug_) return;

        debugObjs_.clear();
        debugObjs_.reserve(walls_.size());

        for (size_t i = 0; i < walls_.size(); ++i) {
            std::unique_ptr<Object3d> o(new Object3d());
            o->Initialize(mgr_);
            o->SetModel(modelName_.c_str());
            o->SetTranslate(walls_[i].center);
            o->SetScale(walls_[i].half);
            if (walls_[i].type == Type::OBB) o->SetRotate(walls_[i].rot);
            o->Update();
            debugObjs_.push_back(std::move(o));
        }

        dirtyDebug_ = false;
    }

private:
    std::vector<Wall> walls_;

    // debug draw
    Object3dManager* mgr_ = nullptr;
    std::string modelName_ = "cube.obj";
    bool dirtyDebug_ = true;
    std::vector<std::unique_ptr<Object3d>> debugObjs_;
};
