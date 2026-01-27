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

    // 選択中か
    bool selected = false;
    void SetSelected(bool v) { selected = v; }

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
        const bool flashing = (gate.feedbackTimer > 0.0f);
        const Color4 flash = gate.GetDrawColor();

        // ★選択時の色（目立つやつ）
        const Color4 selGood{ 1.0f, 0.9f, 0.1f, 0.95f }; // 黄色
        const Color4 selPerfect{ 1.0f, 0.5f, 0.1f, 0.95f }; // オレンジ寄り（内側）

        // Good（外側）
        if (Material* m = objGood.GetMaterial()) {
            if (flashing) {
                m->color = { flash.r, flash.g, flash.b, 0.9f };
            } else if (selected) {
                m->color = { selGood.r, selGood.g, selGood.b, selGood.a };
            } else {
                m->color = { 0.2f, 1.0f, 0.3f, 1.0f }; // 薄緑
            }
        }

        // Perfect（内側）
        if (Material* m = objPerfect.GetMaterial()) {
            if (flashing) {
                m->color = { flash.r, flash.g, flash.b, 0.9f };
            } else if (selected) {
                m->color = { selPerfect.r, selPerfect.g, selPerfect.b, selPerfect.a };
            } else {
                m->color = { 0.2f, 0.7f, 1.0f, 1.0f }; // 薄青
            }
        }
    }

};
