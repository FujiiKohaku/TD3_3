#pragma once
#include "MathStruct.h"

struct CameraData {
	Transform transform;
	Matrix4x4 world;
	Matrix4x4 view;
	Matrix4x4 proj;
	Matrix4x4 vp;
};

class BaseCamera {
public:
	//デフォルトデストラクタ
	virtual ~BaseCamera () = default;

	//初期化処理 (純粋仮想関数)
	virtual void Initialize (const Transform& transform) = 0;
	//更新処理 (純粋仮想関数)
	virtual void Update () = 0;
	//ImGui (純粋仮想関数)
	virtual void ImGui () = 0;

	//CameraData取得
	CameraData& GetCameraData () { return camera_; }
	//transform取得
	const Transform& GetTransform () { return camera_.transform; }
	//scale設定
	Vector3 GetScale () { return camera_.transform.scale; }
	void SetScale (const Vector3& scale) { camera_.transform.scale = scale; }
	//rotate設定
	Vector3 GetRotate () { return camera_.transform.rotate; }
	void SetRotate (const Vector3& rotate) { camera_.transform.rotate = rotate; }
	//translate設定
	Vector3 GetTranslate () { return camera_.transform.translate; }
	void SetTranslate (const Vector3& translate) { camera_.transform.translate = translate; }
	//world行列取得
	const Matrix4x4& GetWorldMat () { return camera_.world; }
	//view行列取得
	const Matrix4x4& GetViewMat () { return camera_.view; };
	//projection行列取得
	const Matrix4x4& GetProjMat () { return camera_.proj; }
	//vp行列取得
	const Matrix4x4& GetVPMat () { return camera_.vp; }

protected:
	CameraData camera_ = {};
};