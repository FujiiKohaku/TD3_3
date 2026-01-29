#pragma once
#include <string>
#include <vector>
#include <memory>
#include "Sprite.h"
#include "SpriteManager.h"

class BitmapFont {
public:
    void Initialize(SpriteManager* sm,
        const std::string& texPath,
        int cols = 16, int rows = 6,
        int cellW = 32, int cellH = 32,
        int firstChar = 32)
    {
        sm_ = sm;
        texPath_ = texPath;
        cols_ = cols; rows_ = rows;
        cellW_ = cellW; cellH_ = cellH;
        firstChar_ = firstChar;

        // 最初は少し確保（必要に応じて増える）
        EnsureCapacity_(64);
        SetColor({ 1,1,1,1 });
    }

    void SetColor(const Vector4& c) {
        color_ = c;
       
    }

    void DrawString(float x, float y, const std::string& text, float scale = 1.0f)
    {
        EnsureCapacity_(used_ + (int)text.size()); // ざっくり多めに確保

        float penX = x, penY = y;
        const float drawW = (float)cellW_ * scale;
        const float drawH = (float)cellH_ * scale;

        for (unsigned char uc : text)
        {
            if (uc == '\n') { penX = x; penY += drawH; continue; }
            if (uc == '\t') { penX += drawW * 4.0f; continue; }
            if (uc == ' ') { penX += drawW; continue; }

            int index = (int)uc - firstChar_;
            if (index < 0 || index >= cols_ * rows_) {
                int q = (int)'?' - firstChar_;
                if (q >= 0 && q < cols_ * rows_) index = q;
                else { penX += drawW; continue; }
            }

            int gx = index % cols_;
            int gy = index / cols_;

            Sprite* sp = sprites_[used_].get();
            used_++;

            sp->SetColor(color_);
            sp->SetPosition({ penX, penY });
            sp->SetSize({ drawW, drawH });
            sp->SetTextureLeftTop({ (float)(gx * cellW_), (float)(gy * cellH_) });
            sp->SetTextureSize({ (float)cellW_, (float)cellH_ });

            sp->Update();
            sp->Draw();

            penX += drawW;
        }
    }

    void BeginFrame() { used_ = 0; }

private:
    void EnsureCapacity_(int needed)
    {
        if ((int)sprites_.size() >= needed) return;

        int old = (int)sprites_.size();
        int newCap = std::max<float>(needed, old * 2 + 1);
        sprites_.resize(newCap);

        for (int i = old; i < newCap; ++i) {
            sprites_[i] = std::make_unique<Sprite>();
            sprites_[i]->Initialize(sm_, texPath_);
        }
    }

private:
    SpriteManager* sm_ = nullptr;
    std::string texPath_;
    int cols_ = 16, rows_ = 6;
    int cellW_ = 32, cellH_ = 32;
    int firstChar_ = 32;
    Vector4 color_ = { 1,1,1,1 };

    int used_ = 0;

    std::vector<std::unique_ptr<Sprite>> sprites_;
};
