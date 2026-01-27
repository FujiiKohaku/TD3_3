#include "StageSelectScene.h"
#include "SceneManager.h"
#include "../input/Input.h"
#include "GamePlayScene.h"
#include "StageEditorScene.h"
#include "TitleScene.h"

#include "Sprite.h"
#include "SpriteManager.h"
#include "TextureManager.h"
#include "Object3dManager.h"
#include "ParticleManager.h"
#include "Camera.h"
#include "WinApp.h"

#include <algorithm>
#include <Windows.h>
#include <filesystem>
#include <cstring>   // std::max initializer_list で要る環境もあるので保険
#include <vector>

// -------------------- wide -> utf8 --------------------
std::string StageSelectScene::WideToUtf8_(const std::wstring& ws) {
	if (ws.empty()) return {};
	int size = WideCharToMultiByte(CP_UTF8, 0, ws.data(), (int)ws.size(),
		nullptr, 0, nullptr, nullptr);
	std::string out(size, '\0');
	WideCharToMultiByte(CP_UTF8, 0, ws.data(), (int)ws.size(),
		out.data(), size, nullptr, nullptr);
	return out;
}

// -------------------- GDIで日本語をRGBAへ --------------------
void StageSelectScene::RenderTextToRGBA_GDI_(
	const std::wstring& text,
	uint32_t width,
	uint32_t height,
	std::vector<uint8_t>& outRgba,
	int fontSizePx
) {
	outRgba.assign((size_t)width * height * 4, 0);
	if (width == 0 || height == 0) return;

	BITMAPINFO bmi{};
	bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
	bmi.bmiHeader.biWidth = (LONG)width;
	bmi.bmiHeader.biHeight = -(LONG)height; // top-down
	bmi.bmiHeader.biPlanes = 1;
	bmi.bmiHeader.biBitCount = 32;
	bmi.bmiHeader.biCompression = BI_RGB;

	void* bits = nullptr;
	HDC hdc = GetDC(nullptr);
	HDC memDC = CreateCompatibleDC(hdc);

	HBITMAP dib = CreateDIBSection(memDC, &bmi, DIB_RGB_COLORS, &bits, nullptr, 0);
	HGDIOBJ oldBmp = SelectObject(memDC, dib);

	PatBlt(memDC, 0, 0, (int)width, (int)height, BLACKNESS);

	HFONT font = CreateFontW(
		fontSizePx, 0, 0, 0,
		FW_NORMAL, FALSE, FALSE, FALSE,
		DEFAULT_CHARSET,
		OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY,
		DEFAULT_PITCH | FF_DONTCARE,
		L"Meiryo"
	);
	HGDIOBJ oldFont = SelectObject(memDC, font);

	SetBkMode(memDC, TRANSPARENT);
	SetTextColor(memDC, RGB(255, 255, 255));

	RECT rc{ 0, 0, (LONG)width, (LONG)height };
	DrawTextW(memDC, text.c_str(), (int)text.size(), &rc,
		DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);

	const uint8_t* src = reinterpret_cast<const uint8_t*>(bits);

	for (uint32_t y = 0; y < height; ++y) {
		for (uint32_t x = 0; x < width; ++x) {
			const size_t i = ((size_t)y * width + x) * 4;

			uint8_t B = src[i + 0];
			uint8_t G = src[i + 1];
			uint8_t R = src[i + 2];

			// 明るさをαに（アンチエイリアス対応）
			int m = (int)R;
			if ((int)G > m) m = (int)G;
			if ((int)B > m) m = (int)B;
			if (m < 0) m = 0;
			if (m > 255) m = 255;

			outRgba[i + 0] = 255;
			outRgba[i + 1] = 255;
			outRgba[i + 2] = 255;
			outRgba[i + 3] = (uint8_t)m;
		}
	}

	SelectObject(memDC, oldFont);
	DeleteObject(font);

	SelectObject(memDC, oldBmp);
	DeleteObject(dib);

	DeleteDC(memDC);
	ReleaseDC(nullptr, hdc);
}

