#include "StageSelectScene.h"
#include "../input/Input.h"
#include "GamePlayScene.h"
#include "SceneManager.h"
#include "StageEditorScene.h"
#include "TitleScene.h"

#include "../Light/LightManager.h"
#include "Camera.h"
#include "Object3dManager.h"
#include "ParticleManager.h"
#include "Sprite.h"
#include "SpriteManager.h"
#include "TextureManager.h"
#include "WinApp.h"

#include <Windows.h>
#include <algorithm>
#include <cstring> // std::max initializer_list で要る環境もあるので保険
#include <filesystem>
#include <vector>

#include <cctype> // isdigit
#include <cstdlib> // strtol
static bool prevAButton_ = false;
static bool prevDpadLeft_  = false;
static bool prevDpadRight_ = false;

static SoundData se_;
// -------------------- wide -> utf8 --------------------
std::string StageSelectScene::WideToUtf8_(const std::wstring& ws)
{
    if (ws.empty())
        return {};
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
    int fontSizePx)
{
    outRgba.assign((size_t)width * height * 4, 0);
    if (width == 0 || height == 0)
        return;

    BITMAPINFO bmi {};
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
        L"Meiryo");
    HGDIOBJ oldFont = SelectObject(memDC, font);

    SetBkMode(memDC, TRANSPARENT);
    SetTextColor(memDC, RGB(255, 255, 255));

    RECT rc { 0, 0, (LONG)width, (LONG)height };
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
            if ((int)G > m)
                m = (int)G;
            if ((int)B > m)
                m = (int)B;
            if (m < 0)
                m = 0;
            if (m > 255)
                m = 255;

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
void StageSelectScene::UpdateStageNameTexture_()
{
    if (selected_ < 0 || selected_ >= (int)entries_.size())
        return;

    // ★Explorerに出てるそのままのファイル名（例: "シーンテスト.json"）
    // const std::wstring w = entries_[selected_].fileW;
    const std::wstring w = entries_[selected_].path.stem().wstring();

    RenderTextToRGBA_GDI_(w, kStageNameTexW, kStageNameTexH, stageNameRgba_, 32);

    TextureManager::GetInstance()->UpdateDynamicTextureRGBA8(
        kStageNameTexKey, stageNameRgba_.data(), kStageNameTexW, kStageNameTexH);
}

// -------------------- Initialize/Finalize --------------------
void StageSelectScene::Initialize()
{
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
    stageNameSprite_->SetColor({ 1, 1, 1, 1 });
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
        e.thumbSprite->SetColor({ 1, 1, 1, 1 });
        e.thumbSprite->Update();
    }

    lastSelected_ = selected_;
    if (selected_ >= 0)
        UpdateStageNameTexture_();

    ModelManager::GetInstance()->LoadModel("skydome.obj");
    TextureManager::GetInstance()->LoadTexture("resources/skydome.png");
    skydome_ = std::make_unique<Object3d>();
    skydome_->Initialize(Object3dManager::GetInstance());
    skydome_->SetModel("skydome.obj");
    skydome_->SetCamera(camera_);
    skydome_->SetEnableLighting(false);
    skydome_->SetTranslate({ 0.0f, 0.01f, 0.0f });
    skydome_->SetTranslate({ 0.0f, 0.01f, 0.0f });

    lastSelected_ = selected_;
    if (selected_ >= 0)
        UpdateStageNameTexture_();

    // BGM、SE

    selectSeData_ = SoundManager::GetInstance()->SoundLoadFile("resources/Select.mp3");

    bgm = SoundManager::GetInstance()->SoundLoadFile("Resources/BGM.wav");
    se_ = SoundManager::GetInstance()->SoundLoadFile("resources/maou_se_system49.mp3");
    carouselCenterX_ = WinApp::kClientWidth * 0.5f;
    carouselCenterY_ = WinApp::kClientHeight * 0.55f; // 少し下寄せ

    FadeManager::GetInstance()->StartFadeIn(1.0f);

    titleSprite_ = new Sprite();
    titleSprite_->Initialize(SpriteManager::GetInstance(), "resources/space11.png");
    titleSprite_->SetPosition({ 0.0f, 0.0f });
}

