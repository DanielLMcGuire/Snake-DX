#include "pch.h"

using namespace winrt;
using namespace winrt::Windows::ApplicationModel::Core;
using namespace winrt::Windows::UI::Core;
using namespace winrt::Windows::UI::ViewManagement;
using namespace winrt::Windows::Gaming::Input;
using namespace winrt::Windows::Graphics::Display;
using namespace winrt::Windows::System;
using namespace DirectX;
#pragma region Data

std::mt19937 rng;

#ifdef TRIPLE_BUFFER
constexpr int FRAME_COUNT = 3;
#else
constexpr int FRAME_COUNT = 2;
#endif

constexpr int GRID_X = 22;
constexpr int GRID_Y = 22;
constexpr int GRID_Z = 22;
constexpr int MAX_SNAKE = (18 * 18 * 18);

constexpr int time1 = 170; // ms per tick: score  0‥threshold
constexpr int time2 = 154; //               score  threshold‥2×threshold
constexpr int time3 = 113; //               score > 2×threshold
constexpr int timeThreshold = 6;

#pragma endregion

#pragma region Types

struct Int3 {
    int x, y, z;
    constexpr Int3() noexcept : x(0), y(0), z(0) {}
    constexpr Int3(int x_, int y_, int z_) noexcept : x(x_), y(y_), z(z_) {}
    constexpr bool operator==(const Int3& o) const noexcept { return x==o.x && y==o.y && z==o.z; }
    constexpr Int3 operator+(const Int3& o) const noexcept { return {x+o.x, y+o.y, z+o.z}; }
    constexpr Int3 operator-(const Int3& o) const noexcept { return {x-o.x, y-o.y, z-o.z}; }
};

struct Float3 {
    float x, y, z;
    constexpr Float3() noexcept : x(0), y(0), z(0) {}
    constexpr Float3(float x_, float y_, float z_) noexcept : x(x_), y(y_), z(z_) {}
};

struct Float4 {
    float x, y, z, w;
    constexpr Float4() noexcept : x(0), y(0), z(0), w(0) {}
    constexpr Float4(float x_, float y_, float z_, float w_) noexcept : x(x_), y(y_), z(z_), w(w_) {}
    constexpr Float4(const Float3& v, float w_) noexcept : x(v.x), y(v.y), z(v.z), w(w_) {}
};

struct Segment { Int3 position; };
struct Corner  { Int3 position; int segments_remaining = 0; };
struct Vertex  { Float3 position; Float4 color; };
struct ConstantBuffer { XMFLOAT4X4 projection; };

#pragma endregion

#pragma region State

struct ViewLayout {
    float px = 0, py = 0;      // pixel top-left
    float pw = 0, ph = 0;      // pixel size
    float cell_px = 1.0f;      // cell size in pixels
    float off_x = 0, off_y = 0;// centering offset within viewport
    int   cols = 1, rows = 1;  // grid cell count
};

struct SnakeGameState {
    Segment snake[MAX_SNAKE];
    size_t  snake_length = 5;
    Segment prev_snake[MAX_SNAKE];
    size_t  prev_snake_length = 5;
    Corner  corners[MAX_SNAKE];
    size_t  corner_count = 0;

    int food_x = 0, food_y = 0, food_z = 0;
    int score  = 0;
    int dir_x  = 1, dir_y = 0, dir_z = 0;
};

struct EngineState {
    bool is_first = true;
    bool running  = true;
    bool quit     = false;
    bool paused   = false;

    uint32_t windowWidth  = 800;
    uint32_t windowHeight = 600;

    SnakeGameState* game = nullptr;

    // Three orthographic views + info panel
    ViewLayout view_top;    // top-left  : X-Z plane, looking down  (-Y)
    ViewLayout view_front;  // top-right : X-Y plane, looking front (-Z)
    ViewLayout view_side;   // bot-left  : Z-Y plane, looking side  (+X)
    ViewLayout view_info;   // bot-right : score / controls
};

#pragma endregion

#pragma region DirectX Shaders

const std::string DX_SHADER = R"(
    cbuffer ConstantBuffer : register(b0)
    {
        float4x4 projection;
    };

    struct VSInput  { float3 position : POSITION;  float4 color : COLOR; };
    struct PSInput  { float4 position : SV_POSITION; float4 color : COLOR; };

    PSInput VSMain(VSInput input)
    {
        PSInput o;
        o.position = mul(float4(input.position, 1.0f), projection);
        o.color    = input.color;
        return o;
    }

    float4 PSMain(PSInput input) : SV_TARGET { return input.color; }
)";

#pragma endregion

#pragma region DirectX 12 State

Microsoft::WRL::ComPtr<ID3D12Device>              device;
Microsoft::WRL::ComPtr<ID3D12CommandQueue>         commandQueue;
Microsoft::WRL::ComPtr<IDXGISwapChain3>            swapChain;
Microsoft::WRL::ComPtr<ID3D12DescriptorHeap>       rtvHeap;
Microsoft::WRL::ComPtr<ID3D12DescriptorHeap>       cbvHeap;
Microsoft::WRL::ComPtr<ID3D12Resource>             renderTargets[FRAME_COUNT];
Microsoft::WRL::ComPtr<ID3D12CommandAllocator>     commandAllocators[FRAME_COUNT];
Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList>  commandList;
Microsoft::WRL::ComPtr<ID3D12RootSignature>        rootSignature;
Microsoft::WRL::ComPtr<ID3D12PipelineState>        pipelineState;
Microsoft::WRL::ComPtr<ID3D12Resource>             vertexBuffer;
Microsoft::WRL::ComPtr<ID3D12Resource>             constantBuffer;
D3D12_VERTEX_BUFFER_VIEW                           vertexBufferView;