// -------------------- 選択中ファイル名を動的テクスチャへ --------------------
void StageSelectScene::UpdateStageNameTexture_() {
	if (selected_ < 0 || selected_ >= (int)entries_.size()) return;

	// ★Explorerに出てるそのままのファイル名（例: "シーンテスト.json"）
	//const std::wstring w = entries_[selected_].fileW;
	const std::wstring w = entries_[selected_].path.stem().wstring();

	RenderTextToRGBA_GDI_(w, kStageNameTexW, kStageNameTexH, stageNameRgba_, 32);

	TextureManager::GetInstance()->UpdateDynamicTextureRGBA8(
		kStageNameTexKey, stageNameRgba_.data(), kStageNameTexW, kStageNameTexH);
}

// -------------------- Initialize/Finalize --------------------
void StageSelectScene::Initialize() {
	camera_ = new Camera();
	camera_->Initialize();
	camera_->SetTranslate({ 0, 0, 0 });
	Object3dManager::GetInstance()->SetDefaultCamera(camera_);

	ParticleManager::GetInstance()->SetCamera(camera_);

	// ★動的テクスチャを作る（1回だけ）
	TextureManager::GetInstance()->CreateDynamicTextureRGBA8(
		kStageNameTexKey, kStageNameTexW, kStageNameTexH);

	// ★表示用スプライト
	stageNameSprite_ = new Sprite();
	stageNameSprite_->Initialize(SpriteManager::GetInstance(), kStageNameTexKey);
	stageNameSprite_->SetPosition({ 16.0f, 16.0f });
	stageNameSprite_->SetSize({ (float)kStageNameTexW, (float)kStageNameTexH });
	stageNameSprite_->SetColor({ 1,1,1,1 });
	stageNameSprite_->Update();

	Rescan_();

	// ★ここ追加：サムネSpriteを作る
	for (auto& e : entries_) {
		// TextureManagerが「未ロードならロード」が必要な実装なら先にLoad
		auto* tm = TextureManager::GetInstance();
		if (!tm->GetTextureData(e.thumbKeyUtf8)) {
			tm->LoadTexture(e.thumbKeyUtf8);
		}

		e.thumbSprite = new Sprite();
		e.thumbSprite->Initialize(SpriteManager::GetInstance(), e.thumbKeyUtf8);
		e.thumbSprite->SetSize({ kThumbW, kThumbH });
		e.thumbSprite->SetColor({ 1,1,1,1 });
		e.thumbSprite->Update();
	}

	lastSelected_ = selected_;
	if (selected_ >= 0) UpdateStageNameTexture_();
}

void StageSelectScene::Finalize() {
	for (auto& e : entries_) {
		delete e.thumbSprite;
		e.thumbSprite = nullptr;
	}
	entries_.clear();

	delete stageNameSprite_;
	stageNameSprite_ = nullptr;

	delete camera_;
	camera_ = nullptr;
}

// -------------------- Rescan --------------------
void StageSelectScene::Rescan_() {
	// 既存thumbSpriteがあるなら先に破棄（Rescanで作り直すため）
	for (auto& e : entries_) {
		delete e.thumbSprite;
		e.thumbSprite = nullptr;
	}
	entries_.clear();

	const std::filesystem::path dir = std::filesystem::path(L"resources") / L"stage";
	if (!std::filesystem::exists(dir)) {
		selected_ = -1;
		return;
	}

	const std::filesystem::path thumbsDir = dir / L"thumbs";

	for (const auto& e : std::filesystem::directory_iterator(dir)) {
		if (!e.is_regular_file()) continue;

		const auto& p = e.path();
		if (p.extension() != L".json") continue;

		StageEntry se{};
		se.path = p;
		se.fileW = p.filename().wstring();
		se.fileUtf8 = WideToUtf8_(se.fileW);
		se.titleUtf8 = WideToUtf8_(p.stem().wstring());

		// ★サムネパス：resources/stage/thumbs/<stem>.png
		se.thumbPath = thumbsDir / (p.stem().wstring() + L".png");

		// 存在しないならダミー（no_thumb）
		if (!std::filesystem::exists(se.thumbPath)) {
			se.thumbKeyUtf8 = kNoThumbPath;
		}
		else {
			// TextureManagerに渡すためutf8化（パスは「resources/...」でOK想定）
			// Windowsのwstring path → UTF8に
			se.thumbKeyUtf8 = WideToUtf8_(se.thumbPath.wstring());
		}

		entries_.push_back(std::move(se));
	}

	std::sort(entries_.begin(), entries_.end(),
		[](const StageEntry& a, const StageEntry& b) { return a.fileW < b.fileW; });

	selected_ = entries_.empty() ? -1 : 0;

	lastSelected_ = selected_;
	if (selected_ >= 0) {
		UpdateStageNameTexture_();
	}
}

