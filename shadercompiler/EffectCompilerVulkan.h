// Copyright © 2026 CCP ehf.

#pragma once

#if SHADERCOMPILER_WITH_DXC
#include "EffectCompilerBase.h"

struct ID3D12ShaderReflection;

// Compiles effects for the Vulkan platform (TRINITY_VULKAN). The HLSL front end is the DX12 one (register spaces,
// shader model 6 profiles); each stage is compiled by dxc twice from the same source: to SPIR-V, which is the shipped
// bytecode, and to DXIL, whose D3D12 reflection fills the stage signature exactly as for DX12. Bindings follow the
// convention in the superproject's docs/vulkan-backend-plan.md: descriptor set = register space, binding = register +
// 0 (b), 32 (s), 64 (t), 128 (u).
class EffectCompilerVulkan : public EffectCompilerBase
{
public:
	bool Create() override;
	bool CompileEffect( const char* source, size_t sourceLength, const std::vector<Macro>& defines, EffectData& result, class IWorkQueue* workQueue ) override;

	// dxc arguments shared by the SPIR-V and DXIL compiles, and the SPIR-V-only ones (exposed for tests and tools).
	static std::vector<std::wstring> CommonArguments( const std::string& entryPoint, const std::string& profile );
	static std::vector<std::wstring> SpirvArguments();

private:
	struct CompiledStage
	{
		std::mutex mutex;
		std::condition_variable conditionVariable;
		bool done = false;
		bool succeeded = false;
		std::string spirv;
		CComPtr<ID3D12ShaderReflection> reflection;
	};

	bool CompileStage( const std::string& code, const std::string& entryPoint, const std::string& profile, CompiledStage& out );

	std::unordered_map<std::string, std::shared_ptr<CompiledStage>> m_compiled;
	std::mutex m_compiledCS;
};
#endif
