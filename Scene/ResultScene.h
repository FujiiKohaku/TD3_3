#pragma once
#include "BaseScene.h"


class ResultScene : public BaseScene {
public:
	void Initialize () override;

	void Finalize () override;

	void Update () override;

	void Draw2D () override;
	void Draw3D () override;
	void DrawImGui () override;

private:

};