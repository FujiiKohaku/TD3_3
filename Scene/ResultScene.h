#pragma once
#include "BaseScene.h"
#include <memory>
#include "Camera.h"
#include "Object3d.h"

class ResultScene : public BaseScene {
public:
	ResultScene (int perfectCount, int goodCount);

	void Initialize () override;

	void Finalize () override;

	void Update () override;

	void Draw2D () override;
	void Draw3D () override;
	void DrawImGui () override;

private:
	//カメラ
	std::unique_ptr<Camera> camera_ = nullptr;

	//天球
	std::unique_ptr<Object3d> skydome_ = nullptr;

	//ステージの成績
	int perfectCount_ = 0;
	int goodCount_ = 0;
};