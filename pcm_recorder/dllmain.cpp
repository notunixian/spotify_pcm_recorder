#include <iostream>

#include "dependencies/safetyhook/safetyhook.hpp"
#include "SymParser.h"

#include <filesystem>
#include <mmeapi.h>

safetyhook::InlineHook gAudioClientInit;
safetyhook::InlineHook gAudioClientStart;
safetyhook::InlineHook gAudioClientStop;

safetyhook::InlineHook gAudioRenderGetBuf;
safetyhook::InlineHook gAudioRenderReleaseBuf;

std::vector<uint8_t> gPcmBytes = {};
WAVEFORMATEX gAudioFormat = {
    .wFormatTag = 0,
    .nChannels = 0,
    .nSamplesPerSec = 0,
    .nAvgBytesPerSec = 0,
    .nBlockAlign = 0,
    .wBitsPerSample = 0,
	.cbSize = 0
};

HRESULT __stdcall AudioClientInit(void* pThis, int ShareMode, DWORD StreamFlags, LONGLONG hnsBufferDuration,
                                  LONGLONG hnsPeriodicity, const WAVEFORMATEX* pFormat, LPCGUID AudioSessionGuid)
{
    std::cout << "[pcm_recorder] AudioClient::Initialize called\n";
    HRESULT hres = gAudioClientInit.stdcall<HRESULT>(pThis, ShareMode, StreamFlags, hnsBufferDuration, hnsPeriodicity, pFormat, AudioSessionGuid);

    if (SUCCEEDED(hres))
    {
        std::cout << "[pcm_recorder] Audio Format - Channels: " << std::dec << pFormat->nChannels
                  << ", Sample Rate: " << pFormat->nSamplesPerSec
                  << ", Bits Per Sample: " << pFormat->wBitsPerSample << '\n';

		gAudioFormat = *pFormat;

		std::cout << "[pcm_recorder] Ready to capture.\n";
	}
    return hres;
}

HRESULT __stdcall AudioClientStart(void* pThis)
{
	std::cout << "[pcm_recorder] AudioClient::Start\n";
	return gAudioClientStart.stdcall<HRESULT>(pThis);
}

HRESULT __stdcall AudioClientStop(void* pThis)
{
    std::cout << "[pcm_recorder] AudioClient::Stop called, writiting pcm dump...\n";

    static int dumpIndex = 0;
    std::string filename = "dumps/dump_" + std::to_string(dumpIndex++) + ".pcm";
    FILE* outFile = nullptr;
    fopen_s(&outFile, filename.c_str(), "wb");
    if (outFile)
    {
        fwrite(gPcmBytes.data(), 1, gPcmBytes.size(), outFile);
        fclose(outFile);
        std::cout << "[pcm_recorder] Wrote PCM dump to " << filename << " (" << gPcmBytes.size() << " bytes)\n";
    }
    else
    {
        std::cout << "[pcm_recorder] Failed to open file for writing: " << filename << '\n';
	}

	gPcmBytes.clear();

    return gAudioClientStop.stdcall<HRESULT>(pThis);
}

BYTE** lastPpData = nullptr;
HRESULT __stdcall AudioRenderGetBuf(void* pThis, UINT32 NumFramesRequested, BYTE** ppData)
{
    HRESULT hres = gAudioRenderGetBuf.stdcall<HRESULT>(pThis, NumFramesRequested, ppData);
	lastPpData = ppData;
    return hres;
}

HRESULT __stdcall AudioRenderReleaseBuf(void* pThis, UINT32 NumFramesWritten, DWORD dwFlags)
{
    if (gAudioFormat.nChannels == 0)
    {
		std::cout << "[pcm_recorder] Audio format not initialized, skipping PCM capture.\n";
		return gAudioRenderReleaseBuf.stdcall<HRESULT>(pThis, NumFramesWritten, dwFlags);
    }

	size_t bytesPerFrame = (gAudioFormat.nChannels * gAudioFormat.wBitsPerSample) / 8;
	size_t size = NumFramesWritten * bytesPerFrame;

    gPcmBytes.insert(gPcmBytes.end(), *lastPpData, *lastPpData + size);

	// feel free to comment this out if you want to hear the audio while recording
	memset(*lastPpData, 0, size);
	lastPpData = nullptr;

    return gAudioRenderReleaseBuf.stdcall<HRESULT>(pThis, NumFramesWritten, dwFlags);
}