uint32_t rtvDescriptorSize = 0;
uint32_t frameIndex        = 0;
HANDLE   fenceEvent        = nullptr;
Microsoft::WRL::ComPtr<ID3D12Fence> fence;
uint64_t fenceValues[FRAME_COUNT]   = {};

#pragma endregion

#pragma region Logging

#ifdef _DEBUG
#define LOG(msg) OutputDebugStringW(L"[Snake3D] " msg L"\n")
#else
#define LOG(msg) ((void)0)
#endif

#pragma endregion

#pragma region Forward Declarations

void InitGame(EngineState& engine);
void UpdateGame(EngineState& engine);
void PlaceFood(SnakeGameState& game);
std::vector<Vertex> DrawGame(float alpha, EngineState& engine);
static void DrawOrthoView(std::vector<Vertex>& verts, const ViewLayout& vl,
                          const SnakeGameState& game, int hAxis, int vAxis, int dAxis,
                          Float4 borderColor, float alpha);
static void DrawInfoPanel(std::vector<Vertex>& verts, const ViewLayout& vl,
                          const SnakeGameState& game);
void CleanupGame(EngineState& engine);

void RecalcScale(uint32_t width, uint32_t height, EngineState& engine);
void AddRect(std::vector<Vertex>& v, float x, float y, float w, float h, Float4 color);

void InitDX12(EngineState& engine, CoreWindow const& window);
void CreateRootSignatureDX12();
void CreatePipelineStateDX12();
void CreateVertexBufferDX12();
void CreateConstantBufferDX12();
void CleanupDX12();
void DrawDX12(float alpha, EngineState& engine);
void UpdateConstantBufferDX12(EngineState& engine);
void WaitForGpuDX12();
void MoveToNextFrameDX12();
void ResizeDX12(EngineState& engine, uint32_t width, uint32_t height);

#pragma endregion

#pragma region Controller

// Controller mapping (Xbox):
//   D-Pad Up/Down/Left/Right  -> XY movement  (matches keyboard W/S/A/D)
//   Left Thumbstick           -> XY movement  (alternative; direction-latched)
//   LB (Left Shoulder)        -> Z toward viewer  (was: Q)
//   RB (Right Shoulder)       -> Z away from viewer (was: E)
//   Menu (≡)                  -> Pause / Unpause
//   View (⧉)                 -> Restart after game over (quit current round)

struct ControllerState {
    GamepadButtons prevButtons   = GamepadButtons::None;
    int            prevStickDirX = 0; // -1 / 0 / +1 (for latching)
    int            prevStickDirY = 0;
};
static ControllerState g_ctrl;

// C++/WinRT generates operator& for [Flags] enums
static bool HasFlag(GamepadButtons val, GamepadButtons flag) {
    return (val & flag) == flag;
}

void PollController(EngineState& engine) {
    auto gamepads = Gamepad::Gamepads();
    if (gamepads.Size() == 0) return;

    GamepadReading rd = gamepads.GetAt(0).GetCurrentReading();
    GamepadButtons newBtns = rd.Buttons & ~g_ctrl.prevButtons;
    g_ctrl.prevButtons = rd.Buttons;

    SnakeGameState* g = engine.game;

    if (HasFlag(newBtns, GamepadButtons::Menu))
        engine.paused = !engine.paused;

    if (HasFlag(newBtns, GamepadButtons::View))
        engine.running = false;

    if (engine.paused) return;

    if (HasFlag(newBtns, GamepadButtons::DPadUp)    && g->dir_y !=  1) { g->dir_x=0;  g->dir_y=-1; g->dir_z=0; }
    if (HasFlag(newBtns, GamepadButtons::DPadDown)  && g->dir_y != -1) { g->dir_x=0;  g->dir_y= 1; g->dir_z=0; }
    if (HasFlag(newBtns, GamepadButtons::DPadLeft)  && g->dir_x !=  1) { g->dir_x=-1; g->dir_y=0;  g->dir_z=0; }
    if (HasFlag(newBtns, GamepadButtons::DPadRight) && g->dir_x != -1) { g->dir_x= 1; g->dir_y=0;  g->dir_z=0; }

    if (HasFlag(newBtns, GamepadButtons::LeftShoulder)  && g->dir_z !=  1) { g->dir_x=0; g->dir_y=0; g->dir_z=-1; }
    if (HasFlag(newBtns, GamepadButtons::RightShoulder) && g->dir_z != -1) { g->dir_x=0; g->dir_y=0; g->dir_z= 1; }

    // UWP: LeftThumbstickY = +1 when pushed up; invert so that
    // stick up -> dir_y = -1 (screen up, same as W key).
    const float DEAD = 0.5f;
    float lx =  rd.LeftThumbstickX;
    float ly = -rd.LeftThumbstickY; // invert: up on stick = move up

    if (fabsf(lx) > DEAD || fabsf(ly) > DEAD) {
        int sx, sy;
        if (fabsf(lx) >= fabsf(ly)) { sx = (lx > 0) ? 1 : -1; sy = 0; }
        else                        { sx = 0; sy = (ly > 0) ? 1 : -1; }

        if (sx != g_ctrl.prevStickDirX || sy != g_ctrl.prevStickDirY) {
            g_ctrl.prevStickDirX = sx;
            g_ctrl.prevStickDirY = sy;
            if (sx ==  1 && g->dir_x != -1) { g->dir_x= 1; g->dir_y=0; g->dir_z=0; }
            if (sx == -1 && g->dir_x !=  1) { g->dir_x=-1; g->dir_y=0; g->dir_z=0; }
            if (sy ==  1 && g->dir_y != -1) { g->dir_x=0; g->dir_y= 1; g->dir_z=0; }
            if (sy == -1 && g->dir_y !=  1) { g->dir_x=0; g->dir_y=-1; g->dir_z=0; }
        }
    } else {
        g_ctrl.prevStickDirX = 0;
        g_ctrl.prevStickDirY = 0;
    }
}

#pragma endregion

