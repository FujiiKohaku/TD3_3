#pragma once
#include "BaseScene.h"
#include <memory>
#include "MainCamera.h"

class ResultScene : public BaseScene {
public:
	ResultScene ();

	void Initialize () override;

	void Finalize () override;

	void Update () override;

	void Draw2D () override;
	void Draw3D () override;
	void DrawImGui () override;

private:
	//カメラ
	std::unique_ptr<BaseCamera> camera_ = nullptr;
};