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


### Screenshot:
<img width="802" height="632" alt="image" src="https://github.com/user-attachments/assets/d59663ef-c18d-410c-98b0-ab3cacf10eba" />
