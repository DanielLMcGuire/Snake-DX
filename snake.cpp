#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <shellapi.h>
#pragma comment(lib, "user32.lib")
#pragma comment(lib, "shell32.lib")

#ifdef DX11
#include <d3d11.h>
#pragma comment(lib, "d3d11.lib")
#elif DX12
#include <d3d12.h>
#pragma comment(lib, "d3d12.lib")
#endif

#if defined(DX11) || defined(DX12)
#include <dxgi1_6.h>
#include <d3dcompiler.h>
#include <DirectXMath.h>
#include <wrl/client.h>
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "d3dcompiler.lib")
#endif

#include <set>
#include <string>
#include <vector>
#include <random>
#include <chrono>
#include <thread>
#include <cinttypes>

#ifdef _DEBUG
#include <stdio.h>
#include <io.h>
#include <fcntl.h>
#include <iostream>
#include <iomanip>
#include <sstream>
#endif

#pragma region Data

std::mt19937 rng;

#ifdef TRIPLE_BUFFER
constexpr int FRAME_COUNT = 3;
#elif SINGLE_BUFFER
constexpr int FRAME_COUNT = 1;
#else
constexpr int FRAME_COUNT = 2;
#endif

constexpr LPCTSTR CLASS_NAME = L"Snake";

constexpr int WIDTH = 32;
constexpr int HEIGHT = 30;
constexpr int MAX_SNAKE = ((WIDTH - 1) * (HEIGHT - 1));

std::uniform_int_distribution<int> dist_x(1, WIDTH - 2);
std::uniform_int_distribution<int> dist_y(1, HEIGHT - 2);

#ifndef _DEBUG
constexpr int time1 = 170;
constexpr int time2 = 154;
constexpr int time3 = 113;
#else
int basetick = 150;
int time1 = basetick;
int time2 = basetick;
int time3 = basetick;
#endif
constexpr int timeThreshold = 6;

#pragma endregion

#pragma region Types

/**
 * @brief 2D Integer 
 */
struct Int2
{
    int x;
    int y;

    constexpr Int2() noexcept
        : x(0), y(0) {
    }

    constexpr Int2(int x_, int y_) noexcept
        : x(x_), y(y_) {
    }

    constexpr Int2 operator+(const Int2& rhs) const noexcept
    {
        return Int2(x + rhs.x, y + rhs.y);
    }

    constexpr Int2 operator-(const Int2& rhs) const noexcept
    {
        return Int2(x - rhs.x, y - rhs.y);
    }

    constexpr Int2 operator*(int s) const noexcept
    {
        return Int2(x * s, y * s);
    }
};

/**
 * @brief 3D Float
 */
struct Float3
{
    float x;
    float y;
    float z;

    constexpr Float3() noexcept
        : x(0.0f), y(0.0f), z(0.0f) {}

    constexpr Float3(float x_, float y_, float z_) noexcept
        : x(x_), y(y_), z(z_) {}

    constexpr Float3 operator+(const Float3& rhs) const noexcept
    {
        return Float3(x + rhs.x, y + rhs.y, z + rhs.z);
    }

    constexpr Float3 operator-(const Float3& rhs) const noexcept
    {
        return Float3(x - rhs.x, y - rhs.y, z - rhs.z);
    }

    constexpr Float3 operator*(float s) const noexcept
    {
        return Float3(x * s, y * s, z * s);
    }
};

/**
 * @brief 4D Float
 */
struct Float4
{
    float x;
    float y;
    float z;
    float w;

    constexpr Float4() noexcept
        : x(0.0f), y(0.0f), z(0.0f), w(0.0f) {}

    constexpr Float4(float x_, float y_, float z_, float w_) noexcept
        : x(x_), y(y_), z(z_), w(w_) {}

    constexpr Float4(const Float3& v, float w_) noexcept
        : x(v.x), y(v.y), z(v.z), w(w_) {}

    constexpr Float4 operator+(const Float4& rhs) const noexcept
    {
        return Float4(x + rhs.x, y + rhs.y, z + rhs.z, w + rhs.w);
    }

    constexpr Float4 operator-(const Float4& rhs) const noexcept
    {
        return Float4(x - rhs.x, y - rhs.y, z - rhs.z, w - rhs.w);
    }

    constexpr Float4 operator*(float s) const noexcept
    {
        return Float4(x * s, y * s, z * s, w * s);
    }
};

/**
 * @brief Piece of snake body
 */
struct Segment {
    Int2 position;
};

#if defined(DX11) || defined(DX12)
struct ConstantBuffer {
    DirectX::XMFLOAT4X4 projection;
};
#endif

struct Vertex {
    Float3 position;
    Float4 color;
};

struct Corner {
    Int2 position;
    int segments_remaining = 0;
};

#pragma endregion

#pragma region State

struct SnakeGameState {
    std::wstring name = CLASS_NAME;
    Segment snake[MAX_SNAKE];
    size_t snake_length = 5;
    Segment prev_snake[MAX_SNAKE];
    size_t prev_snake_length = 5;
    Corner corners[MAX_SNAKE];
    size_t corner_count = 0;

    int food_x = 0;
    int food_y = 0;
    int score = 0;

    int dir_x = 1;
    int dir_y = 0;
};

typedef SnakeGameState GameState;

struct EngineState {
    bool is_first = true;
    bool running = true;
    bool quit = false;
    bool stuck = false;
    bool paused = false;

    float cell_px = 20.0f;
    float offset_x = 0.0f;
    float offset_y = 0.0f;

    bool is_fullscreen = false;
    RECT windowed_rect = {};
    DWORD windowed_style = 0;

    uint32_t windowWidth = 800;
    uint32_t windowHeight = 600;

    GameState *game = nullptr;

    HWND *window = nullptr;
};

#pragma endregion

#pragma region DirectX Shaders
#if defined(DX11) || defined(DX12)
const std::string DX_SHADER = R"(
    cbuffer ConstantBuffer : register(b0)
    {
        float4x4 projection;
    };

    struct VSInput
    {
        float3 position : POSITION;
        float4 color : COLOR;
    };

    struct PSInput
    {
        float4 position : SV_POSITION;
        float4 color : COLOR;
    };

    PSInput VSMain(VSInput input)
    {
        PSInput result;
        result.position = mul(float4(input.position, 1.0f), projection);
        result.color = input.color;
        return result;
    }

    float4 PSMain(PSInput input) : SV_TARGET
    {
        return input.color;
    }
)";

#endif
#pragma endregion

#pragma region DirectX11 State
#ifdef DX11

Microsoft::WRL::ComPtr<ID3D11Device> device;
Microsoft::WRL::ComPtr<ID3D11DeviceContext> context;
Microsoft::WRL::ComPtr<IDXGISwapChain> swapChain;
Microsoft::WRL::ComPtr<ID3D11RenderTargetView> renderTargetView;
Microsoft::WRL::ComPtr<ID3D11VertexShader> vertexShader;
Microsoft::WRL::ComPtr<ID3D11PixelShader> pixelShader;
Microsoft::WRL::ComPtr<ID3D11InputLayout> inputLayout;
Microsoft::WRL::ComPtr<ID3D11Buffer> vertexBuffer;
Microsoft::WRL::ComPtr<ID3D11Buffer> constantBuffer;
Microsoft::WRL::ComPtr<ID3D11RasterizerState> rasterizerState;
Microsoft::WRL::ComPtr<ID3D11DepthStencilState> depthStencilState;
Microsoft::WRL::ComPtr<ID3D11BlendState> blendState;

#endif
#pragma endregion

#pragma region DirectX12 State
#ifdef DX12