DWORD WINAPI Entry(LPVOID lpParam)
{
    auto cmdLine = GetCommandLine();
    int numArgs = 0;
    CommandLineToArgvW(cmdLine, &numArgs);

    if (numArgs > 1)
    {
		MessageBoxA(NULL, "injected into wrong spotify process!", "pcm_recorder", MB_OK | MB_ICONERROR);
        return 0;
    }

    AllocConsole();
    freopen_s(reinterpret_cast<FILE**>(stdout), "CONOUT$", "w", stdout);

    while (!GetModuleHandleA("AudioSes.dll"))
    {
        Sleep(50);
    }

	std::cout << "[pcm_recorder] AudioSes loaded, parsing pdb..." << '\n';

    SymParser parser;
    if (!parser.IsInitialized())
    {
        std::cout << "[pcm_recorder] Failed to initialize symbol parser! Make sure you follow the instructions in the README." << '\n';
        return 1;
    }

	std::filesystem::create_directory("dumps");

    parser.LoadModule(L"C:\\Windows\\System32\\AudioSes.dll");
    SymParser::SYM_INFO info = {};

    parser.DumpSymbol(L"?GetBuffer@CAudioRenderClient@@UEAAJIPEAPEAE@Z", info);
	ULONG getBufferOff = info.Offset + 0x1000;

    parser.DumpSymbol(L"?ReleaseBuffer@CAudioRenderClient@@UEAAJIK@Z", info);
	ULONG releaseBufferOff = info.Offset + 0x1000;

    parser.DumpSymbol(L"?Initialize@CAudioClient@@UEAAJW4_AUDCLNT_SHAREMODE@@K_J1PEBUtWAVEFORMATEX@@PEBU_GUID@@@Z", info);
    ULONG initOff = info.Offset + 0x1000;

    parser.DumpSymbol(L"?Start@CAudioClient@@UEAAJXZ", info);
    ULONG startOff = info.Offset + 0x1000;

    parser.DumpSymbol(L"?Stop@CAudioClient@@UEAAJXZ", info);
	ULONG stopOff = info.Offset + 0x1000;

	uintptr_t base = reinterpret_cast<uintptr_t>(GetModuleHandleA("AudioSes.dll"));

    std::cout << "[pcm_recorder] Hooking AudioClient::Initialize at offset: 0x" << std::hex << initOff << '\n';
    gAudioClientInit = safetyhook::create_inline(base + initOff, AudioClientInit);

	std::cout << "[pcm_recorder] Hooking AudioClient::Start at offset: 0x" << std::hex << startOff << '\n';
    gAudioClientStart = safetyhook::create_inline(base + startOff, AudioClientStart);

	std::cout << "[pcm_recorder] Hooking AudioClient::Stop at offset: 0x" << std::hex << stopOff << '\n';
    gAudioClientStop = safetyhook::create_inline(base + stopOff, AudioClientStop);

	std::cout << "[pcm_recorder] Hooking AudioRenderClient::GetBuffer at offset: 0x" << std::hex << getBufferOff << '\n';
    gAudioRenderGetBuf = safetyhook::create_inline(base + getBufferOff, AudioRenderGetBuf);

	std::cout << "[pcm_recorder] Hooking AudioRenderClient::ReleaseBuffer at offset: 0x" << std::hex << releaseBufferOff << '\n';
    gAudioRenderReleaseBuf = safetyhook::create_inline(base + releaseBufferOff, AudioRenderReleaseBuf);

    if (gAudioClientInit && gAudioClientStart && gAudioClientStop &&
        gAudioRenderGetBuf && gAudioRenderReleaseBuf)
    {
        std::cout << "[pcm_recorder] Hooks installed successfully.\n";
    }
	else
    {
		std::cout << "[pcm_recorder] Failed to install one or more hooks.\n";
    }

	return 0;
}

BOOL APIENTRY DllMain( HMODULE hModule,
                       DWORD  ul_reason_for_call,
                       LPVOID lpReserved
                     )
{
    switch (ul_reason_for_call)
    {
    case DLL_PROCESS_ATTACH:
        CreateThread(nullptr, 0, Entry, hModule, 0, nullptr);
        break;
    case DLL_THREAD_ATTACH:
    case DLL_THREAD_DETACH:
    case DLL_PROCESS_DETACH:
        break;
    }
    return TRUE;
}

