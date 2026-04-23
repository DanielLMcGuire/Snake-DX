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


### Screenshots:
SNAKE2D:
<img width="770" height="752" alt="image" src="https://github.com/user-attachments/assets/72919270-d899-494b-a73f-052e7841596f" />
SNAKE3D:
<img width="802" height="632" alt="image" src="https://github.com/user-attachments/assets/0a7ba1d9-5afb-4572-b568-b75ab0019224" />