#pragma region App  (UWP IFrameworkView)

struct App : winrt::implements<App, IFrameworkViewSource, IFrameworkView>
{
    EngineState     engine;
    SnakeGameState  game;
    CoreWindow      m_window { nullptr };
    bool     m_pendingResize = false;
    uint32_t m_pendingW      = 0;
    uint32_t m_pendingH      = 0;

    IFrameworkView CreateView() { return *this; }

    void Initialize(CoreApplicationView const&) {}

    void SetWindow(CoreWindow const& window)
    {
        m_window   = window;
        engine.game = &game;

        window.KeyDown({ this, &App::OnKeyDown });
        window.SizeChanged({ this, &App::OnSizeChanged });

        DisplayInformation::GetForCurrentView().DpiChanged(
            { this, &App::OnDpiChanged });
    }

    void Load(winrt::hstring const&) {}

    void Run()
    {
        {
            auto b   = m_window.Bounds();
            auto dpi = DisplayInformation::GetForCurrentView().RawPixelsPerViewPixel();
            engine.windowWidth  = static_cast<uint32_t>(b.Width  * dpi);
            engine.windowHeight = static_cast<uint32_t>(b.Height * dpi);
        }

        InitDX12(engine, m_window);
        CreateRootSignatureDX12();
        CreatePipelineStateDX12();
        CreateVertexBufferDX12();
        CreateConstantBufferDX12();

        winrt::check_hresult(device->CreateCommandList(
            0, D3D12_COMMAND_LIST_TYPE_DIRECT,
            commandAllocators[frameIndex].Get(),
            pipelineState.Get(),
            IID_PPV_ARGS(&commandList)));
        commandList->Close();

        winrt::check_hresult(device->CreateFence(
            0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&fence)));
        fenceEvent = CreateEvent(nullptr, FALSE, FALSE, nullptr);
        if (!fenceEvent) winrt::throw_last_error();
        for (uint32_t i = 0; i < FRAME_COUNT; i++) fenceValues[i] = 1;

        m_window.Activate();

    restart:
        InitGame(engine);

        using clock    = std::chrono::steady_clock;
        using ns       = std::chrono::nanoseconds;
        auto  lastTime = clock::now();
        ns    accumulator{ 0 };
        float alpha = 0.0f;

        while (engine.running) {
            m_window.Dispatcher().ProcessEvents(
                CoreProcessEventsOption::ProcessAllIfPresent);

            if (m_pendingResize) {
                m_pendingResize = false;
                ResizeDX12(engine, m_pendingW, m_pendingH);
            }

            PollController(engine);

            if (engine.quit) break;

            auto now   = clock::now();
            auto delta = now - lastTime;
            lastTime   = now;
            if (delta > std::chrono::milliseconds(50))
                delta = std::chrono::milliseconds(50);

            if (!engine.paused) {
                accumulator += delta;

                int tickMs =
                    game.score <= timeThreshold     ? time1 :
                    game.score <= timeThreshold * 2 ? time2 : time3;
                auto tick = std::chrono::milliseconds(tickMs);

                while (accumulator >= tick) {
                    UpdateGame(engine);
                    accumulator -= tick;
                }

                alpha = static_cast<float>(
                    std::chrono::duration_cast<std::chrono::duration<float>>(accumulator).count() /
                    std::chrono::duration_cast<std::chrono::duration<float>>(tick).count());
            } else {
                alpha       = 0.0f;
                accumulator = ns{ 0 };
            }

            DrawDX12(alpha, engine);
        }

        engine.is_first = false;
        WaitForGpuDX12();
        CleanupGame(engine);

        if (!engine.quit) goto restart;

        WaitForGpuDX12();
        if (fenceEvent) CloseHandle(fenceEvent);
        CleanupDX12();
    }

    void Uninitialize() {}

    void OnKeyDown(CoreWindow const&, KeyEventArgs const& args)
    {
        VirtualKey k = args.VirtualKey();

        if (k == VirtualKey::F11) {
            auto av = ApplicationView::GetForCurrentView();
            if (av.IsFullScreenMode()) av.ExitFullScreenMode();
            else                       av.TryEnterFullScreenMode();
            return;
        }

        if (k == VirtualKey::Escape) {
            engine.paused = !engine.paused;
            return;
        }

        if (engine.paused) return;

        if ((k==VirtualKey::W || k==VirtualKey::Up)    && engine.game->dir_y != 1)  { engine.game->dir_x=0;  engine.game->dir_y=-1; engine.game->dir_z=0; }
        if ((k==VirtualKey::S || k==VirtualKey::Down)  && engine.game->dir_y != -1) { engine.game->dir_x=0;  engine.game->dir_y= 1; engine.game->dir_z=0; }
        if ((k==VirtualKey::A || k==VirtualKey::Left)  && engine.game->dir_x != 1)  { engine.game->dir_x=-1; engine.game->dir_y=0;  engine.game->dir_z=0; }
        if ((k==VirtualKey::D || k==VirtualKey::Right) && engine.game->dir_x != -1) { engine.game->dir_x= 1; engine.game->dir_y=0;  engine.game->dir_z=0; }

        if (k==VirtualKey::Q && engine.game->dir_z !=  1) { engine.game->dir_x=0; engine.game->dir_y=0; engine.game->dir_z=-1; }
        if (k==VirtualKey::E && engine.game->dir_z != -1) { engine.game->dir_x=0; engine.game->dir_y=0; engine.game->dir_z= 1; }
    }

    void OnSizeChanged(CoreWindow const&, WindowSizeChangedEventArgs const& args)
    {
        auto dpi = DisplayInformation::GetForCurrentView().RawPixelsPerViewPixel();
        m_pendingW      = static_cast<uint32_t>(args.Size().Width  * dpi);
        m_pendingH      = static_cast<uint32_t>(args.Size().Height * dpi);
        m_pendingResize = true;
    }

    void OnDpiChanged(DisplayInformation const& info, IInspectable const&)
    {
        auto b   = m_window.Bounds();
        auto dpi = info.RawPixelsPerViewPixel();
        m_pendingW      = static_cast<uint32_t>(b.Width  * dpi);
        m_pendingH      = static_cast<uint32_t>(b.Height * dpi);
        m_pendingResize = true;
    }
};

