An implementation of snake in dx11 and dx12

The dx12 renderer is a bit flickery, so the dx11 one is better

debug builds allocate a console and write to a log file, and write any dx resource leaks to vs ide console


```
# DX11 
$ msbuild .\snakedx.slnx -p:Configuration=Release -p:CustomDefines="DX11=1"
# DX12
$ msbuild .\snakedx.slnx -p:Configuration=Release -p:CustomDefines="DX12=1"
```


<img width="802" height="632" alt="image" src="https://github.com/user-attachments/assets/d59663ef-c18d-410c-98b0-ab3cacf10eba" />