Microsoft::WRL::ComPtr<ID3D12Device> device;
Microsoft::WRL::ComPtr<ID3D12CommandQueue> commandQueue;
Microsoft::WRL::ComPtr<IDXGISwapChain3> swapChain;
Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> rtvHeap;
Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> cbvHeap;
Microsoft::WRL::ComPtr<ID3D12Resource> renderTargets[FRAME_COUNT];
Microsoft::WRL::ComPtr<ID3D12CommandAllocator> commandAllocators[FRAME_COUNT];
Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> commandList;
Microsoft::WRL::ComPtr<ID3D12RootSignature> rootSignature;
Microsoft::WRL::ComPtr<ID3D12PipelineState> pipelineState;
Microsoft::WRL::ComPtr<ID3D12Resource> vertexBuffer;
Microsoft::WRL::ComPtr<ID3D12Resource> constantBuffer;
D3D12_VERTEX_BUFFER_VIEW vertexBufferView;

uint32_t rtvDescriptorSize;
uint32_t frameIndex;
HANDLE fenceEvent;
Microsoft::WRL::ComPtr<ID3D12Fence> fence;
uint64_t fenceValues[FRAME_COUNT];

#endif
#pragma endregion

#pragma region Logging
#ifdef _DEBUG

static FILE* g_logFile = nullptr;

static std::string getCurrentTimeISO8601() {
    using namespace std::chrono;
    auto now = system_clock::now();
    auto now_time_t = system_clock::to_time_t(now);
    auto now_ms = duration_cast<milliseconds>(now.time_since_epoch()) % 1000;

    std::tm tm_utc;
#if defined(_WIN32) || defined(_WIN64)
    gmtime_s(&tm_utc, &now_time_t);
#else
    gmtime_r(&now_time_t, &tm_utc);
#endif

    std::ostringstream oss;
    oss << std::put_time(&tm_utc, "%Y-%m-%dT%H:%M:%S");
    oss << '.' << std::setw(3) << std::setfill('0') << now_ms.count() << 'Z';

    return oss.str();
}

static void init_debug_console() {
    FreeConsole();
    if (!AllocConsole()) {
        MessageBox(nullptr, L"AllocConsole failed", L"Error", MB_OK);
        return;
    }
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);
    FILE* fDummy;
    freopen_s(&fDummy, "CONOUT$", "w", stdout);
    freopen_s(&fDummy, "CONOUT$", "w", stderr);
    freopen_s(&fDummy, "CONIN$", "r", stdin);

    _wfopen_s(&g_logFile, L"debug_log.txt", L"a, ccs=UTF-8");
    wprintf(L"[DEBUG] Console active\n");
    if (g_logFile) {
        fwprintf(g_logFile, L"[DEBUG] Console active\n");
        fflush(g_logFile);
    }

    SetConsoleTitle(L"Debug Console");
}

#define LOG(fmt, ...) do { \
    wprintf(L"[DEBUG] [%hs] [%hs] " fmt L"\n", getCurrentTimeISO8601().c_str(), __FUNCTION__, __VA_ARGS__); \
    if (g_logFile) { \
        fwprintf(g_logFile, L"[DEBUG] [%hs] [%hs] " fmt L"\n", getCurrentTimeISO8601().c_str(), __FUNCTION__, __VA_ARGS__); \
        fflush(g_logFile); \
    } \
} while(0)
#else
#define LOG(fmt, ...) ((void)0)
#endif
#pragma endregion

#pragma region Declarations

// Program
int WINAPI wWinMain(_In_ HINSTANCE hInstance, _In_opt_ HINSTANCE hPrevInstance, _In_ LPWSTR lpCmdLine, _In_ int nCmdShow);
LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

// Arguments
std::set<std::wstring> parseArguments();
void processArguments(std::set<std::wstring> args, EngineState &engine);

// Game
void InitGame(EngineState &engine);
void StartGame(EngineState& engine);
void UpdateGame(EngineState& engine);
void PlaceFood(SnakeGameState &game);
std::vector<Vertex> DrawGame(float alpha, EngineState& engine);
void CleanupGame(EngineState &engine);

// Utils
void PrintHook(const std::wstring& name, void* ptr);
void ToggleFullscreen(EngineState &engine);
void RecalcScale(uint32_t width, uint32_t height, EngineState& engine);
void AddRect(std::vector<Vertex>& vertices, float x, float y, float w, float h, Float4 color);

// Initialization
HWND InitWindow(HINSTANCE hInstance, WNDCLASS wc, EngineState &engine);

#ifdef DX11
// DX11 Initialization
void InitDX11(EngineState& engine);
void CreatePipelineStateDX11();
void CreateVertexBufferDX11();
void CreateConstantBufferDX11();
void CleanupDX11();

// DX11 Rendering
void DrawDX11(float alpha, EngineState &engine);
void UpdateConstantBufferDX11(EngineState& engine);
void ReportLeaksDX11();

#define InitDX InitDX11
#define CreatePipelineState CreatePipelineStateDX11
#define CreateVertexBuffer CreateVertexBufferDX11
#define CreateConstantBuffer CreateConstantBufferDX11
#define CleanupDX CleanupDX11
#define DrawDX DrawDX11
#define UpdateConstantBuffer UpdateConstantBufferDX11
#define ReportLeaksDX ReportLeaksDX11
#elif DX12
// DX12 Initialization
void InitDX12(EngineState &engine);
void CreateRootSignatureDX12();
void CreatePipelineStateDX12();
void CreateVertexBufferDX12();
void CreateConstantBufferDX12();
void CleanupDX12();

// DX12 Rendering
void DrawDX12(float alpha, EngineState& engine);
void UpdateConstantBufferDX12(EngineState& engine);
void WaitForGpuDX12();
void MoveToNextFrameDX12();
void ReportLeaksDX12();

#define InitDX InitDX12
#define CreatePipelineState CreatePipelineStateDX12
#define CreateVertexBuffer CreateVertexBufferDX12
#define CreateConstantBuffer CreateConstantBufferDX12
#define CleanupDX CleanupDX12
#define DrawDX DrawDX12
#define UpdateConstantBuffer UpdateConstantBufferDX12
#define ReportLeaksDX ReportLeaksDX12
#endif

#pragma endregion

#pragma region Program

int WINAPI wWinMain(
    _In_ HINSTANCE hInstance,
    _In_opt_ HINSTANCE hPrevInstance,
    _In_ LPWSTR lpCmdLine,
    _In_ int nCmdShow
) {
    UNREFERENCED_PARAMETER(hPrevInstance);
    UNREFERENCED_PARAMETER(lpCmdLine);
    UNREFERENCED_PARAMETER(nCmdShow);

#ifdef _DEBUG
    init_debug_console();
#endif
    EngineState* engine = new EngineState;
    GameState* game = new GameState;
    engine->game = game;
    auto args = parseArguments();
    WNDCLASS wc = {};
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = CLASS_NAME;
    wc.cbWndExtra = sizeof(void*);
    RegisterClass(&wc);
    HWND hwnd = InitWindow(hInstance, wc, *engine);
    engine->window = &hwnd;

#if defined(DX11) || defined(DX12)
    InitDX(*engine);
    #ifdef DX12
    CreateRootSignatureDX12();
    #endif
    CreatePipelineState();
    CreateVertexBuffer();
    CreateConstantBuffer();
    #ifdef DX12
    device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, commandAllocators[frameIndex].Get(), pipelineState.Get(), IID_PPV_ARGS(&commandList));
    commandList->Close();

    device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&fence));
    fenceEvent = CreateEvent(nullptr, FALSE, FALSE, nullptr);
    if (!fenceEvent) { 
        LOG(L"PANIC: Couldn't create fenceEvent!!!");
        return -1; 
    }
    for (uint32_t i = 0; i < FRAME_COUNT; i++)
        fenceValues[i] = 1;

    #endif
