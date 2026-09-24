// Copyright © 2026 CCP ehf.
#pragma once
// Stand-in for the Windows header that DirectX-Headers' d3d12shader.h/d3dcommon.h include on Linux. The COM base types
// come from DXC's WinAdapter.h (included by stdafx.h), so this only supplies what the D3D headers additionally expect.
// DirectX-Headers ships its own stand-ins (wsl/stubs), but those redefine WinAdapter's types.
#define __RPCNDR_H_VERSION__ 500
#ifndef interface
#define interface struct
#endif
#define CONST_VTBL
#define __CRT_UUID_DECL( ... ) // interface IDs are passed explicitly (e.g. IID_ID3D12ShaderReflection)