void StageSelectScene::Finalize()
{
    for (auto& e : entries_) {
        delete e.thumbSprite;
        e.thumbSprite = nullptr;
    }
    entries_.clear();

    delete stageNameSprite_;
    stageNameSprite_ = nullptr;

    delete camera_;
    camera_ = nullptr;

    delete titleSprite_;
    titleSprite_ = nullptr;
    SoundManager::GetInstance()->StopBGMAll();
}

// -------------------- Rescan --------------------
void StageSelectScene::Rescan_()
{
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
        if (!e.is_regular_file())
            continue;

        const auto& p = e.path();
        if (p.extension() != L".json")
            continue;

        const std::wstring stem = p.stem().wstring(); // 拡張子抜き
        if (stem.rfind(L"__", 0) == 0)
            continue;

        StageEntry se {};
        se.path = p;
        se.fileW = p.filename().wstring();
        se.fileUtf8 = WideToUtf8_(se.fileW);
        se.titleUtf8 = WideToUtf8_(p.stem().wstring());

        // ★サムネパス：resources/stage/thumbs/<stem>.png
        se.thumbPath = thumbsDir / (p.stem().wstring() + L".png");

        // 存在しないならダミー（no_thumb）
        if (!std::filesystem::exists(se.thumbPath)) {
            se.thumbKeyUtf8 = kNoThumbPath;
        } else {
            // ★キーを必ず / 区切り・相対パスで統一（TextureManagerの衝突回避）
            se.thumbKeyUtf8 = "resources/stage/thumbs/" + se.titleUtf8 + ".png";
        }

        entries_.push_back(std::move(se));
    }

    std::sort(entries_.begin(), entries_.end(),
        [](const StageEntry& a, const StageEntry& b) {
            auto isTutorial = [](const StageEntry& e) {
                const std::string& t = e.titleUtf8;
                return (t == "チュートリアル" || t == "tutorial" || t == "Tutorial");
            };

            // titleUtf8 から "stage12" / "stage_12" / "Stage 12" みたいなのを拾って番号を返す
            // 見つからなければ -1
            auto parseStageNumber = [](const std::string& t) -> int {
                // 小文字化せずにざっくり対応（必要なら増やせる）
                // "stage" を探す
                size_t pos = t.find("stage");
                if (pos == std::string::npos)
                    pos = t.find("Stage");
                if (pos == std::string::npos)
                    return -1;

                pos += 5; // "stage" の後ろ

                // 区切り（' ', '_', '-' など）を飛ばす
                while (pos < t.size() && (t[pos] == ' ' || t[pos] == '_' || t[pos] == '-'))
                    pos++;

                // 数字が無ければ失敗
                if (pos >= t.size() || !std::isdigit((unsigned char)t[pos]))
                    return -1;

                // 数字を読む
                int num = 0;
                while (pos < t.size() && std::isdigit((unsigned char)t[pos])) {
                    num = num * 10 + (t[pos] - '0');
                    pos++;
                }
                return num; // 1,2,3...
            };

            const bool at = isTutorial(a);
            const bool bt = isTutorial(b);
            if (at != bt)
                return at; // チュートリアル最優先

            const int an = parseStageNumber(a.titleUtf8);
            const int bn = parseStageNumber(b.titleUtf8);

            // どっちも stage番号を持つ → 数字順
            if (an >= 0 && bn >= 0) {
                if (an != bn)
                    return an < bn;
                return a.fileW < b.fileW;
            }

            // 片方だけ番号を持つ → 番号持ちを先に
            if ((an >= 0) != (bn >= 0))
                return an >= 0;

            // どっちも番号なし → 普通にファイル名順
            return a.fileW < b.fileW;
        });

    selected_ = entries_.empty() ? -1 : 0;

    lastSelected_ = selected_;

    if (selected_ >= 0) {
        const float step = (2.0f * 3.14159265f) / (float)entries_.size();
        // selected_ が正面（角度0）に来るように回転角を合わせる
        carouselAngle_ = -selected_ * step;
        carouselTarget_ = carouselAngle_;
    }

    if (selected_ >= 0) {
        UpdateStageNameTexture_();
    }
}

// -------------------- Decide/Update --------------------
void StageSelectScene::Decide_()
{
    if (selected_ < 0 || selected_ >= (int)entries_.size())
        return;

    SceneManager::GetInstance()->SetSelectedStageFile(entries_[selected_].fileUtf8);
}