#pragma endregion

#pragma region Entry Point

int WINAPI wWinMain(HINSTANCE, HINSTANCE, PWSTR, int)
{
    winrt::init_apartment();
    CoreApplication::Run(winrt::make<App>());
    return 0;
}

#pragma endregion

#pragma region Game Logic

void InitGame(EngineState& engine) {
    SnakeGameState* g = engine.game;
    std::random_device rd;
    rng.seed(rd());

    if (engine.is_first) {
        RecalcScale(engine.windowWidth, engine.windowHeight, engine);
        UpdateConstantBufferDX12(engine);
    }

    for (size_t i = 0; i < g->snake_length; i++) {
        g->snake[i].position.x = GRID_X / 2 - static_cast<int>(i);
        g->snake[i].position.y = GRID_Y / 2;
        g->snake[i].position.z = GRID_Z / 2;
        g->prev_snake[i] = g->snake[i];
    }
    g->prev_snake_length = g->snake_length;
    g->corner_count = 0;

    PlaceFood(*g);
    g->score = 0;
    engine.running = true;
    g->dir_x = 1; g->dir_y = 0; g->dir_z = 0;
}

void UpdateGame(EngineState& engine) {
    SnakeGameState* g = engine.game;
    if (g->snake_length == 0 || g->snake_length > MAX_SNAKE)
        g->snake_length = (g->snake_length == 0) ? 1 : MAX_SNAKE;

    g->prev_snake_length = g->snake_length;
    for (size_t i = 0; i < g->snake_length; i++)
        g->prev_snake[i] = g->snake[i];

    int nhx = g->snake[0].position.x + g->dir_x;
    int nhy = g->snake[0].position.y + g->dir_y;
    int nhz = g->snake[0].position.z + g->dir_z;

    if (g->snake_length > 1) {
        int pdx = g->snake[0].position.x - g->snake[1].position.x;
        int pdy = g->snake[0].position.y - g->snake[1].position.y;
        int pdz = g->snake[0].position.z - g->snake[1].position.z;
        if (pdx != g->dir_x || pdy != g->dir_y || pdz != g->dir_z) {
            if (g->corner_count < MAX_SNAKE) {
                g->corners[g->corner_count].position          = g->snake[0].position;
                g->corners[g->corner_count].segments_remaining = static_cast<int>(g->snake_length);
                g->corner_count++;
            }
        }
        for (size_t i = g->snake_length - 1; i > 0; i--)
            g->snake[i] = g->snake[i - 1];
    }

    g->snake[0].position.x = nhx;
    g->snake[0].position.y = nhy;
    g->snake[0].position.z = nhz;

    for (size_t i = 0; i < g->corner_count; i++)
        g->corners[i].segments_remaining--;
    size_t wIdx = 0;
    for (size_t i = 0; i < g->corner_count; i++)
        if (g->corners[i].segments_remaining > 0)
            g->corners[wIdx++] = g->corners[i];
    g->corner_count = wIdx;

    if (nhx <= 0 || nhx >= GRID_X-1 || nhy <= 0 || nhy >= GRID_Y-1 || nhz <= 0 || nhz >= GRID_Z-1)
        engine.running = false;

    for (size_t i = 1; i < g->snake_length; i++) {
        if (g->snake[0].position.x == g->snake[i].position.x &&
            g->snake[0].position.y == g->snake[i].position.y &&
            g->snake[0].position.z == g->snake[i].position.z) {
            engine.running = false;
        }
    }

    if (nhx == g->food_x && nhy == g->food_y && nhz == g->food_z) {
        if (g->snake_length < MAX_SNAKE) {
            g->snake[g->snake_length] = g->snake[g->snake_length - 1];
            g->snake_length++;
            for (size_t i = 0; i < g->corner_count; i++)
                g->corners[i].segments_remaining++;
        }
        PlaceFood(*g);
        g->score++;
    }
}

void PlaceFood(SnakeGameState& g) {
    std::vector<Int3> empty;
    empty.reserve((GRID_X-2) * (GRID_Y-2) * (GRID_Z-2));

    for (int x = 1; x < GRID_X-1; x++)
    for (int y = 1; y < GRID_Y-1; y++)
    for (int z = 1; z < GRID_Z-1; z++) {
        bool occ = false;
        for (size_t i = 0; i < g.snake_length; i++) {
            if (g.snake[i].position.x==x && g.snake[i].position.y==y && g.snake[i].position.z==z)
                { occ = true; break; }
        }
        if (!occ) empty.push_back({x, y, z});
    }

    if (empty.empty()) { g.food_x = g.food_y = g.food_z = -1; return; }

    std::uniform_int_distribution<size_t> dist(0, empty.size()-1);
    Int3 c = empty[dist(rng)];
    g.food_x = c.x; g.food_y = c.y; g.food_z = c.z;
}

std::vector<Vertex> DrawGame(float alpha, EngineState& engine) {
    SnakeGameState* g = engine.game;
    std::vector<Vertex> verts;
    verts.reserve(MAX_SNAKE * 4 + 600);

    float hw = engine.windowWidth  * 0.5f;
    float hh = engine.windowHeight * 0.5f;
    Float4 sep(0.18f, 0.18f, 0.18f, 1.0f);
    AddRect(verts, hw-1.5f, 0, 3.0f, (float)engine.windowHeight, sep);
    AddRect(verts, 0, hh-1.5f, (float)engine.windowWidth, 3.0f, sep);

    DrawOrthoView(verts, engine.view_top,   *g, 0, 2, 1, {0.0f,0.75f,0.75f,1.0f}, alpha);
    DrawOrthoView(verts, engine.view_front, *g, 0, 1, 2, {0.75f,0.75f,0.0f,1.0f}, alpha);
    DrawOrthoView(verts, engine.view_side,  *g, 2, 1, 0, {0.75f,0.0f,0.75f,1.0f}, alpha);

    DrawInfoPanel(verts, engine.view_info, *g);
    return verts;
}

