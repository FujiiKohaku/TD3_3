#pragma once
#include <algorithm>
#include "Gate.h"
#include "Object3d.h"

struct GateVisual {
    Gate gate;

    // 外側(Good範囲)と内側(Perfect範囲)を別オブジェクトで描く
    Object3d objGood;
    Object3d objPerfect;

    // 見た目の厚み（板っぽくする）
    float visualThicknessMul = 1.0f;

    void Initialize(Object3dManager* mgr, const std::string& modelPath, Camera* cam) {
        objGood.Initialize(mgr);
        objGood.SetModel(modelPath);
        objGood.SetCamera(cam);

        objPerfect.Initialize(mgr);
        objPerfect.SetModel(modelPath);
        objPerfect.SetCamera(cam);
    }

    void Tick(float dt) {
        gate.UpdateMatrices();
        gate.Tick(dt);

        // 半径を見た目に直結：XY=半径*2、Z=薄く
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

    bool TryPass(const Vector3& dronePos, GateResult& res) {
        return gate.TryPass(dronePos, res);
    }

    void Draw() {
        objGood.Draw();
        objPerfect.Draw();
    }

private:
    void ApplyColor_() {
        // 通常時：ゾーンが分かるように薄色で固定
        // フラッシュ中：判定色で強調
        const bool flashing = (gate.feedbackTimer > 0.0f);
        const Color4 flash = gate.GetDrawColor();

        // Good（外側）
        if (Material* m = objGood.GetMaterial()) {
            if (flashing) {
                m->color = { flash.r, flash.g, flash.b, 0.9f };
            } else {
                m->color = { 0.2f, 1.0f, 0.3f, 0.35f }; // 薄緑
            }
        }

        // Perfect（内側）
        if (Material* m = objPerfect.GetMaterial()) {
            if (flashing) {
                m->color = { flash.r, flash.g, flash.b, 0.9f };
            } else {
                m->color = { 0.2f, 0.7f, 1.0f, 0.35f }; // 薄青
            }
        }
    }
};