void StageSelectScene::Update()
{
    Input& input = *Input::GetInstance();

    if (input.IsKeyTrigger(DIK_F5)) {
        Rescan_();
    }
    titleSprite_->Update();
    auto fadeStatus = FadeManager::GetInstance()->GetStatus();
    bool aButtonTrigger = false;

    // ===== gamepad (Aボタン) =====
    XINPUT_STATE st {};
    if (XInputGetState(0, &st) == ERROR_SUCCESS) {

        bool nowAButton = (st.Gamepad.wButtons & XINPUT_GAMEPAD_A) != 0;

        if (nowAButton && !prevAButton_) {
            aButtonTrigger = true; // 押した瞬間
        }

        prevAButton_ = nowAButton;
    } else {
        prevAButton_ = false;
    }

    // ===== 決定（A or SPACE） =====
    if (aButtonTrigger || input.IsKeyTrigger(DIK_SPACE)) {

        if (fadeStatus == FadeManager::Status::FadeInFinished || fadeStatus == FadeManager::Status::None) {

            Decide_();
            SoundManager::GetInstance()->PlaySE(se_, 1.0f);
            FadeManager::GetInstance()->StartFadeOut(1.0f);
        }
    }


    FadeManager::GetInstance()->Update();

    camera_->Update();
    skydome_->Update();

    if (entries_.empty())
        return;

    if (entries_.empty())
        return;

    const int n = (int)entries_.size();
    if (n <= 0)
        return;

    const float step = (2.0f * 3.14159265f) / (float)n;

    if (input.IsKeyTrigger(DIK_LEFT))
        carouselTarget_ += step;
    if (input.IsKeyTrigger(DIK_RIGHT))
        carouselTarget_ -= step;

    // 追従
    carouselAngle_ += (carouselTarget_ - carouselAngle_) * carouselEase_;

    {
        const int n = (int)entries_.size();
        if (n > 0) {
            const float step = (2.0f * 3.14159265f) / (float)n;

            int bestIdx = 0;
            float bestFront = -999.0f;

            for (int i = 0; i < n; ++i) {
                float a = carouselAngle_ + i * step;
                float front = std::cos(a);
                if (front > bestFront) {
                    bestFront = front;
                    bestIdx = i;
                }
            }

            selected_ = bestIdx;
        }
    }

    if (selected_ != lastSelected_) {
        lastSelected_ = selected_;
        UpdateStageNameTexture_();
    }

    if (input.IsKeyTrigger(DIK_LEFT)) {

        SoundManager::GetInstance()->PlaySE(selectSeData_, 1.0f);
        // SoundManager::GetInstance()->ResetSE(selectSeData_);
        selected_ = std::max<int>(0, selected_ - 1);
    }
    if (input.IsKeyTrigger(DIK_RIGHT)) {
        SoundManager::GetInstance()->PlaySE(selectSeData_, 1.0f);
        // SoundManager::GetInstance()->ResetSE(selectSeData_);
        selected_ = std::min((int)entries_.size() - 1, selected_ + 1);
    }
    if (input.IsKeyTrigger(DIK_UP)) {
        SoundManager::GetInstance()->PlaySE(selectSeData_, 1.0f);
        // SoundManager::GetInstance()->ResetSE(selectSeData_);
        selected_ = std::max<int>(0, selected_ - kThumbCols);
    }
    if (input.IsKeyTrigger(DIK_DOWN)) {
        SoundManager::GetInstance()->PlaySE(selectSeData_, 1.0f);
        // SoundManager::GetInstance()->ResetSE(selectSeData_);
        selected_ = std::min((int)entries_.size() - 1, selected_ + kThumbCols);
    }
    // ★選択が変わったら日本語表示更新（ここが重要）
    if (selected_ != lastSelected_) {
        lastSelected_ = selected_;
        UpdateStageNameTexture_();
    }

    if (input.IsKeyTrigger(DIK_BACKSPACE)) {
        SceneManager::GetInstance()->SetNextScene(new TitleScene());
    }

    if (input.IsKeyTrigger(DIK_T)) {
        auto* sm = SceneManager::GetInstance();
        sm->RequestOpenEditorFile("_test/testStage.json"); // ★ここ
        sm->SetNextScene(new StageEditorScene());
    }

    if (fadeStatus == FadeManager::Status::FadeOutFinished) {
        // フェードアウト（画面が暗くなる）が終わったので、次のシーンへ
        SceneManager::GetInstance()->SetNextScene(new GamePlayScene());
    }

    DrawImGui();
}

