#pragma once
#include "BaseCamera.h"

class MainCamera : public BaseCamera {
public:
	//コンストラクタ
	MainCamera ();
	//デフォルトデストラクタ
	~MainCamera () override;

	//初期化処理 (純粋仮想関数)
	void Initialize (const Transform& transform) override;
	//更新処理 (純粋仮想関数)
	void Update () override;
	//ImGui (純粋仮想関数)
	void ImGui () override;
};