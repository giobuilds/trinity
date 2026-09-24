# TrinityAL Vulkan backend

Work in progress (Linux, Vulkan 1.3). The plan, decisions and phases live in the Linux-port superproject:
`docs/vulkan-backend-plan.md`.

Current state (phase 0): every file here started as a copy of `../stub` with the class and file suffix changed
to `Vulkan`, so the platform builds, links and runs end to end while the real implementation replaces it class
by class in phase 2. Deliberate differences from the stub so far:

- `Tr2VideoAdapterInfo::GetAdapterCount` reports no adapters, so GPU tests skip instead of passing against a
  backend that does nothing.
- `TRINITY_PLATFORM_IS_LOW_PERFORMACE` is 0 (desktop GPUs).

Build with `BUILD_VULKAN=ON` (vcpkg feature `vulkan`: volk, SDL3, directx-dxc). Tests: `TrinityALTest_vulkan`,
whose shaders are the DX12 test HLSL compiled by dxc to SPIR-V with bindings b: 0, s: 32, t: 64, u: 128 and
descriptor set = register space.
