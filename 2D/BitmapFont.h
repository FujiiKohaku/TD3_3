#pragma once
#include <string>
#include <memory>
#include "Sprite.h"
#include "SpriteManager.h"

// 1枚のフォント画像（グリッド）から文字を並べて描画する
class BitmapFont {
public:
    // cols, rows: グリッド分割数
    // cellW, cellH: 1文字セルのピクセルサイズ
    // firstChar: フォント画像の左上が何番の文字か（ASCIIなら32が多い）
    void Initialize(SpriteManager* sm,
        const std::string& fontTexturePath,
        int cols, int rows,
        int cellW, int cellH,
        int firstChar = 32)
    {
        sm_ = sm;
        texPath_ = fontTexturePath;
        cols_ = cols;
        rows_ = rows;
        cellW_ = cellW;
        cellH_ = cellH;
        firstChar_ = firstChar;

        spr_ = std::make_unique<Sprite>();
        spr_->Initialize(sm_, texPath_);
        spr_->SetColor({ 1,1,1,1 });
        spr_->SetAnchorPoint({ 0,0 });  // 左上起点で置きたいので
        spr_->SetTextureSize({ (float)cellW_, (float)cellH_ });
        spr_->SetSize({ (float)cellW_, (float)cellH_ });
        spr_->Update();
    }

    // scale: 文字の拡大率
    // spacing: 文字間の追加ピクセル
    void DrawString(float x, float y, const std::string& text,
        float scale = 1.0f, float spacing = 0.0f)
    {
        if (!spr_) return;

        float penX = x;
        float penY = y;

        const float drawW = cellW_ * scale;
        const float drawH = cellH_ * scale;

        spr_->SetSize({ drawW, drawH });
        spr_->SetTextureSize({ (float)cellW_, (float)cellH_ });

        for (unsigned char uc : text) {

            if (uc == '\n') {
                penX = x;
                penY += drawH;
                continue;
            }

            // 範囲外はスキップ（□にしたければ別セルを割り当ててもOK）
            int index = (int)uc - firstChar_;
            if (index < 0) {
                penX += (drawW + spacing);
                continue;
            }

            int gx = index % cols_;
            int gy = index / cols_;
            if (gy >= rows_) { // フォント画像に無い
                penX += (drawW + spacing);
                continue;
            }

            // フォント画像内の切り出し左上（ピクセル）
            Vector2 leftTop{
                (float)(gx * cellW_),
                (float)(gy * cellH_)
            };

            spr_->SetTextureLeftTop(leftTop);
            spr_->SetPosition({ penX, penY });
            spr_->Update();
            spr_->Draw();

            penX += (drawW + spacing);
        }
    }

    void SetColor(const Vector4& c) { if (spr_) spr_->SetColor(c); }

private:
    SpriteManager* sm_ = nullptr;
    std::string texPath_;

    int cols_ = 16;
    int rows_ = 16;
    int cellW_ = 16;
    int cellH_ = 16;
    int firstChar_ = 32;

    std::unique_ptr<Sprite> spr_;
};
