## Snake (dx11/dx12, win32/UWP, 2D/3D)

[Binaries](https://github.com/DanielLMcGuire/Snake-DX/releases/latest)

> [!NOTE]
> UWP support is 3D only and DX12 only
>
> UWP will require a self signed cert to build (setup in VS > appxmanifest > Packaging tab)

### Features:
- Variable tick rate, increases as score goes up
- Fixed (VSYNC) framerate using interpolation
- Debugging
  - win32 debug builds will allocate a console and log to it and a file
  - UWP builds will log to the VS IDE
- 3D
  -  Orthographic views (to avoid making a 3D engine)
  -  Info panel (just score for now)

### Bindings:
| Control | Xbox Controller (UWP only) | Mouse / KB | 
|---|---|---|
| Move 2D (X,Y) | `DPAD`/`LS` | WASD / Arrow keys | 
| Move 3D forwards (Z-) | `LB` | `Q` |
| Move 3D backwards (Z+) | `RB` | `E` | 
| Pause | `≡` | `Escape` |
| Reset | `⧉` | N/A |
| Fullscreen | N/A | `F11` / `Alt`+`Enter` |

### Build: (MSVC Only, C++20)
|Option|Details|
|---|---|
|-p:CustomDefines="[DX12,DX11]=1"|DirectX (win32 only)|
|-p:Configuration=[Release,Debug]|Build Config|
|-t:[Snake2D,Snake3D,Snake3DUWP]|Project|

```batch
msbuild .\snakedx.slnx <OPTIONS>
```

### Screenshots:
SNAKE2D:
<img width="770" height="752" alt="image" src="https://github.com/user-attachments/assets/72919270-d899-494b-a73f-052e7841596f" />
SNAKE3D:
<img width="802" height="632" alt="image" src="https://github.com/user-attachments/assets/6fc487f7-c3b3-4cba-9a7a-aa2aaf1b424c" />
SNAKE3D (UWP):
<img width="1920" height="1080" alt="Xbox Series S ScreenShot" src="https://github.com/user-attachments/assets/468b9f20-39d4-471b-9f84-d5587f1016ca" />