#endif
    processArguments(args, *engine);
start:
    InitGame(*engine);
    StartGame(*engine);
    CleanupGame(*engine);

    if (!engine->quit) goto start;

#ifdef DX12
    WaitForGpuDX12();
    if (fenceEvent) {
        CloseHandle(fenceEvent);
    }
#endif
    LOG(L"Shutting down");
    delete game;
    delete engine;
    UnregisterClass(wc.lpszClassName, hInstance);
#if defined(DX11) || defined(DX12)
    CleanupDX();
#endif
    return 0;
}

LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    EngineState* engine = (EngineState*)GetWindowLongPtr(hwnd, 0);

    switch (msg) {
    case WM_SIZE:
#if defined (DX11) || defined (DX12)
        if (swapChain) {
            engine->windowWidth = LOWORD(lParam);
            engine->windowHeight = HIWORD(lParam);
    #ifdef DX12
            WaitForGpuDX12();

            for (uint32_t i = 0; i < FRAME_COUNT; i++) {
                renderTargets[i].Reset();
                fenceValues[i] = fenceValues[frameIndex];
            }

            swapChain->ResizeBuffers(FRAME_COUNT, engine->windowWidth, engine->windowHeight, DXGI_FORMAT_R8G8B8A8_UNORM, 0);

            frameIndex = swapChain->GetCurrentBackBufferIndex();

            D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle(rtvHeap->GetCPUDescriptorHandleForHeapStart());
            for (uint32_t i = 0; i < FRAME_COUNT; i++) {
                swapChain->GetBuffer(i, IID_PPV_ARGS(&renderTargets[i]));
                device->CreateRenderTargetView(renderTargets[i].Get(), nullptr, rtvHandle);
                rtvHandle.ptr += rtvDescriptorSize;
            }
    #elif DX11
            context->OMSetRenderTargets(0, nullptr, nullptr);
            renderTargetView.Reset();
            swapChain->ResizeBuffers(0, engine->windowWidth, engine->windowHeight, DXGI_FORMAT_UNKNOWN, 0);
            Microsoft::WRL::ComPtr<ID3D11Texture2D> backBuffer;
            swapChain->GetBuffer(0, IID_PPV_ARGS(&backBuffer));
            device->CreateRenderTargetView(backBuffer.Get(), nullptr, &renderTargetView);
            context->OMSetRenderTargets(1, renderTargetView.GetAddressOf(), nullptr);
    #endif

            RecalcScale(engine->windowWidth, engine->windowHeight, *engine);
            UpdateConstantBuffer(*engine);
        }
#endif
        return 0;

    case WM_SYSKEYDOWN:
        if (wParam == VK_RETURN && (lParam & (1 << 29))) {
            ToggleFullscreen(*engine);
            return 0;
        }
        break;

    case WM_KEYDOWN:
        if (wParam == VK_F11) {
            ToggleFullscreen(*engine);
            return 0;
        }

#ifdef _DEBUG
        if (wParam == VK_F6) {
            basetick = basetick + 4;
            time1 = basetick;
            time2 = basetick;
            time3 = basetick;
        }
        if (wParam == VK_F7) {
            basetick = basetick - 4;
            time1 = basetick;
            time2 = basetick;
            time3 = basetick;
        }
        if (wParam == VK_F8) {
            LOG("F8: TICKRATE@%d", basetick);
        }

        if (wParam == 'C' && (GetKeyState(VK_CONTROL) & 0x8000)) {
            if (!engine->stuck) engine->quit = true;
            else std::exit(1);
            return 0;
        }
#endif
        if (wParam == VK_ESCAPE) {
            LOG("%s", engine->paused ? L"Unpaused" : L"Paused");
            engine->paused = !engine->paused;
            return 0;
        }

        if (!engine->paused) {
            if ((wParam == 'W' || wParam == VK_UP) && engine->game->dir_y != 1) { engine->game->dir_x = 0; engine->game->dir_y = -1; }
            if ((wParam == 'S' || wParam == VK_DOWN) && engine->game->dir_y != -1) { engine->game->dir_x = 0; engine->game->dir_y = 1; }
            if ((wParam == 'A' || wParam == VK_LEFT) && engine->game->dir_x != 1) { engine->game->dir_x = -1; engine->game->dir_y = 0; }
            if ((wParam == 'D' || wParam == VK_RIGHT) && engine->game->dir_x != -1) { engine->game->dir_x = 1; engine->game->dir_y = 0; }
        }

        return 0;

    case WM_CLOSE:
        if (engine->stuck) std::exit(1);
        LOG(L"Window closed");
        if (engine->quit || !engine->running) {
            PostQuitMessage(0);
            engine->stuck = true;
            return 0;
        }
        engine->running = false;
        engine->quit = true;
        PostQuitMessage(0);
        return 0;

    case WM_DESTROY:
        LOG(L"Window destroyed");
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProc(hwnd, msg, wParam, lParam);
}

#pragma endregion

#pragma region Arguments

std::set<std::wstring> parseArguments() {
    std::set<std::wstring> args;

    LPWSTR cmdLine = GetCommandLine();
    if (!cmdLine) return args;

    int argc = 0;
    LPWSTR* argv = CommandLineToArgvW(cmdLine, &argc);
    if (!argv) return args;

    for (int i = 0; i < argc; ++i) {
        args.emplace(argv[i]);
    }

    LocalFree(argv);
    return args;
}

void processArguments(std::set<std::wstring> args, EngineState &engine) {
    if(engine.window == nullptr) {
        LOG("ERROR: Window doesn't exist!");
        std::exit(-1);
    }
    if (args.contains(L"-fs") || args.contains(L"--fullscreen"))
        ToggleFullscreen(engine);
    if (args.contains(L"-h") || args.contains(L"--help")) {
        MessageBoxW(*engine.window,
            L"Usage: snake.exe [options]\n-h, --help - Show help\n-fs, --fullscreen - Toggle fullscreen mode",
            L"Snake", MB_OK);
        exit(0);
    }
}

#pragma endregion

#pragma region Game

void InitGame(EngineState &engine) {
    if (engine.window == nullptr) {
        LOG("ERROR: Window doesn't exist!");
        std::exit(-1);
    }
    if (engine.game == nullptr) {
        LOG("ERROR: Game doesn't exist!");
        std::exit(-1);
    }
    SnakeGameState* game = engine.game;
    LOG(L"Initializing game \"%ls\"", game->name.c_str());

    PrintHook(L"ENGINE", (void*)&engine);
    PrintHook(L"GAME", (void*)engine.game);

    std::random_device rd;
    rng.seed(rd());

    if (engine.is_first) {
        RECT rc;
        GetClientRect(*engine.window, &rc);
        RecalcScale(rc.right, rc.bottom, engine);
#if defined(DX11) || defined(DX12)
        UpdateConstantBuffer(engine);
#endif
    }
    for (size_t i = 0; i < game->snake_length; i++) {
        game->snake[i].position.x = WIDTH / 2 - static_cast<int>(i);
        game->snake[i].position.y = HEIGHT / 2;
        game->prev_snake[i] = game->snake[i];
        LOG(L"Snake segment %zu initialized at (%d, %d)", i, game->snake[i].position.x, game->snake[i].position.y);
    }
    game->prev_snake_length = game->snake_length;
    game->corner_count = 0;

    PlaceFood(*game);
    game->score = 0;
    engine.running = true;
    game->dir_x = 1;
    game->dir_y = 0;
}

