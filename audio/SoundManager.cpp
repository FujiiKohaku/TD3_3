#include "SoundManager.h"

void SoundManager::Initialize()
{
    HRESULT result;

    result = MFStartup(MF_VERSION, MFSTARTUP_NOSOCKET);
    assert(SUCCEEDED(result));

    result = XAudio2Create(&xAudio2, 0, XAUDIO2_DEFAULT_PROCESSOR);
    assert(SUCCEEDED(result));

    result = xAudio2->CreateMasteringVoice(&masterVoice);
    assert(SUCCEEDED(result));
}

void SoundManager::Finalize()
{
    if (seVoice_) {
        seVoice_->Stop();
        seVoice_->DestroyVoice();
        seVoice_ = nullptr;
    }

    if (bgmVoice_) {
        bgmVoice_->Stop();
        bgmVoice_->DestroyVoice();
        bgmVoice_ = nullptr;
    }

    if (masterVoice) {
        masterVoice->DestroyVoice();
        masterVoice = nullptr;
    }

    xAudio2.Reset();
}



// チャンクヘッダ
struct ChunkHeader {
    char id[4]; // チャンクID
    uint32_t size; // チャンクサイズ
};
// RIFFヘッダチャンク
struct RiffHeader {
    ChunkHeader chunk; // チャンクヘッダ(RIFF)
    char type[4]; // フォーマット（"WAVE"）
};
// FMTチャンク
struct FormatChunk {
    ChunkHeader chunk; // チャンクヘッダ(FMT)
    WAVEFORMATEX fmt; // WAVEフォーマット
};

SoundData SoundManager::SoundLoadFile(const std::string& filename)
{
    HRESULT result;
    SoundData soundData {};

    // パスをワイド文字列へ
    std::wstring path = StringUtility::ConvertString(filename);

    Microsoft::WRL::ComPtr<IMFSourceReader> reader;
    result = MFCreateSourceReaderFromURL(path.c_str(), nullptr, &reader);
    if (FAILED(result)) {
        std::string msg = "SoundLoadFile failed: " + filename + "\n";
        OutputDebugStringA(msg.c_str());
        return SoundData {};
    }
    if (FAILED(result)) {
        char buf[512];
        sprintf_s(buf,
            "SoundLoadFile failed\nFile: %s\nHRESULT: 0x%08X\n",
            filename.c_str(),
            result);
        OutputDebugStringA(buf);
        return SoundData {};
    }

    // PCM指定
    Microsoft::WRL::ComPtr<IMFMediaType> pPCType;
    MFCreateMediaType(&pPCType);
    pPCType->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Audio);
    pPCType->SetGUID(MF_MT_SUBTYPE, MFAudioFormat_PCM);
    result = reader->SetCurrentMediaType(MF_SOURCE_READER_FIRST_AUDIO_STREAM,nullptr,pPCType.Get());

    if (FAILED(result)) {
        OutputDebugStringA(("PCM set failed: " + filename + "\n").c_str());
        return SoundData {};
    }


    // 実際のWaveFormat取得
    Microsoft::WRL::ComPtr<IMFMediaType> pOutType;
    reader->GetCurrentMediaType(
        MF_SOURCE_READER_FIRST_AUDIO_STREAM,
        &pOutType);

    WAVEFORMATEX* waveFormat = nullptr;
    MFCreateWaveFormatExFromMFMediaType(pOutType.Get(), &waveFormat, nullptr);

    // コンテナに格納
   
    soundData.wfex = *waveFormat;
    // 音声データの読み込み
    CoTaskMemFree(waveFormat);
    // バッファサイズの取得
    while (true) {

        Microsoft::WRL::ComPtr<IMFSample> pSample;
        DWORD streamIndex = 0, flags = 0;
        LONGLONG llTimeStamp = 0;
        // サンプルの読み込み
        result = reader->ReadSample(
            MF_SOURCE_READER_FIRST_AUDIO_STREAM,
            0,
            &streamIndex,
            &flags,
            &llTimeStamp,
            &pSample);

        // ストリームの末尾に達したら終了
        if (flags & MF_SOURCE_READERF_ENDOFSTREAM)
            break;

        if (pSample) {

            Microsoft::WRL::ComPtr<IMFMediaBuffer> pBuffer;
            // サンプルからメディアバッファを取得
            result = pSample->ConvertToContiguousBuffer(&pBuffer);

            BYTE* pData = nullptr;
            DWORD maxLength = 0,currrentLength=0;
            //バッファ読み込み用にロック
            pBuffer->Lock(&pData, &maxLength, &currrentLength);
            //バッファの末尾にデータ追加
            soundData.buffer.insert(soundData.buffer.end(), pData, pData + currrentLength);
            // バッファのロック解除
            pBuffer->Unlock();

        }
    }
    return soundData;
}

//==音声データ解放==//
void SoundManager::SoundUnload(SoundData* soundData)
{

    soundData->buffer.clear();
    soundData->wfex = {};
}

void SoundManager::StopSE()
{
    if (seVoice_) {
        seVoice_->Stop();
        seVoice_->FlushSourceBuffers();
    }
}

void SoundManager::StopBGM(const SoundData& soundData)
{
    if (!bgmVoice_) {
        return;
    }

    // 再生中のBGMと違うなら何もしない
    if (currentBgm_ != &soundData) {
        return;
    }

    bgmVoice_->Stop();
    bgmVoice_->FlushSourceBuffers();
    currentBgm_ = nullptr;
}



void SoundManager::PlaySE(const SoundData& soundData, float volume)
{
    if (soundData.buffer.empty()) {
        return;
    }

    IXAudio2SourceVoice* voice = nullptr;

    HRESULT result = xAudio2->CreateSourceVoice(&voice, &soundData.wfex);
    if (FAILED(result)) {
        return;
    }

    XAUDIO2_BUFFER buffer {};
    buffer.pAudioData = soundData.buffer.data();
    buffer.AudioBytes = static_cast<UINT32>(soundData.buffer.size());
    buffer.Flags = XAUDIO2_END_OF_STREAM;

    voice->SetVolume(volume);
    voice->SubmitSourceBuffer(&buffer);
    voice->Start();
}



void SoundManager::PlayBGM(const SoundData& soundData, float volume)
{
    if (soundData.buffer.empty()) {
        return;
    }

    HRESULT result;

    if (!bgmVoice_) {
        result = xAudio2->CreateSourceVoice(&bgmVoice_, &soundData.wfex);
        if (FAILED(result)) {
            return;
        }
    }

    XAUDIO2_VOICE_STATE state {};
    bgmVoice_->GetState(&state);
    if (state.BuffersQueued > 0) {
        bgmVoice_->SetVolume(volume);
        return;
    }

    bgmVoice_->SetVolume(volume);

    XAUDIO2_BUFFER buf {};
    buf.pAudioData = soundData.buffer.data();
    buf.AudioBytes = static_cast<UINT32>(soundData.buffer.size());
    buf.Flags = XAUDIO2_END_OF_STREAM;
    buf.LoopBegin = 0;
    buf.LoopLength = 0;
    buf.LoopCount = XAUDIO2_LOOP_INFINITE;

    result = bgmVoice_->SubmitSourceBuffer(&buf);
    if (FAILED(result)) {
        return;
    }

    bgmVoice_->Start();

    // どのBGMを再生しているか記録
    currentBgm_ = &soundData;
}
void SoundManager::StopBGMAll()
{
    if (!bgmVoice_) {
        return;
    }

    bgmVoice_->Stop();
    bgmVoice_->FlushSourceBuffers();
    currentBgm_ = nullptr;
}




