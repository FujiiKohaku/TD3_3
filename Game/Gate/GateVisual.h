#pragma once
#include "Gate.h"
#include "Object3d.h"
#include <algorithm>
#include <cmath>

struct GateVisual {
    Gate gate;

    // 外側(Good範囲)と内側(Perfect範囲)
    Object3d objGood;
    Object3d objPerfect;

    // 見た目の厚み
    float visualThicknessMul = 1.0f;

    // ネオン用時間
    float neonTime = 0.0f;

    // 選択中か
    bool selected = false;
    void SetSelected(bool v) { selected = v; }

    void Initialize(Object3dManager* mgr, const std::string& modelPath, Camera* cam)
    {
        objGood.Initialize(mgr);
        objGood.SetModel(modelPath);
        objGood.SetCamera(cam);
        objGood.SetEnableLighting(false);

        objPerfect.Initialize(mgr);
        objPerfect.SetModel(modelPath);
        objPerfect.SetCamera(cam);
        objPerfect.SetEnableLighting(false);
    }

    void Tick(float dt)
    {
        neonTime += dt;

        gate.UpdateMatrices();
        gate.Tick(dt);

        const float z = std::max<float>(0.05f, gate.thickness * visualThicknessMul);

        // Good（外側）
        objGood.SetTranslate(gate.pos);
        objGood.SetRotate(gate.rot);
        objGood.SetScale({ gate.gateRadius, gate.gateRadius, z });

        // Perfect（内側）
        objPerfect.SetTranslate(gate.pos);
        objPerfect.SetRotate(gate.rot);
        objPerfect.SetScale({ gate.perfectRadius, gate.perfectRadius, z });

        ApplyColor_();

        objGood.Update();
        objPerfect.Update();
    }

    bool TryPass(const Vector3& dronePos, GateResult& res)
    {
        return gate.TryPass(dronePos, res);
    }

    void Draw()
    {
        objGood.Draw();
        objPerfect.Draw();
    }

private:
    void ApplyColor_()
    {
        const bool flashing = (gate.feedbackTimer > 0.0f);
        const Color4 flash = gate.GetDrawColor();

        // -----------------------------
        // ネオン脈動（やさしめ）
        // -----------------------------
        // 0.75 ～ 1.0 を往復
        float pulse = 0.875f + 0.125f * std::sin(neonTime * 4.0f);

        // -----------------------------
        // ベース色
        // -----------------------------
        Color4 normalGood {
            0.2f * pulse,
            1.0f * pulse,
            0.3f * pulse,
            1.0f
        };

        Color4 normalPerfect {
            0.2f * pulse,
            0.7f * pulse,
            1.0f * pulse,
            1.0f
        };

        // 選択時（少し強め）
        Color4 selGood {
            1.0f * pulse,
            0.9f * pulse,
            0.1f * pulse,
            0.95f
        };

        Color4 selPerfect {
            1.0f * pulse,
            0.5f * pulse,
            0.1f * pulse,
            0.95f
        };

        // -----------------------------
        // Good（外側）
        // -----------------------------
        if (Material* m = objGood.GetMaterial()) {
            if (flashing) {
                m->color = { flash.r, flash.g, flash.b, 0.9f };
            } else if (selected) {
                m->color = { selGood.r, selGood.g, selGood.b, selGood.a };
            } else {
                m->color = { normalGood.r, normalGood.g, normalGood.b, normalGood.a };
            }
        }

        // -----------------------------
        // Perfect（内側）
        // -----------------------------
        if (Material* m = objPerfect.GetMaterial()) {
            if (flashing) {
                m->color = { flash.r, flash.g, flash.b, 0.9f };
            } else if (selected) {
                m->color = { selPerfect.r, selPerfect.g, selPerfect.b, selPerfect.a };
            } else {
                m->color = { normalPerfect.r, normalPerfect.g, normalPerfect.b, normalPerfect.a };
            }
        }
    }
};