static void DrawOrthoView(
    std::vector<Vertex>& verts,
    const ViewLayout& vl,
    const SnakeGameState& g,
    int hAxis, int vAxis, int /*dAxis*/,
    Float4 borderColor,
    float alpha)
{
    const float cx = vl.cell_px;
    const float ox = vl.off_x;
    const float oy = vl.off_y;

    const int gridH = (hAxis==0) ? GRID_X : (hAxis==1 ? GRID_Y : GRID_Z);
    const int gridV = (vAxis==0) ? GRID_X : (vAxis==1 ? GRID_Y : GRID_Z);

    auto ax = [](const Int3& p, int a) -> int {
        return a==0 ? p.x : a==1 ? p.y : p.z;
    };

    AddRect(verts, vl.px, vl.py, vl.pw, vl.ph, {0.04f,0.04f,0.04f,1.0f});

    for (int i = 0; i < gridH; i++) {
        AddRect(verts, ox+i*cx, oy,              cx, cx, borderColor);
        AddRect(verts, ox+i*cx, oy+(gridV-1)*cx, cx, cx, borderColor);
    }
    for (int j = 1; j < gridV-1; j++) {
        AddRect(verts, ox,              oy+j*cx, cx, cx, borderColor);
        AddRect(verts, ox+(gridH-1)*cx, oy+j*cx, cx, cx, borderColor);
    }

    Float4 bodyCol(0.05f, 0.55f, 0.12f, 1.0f);
    for (size_t i = g.snake_length; i > 1; i--) {
        size_t idx = i-1;
        float ph, pv;
        if (idx < g.prev_snake_length) {
            ph = static_cast<float>(ax(g.prev_snake[idx].position, hAxis))
               + (ax(g.snake[idx].position, hAxis) - ax(g.prev_snake[idx].position, hAxis)) * alpha;
            pv = static_cast<float>(ax(g.prev_snake[idx].position, vAxis))
               + (ax(g.snake[idx].position, vAxis) - ax(g.prev_snake[idx].position, vAxis)) * alpha;
        } else {
            ph = static_cast<float>(ax(g.snake[idx].position, hAxis));
            pv = static_cast<float>(ax(g.snake[idx].position, vAxis));
        }
        AddRect(verts, ox+ph*cx, oy+pv*cx, cx, cx, bodyCol);
    }

    for (size_t i = 0; i < g.corner_count; i++) {
        float ph = static_cast<float>(ax(g.corners[i].position, hAxis));
        float pv = static_cast<float>(ax(g.corners[i].position, vAxis));
        AddRect(verts, ox+ph*cx, oy+pv*cx, cx, cx, bodyCol);
    }

    Int3 fp = { g.food_x, g.food_y, g.food_z };
    float fh = static_cast<float>(ax(fp, hAxis));
    float fv = static_cast<float>(ax(fp, vAxis));
    AddRect(verts, ox+fh*cx, oy+fv*cx, cx, cx, {0.95f,0.15f,0.15f,1.0f});

    if (g.snake_length > 0) {
        float hh2, hv;
        if (g.prev_snake_length > 0) {
            hh2 = static_cast<float>(ax(g.prev_snake[0].position, hAxis))
                + (ax(g.snake[0].position, hAxis) - ax(g.prev_snake[0].position, hAxis)) * alpha;
            hv  = static_cast<float>(ax(g.prev_snake[0].position, vAxis))
                + (ax(g.snake[0].position, vAxis) - ax(g.prev_snake[0].position, vAxis)) * alpha;
        } else {
            hh2 = static_cast<float>(ax(g.snake[0].position, hAxis));
            hv  = static_cast<float>(ax(g.snake[0].position, vAxis));
        }
        AddRect(verts, ox+hh2*cx, oy+hv*cx, cx, cx, {0.2f,1.0f,0.2f,1.0f});
    }
}

static void DrawInfoPanel(std::vector<Vertex>& verts, const ViewLayout& vl, const SnakeGameState& g)
{
    AddRect(verts, vl.px, vl.py, vl.pw, vl.ph, {0.04f,0.04f,0.04f,1.0f});
    if (vl.pw < 10.0f || vl.ph < 10.0f) return;

    const float margin  = vl.pw * 0.06f;
    const float dotSize = (vl.pw*0.04f < vl.ph*0.06f) ? vl.pw*0.04f : vl.ph*0.06f;
    const float gap     = dotSize * 0.4f;
    const int   maxCols = static_cast<int>((vl.pw - 2.0f*margin) / (dotSize+gap));
    int displayed = (g.score > maxCols*5) ? maxCols*5 : g.score;

    for (int i = 0; i < displayed; i++) {
        int col = i % maxCols;
        int row = i / maxCols;
        float dx = vl.px + margin + col*(dotSize+gap);
        float dy = vl.py + margin + row*(dotSize+gap);
        AddRect(verts, dx, dy, dotSize, dotSize, {0.0f,0.75f,0.75f,1.0f});
    }
}

void CleanupGame(EngineState& engine) {
    SnakeGameState* g = engine.game;
    for (size_t i = 0; i < g->corner_count; i++) {
        g->corners[i].position.x = g->corners[i].position.y = g->corners[i].position.z = 0;
        g->corners[i].segments_remaining = 0;
    }
    g->corner_count = 0;
    for (size_t i = 0; i < g->snake_length; i++) {
        g->snake[i].position.x = g->snake[i].position.y = g->snake[i].position.z = 0;
    }
    g->snake_length = 5;
    g->food_x = g->food_y = g->food_z = 0;
    g->score  = 0;
    g->dir_x  = g->dir_y = g->dir_z = 0;
    engine.running = false;
}