void StartGame(EngineState &engine) {
    if (engine.game == nullptr) {
        LOG("ERROR: Game doesn't exist!");
        std::exit(-1);
    }
    SnakeGameState* game = engine.game;
    MSG msg = {};
    auto lastTime = std::chrono::steady_clock::now();
    std::chrono::nanoseconds accumulator{ 0 };
    float alpha = 0.0f;

    using duration = std::chrono::steady_clock::duration;

    while (engine.running) {
        auto now = std::chrono::steady_clock::now();
        auto delta = now - lastTime;
        lastTime = now;

        if (delta > std::chrono::milliseconds(50))
            delta = std::chrono::milliseconds(50);

        // START PRERENDER

        if (engine.stuck) LOG("Encountered an expected issue");
        if (engine.quit) break;

        while (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE)) {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }

        if (!engine.paused) {
            accumulator += delta;

            auto tickMs =
                game->score <= timeThreshold ? time1 :
                game->score <= timeThreshold * 2 ? time2 : time3;

            auto tick = std::chrono::milliseconds(tickMs);

            while (accumulator >= tick) {
                UpdateGame(engine);
                accumulator -= tick;
            }

            alpha = static_cast<float>(
                std::chrono::duration_cast<std::chrono::duration<float>>(accumulator).count() /
                std::chrono::duration_cast<std::chrono::duration<float>>(tick).count()
                );
        }
        else {
            alpha = 0.0f;
            accumulator = duration::zero();
        }

        // END PRERENDER

#if defined(DX11) || defined(DX12)
        DrawDX(alpha, engine);
#endif
    }


    engine.is_first = false;
#ifdef DX12
    WaitForGpuDX12();
#else
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
#endif

    LOG(L"Exiting main loop");
    std::wstring message = L"Game Over, Score ";
    message += std::to_wstring(game->score);
    if (!engine.quit) MessageBox(*engine.window, message.c_str(), L"Snake", MB_OK);
}

void UpdateGame(EngineState &engine) {
    if (engine.game == nullptr) {
        LOG("ERROR: Game doesn't exist!");
        std::exit(-1);
    }
    SnakeGameState* game = engine.game;
    if (game->snake_length == 0 || game->snake_length > MAX_SNAKE) {
        game->snake_length = (game->snake_length == 0) ? 1 : MAX_SNAKE;
    }

    game->prev_snake_length = game->snake_length;
    for (size_t i = 0; i < game->snake_length; i++) {
        game->prev_snake[i] = game->snake[i];
    }

    int new_head_x = game->snake[0].position.x + game->dir_x;
    int new_head_y = game->snake[0].position.y + game->dir_y;

    if (game->snake_length > 1) {
        int prev_dir_x = game->snake[0].position.x - game->snake[1].position.x;
        int prev_dir_y = game->snake[0].position.y - game->snake[1].position.y;

        if (prev_dir_x != game->dir_x || prev_dir_y != game->dir_y) {
            if (game->corner_count < MAX_SNAKE) {
                game->corners[game->corner_count].position.x = game->snake[0].position.x;
                game->corners[game->corner_count].position.y = game->snake[0].position.y;
                game->corners[game->corner_count].segments_remaining = static_cast<int>(game->snake_length);
                game->corner_count++;
                LOG(L"Added corner at (%d, %d), segments_remaining: %zu", game->snake[0].position.x, game->snake[0].position.y, game->snake_length);
            }
        }
        for (size_t i = game->snake_length - 1; i > 0; i--) {
            game->snake[i] = game->snake[i - 1];
        }
    }

    game->snake[0].position.x = new_head_x;
    game->snake[0].position.y = new_head_y;

    for (size_t i = 0; i < game->corner_count; i++) {
        game->corners[i].segments_remaining--;
    }

    size_t write_idx = 0;
    for (size_t i = 0; i < game->corner_count; i++) {
        if (game->corners[i].segments_remaining > 0) {
            game->corners[write_idx++] = game->corners[i];
        }
    }
    game->corner_count = write_idx;

    if (game->snake[0].position.x <= 0 || game->snake[0].position.x >= WIDTH - 1 ||
        game->snake[0].position.y <= 0 || game->snake[0].position.y >= HEIGHT - 1) {
        engine.running = false;
        LOG(L"Snake hit wall. Game over.");
    }

    for (size_t i = 1; i < game->snake_length; i++) {
        if (game->snake[0].position.x == game->snake[i].position.x && game->snake[0].position.y == game->snake[i].position.y) {
            engine.running = false;
            LOG(L"Snake collided with itself at (%d, %d). Game over.", game->snake[0].position.x, game->snake[0].position.y);
        }
    }

    if (game->snake[0].position.x == game->food_x && game->snake[0].position.y == game->food_y) {
        if (game->snake_length > 0 && game->snake_length < MAX_SNAKE) {
            size_t new_segment_idx = game->snake_length;
            size_t source_segment_idx = game->snake_length - 1;

            game->snake[new_segment_idx] = game->snake[source_segment_idx];
            game->snake_length++;

            for (size_t i = 0; i < game->corner_count; i++)
                game->corners[i].segments_remaining++;

            LOG(L"Ate food. New length: %zu, New Score: %d", game->snake_length, game->score + 1);
        }
        PlaceFood(*game);
        game->score++;
    }
}

void PlaceFood(SnakeGameState& game) {
    std::vector<Int2> empty_cells;
    empty_cells.reserve((WIDTH - 2) * (HEIGHT - 2));

    for (int x = 1; x < WIDTH - 1; x++) {
        for (int y = 1; y < HEIGHT - 1; y++) {
            bool occupied = false;
            for (size_t i = 0; i < game.snake_length; i++) {
                if (game.snake[i].position.x == x && game.snake[i].position.y == y) {
                    occupied = true;
                    break;
                }
            }
            if (!occupied) empty_cells.push_back(Int2(x, y));
        }
    }

    if (empty_cells.empty()) {
        game.food_x = -1;
        game.food_y = -1;
        return;
    }

    std::uniform_int_distribution<size_t> dist(0, empty_cells.size() - 1);
    Int2 chosen = empty_cells[dist(rng)];
    game.food_x = chosen.x;
    game.food_y = chosen.y;
}

std::vector<Vertex> DrawGame(float alpha, EngineState &engine) {
    if (engine.game == nullptr) {
        LOG("ERROR: Game doesn't exist!");
        std::exit(-1);
    }
    SnakeGameState* game = engine.game;
    std::vector<Vertex> vertices;
    // Draw walls
    Float4 grayColor(0.5f, 0.5f, 0.5f, 1.0f);
    for (int x = 0; x < WIDTH; x++) {
        AddRect(vertices, engine.offset_x + x * engine.cell_px, engine.offset_y + 0 * engine.cell_px, engine.cell_px, engine.cell_px, grayColor);
        AddRect(vertices, engine.offset_x + x * engine.cell_px, engine.offset_y + (HEIGHT - 1) * engine.cell_px, engine.cell_px, engine.cell_px, grayColor);
    }
    for (int y = 0; y < HEIGHT; y++) {
        AddRect(vertices, engine.offset_x + 0 * engine.cell_px, engine.offset_y + y * engine.cell_px, engine.cell_px, engine.cell_px, grayColor);
        AddRect(vertices, engine.offset_x + (WIDTH - 1) * engine.cell_px, engine.offset_y + y * engine.cell_px, engine.cell_px, engine.cell_px, grayColor);
    }

    // Draw food
    Float4 redColor(1.0f, 0.0f, 0.0f, 1.0f);
    AddRect(vertices, engine.offset_x + game->food_x * engine.cell_px, engine.offset_y + game->food_y * engine.cell_px, engine.cell_px, engine.cell_px, redColor);

    // Draw corners
    Float4 darkGreenColor(0.0f, 0.6f, 0.0f, 1.0f);
    for (size_t i = 0; i < game->corner_count; i++) {
        AddRect(vertices, engine.offset_x + game->corners[i].position.x * engine.cell_px, engine.offset_y + game->corners[i].position.y * engine.cell_px, engine.cell_px, engine.cell_px, darkGreenColor);
    }

    // Draw snake body (all segments except head)
    Float4 bodyColor(0.0f, 0.6f, 0.0f, 1.0f);
    size_t draw_length = game->snake_length;
    for (size_t i = 1; i < draw_length; i++) {
        float x, y;

        if (i < game->prev_snake_length) {
            x = game->prev_snake[i].position.x + (game->snake[i].position.x - game->prev_snake[i].position.x) * alpha;
            y = game->prev_snake[i].position.y + (game->snake[i].position.y - game->prev_snake[i].position.y) * alpha;
        }
        else {
            x = static_cast<float>(game->snake[i].position.x);
            y = static_cast<float>(game->snake[i].position.y);
        }

        AddRect(vertices, engine.offset_x + x * engine.cell_px, engine.offset_y + y * engine.cell_px, engine.cell_px, engine.cell_px, bodyColor);
    }

    // Draw snake head (always on top with bright green)
    if (draw_length > 0) {
        float headX = game->prev_snake[0].position.x + (game->snake[0].position.x - game->prev_snake[0].position.x) * alpha;
        float headY = game->prev_snake[0].position.y + (game->snake[0].position.y - game->prev_snake[0].position.y) * alpha;
        Float4 headColor(0.0f, 1.0f, 0.0f, 1.0f);
        AddRect(vertices, engine.offset_x + headX * engine.cell_px, engine.offset_y + headY * engine.cell_px, engine.cell_px, engine.cell_px, headColor);
    }
    return vertices;
}