// -------------------- Draw --------------------
void StageSelectScene::Draw2D()
{
    SpriteManager::GetInstance()->PreDraw();

    // ステージ名（日本語）
    if (stageNameSprite_ && selected_ >= 0) {
        stageNameSprite_->Update();
        stageNameSprite_->Draw();
    }
    titleSprite_->Draw();
    const int n = (int)entries_.size();
    if (n <= 0)
        return;

    // 奥行き順に描くため、描画順リストを作る
    struct DrawItem {
        int idx;
        float depth; // 小さいほど奥（後ろ）にしたい
        float x, y;
        float scale;
        float bright;
    };
    std::vector<DrawItem> items;
    items.reserve(n);

    const float step = (2.0f * 3.14159265f) / (float)n;

    int bestIdx = -1;
    float bestFront = -999.0f;

    for (int i = 0; i < n; ++i) {
        auto& e = entries_[i];
        if (!e.thumbSprite)
            continue;

        const float a = carouselAngle_ + i * step;

        const float front = std::cos(a);
        const float side = std::sin(a);

        // 正面判定（front最大）
        if (front > bestFront) {
            bestFront = front;
            bestIdx = i;
        }

        float x = carouselCenterX_ + side * carouselRadiusX_;
        float y = carouselCenterY_ + front * carouselRadiusY_ + (front * carouselFrontLift_);

        float t = (front + 1.0f) * 0.5f; // 0..1
        float scale = carouselMinScale_ + (carouselMaxScale_ - carouselMinScale_) * t;
        float bright = carouselMinBright_ + (carouselMaxBright_ - carouselMinBright_) * t;

        items.push_back({ i, front, x, y, scale, bright });
    }

    // 奥（frontが小さい）→手前（frontが大きい）の順に描画
    std::sort(items.begin(), items.end(),
        [](const DrawItem& a, const DrawItem& b) { return a.depth < b.depth; });

    for (auto& it : items) {
        auto& e = entries_[it.idx];
        Sprite* s = e.thumbSprite;

        // サイズ
        const float w = kThumbW * it.scale;
        const float h = kThumbH * it.scale;

        // 位置：中心に置きたいので左上に補正（Spriteが左上基準なら）
        s->SetPosition({ it.x - w * 0.5f, it.y - h * 0.5f });
        s->SetSize({ w, h });

        // 色：選択中は強調
        if (it.idx == selected_) {
            s->SetColor({ 1.0f, 1.0f, 1.0f, 1.0f });
        } else {
            s->SetColor({ it.bright, it.bright, it.bright, 1.0f });
        }

        s->Update();
        s->Draw();
    }
}

//// -------------------- Draw --------------------
// void StageSelectScene::Draw2D() {
//	SpriteManager::GetInstance()->PreDraw();
//
//	// ステージ名（日本語）
//	if (stageNameSprite_ && selected_ >= 0) {
//		stageNameSprite_->Update();
//		stageNameSprite_->Draw();
//	}
//
//	// サムネ一覧
//	for (int i = 0; i < (int)entries_.size(); ++i) {
//		auto& e = entries_[i];
//		if (!e.thumbSprite) continue;
//
//		const int col = i % kThumbCols;
//		const int row = i / kThumbCols;
//
//		const float x = kThumbStartX + col * (kThumbW + kThumbPadX);
//		const float y = kThumbStartY + row * (kThumbH + kThumbPadY);
//
//		e.thumbSprite->SetPosition({ x, y });
//
//		// 選択中だけ少し明るく／色変え
//		if (i == selected_) {
//			e.thumbSprite->SetColor({ 1.0f, 1.0f, 1.0f, 1.0f });
//		} else {
//			e.thumbSprite->SetColor({ 0.75f, 0.75f, 0.75f, 1.0f });
//		}
//
//		e.thumbSprite->Update();
//		e.thumbSprite->Draw();
//	}
// }

void StageSelectScene::Draw3D()
{
    Object3dManager::GetInstance()->PreDraw();
    LightManager::GetInstance()->Bind(DirectXCommon::GetInstance()->GetCommandList());
    Object3dManager::GetInstance()->SetBlendMode(kBlendModeNone);
    Object3dManager::GetInstance()->SetNormalPSO();
    skydome_->Draw();
}

void StageSelectScene::DrawImGui()
{
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
