## Snake (C++20, dx11/dx12)

### Build:
```batch
REM DX11 
msbuild .\snakedx.slnx -p:Configuration=Release -p:CustomDefines="DX11=1"
REM DX12
msbuild .\snakedx.slnx -p:Configuration=Release -p:CustomDefines="DX12=1"
```

> [!NOTE]
> The dx12 renderer does flicker at time
>
> debug builds allocate a console and write to a log file, and write any dx resource leaks to vs ide console
> 
> UWP version is DX12 only

### Features:
- Variable tick rate, increases as score goes up
- Fixed (VSYNC) framerate using interpolation

### Screenshots:
SNAKE2D:
<img width="770" height="752" alt="image" src="https://github.com/user-attachments/assets/72919270-d899-494b-a73f-052e7841596f" />
SNAKE3D:
<img width="802" height="632" alt="image" src="https://github.com/user-attachments/assets/6fc487f7-c3b3-4cba-9a7a-aa2aaf1b424c" />
SNAKE3D (UWP):
<img width="1920" height="1080" alt="Xbox Series S ScreenShot" src="https://github.com/user-attachments/assets/468b9f20-39d4-471b-9f84-d5587f1016ca" />


