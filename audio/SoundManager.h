#pragma once

#include <Windows.h>
#include <cassert>
#include <string>
#include <vector>

#include <wrl.h>
#include <xaudio2.h>
#pragma comment(lib, "xaudio2.lib")
#include <unordered_set>

// ===== Media Foundation =====
#include <mfapi.h>
#include <mfidl.h>
#include <mfobjects.h>
#include <mfreadwrite.h>

#pragma comment(lib, "mfplat.lib")
#pragma comment(lib, "mfreadwrite.lib")
#pragma comment(lib, "mfuuid.lib")

#include "StringUtility.h"

// --------------------------------------
// WAVファイルデータ保持用
// --------------------------------------
struct SoundData {
    WAVEFORMATEX wfex {};
    std::vector<BYTE> buffer;
};

// --------------------------------------
// XAudio2ベースのサウンド管理クラス
// Singleton 対応版
// --------------------------------------
class SoundManager {
public:
    // ================================
    // Singleton
    // ================================
    static SoundManager* GetInstance()
    {
        static SoundManager instance;
        return &instance;
    }

    // ================================
    // 基本操作
    // ================================
    void Initialize();
    void Finalize();

    SoundData SoundLoadFile(const std::string& filename);
    void SoundUnload(SoundData* soundData);
    void StopSE();
    // SE再生用
    void PlaySE(const SoundData& soundData, float volume);
    void PlayBGM(const SoundData& soundData, float volume);

    void StopBGM(const SoundData& soundData);
    void StopBGMAll();
    void ResetSE(const SoundData& soundData);

private:
    // シングルトン用
    SoundManager()
        = default;
    ~SoundManager() = default;

    SoundManager(const SoundManager&) = delete;
    SoundManager& operator=(const SoundManager&) = delete;

private:
    IXAudio2SourceVoice* seVoice_ = nullptr;
    bool isSEPlaying_ = false;
    IXAudio2SourceVoice* bgmVoice_ = nullptr;
    const SoundData* currentBgm_ = nullptr;

    Microsoft::WRL::ComPtr<IXAudio2> xAudio2;
    IXAudio2MasteringVoice* masterVoice = nullptr;

    std::unordered_set<const SoundData*> playedSE_;
};
