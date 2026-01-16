#include "TextureManager.h"
#include <format>

TextureManager* TextureManager::instance = nullptr;
// ImGuiで0番を使用するため、1番から使用
uint32_t TextureManager::kSRVIndexTop = 1;

//=================================================================
// インスタンス取得（シングルトン）
//=================================================================
TextureManager* TextureManager::GetInstance()
{
    if (instance == nullptr) {
        instance = new TextureManager();
    }
    return instance;
}

//=================================================================
// 終了処理（メモリ解放）
//=================================================================
void TextureManager::Finalize()
{
    if (instance) {
        delete instance;
        instance = nullptr;
    }
}

//=================================================================
// 初期化処理
//=================================================================
void TextureManager::Initialize(DirectXCommon* dxCommon, SrvManager* srvManager)
{
    dxCommon_ = dxCommon;
    srvManager_ = srvManager;
    // SRVの数をあらかじめ確保
    textureDatas.reserve(SrvManager::kMaxSRVCount);
}

//=================================================================
// テクスチャの読み込み
//=================================================================
void TextureManager::LoadTexture(const std::string& filePath)
{

    //読み込み済みテクスチャを検索
    if (textureDatas.contains(filePath)) {
        return; // すでに読み込まれているなら何もしない
    }

    // テクスチャ上限チェック
    assert(srvManager_->CanAllocate());

    // WIC経由で画像を読み込む
    DirectX::ScratchImage image {};
    std::wstring filePathW = StringUtility::ConvertString(filePath);
    HRESULT hr = DirectX::LoadFromWICFile(filePathW.c_str(), DirectX::WIC_FLAGS_FORCE_SRGB, nullptr, image);
    assert(SUCCEEDED(hr));

    // ミップマップ生成
    DirectX::ScratchImage mipImages {};
    hr = DirectX::GenerateMipMaps(image.GetImages(), image.GetImageCount(), image.GetMetadata(),
        DirectX::TEX_FILTER_SRGB, 0, mipImages);
    assert(SUCCEEDED(hr));

// テクスチャデータを追加して書き込む
    TextureData& textureData = textureDatas[filePath];

    // 情報を記録
    textureData.metadata = mipImages.GetMetadata();

    // テクスチャリソース生成
    textureData.resource = dxCommon_->CreateTextureResource(dxCommon_->GetDevice(), textureData.metadata);
    Microsoft::WRL::ComPtr<ID3D12Resource> intermediateResource = dxCommon_->UploadTextureData(textureData.resource, mipImages);

    // コマンド送信
    dxCommon_->GetCommandList()->Close();
    ID3D12CommandList* lists[] = { dxCommon_->GetCommandList() };
    dxCommon_->GetCommandQueue()->ExecuteCommandLists(_countof(lists), lists);

    // SRVインデックス計算
    uint32_t srvIndex = static_cast<uint32_t>(textureDatas.size() - 1) + kSRVIndexTop;
    Logger::Log(std::format("Texture Loaded: {}, SRV Index: {}", filePath, srvIndex));

    // CPU/GPUハンドル取得
    textureData.srvIndex = srvManager_->Allocate();
    textureData.srvHandleCPU = srvManager_->GetCPUDescriptorHandle(textureData.srvIndex);
    textureData.srvHandleGPU = srvManager_->GetGPUDescriptorHandle(textureData.srvIndex);

    // SRV設定
    D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc {};
    srvDesc.Format = textureData.metadata.format;
    srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
    srvDesc.Texture2D.MipLevels = static_cast<UINT>(textureData.metadata.mipLevels);

    dxCommon_->GetDevice()->CreateShaderResourceView(textureData.resource.Get(), &srvDesc, textureData.srvHandleCPU);

    // GPU待機＆再利用準備
    dxCommon_->WaitForGPU();
    dxCommon_->GetCommandAllocator()->Reset();
    dxCommon_->GetCommandList()->Reset(dxCommon_->GetCommandAllocator(), nullptr);

    Logger::Log(std::format("srvIndex = {}", srvIndex));
}

//=================================================================
// ファイルパスからテクスチャインデックスを取得
//=================================================================
uint32_t TextureManager::GetTextureIndexByFilePath(const std::string& filePath)
{
    // 指定ファイルが登録済みか確認
    if (textureDatas.contains(filePath)) {
        return textureDatas.at(filePath).srvIndex;
    }

    assert(0 && "指定したテクスチャが読み込まれていません。");
    return 0;
}

//=================================================================
// GPUハンドル取得
//=================================================================
D3D12_GPU_DESCRIPTOR_HANDLE TextureManager::GetSrvHandleGPU(const std::string& filePath)
{
    if (textureDatas.contains(filePath)) {
        return textureDatas.at(filePath).srvHandleGPU;
    }

    assert(0 && "指定したテクスチャが読み込まれていません。");
    return D3D12_GPU_DESCRIPTOR_HANDLE {};
}
//=================================================================
// メタデータ取得
//=================================================================
const DirectX::TexMetadata& TextureManager::GetMetaData(const std::string& filePath)
{
    if (textureDatas.contains(filePath)) {
        return textureDatas.at(filePath).metadata;
    }

    assert(0 && "指定したテクスチャが読み込まれていません。");
    static DirectX::TexMetadata dummy {}; 
    return dummy;
}