#pragma endregion

#pragma region Utils

void RecalcScale(uint32_t width, uint32_t height, EngineState& engine) {
    const float sep = 3.0f;
    const float hw  = static_cast<float>(width)  * 0.5f;
    const float hh  = static_cast<float>(height) * 0.5f;

    auto makeView = [](ViewLayout& v, float px, float py, float pw, float ph, int cols, int rows) {
        v.px=px; v.py=py; v.pw=pw; v.ph=ph; v.cols=cols; v.rows=rows;
        float sx = pw/static_cast<float>(cols);
        float sy = ph/static_cast<float>(rows);
        v.cell_px = (sx<sy) ? sx : sy;
        v.off_x   = px + (pw - v.cell_px*cols) * 0.5f;
        v.off_y   = py + (ph - v.cell_px*rows) * 0.5f;
    };

    makeView(engine.view_top,   0.f,    0.f,    hw-sep, hh-sep, GRID_X, GRID_Z);
    makeView(engine.view_front, hw+sep, 0.f,    hw-sep, hh-sep, GRID_X, GRID_Y);
    makeView(engine.view_side,  0.f,    hh+sep, hw-sep, hh-sep, GRID_Z, GRID_Y);
    makeView(engine.view_info,  hw+sep, hh+sep, hw-sep, hh-sep, 1, 1);
}

void AddRect(std::vector<Vertex>& v, float x, float y, float w, float h, Float4 color) {
    v.push_back({ {x,   y,   0.f}, color });
    v.push_back({ {x+w, y,   0.f}, color });
    v.push_back({ {x,   y+h, 0.f}, color });
    v.push_back({ {x,   y+h, 0.f}, color });
    v.push_back({ {x+w, y,   0.f}, color });
    v.push_back({ {x+w, y+h, 0.f}, color });
}

#pragma endregion

#pragma region DX12 Initialization

void InitDX12(EngineState& engine, CoreWindow const& window) {
    uint32_t factoryFlags = 0;

#ifdef _DEBUG
    Microsoft::WRL::ComPtr<ID3D12Debug> dbg;
    if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&dbg)))) {
        dbg->EnableDebugLayer();
        factoryFlags |= DXGI_CREATE_FACTORY_DEBUG;
    }
#endif

    Microsoft::WRL::ComPtr<IDXGIFactory4> factory;
    winrt::check_hresult(CreateDXGIFactory2(factoryFlags, IID_PPV_ARGS(&factory)));

    winrt::check_hresult(D3D12CreateDevice(
        nullptr,
        D3D_FEATURE_LEVEL_11_0,
        IID_PPV_ARGS(&device)));

    D3D12_COMMAND_QUEUE_DESC qd = {};
    qd.Type  = D3D12_COMMAND_LIST_TYPE_DIRECT;
    qd.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
    winrt::check_hresult(device->CreateCommandQueue(&qd, IID_PPV_ARGS(&commandQueue)));

    DXGI_SWAP_CHAIN_DESC1 scd = {};
    scd.BufferCount  = FRAME_COUNT;
    scd.Width        = engine.windowWidth;
    scd.Height       = engine.windowHeight;
    scd.Format       = DXGI_FORMAT_R8G8B8A8_UNORM;
    scd.BufferUsage  = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    scd.SwapEffect   = DXGI_SWAP_EFFECT_FLIP_DISCARD;
    scd.SampleDesc.Count = 1;
    scd.Scaling      = DXGI_SCALING_STRETCH;
    scd.AlphaMode    = DXGI_ALPHA_MODE_IGNORE;

    Microsoft::WRL::ComPtr<IDXGISwapChain1> sc1;
    winrt::check_hresult(factory->CreateSwapChainForCoreWindow(
        commandQueue.Get(),
        winrt::get_unknown(window),
        &scd,
        nullptr,
        &sc1));
    winrt::check_hresult(sc1.As(&swapChain)); // IDXGISwapChain3
    frameIndex = swapChain->GetCurrentBackBufferIndex();

    D3D12_DESCRIPTOR_HEAP_DESC rtvDesc = {};
    rtvDesc.NumDescriptors = FRAME_COUNT;
    rtvDesc.Type  = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
    rtvDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
    winrt::check_hresult(device->CreateDescriptorHeap(&rtvDesc, IID_PPV_ARGS(&rtvHeap)));
    rtvDescriptorSize = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);

    D3D12_DESCRIPTOR_HEAP_DESC cbvDesc = {};
    cbvDesc.NumDescriptors = 1;
    cbvDesc.Type  = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
    cbvDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
    winrt::check_hresult(device->CreateDescriptorHeap(&cbvDesc, IID_PPV_ARGS(&cbvHeap)));

    D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle(rtvHeap->GetCPUDescriptorHandleForHeapStart());
    for (uint32_t n = 0; n < FRAME_COUNT; n++) {
        winrt::check_hresult(swapChain->GetBuffer(n, IID_PPV_ARGS(&renderTargets[n])));
        device->CreateRenderTargetView(renderTargets[n].Get(), nullptr, rtvHandle);
        rtvHandle.ptr += rtvDescriptorSize;
        winrt::check_hresult(device->CreateCommandAllocator(
            D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&commandAllocators[n])));
    }

    LOG(L"DirectX 12 initialized (CoreWindow swap chain)");
}