void CleanupGame(EngineState &engine) {
    if (engine.game == nullptr) {
        LOG("ERROR: Game doesn't exist!");
        std::exit(-1);
    }
    SnakeGameState* game = engine.game;
    LOG(L"Cleaning up");
    for (size_t i = 0; i < game->corner_count; i++) {
        game->corners[i].position.x = 0;
        game->corners[i].position.y = 0;
        game->corners[i].segments_remaining = 0;
    }
    game->corner_count = 0;
    for (size_t i = 0; i < game->snake_length; i++) {
        game->snake[i].position.x = 0;
        game->snake[i].position.y = 0;
    }
    game->snake_length = 5;
    game->food_x = 0;
    game->food_y = 0;
    game->score = 0;
    game->dir_x = 0;
    game->dir_y = 0;
    engine.running = false;
}

#pragma endregion

#pragma region Utils

void PrintHook(const std::wstring &name, void* ptr) {
    LOG("%s@0x%" PRIxPTR, name.c_str(), (uintptr_t)ptr);
}

void ToggleFullscreen(EngineState &engine) {
    if (engine.window == nullptr) {
        LOG("ERROR: Window doesn't exist!");
        std::exit(-1);
    }
    LOG(L"Toggling fullscreen. Currently %s", engine.is_fullscreen ? L"ON" : L"OFF");
    engine.is_fullscreen = !engine.is_fullscreen;

    if (engine.is_fullscreen) {
        engine.windowed_style = GetWindowLong(*engine.window, GWL_STYLE);
        GetWindowRect(*engine.window, &engine.windowed_rect);

        MONITORINFO mi = { sizeof(mi) };
        GetMonitorInfo(MonitorFromWindow(*engine.window, MONITOR_DEFAULTTONEAREST), &mi);

        SetWindowLong(*engine.window, GWL_STYLE, engine.windowed_style & ~WS_OVERLAPPEDWINDOW);
        SetWindowPos(
            *engine.window,
            HWND_TOP,
            mi.rcMonitor.left,
            mi.rcMonitor.top,
            mi.rcMonitor.right - mi.rcMonitor.left,
            mi.rcMonitor.bottom - mi.rcMonitor.top,
            SWP_NOOWNERZORDER | SWP_FRAMECHANGED
        );

        LOG(L"Entered fullscreen mode");
    } else {
        SetWindowLong(*engine.window, GWL_STYLE, engine.windowed_style);
        SetWindowPos(
            *engine.window,
            nullptr,
            engine.windowed_rect.left,
            engine.windowed_rect.top,
            engine.windowed_rect.right - engine.windowed_rect.left,
            engine.windowed_rect.bottom - engine.windowed_rect.top,
            SWP_NOOWNERZORDER | SWP_FRAMECHANGED
        );
        LOG(L"Exited fullscreen mode");
    }
}

void RecalcScale(uint32_t width, uint32_t height, EngineState &engine) {
    LOG(L"%s scale for window size %u x %u", engine.is_first ? L"Calculating" : L"Recalculating", width, height);
    float sx = static_cast<float>(width) / WIDTH;
    float sy = static_cast<float>(height) / HEIGHT;

    engine.cell_px = sx < sy ? sx : sy;
    engine.offset_x = (width - engine.cell_px * WIDTH) * 0.5f;
    engine.offset_y = (height - engine.cell_px * HEIGHT) * 0.5f;

    LOG(L"cell_px: %.2f, offset_x: %.2f, offset_y: %.2f", engine.cell_px, engine.offset_x, engine.offset_y);
}

void AddRect(std::vector<Vertex>& vertices, float x, float y, float w, float h, Float4 color) {
    vertices.push_back({ Float3(x, y, 0.0f), color });
    vertices.push_back({ Float3(x + w, y, 0.0f), color });
    vertices.push_back({ Float3(x, y + h, 0.0f), color });

    vertices.push_back({ Float3(x, y + h, 0.0f), color });
    vertices.push_back({ Float3(x + w, y, 0.0f), color });
    vertices.push_back({ Float3(x + w, y + h, 0.0f), color });
}

#pragma endregion

#pragma region Initialization

HWND InitWindow(HINSTANCE hInstance, WNDCLASS wc, EngineState& engine) {
    LOG(L"Initializing window");
    RECT desired_client = { 0, 0, (LONG)engine.windowWidth, (LONG)engine.windowHeight };
    DWORD style = WS_OVERLAPPEDWINDOW;
    AdjustWindowRect(&desired_client, style, FALSE);
    int window_width = desired_client.right - desired_client.left;
    int window_height = desired_client.bottom - desired_client.top;
    int screen_width = GetSystemMetrics(SM_CXSCREEN);
    int screen_height = GetSystemMetrics(SM_CYSCREEN);
    int x = (screen_width - window_width) / 2;
    int y = (screen_height - window_height) / 2;

    HWND hwnd = CreateWindow(
        wc.lpszClassName,
        engine.game->name.c_str(),
        style,
        x, y,
        window_width,
        window_height,
        nullptr, nullptr, hInstance, nullptr  // Changed &engine to nullptr
    );

    SetWindowLongPtr(hwnd, 0, (LONG_PTR)&engine);

    LOG(L"Window created");
    ShowWindow(hwnd, SW_SHOW);
    return hwnd;
}

#pragma endregion

#pragma region DX11-Initialization
#ifdef DX11