//動的テクスチャ
void TextureManager::CreateDynamicTextureRGBA8(const std::string& key, uint32_t width, uint32_t height)
{
    if (textureDatas.contains(key)) return;

    assert(dxCommon_ && srvManager_);
    assert(srvManager_->CanAllocate());

    TextureData& td = textureDatas[key];

    DirectX::TexMetadata md{};
    md.width = width;
    md.height = height;
    md.depth = 1;
    md.arraySize = 1;
    md.mipLevels = 1;
    md.format = DXGI_FORMAT_R8G8B8A8_UNORM;
    md.dimension = DirectX::TEX_DIMENSION_TEXTURE2D;
    td.metadata = md;

    // 初期 COPY_DEST
    td.resource = dxCommon_->CreateTextureResource(dxCommon_->GetDevice(), td.metadata);

    td.srvIndex = srvManager_->Allocate();
    td.srvHandleCPU = srvManager_->GetCPUDescriptorHandle(td.srvIndex);
    td.srvHandleGPU = srvManager_->GetGPUDescriptorHandle(td.srvIndex);

    D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc{};
    srvDesc.Format = td.metadata.format;
    srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
    srvDesc.Texture2D.MipLevels = 1;

    dxCommon_->GetDevice()->CreateShaderResourceView(td.resource.Get(), &srvDesc, td.srvHandleCPU);

    // =========================================================
    // ★B案：ここで COPY_DEST -> GENERIC_READ にしておく
    // （描画でサンプルできる状態にする）
    // =========================================================
    {
        auto* cl = dxCommon_->GetCommandList();

        D3D12_RESOURCE_BARRIER b{};
        b.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
        b.Transition.pResource = td.resource.Get();
        b.Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_DEST;
        b.Transition.StateAfter = D3D12_RESOURCE_STATE_GENERIC_READ;
        b.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
        cl->ResourceBarrier(1, &b);

        // ここは “初期化中に一回だけ” なので Execute して待ってOK
        cl->Close();
        ID3D12CommandList* lists[] = { cl };
        dxCommon_->GetCommandQueue()->ExecuteCommandLists(1, lists);

        dxCommon_->WaitForGPU();

        dxCommon_->GetCommandAllocator()->Reset();
        cl->Reset(dxCommon_->GetCommandAllocator(), nullptr);
    }

    td.everUploaded = true; // ★「GENERIC_READにした」印として使う
}


void TextureManager::UpdateDynamicTextureRGBA8(
    const std::string& key, const uint8_t* rgba, uint32_t width, uint32_t height)
{
    if (!textureDatas.contains(key)) {
        CreateDynamicTextureRGBA8(key, width, height);
    }

    assert(dxCommon_ && srvManager_);
    TextureData& td = textureDatas.at(key);

    // サイズ固定運用
    assert(td.metadata.width == width && td.metadata.height == height);

    // ScratchImageへコピー
    DirectX::ScratchImage img{};
    HRESULT hr = img.Initialize2D(DXGI_FORMAT_R8G8B8A8_UNORM, width, height, 1, 1);
    assert(SUCCEEDED(hr));

    const DirectX::Image* im = img.GetImage(0, 0, 0);
    assert(im);

    for (uint32_t y = 0; y < height; ++y) {
        std::memcpy(im->pixels + im->rowPitch * y,
            rgba + (size_t)width * 4 * y,
            (size_t)width * 4);
    }

    // ★2回目以降は GENERIC_READ -> COPY_DEST に戻す
    if (td.everUploaded) {
        D3D12_RESOURCE_BARRIER b{};
        b.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
        b.Transition.pResource = td.resource.Get();
        b.Transition.StateBefore = D3D12_RESOURCE_STATE_GENERIC_READ;
        b.Transition.StateAfter = D3D12_RESOURCE_STATE_COPY_DEST;
        b.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
        dxCommon_->GetCommandList()->ResourceBarrier(1, &b);
    }

    // UploadTextureData は COPY_DEST -> GENERIC_READ までやってくれる
    Microsoft::WRL::ComPtr<ID3D12Resource> intermediate = dxCommon_->UploadTextureData(td.resource, img);

    // 実行
    dxCommon_->GetCommandList()->Close();
    ID3D12CommandList* lists[] = { dxCommon_->GetCommandList() };
    dxCommon_->GetCommandQueue()->ExecuteCommandLists(_countof(lists), lists);

    // 待機して次フレームに備える（あなたの設計に合わせてる）
    dxCommon_->WaitForGPU();
    dxCommon_->GetCommandAllocator()->Reset();
    dxCommon_->GetCommandList()->Reset(dxCommon_->GetCommandAllocator(), nullptr);

    td.everUploaded = true;
}