// -------------------- Decide/Update --------------------
void StageSelectScene::Decide_() {
	if (selected_ < 0 || selected_ >= (int)entries_.size()) return;

	SceneManager::GetInstance()->SetSelectedStageFile(entries_[selected_].fileUtf8);
	SceneManager::GetInstance()->SetNextScene(new GamePlayScene());
}

void StageSelectScene::Update() {
	Input& input = *Input::GetInstance();

	if (input.IsKeyTrigger(DIK_F5)) {
		Rescan_();
	}

	if (entries_.empty()) return;

	if (input.IsKeyTrigger(DIK_LEFT))  selected_ = std::max<int>(0, selected_ - 1);
	if (input.IsKeyTrigger(DIK_RIGHT)) selected_ = std::min((int)entries_.size() - 1, selected_ + 1);

	if (input.IsKeyTrigger(DIK_UP))    selected_ = std::max<int>(0, selected_ - kThumbCols);
	if (input.IsKeyTrigger(DIK_DOWN))  selected_ = std::min((int)entries_.size() - 1, selected_ + kThumbCols);


	// ★選択が変わったら日本語表示更新（ここが重要）
	if (selected_ != lastSelected_) {
		lastSelected_ = selected_;
		UpdateStageNameTexture_();
	}

	if (input.IsKeyTrigger(DIK_RETURN)) {
		Decide_();
	}

	if (input.IsKeyTrigger(DIK_ESCAPE)) {
		SceneManager::GetInstance()->SetNextScene(new TitleScene());
	}

	DrawImGui();
}

// -------------------- Draw --------------------
void StageSelectScene::Draw2D() {
	SpriteManager::GetInstance()->PreDraw();

	// ステージ名（日本語）
	if (stageNameSprite_ && selected_ >= 0) {
		stageNameSprite_->Update();
		stageNameSprite_->Draw();
	}

	// サムネ一覧
	for (int i = 0; i < (int)entries_.size(); ++i) {
		auto& e = entries_[i];
		if (!e.thumbSprite) continue;

		const int col = i % kThumbCols;
		const int row = i / kThumbCols;

		const float x = kThumbStartX + col * (kThumbW + kThumbPadX);
		const float y = kThumbStartY + row * (kThumbH + kThumbPadY);

		e.thumbSprite->SetPosition({ x, y });

		// 選択中だけ少し明るく／色変え
		if (i == selected_) {
			e.thumbSprite->SetColor({ 1.0f, 1.0f, 1.0f, 1.0f });
		}
		else {
			e.thumbSprite->SetColor({ 0.75f, 0.75f, 0.75f, 1.0f });
		}

		e.thumbSprite->Update();
		e.thumbSprite->Draw();
	}
}


void StageSelectScene::Draw3D() {
}

void StageSelectScene::DrawImGui() {
	/* ImGui::Begin("Stage Select (A: ImGui)");

	 if (ImGui::Button("Rescan (F5)")) {
		 Rescan_();
	 }

	 ImGui::Separator();

	 if (entries_.empty()) {
		 ImGui::Text("No .json in resources/stage/");
		 ImGui::End();
		 return;
	 }

	 ImGui::Text("Enter : Decide");
	 ImGui::Text("Up/Down : Select");
	 ImGui::Separator();

	 for (int i = 0; i < (int)entries_.size(); ++i) {
		 bool sel = (i == selected_);
		 if (ImGui::Selectable(entries_[i].titleUtf8.c_str(), sel)) {
			 selected_ = i;
		 }
	 }

	 ImGui::Separator();
	 ImGui::Text("Selected file: %s", entries_[selected_].fileUtf8.c_str());

	 if (ImGui::Button("Decide")) {
		 Decide_();
	 }

	 ImGui::End();*/
}