void InitDX11(EngineState &engine) {
    LOG(L"Initializing DirectX 11");

    UINT createDeviceFlags = 0;
#ifdef _DEBUG
    createDeviceFlags |= D3D11_CREATE_DEVICE_DEBUG;
#endif

    D3D_FEATURE_LEVEL featureLevel;
    const D3D_FEATURE_LEVEL featureLevels[] = { D3D_FEATURE_LEVEL_11_0 };

    LOG("Creating Swap Chain");
    DXGI_SWAP_CHAIN_DESC sd = {};
    sd.BufferCount = 2;
    sd.BufferDesc.Width = engine.windowWidth;
    sd.BufferDesc.Height = engine.windowHeight;
    sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    sd.BufferDesc.RefreshRate.Numerator = 60;
    sd.BufferDesc.RefreshRate.Denominator = 1;
    sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    sd.OutputWindow = *engine.window;
    sd.SampleDesc.Count = 1;
    sd.SampleDesc.Quality = 0;
    sd.Windowed = TRUE;
    sd.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;

    HRESULT hr = D3D11CreateDeviceAndSwapChain(
        nullptr,
        D3D_DRIVER_TYPE_HARDWARE,
        nullptr,
        createDeviceFlags,
        featureLevels,
        1,
        D3D11_SDK_VERSION,
        &sd,
        &swapChain,
        &device,
        &featureLevel,
        &context
    );

    if (FAILED(hr)) {
        LOG(L"Failed to initialize DX11");
        return;
    }


    LOG(L"Creating Render Target View");
    Microsoft::WRL::ComPtr<ID3D11Texture2D> backBuffer;
    swapChain->GetBuffer(0, IID_PPV_ARGS(&backBuffer));

    device->CreateRenderTargetView(backBuffer.Get(), nullptr, &renderTargetView);

    context->OMSetRenderTargets(1, renderTargetView.GetAddressOf(), nullptr);

    LOG(L"DirectX 11 initialized successfully");
}

void CreatePipelineStateDX11() {
    LOG(L"Creating pipeline state");

    Microsoft::WRL::ComPtr<ID3DBlob> vsBlob;
    Microsoft::WRL::ComPtr<ID3DBlob> psBlob;
    Microsoft::WRL::ComPtr<ID3DBlob> error;

    UINT compileFlags = D3DCOMPILE_ENABLE_STRICTNESS;
#ifdef _DEBUG
    compileFlags |= D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
#endif
    LOG(L"Compiling shaders");
    D3DCompile(DX_SHADER.c_str(), DX_SHADER.length(), nullptr, nullptr, nullptr, "VSMain", "vs_5_0", compileFlags, 0, &vsBlob, &error);
    D3DCompile(DX_SHADER.c_str(), DX_SHADER.length(), nullptr, nullptr, nullptr, "PSMain", "ps_5_0", compileFlags, 0, &psBlob, &error);

    device->CreateVertexShader(vsBlob->GetBufferPointer(), vsBlob->GetBufferSize(), nullptr, &vertexShader);
    device->CreatePixelShader(psBlob->GetBufferPointer(), psBlob->GetBufferSize(), nullptr, &pixelShader);

    D3D11_INPUT_ELEMENT_DESC layout[] = {
        { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT,    0, 0,  D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "COLOR",    0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0 }
    };

    device->CreateInputLayout(layout, _countof(layout), vsBlob->GetBufferPointer(), vsBlob->GetBufferSize(), &inputLayout);

    LOG("Creating rasterizer state");
    D3D11_RASTERIZER_DESC rasterDesc = {};
    rasterDesc.FillMode = D3D11_FILL_SOLID;
    rasterDesc.CullMode = D3D11_CULL_NONE;
    device->CreateRasterizerState(&rasterDesc, &rasterizerState);

    LOG("Creating blend state");
    D3D11_BLEND_DESC blendDesc = {};
    blendDesc.RenderTarget[0].BlendEnable = FALSE;
    blendDesc.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
    device->CreateBlendState(&blendDesc, &blendState);

    LOG("Creating depth stencil state");
    D3D11_DEPTH_STENCIL_DESC dsDesc = {};
    dsDesc.DepthEnable = FALSE;
    dsDesc.StencilEnable = FALSE;
    device->CreateDepthStencilState(&dsDesc, &depthStencilState);

    LOG(L"Pipeline states created individually");
}

void CreateVertexBufferDX11() {
    LOG(L"Creating vertex buffer");

    const size_t vertexBufferSize = sizeof(Vertex) * 6 * (MAX_SNAKE + WIDTH * 2 + HEIGHT * 2 + 100);

    D3D11_BUFFER_DESC bufferDesc = {};
    bufferDesc.ByteWidth = static_cast<UINT>(vertexBufferSize);
    bufferDesc.Usage = D3D11_USAGE_DYNAMIC;
    bufferDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
    bufferDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;

    HRESULT hr = device->CreateBuffer(&bufferDesc, nullptr, &vertexBuffer);

    if (FAILED(hr)) {
        LOG(L"Failed to create vertex buffer");
        return;
    }

    LOG(L"Vertex buffer created");
}

void CreateConstantBufferDX11() {
    LOG(L"Creating constant buffer");

    const size_t constantBufferSize = (sizeof(ConstantBuffer) + 255) & ~255;

    D3D11_BUFFER_DESC cbDesc = {};
    cbDesc.ByteWidth = static_cast<UINT>(constantBufferSize);
    cbDesc.Usage = D3D11_USAGE_DYNAMIC;
    cbDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    cbDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
    cbDesc.MiscFlags = 0;
    cbDesc.StructureByteStride = 0;

    HRESULT hr = device->CreateBuffer(&cbDesc, nullptr, &constantBuffer);

    if (FAILED(hr)) {
        LOG(L"Failed to create constant buffer");
        return;
    }

    LOG(L"Constant buffer created");
}

void CleanupDX11() {
    blendState.Reset();
    depthStencilState.Reset();
    rasterizerState.Reset();
    constantBuffer.Reset();
    vertexBuffer.Reset();
    inputLayout.Reset();
    pixelShader.Reset();
    vertexShader.Reset();
    renderTargetView.Reset();
    swapChain.Reset();
    context.Reset();
    ReportLeaksDX11();
    device.Reset();

    LOG(L"DirectX 11 resources released");
}
#endif
#pragma endregion

#pragma region DX12-Initialization 
#ifdef DX12

void InitDX12(EngineState &engine) {
    LOG(L"Initializing DirectX 12");

    uint32_t dxgiFactoryFlags = 0;

#ifdef _DEBUG
    Microsoft::WRL::ComPtr<ID3D12Debug> debugController;
    if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&debugController)))) {
        debugController->EnableDebugLayer();
        dxgiFactoryFlags |= DXGI_CREATE_FACTORY_DEBUG;
        LOG(L"Debug layer enabled");
    }
#endif

    Microsoft::WRL::ComPtr<IDXGIFactory4> factory;
    CreateDXGIFactory2(dxgiFactoryFlags, IID_PPV_ARGS(&factory));

    D3D12CreateDevice(nullptr, D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(&device));

    D3D12_COMMAND_QUEUE_DESC queueDesc = {};
    queueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
    queueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;

    device->CreateCommandQueue(&queueDesc, IID_PPV_ARGS(&commandQueue));

    DXGI_SWAP_CHAIN_DESC1 swapChainDesc = {};
    swapChainDesc.BufferCount = FRAME_COUNT;
    swapChainDesc.Width = engine.windowWidth;
    swapChainDesc.Height = engine.windowHeight;
    swapChainDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
    swapChainDesc.SampleDesc.Count = 1;

    Microsoft::WRL::ComPtr<IDXGISwapChain1> swapChain1;
    factory->CreateSwapChainForHwnd(
        commandQueue.Get(),
        *engine.window,
        &swapChainDesc,
        nullptr,
        nullptr,
        &swapChain1
    );

    factory->MakeWindowAssociation(*engine.window, DXGI_MWA_NO_ALT_ENTER);
    swapChain1.As(&swapChain);
    frameIndex = swapChain->GetCurrentBackBufferIndex();

    // Create descriptor heaps
    LOG(L"Creating descriptor heaps");
    D3D12_DESCRIPTOR_HEAP_DESC rtvHeapDesc = {};
    rtvHeapDesc.NumDescriptors = FRAME_COUNT;
    rtvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
    rtvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
    device->CreateDescriptorHeap(&rtvHeapDesc, IID_PPV_ARGS(&rtvHeap));

    rtvDescriptorSize = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);

    D3D12_DESCRIPTOR_HEAP_DESC cbvHeapDesc = {};
    cbvHeapDesc.NumDescriptors = 1;
    cbvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
    cbvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
    device->CreateDescriptorHeap(&cbvHeapDesc, IID_PPV_ARGS(&cbvHeap));

    // Create frame resources
    LOG(L"Creating frame resources");
    D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle(rtvHeap->GetCPUDescriptorHandleForHeapStart());

    for (uint32_t n = 0; n < FRAME_COUNT; n++) {
        swapChain->GetBuffer(n, IID_PPV_ARGS(&renderTargets[n]));
        device->CreateRenderTargetView(renderTargets[n].Get(), nullptr, rtvHandle);
        rtvHandle.ptr += rtvDescriptorSize;

        device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&commandAllocators[n]));
    }

    LOG(L"DirectX 12 initialized successfully");
}

