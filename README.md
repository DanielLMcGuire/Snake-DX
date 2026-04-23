## Snake (C++20, dx11/dx12)

### Build:
```batch
REM DX11 
msbuild .\snakedx.slnx -p:Configuration=Release -p:CustomDefines="DX11=1"
REM DX12
msbuild .\snakedx.slnx -p:Configuration=Release -p:CustomDefines="DX12=1"
```

> [!NOTE]
> The dx12 renderer does flicker at times
> 
> UWP version is DX12 only
>
> Building UWP build will require a self signed cert (setup in VS > appxmanifest > Packaging tab)

### Features:
- Variable tick rate, increases as score goes up
- Fixed (VSYNC) framerate using interpolation
- 3D (using the same 2D engine with some tweaks, see below)
- win32 debug builds will show and log to a console; UWP builds will log to the IDE

### Screenshots:
SNAKE2D:
<img width="770" height="752" alt="image" src="https://github.com/user-attachments/assets/72919270-d899-494b-a73f-052e7841596f" />
SNAKE3D:
<img width="802" height="632" alt="image" src="https://github.com/user-attachments/assets/6fc487f7-c3b3-4cba-9a7a-aa2aaf1b424c" />
SNAKE3D (UWP):
<img width="1920" height="1080" alt="Xbox Series S ScreenShot" src="https://github.com/user-attachments/assets/468b9f20-39d4-471b-9f84-d5587f1016ca" />