void CreateRootSignatureDX12() {
    D3D12_FEATURE_DATA_ROOT_SIGNATURE fd = {};
    fd.HighestVersion = D3D_ROOT_SIGNATURE_VERSION_1_1;
    if (FAILED(device->CheckFeatureSupport(D3D12_FEATURE_ROOT_SIGNATURE, &fd, sizeof(fd))))
        fd.HighestVersion = D3D_ROOT_SIGNATURE_VERSION_1_0;

    D3D12_ROOT_PARAMETER1 rp[1] = {};
    rp[0].ParameterType    = D3D12_ROOT_PARAMETER_TYPE_CBV;
    rp[0].Descriptor.ShaderRegister = 0;
    rp[0].Descriptor.RegisterSpace  = 0;
    rp[0].Descriptor.Flags          = D3D12_ROOT_DESCRIPTOR_FLAG_NONE;
    rp[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX;

    D3D12_VERSIONED_ROOT_SIGNATURE_DESC rsd = {};
    rsd.Version               = D3D_ROOT_SIGNATURE_VERSION_1_1;
    rsd.Desc_1_1.NumParameters = _countof(rp);
    rsd.Desc_1_1.pParameters   = rp;
    rsd.Desc_1_1.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;

    Microsoft::WRL::ComPtr<ID3DBlob> sig, err;
    winrt::check_hresult(D3D12SerializeVersionedRootSignature(&rsd, &sig, &err));
    winrt::check_hresult(device->CreateRootSignature(
        0, sig->GetBufferPointer(), sig->GetBufferSize(), IID_PPV_ARGS(&rootSignature)));
}

void CreatePipelineStateDX12() {
    Microsoft::WRL::ComPtr<ID3DBlob> vs, ps, err;
    uint32_t flags = 0;
#ifdef _DEBUG
    flags = D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
#endif
    D3DCompile(DX_SHADER.c_str(), DX_SHADER.length(), nullptr, nullptr, nullptr,
               "VSMain", "vs_5_0", flags, 0, &vs, &err);
    D3DCompile(DX_SHADER.c_str(), DX_SHADER.length(), nullptr, nullptr, nullptr,
               "PSMain", "ps_5_0", flags, 0, &ps, &err);

    D3D12_INPUT_ELEMENT_DESC layout[] = {
        { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT,    0,  0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        { "COLOR",    0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }
    };

    D3D12_GRAPHICS_PIPELINE_STATE_DESC pso = {};
    pso.InputLayout    = { layout, _countof(layout) };
    pso.pRootSignature = rootSignature.Get();
    pso.VS = { vs->GetBufferPointer(), vs->GetBufferSize() };
    pso.PS = { ps->GetBufferPointer(), ps->GetBufferSize() };
    pso.RasterizerState.FillMode = D3D12_FILL_MODE_SOLID;
    pso.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
    pso.BlendState.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
    pso.DepthStencilState.DepthEnable   = FALSE;
    pso.DepthStencilState.StencilEnable = FALSE;
    pso.SampleMask             = UINT_MAX;
    pso.PrimitiveTopologyType  = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    pso.NumRenderTargets       = 1;
    pso.RTVFormats[0]          = DXGI_FORMAT_R8G8B8A8_UNORM;
    pso.SampleDesc.Count       = 1;
    winrt::check_hresult(device->CreateGraphicsPipelineState(&pso, IID_PPV_ARGS(&pipelineState)));
}

void CreateVertexBufferDX12() {
    const size_t sz = sizeof(Vertex) * 6 * (MAX_SNAKE * 4 + 1000);

    D3D12_HEAP_PROPERTIES hp = {};
    hp.Type = D3D12_HEAP_TYPE_UPLOAD;
    D3D12_RESOURCE_DESC rd = {};
    rd.Dimension          = D3D12_RESOURCE_DIMENSION_BUFFER;
    rd.Width              = sz;
    rd.Height             = rd.DepthOrArraySize = rd.MipLevels = 1;
    rd.Format             = DXGI_FORMAT_UNKNOWN;
    rd.SampleDesc.Count   = 1;
    rd.Layout             = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
    winrt::check_hresult(device->CreateCommittedResource(
        &hp, D3D12_HEAP_FLAG_NONE, &rd,
        D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&vertexBuffer)));

    vertexBufferView.BufferLocation = vertexBuffer->GetGPUVirtualAddress();
    vertexBufferView.StrideInBytes  = sizeof(Vertex);
    vertexBufferView.SizeInBytes    = static_cast<uint32_t>(sz);
}

void CreateConstantBufferDX12() {
    const size_t sz = (sizeof(ConstantBuffer) + 255) & ~255;

    D3D12_HEAP_PROPERTIES hp = {};
    hp.Type = D3D12_HEAP_TYPE_UPLOAD;
    D3D12_RESOURCE_DESC rd = {};
    rd.Dimension          = D3D12_RESOURCE_DIMENSION_BUFFER;
    rd.Width              = sz;
    rd.Height             = rd.DepthOrArraySize = rd.MipLevels = 1;
    rd.Format             = DXGI_FORMAT_UNKNOWN;
    rd.SampleDesc.Count   = 1;
    rd.Layout             = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
    winrt::check_hresult(device->CreateCommittedResource(
        &hp, D3D12_HEAP_FLAG_NONE, &rd,
        D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&constantBuffer)));
}

void CleanupDX12() {
    constantBuffer.Reset();
    vertexBuffer.Reset();
    pipelineState.Reset();
    rootSignature.Reset();
    for (uint32_t i = 0; i < FRAME_COUNT; i++) {
        renderTargets[i].Reset();
        commandAllocators[i].Reset();
    }
    commandList.Reset();
    cbvHeap.Reset();
    rtvHeap.Reset();
    swapChain.Reset();
    commandQueue.Reset();

#ifdef _DEBUG
    Microsoft::WRL::ComPtr<ID3D12DebugDevice> dbgDev;
    if (SUCCEEDED(device->QueryInterface(IID_PPV_ARGS(&dbgDev))))
        dbgDev->ReportLiveDeviceObjects(D3D12_RLDO_DETAIL | D3D12_RLDO_IGNORE_INTERNAL);
#endif
    device.Reset();
    LOG(L"DirectX 12 resources released");
}

