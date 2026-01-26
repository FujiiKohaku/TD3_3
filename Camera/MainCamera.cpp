#include "MainCamera.h"
#include <string>
#include "ImGuiManager.h"
#include "MatrixMath.h"
#include "WinApp.h"

MainCamera::MainCamera () {
	camera_.transform = { {1.0f, 1.0f, 1.0f}, {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, -30.0f} };
	camera_.world = MatrixMath::MakeIdentity4x4 ();
	camera_.view = MatrixMath::MakeIdentity4x4 ();
	camera_.proj = MatrixMath::MakeIdentity4x4 ();
}

MainCamera::~MainCamera () {

}

void MainCamera::Initialize (const Transform& transform) {
	camera_.transform = transform;
	camera_.proj = MatrixMath::MakePerspectiveFovMatrix (0.45f, (float)WinApp::kClientWidth / (float)WinApp::kClientHeight, 0.1f, 1000.0f);
}

void MainCamera::Update () {
	camera_.world = MatrixMath::MakeAffineMatrix (camera_.transform.scale, camera_.transform.rotate, camera_.transform.translate);
	camera_.view = MatrixMath::Inverse (camera_.world);
	camera_.vp = MatrixMath::Multiply (camera_.view, camera_.proj);
}

void MainCamera::ImGui () {
	std::string label = "MainCamera";

	ImGui::DragFloat3 (("scale##" + label).c_str (), &camera_.transform.scale.x, 0.01f);
	ImGui::DragFloat3 (("rotate##" + label).c_str (), &camera_.transform.rotate.x, 0.01f);
	ImGui::DragFloat3 (("translate##" + label).c_str (), &camera_.transform.translate.x, 0.01f);
}