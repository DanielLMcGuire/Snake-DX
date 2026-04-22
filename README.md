An implementation of snake in dx11 and dx12

The dx12 renderer is a bit flickery, so the dx11 one is better

debug builds allocate a console and write to a log file

```
# DX11 
$ msbuild .\snakedx.slnx -p:Configuration=Release -p:CustomDefines="DX11=1"
# DX12
$ msbuild .\snakedx.slnx -p:Configuration=Release -p:CustomDefines="DX12=1"
```