void ResizeDX12(EngineState& engine, uint32_t width, uint32_t height) {
    if (width == 0 || height == 0) return;

    WaitForGpuDX12();

    for (uint32_t i = 0; i < FRAME_COUNT; i++) {
        renderTargets[i].Reset();
        fenceValues[i] = fenceValues[frameIndex];
    }

    winrt::check_hresult(swapChain->ResizeBuffers(
        FRAME_COUNT, width, height, DXGI_FORMAT_R8G8B8A8_UNORM, 0));
    frameIndex = swapChain->GetCurrentBackBufferIndex();

    D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle(rtvHeap->GetCPUDescriptorHandleForHeapStart());
    for (uint32_t i = 0; i < FRAME_COUNT; i++) {
        winrt::check_hresult(swapChain->GetBuffer(i, IID_PPV_ARGS(&renderTargets[i])));
        device->CreateRenderTargetView(renderTargets[i].Get(), nullptr, rtvHandle);
        rtvHandle.ptr += rtvDescriptorSize;
    }

    engine.windowWidth  = width;
    engine.windowHeight = height;
    RecalcScale(width, height, engine);
    UpdateConstantBufferDX12(engine);
}

#pragma endregion

#pragma region DX12 Rendering

void UpdateConstantBufferDX12(EngineState& engine) {
    XMMATRIX proj = XMMatrixOrthographicOffCenterLH(
        0.f, static_cast<float>(engine.windowWidth),
        static_cast<float>(engine.windowHeight), 0.f,
        0.f, 1.f);

    ConstantBuffer cb{};
    XMStoreFloat4x4(&cb.projection, XMMatrixTranspose(proj));

    void* pData;
    D3D12_RANGE rr = { 0, 0 };
    winrt::check_hresult(constantBuffer->Map(0, &rr, &pData));
    memcpy(pData, &cb, sizeof(cb));
    constantBuffer->Unmap(0, nullptr);
}

void DrawDX12(float alpha, EngineState& engine) {
    winrt::check_hresult(commandAllocators[frameIndex]->Reset());
    winrt::check_hresult(commandList->Reset(commandAllocators[frameIndex].Get(), pipelineState.Get()));

    commandList->SetGraphicsRootSignature(rootSignature.Get());

    D3D12_VIEWPORT vp = { 0, 0,
        static_cast<float>(engine.windowWidth),
        static_cast<float>(engine.windowHeight),
        0.f, 1.f };
    commandList->RSSetViewports(1, &vp);

    D3D12_RECT sr = { 0, 0,
        static_cast<LONG>(engine.windowWidth),
        static_cast<LONG>(engine.windowHeight) };
    commandList->RSSetScissorRects(1, &sr);

    // Present -> RenderTarget
    D3D12_RESOURCE_BARRIER barrier = {};
    barrier.Type  = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
    barrier.Transition.pResource   = renderTargets[frameIndex].Get();
    barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_PRESENT;
    barrier.Transition.StateAfter  = D3D12_RESOURCE_STATE_RENDER_TARGET;
    barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    commandList->ResourceBarrier(1, &barrier);

    D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle(rtvHeap->GetCPUDescriptorHandleForHeapStart());
    rtvHandle.ptr += static_cast<uint64_t>(frameIndex) * rtvDescriptorSize;
    commandList->OMSetRenderTargets(1, &rtvHandle, FALSE, nullptr);

    const float clear[] = { 0.05f, 0.05f, 0.05f, 1.0f };
    commandList->ClearRenderTargetView(rtvHandle, clear, 0, nullptr);
    commandList->SetGraphicsRootConstantBufferView(0, constantBuffer->GetGPUVirtualAddress());

    std::vector<Vertex> verts = DrawGame(alpha, engine);

    void* pVB;
    D3D12_RANGE rr = { 0, 0 };
    winrt::check_hresult(vertexBuffer->Map(0, &rr, &pVB));
    memcpy(pVB, verts.data(), verts.size() * sizeof(Vertex));
    vertexBuffer->Unmap(0, nullptr);

    commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    commandList->IASetVertexBuffers(0, 1, &vertexBufferView);
    commandList->DrawInstanced(static_cast<uint32_t>(verts.size()), 1, 0, 0);

    // RenderTarget -> Present
    barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
    barrier.Transition.StateAfter  = D3D12_RESOURCE_STATE_PRESENT;
    commandList->ResourceBarrier(1, &barrier);

    winrt::check_hresult(commandList->Close());

    ID3D12CommandList* lists[] = { commandList.Get() };
    commandQueue->ExecuteCommandLists(_countof(lists), lists);

    swapChain->Present(1, 0); // VSync
    MoveToNextFrameDX12();
}

void WaitForGpuDX12() {
    const uint64_t val = fenceValues[frameIndex];
    winrt::check_hresult(commandQueue->Signal(fence.Get(), val));
    winrt::check_hresult(fence->SetEventOnCompletion(val, fenceEvent));
    WaitForSingleObjectEx(fenceEvent, INFINITE, FALSE);
    fenceValues[frameIndex]++;
}

void MoveToNextFrameDX12() {
    const uint64_t cur = fenceValues[frameIndex];
    winrt::check_hresult(commandQueue->Signal(fence.Get(), cur));
    frameIndex = swapChain->GetCurrentBackBufferIndex();
    if (fence->GetCompletedValue() < fenceValues[frameIndex]) {
        winrt::check_hresult(fence->SetEventOnCompletion(fenceValues[frameIndex], fenceEvent));
        WaitForSingleObjectEx(fenceEvent, INFINITE, FALSE);
    }
    fenceValues[frameIndex] = cur + 1;
}

#pragma endregion