void CreateRootSignatureDX12() {
    LOG(L"Creating root signature");
    D3D12_FEATURE_DATA_ROOT_SIGNATURE featureData = {};
    featureData.HighestVersion = D3D_ROOT_SIGNATURE_VERSION_1_1;

    if (FAILED(device->CheckFeatureSupport(D3D12_FEATURE_ROOT_SIGNATURE, &featureData, sizeof(featureData)))) {
        featureData.HighestVersion = D3D_ROOT_SIGNATURE_VERSION_1_0;
    }

    D3D12_ROOT_PARAMETER1 rootParameters[1]{};
    rootParameters[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
    rootParameters[0].Descriptor.ShaderRegister = 0;
    rootParameters[0].Descriptor.RegisterSpace = 0;
    rootParameters[0].Descriptor.Flags = D3D12_ROOT_DESCRIPTOR_FLAG_NONE;
    rootParameters[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX;

    D3D12_VERSIONED_ROOT_SIGNATURE_DESC rootSignatureDesc{};
    rootSignatureDesc.Version = D3D_ROOT_SIGNATURE_VERSION_1_1;
    rootSignatureDesc.Desc_1_1.NumParameters = _countof(rootParameters);
    rootSignatureDesc.Desc_1_1.pParameters = rootParameters;
    rootSignatureDesc.Desc_1_1.NumStaticSamplers = 0;
    rootSignatureDesc.Desc_1_1.pStaticSamplers = nullptr;
    rootSignatureDesc.Desc_1_1.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;

    Microsoft::WRL::ComPtr<ID3DBlob> signature;
    Microsoft::WRL::ComPtr<ID3DBlob> error;
    D3D12SerializeVersionedRootSignature(&rootSignatureDesc, &signature, &error);
    device->CreateRootSignature(0, signature->GetBufferPointer(), signature->GetBufferSize(), IID_PPV_ARGS(&rootSignature));

    LOG(L"Root signature created");
}

void CreatePipelineStateDX12() {
    LOG(L"Creating pipeline state");

    Microsoft::WRL::ComPtr<ID3DBlob> vertexShader;
    Microsoft::WRL::ComPtr<ID3DBlob> pixelShader;
    Microsoft::WRL::ComPtr<ID3DBlob> error;

#ifdef _DEBUG
    uint32_t compileFlags = D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
#else
    uint32_t compileFlags = 0;
#endif
    LOG(L"Compiling shaders");
    D3DCompile(DX_SHADER.c_str(), DX_SHADER.length(), nullptr, nullptr, nullptr, "VSMain", "vs_5_0", compileFlags, 0, &vertexShader, &error);
    D3DCompile(DX_SHADER.c_str(), DX_SHADER.length(), nullptr, nullptr, nullptr, "PSMain", "ps_5_0", compileFlags, 0, &pixelShader, &error);

    D3D12_INPUT_ELEMENT_DESC inputElementDescs[] = {
        { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        { "COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }
    };

    D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = {};
    psoDesc.InputLayout = { inputElementDescs, _countof(inputElementDescs) };
    psoDesc.pRootSignature = rootSignature.Get();
    psoDesc.VS = { vertexShader->GetBufferPointer(), vertexShader->GetBufferSize() };
    psoDesc.PS = { pixelShader->GetBufferPointer(), pixelShader->GetBufferSize() };
    psoDesc.RasterizerState.FillMode = D3D12_FILL_MODE_SOLID;
    psoDesc.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
    psoDesc.BlendState.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
    psoDesc.DepthStencilState.DepthEnable = FALSE;
    psoDesc.DepthStencilState.StencilEnable = FALSE;
    psoDesc.SampleMask = UINT_MAX;
    psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    psoDesc.NumRenderTargets = 1;
    psoDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
    psoDesc.SampleDesc.Count = 1;

    device->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&pipelineState));

    LOG(L"Pipeline state created");
}

void CreateVertexBufferDX12() {
    LOG(L"Creating vertex buffer");
    const size_t vertexBufferSize = sizeof(Vertex) * 6 * (MAX_SNAKE + WIDTH * 2 + HEIGHT * 2 + 100);

    D3D12_HEAP_PROPERTIES heapProps = {};
    heapProps.Type = D3D12_HEAP_TYPE_UPLOAD;

    D3D12_RESOURCE_DESC resourceDesc = {};
    resourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
    resourceDesc.Width = vertexBufferSize;
    resourceDesc.Height = 1;
    resourceDesc.DepthOrArraySize = 1;
    resourceDesc.MipLevels = 1;
    resourceDesc.Format = DXGI_FORMAT_UNKNOWN;
    resourceDesc.SampleDesc.Count = 1;
    resourceDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;

    device->CreateCommittedResource(
        &heapProps,
        D3D12_HEAP_FLAG_NONE,
        &resourceDesc,
        D3D12_RESOURCE_STATE_GENERIC_READ,
        nullptr,
        IID_PPV_ARGS(&vertexBuffer)
    );

    vertexBufferView.BufferLocation = vertexBuffer->GetGPUVirtualAddress();
    vertexBufferView.StrideInBytes = sizeof(Vertex);
    vertexBufferView.SizeInBytes = static_cast<uint32_t>(vertexBufferSize);

    LOG(L"Vertex buffer created");
}

void CreateConstantBufferDX12() {
    LOG(L"Creating constant buffer");
    const size_t constantBufferSize = (sizeof(ConstantBuffer) + 255) & ~255;

    D3D12_HEAP_PROPERTIES heapProps = {};
    heapProps.Type = D3D12_HEAP_TYPE_UPLOAD;

    D3D12_RESOURCE_DESC resourceDesc = {};
    resourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
    resourceDesc.Width = constantBufferSize;
    resourceDesc.Height = 1;
    resourceDesc.DepthOrArraySize = 1;
    resourceDesc.MipLevels = 1;
    resourceDesc.Format = DXGI_FORMAT_UNKNOWN;
    resourceDesc.SampleDesc.Count = 1;
    resourceDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;

    device->CreateCommittedResource(
        &heapProps,
        D3D12_HEAP_FLAG_NONE,
        &resourceDesc,
        D3D12_RESOURCE_STATE_GENERIC_READ,
        nullptr,
        IID_PPV_ARGS(&constantBuffer)
    );

    LOG(L"Constant buffer created");
}

void CleanupDX12() {
    constantBuffer.Reset();
    vertexBuffer.Reset();
    swapChain.Reset();
    ReportLeaksDX12();
    device.Reset();

    LOG(L"DirectX 12 resources released");
}

#endif
#pragma endregion

#pragma region DX11-Rendering
#ifdef DX11

void UpdateConstantBufferDX11(EngineState &engine) {
    DirectX::XMMATRIX proj = DirectX::XMMatrixOrthographicOffCenterLH(0.0f, static_cast<float>(engine.windowWidth), static_cast<float>(engine.windowHeight), 0.0f, 0.0f, 1.0f);

    ConstantBuffer cb{};
    XMStoreFloat4x4(&cb.projection, XMMatrixTranspose(proj));

    D3D11_MAPPED_SUBRESOURCE mappedResource;
    if (SUCCEEDED(context->Map(constantBuffer.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedResource))) {
        memcpy(mappedResource.pData, &cb, sizeof(cb));
        context->Unmap(constantBuffer.Get(), 0);
    }
}

void DrawDX11(float alpha, EngineState &engine) {
    UpdateConstantBufferDX11(engine);

    D3D11_VIEWPORT viewport = { 0.0f, 0.0f, static_cast<float>(engine.windowWidth), static_cast<float>(engine.windowHeight), 0.0f, 1.0f };
    context->RSSetViewports(1, &viewport);
    context->IASetInputLayout(inputLayout.Get());
    context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    context->VSSetShader(vertexShader.Get(), nullptr, 0);
    context->PSSetShader(pixelShader.Get(), nullptr, 0);
    context->RSSetState(rasterizerState.Get());
    context->OMSetBlendState(blendState.Get(), nullptr, 0xffffffff);
    context->OMSetDepthStencilState(depthStencilState.Get(), 0);
    context->OMSetRenderTargets(1, renderTargetView.GetAddressOf(), nullptr);

    const float clearColor[] = { 0.05f, 0.05f, 0.05f, 1.0f };
    context->ClearRenderTargetView(renderTargetView.Get(), clearColor);
    context->VSSetConstantBuffers(0, 1, constantBuffer.GetAddressOf());

    std::vector<Vertex> vertices = DrawGame(alpha, engine);

    D3D11_MAPPED_SUBRESOURCE vMapped;
    if (SUCCEEDED(context->Map(vertexBuffer.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &vMapped))) {
        memcpy(vMapped.pData, vertices.data(), vertices.size() * sizeof(Vertex));
        context->Unmap(vertexBuffer.Get(), 0);
    }

    UINT stride = sizeof(Vertex);
    UINT offset = 0;
    context->IASetVertexBuffers(0, 1, vertexBuffer.GetAddressOf(), &stride, &offset);
    context->Draw(static_cast<uint32_t>(vertices.size()), 0);

    swapChain->Present(1, 0);
}

void ReportLeaksDX11()
{
#ifdef _DEBUG
    if (device.Get() == nullptr) {
        OutputDebugString(L"Device Cleared/Invalid\n");
    }
    Microsoft::WRL::ComPtr<ID3D11Debug> debugDevice;
    if (SUCCEEDED(device->QueryInterface(IID_PPV_ARGS(&debugDevice))))
    {
        debugDevice->ReportLiveDeviceObjects(D3D11_RLDO_DETAIL);
    }
    else
    {
        OutputDebugString(L"Failed to get ID3D11Debug\n");
    }
#endif
}

#endif
#pragma endregion

#pragma region DX12-Rendering
#ifdef DX12

void DrawDX12(float alpha, EngineState &engine) {
    commandAllocators[frameIndex]->Reset();
    commandList->Reset(commandAllocators[frameIndex].Get(), pipelineState.Get());

    commandList->SetGraphicsRootSignature(rootSignature.Get());

    D3D12_VIEWPORT viewport = { 0.0f, 0.0f, static_cast<float>(engine.windowWidth), static_cast<float>(engine.windowHeight), 0.0f, 1.0f };
    commandList->RSSetViewports(1, &viewport);

    D3D12_RECT scissorRect = { 0, 0, static_cast<LONG>(engine.windowWidth), static_cast<LONG>(engine.windowHeight) };
    commandList->RSSetScissorRects(1, &scissorRect);

    D3D12_RESOURCE_BARRIER barrier = {};
    barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
    barrier.Transition.pResource = renderTargets[frameIndex].Get();
    barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_PRESENT;
    barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;
    barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    commandList->ResourceBarrier(1, &barrier);

    D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle(rtvHeap->GetCPUDescriptorHandleForHeapStart());
    rtvHandle.ptr += static_cast<unsigned long long>(frameIndex) * rtvDescriptorSize;
    commandList->OMSetRenderTargets(1, &rtvHandle, FALSE, nullptr);

    const float clearColor[] = { 0.05f, 0.05f, 0.05f, 1.0f };
    commandList->ClearRenderTargetView(rtvHandle, clearColor, 0, nullptr);

    commandList->SetGraphicsRootConstantBufferView(0, constantBuffer->GetGPUVirtualAddress());

    std::vector<Vertex> vertices = DrawGame(alpha, engine);

    // Upload vertex data
    void* pVertexDataBegin;
    D3D12_RANGE readRange = { 0, 0 };
    vertexBuffer->Map(0, &readRange, &pVertexDataBegin);
    memcpy(pVertexDataBegin, vertices.data(), vertices.size() * sizeof(Vertex));
    vertexBuffer->Unmap(0, nullptr);

    commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    commandList->IASetVertexBuffers(0, 1, &vertexBufferView);
    commandList->DrawInstanced(static_cast<uint32_t>(vertices.size()), 1, 0, 0);

    barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
    barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_PRESENT;
    commandList->ResourceBarrier(1, &barrier);

    commandList->Close();

    ID3D12CommandList* ppCommandLists[] = { commandList.Get() };
    commandQueue->ExecuteCommandLists(_countof(ppCommandLists), ppCommandLists);

    swapChain->Present(1, 0);

    MoveToNextFrameDX12();
}

void UpdateConstantBufferDX12(EngineState &engine) {
    DirectX::XMMATRIX proj = DirectX::XMMatrixOrthographicOffCenterLH(0.0f, static_cast<float>(engine.windowWidth), static_cast<float>(engine.windowHeight), 0.0f, 0.0f, 1.0f);

    ConstantBuffer cb{};
    XMStoreFloat4x4(&cb.projection, XMMatrixTranspose(proj));

    void* pData;
    D3D12_RANGE readRange = { 0, 0 };
    constantBuffer->Map(0, &readRange, &pData);
    memcpy(pData, &cb, sizeof(cb));
    constantBuffer->Unmap(0, nullptr);
}

void WaitForGpuDX12() {
    const uint64_t fenceValue = fenceValues[frameIndex];
    commandQueue->Signal(fence.Get(), fenceValue);
    fence->SetEventOnCompletion(fenceValue, fenceEvent);
    WaitForSingleObject(fenceEvent, INFINITE);
}

void MoveToNextFrameDX12() {
    const uint64_t currentFenceValue = fenceValues[frameIndex];
    commandQueue->Signal(fence.Get(), currentFenceValue);

    frameIndex = swapChain->GetCurrentBackBufferIndex();

    if (fence->GetCompletedValue() < fenceValues[frameIndex]) {
        fence->SetEventOnCompletion(fenceValues[frameIndex], fenceEvent);
        WaitForSingleObject(fenceEvent, INFINITE);
    }

    fenceValues[frameIndex] = currentFenceValue + 1;
}

void ReportLeaksDX12()
{
#ifdef _DEBUG
    Microsoft::WRL::ComPtr<ID3D12DebugDevice> debugDevice;
    if (SUCCEEDED(device->QueryInterface(IID_PPV_ARGS(&debugDevice))))
    {
        debugDevice->ReportLiveDeviceObjects(D3D12_RLDO_DETAIL | D3D12_RLDO_IGNORE_INTERNAL);
    }
    else
    {
        OutputDebugString(L"Failed to get ID3D12DebugDevice\n");
    }
#endif
}

#endif
#pragma endregion
