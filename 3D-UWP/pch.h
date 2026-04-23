#pragma once

// Trim Windows headers
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#define NODRAWTEXT
#define NOBITMAP
#define NOMCX
#define NOSERVICE
#define NOHELP

// ── C++/WinRT / UWP ──────────────────────────────────────────────
#include <winrt/Windows.ApplicationModel.Core.h>
#include <winrt/Windows.UI.Core.h>
#include <winrt/Windows.UI.ViewManagement.h>
#include <winrt/Windows.Gaming.Input.h>
#include <winrt/Windows.Graphics.Display.h>
#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Foundation.Collections.h>
#include <winrt/Windows.System.h>

// ── DirectX 12 ───────────────────────────────────────────────────
#include <d3d12.h>
#include <dxgi1_6.h>
#include <d3dcompiler.h>
#include <DirectXMath.h>
#include <wrl/client.h>

// ── STL ──────────────────────────────────────────────────────────
#include <vector>
#include <string>
#include <random>
#include <chrono>
#include <cinttypes>
#include <cmath>

// ── Libs ─────────────────────────────────────────────────────────
// Note: UWP links against WindowsApp.lib (set in project props).
// DX libs are still explicit.
#pragma comment(lib, "d3d12.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "d3dcompiler.lib")
