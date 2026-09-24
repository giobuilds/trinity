// Copyright © 2026 CCP ehf.

#if SHADERCOMPILER_WITH_DXC

#include "TesingUtils.h"
#include "EffectCompilerVulkan.h"
#include "StringTable.h"

#include <cstdio>
#include <fstream>
#include <regex>
#include <set>

extern StringTable g_stringTable;

namespace
{

// A legacy (fxc-era) effect in the style the engine's effects are written in: sampler_state, tex2D, POSITION/COLOR
// semantics, an implicit global and an explicit constant buffer, and two passes with render states.
const char* s_texturedEffect = R"SRC(
cbuffer PerObject : register( b3 )
{
	float4x4 WorldViewProjection;
	float4 Tint;
}

float Exposure;

texture DiffuseMap;
sampler DiffuseSampler = sampler_state
{
	Texture = <DiffuseMap>;
	MinFilter = Linear;
	MagFilter = Linear;
	MipFilter = Linear;
	AddressU = Wrap;
	AddressV = Wrap;
};

struct VS_OUT
{
	float4 Position : POSITION;
	float2 TexCoord : TEXCOORD0;
	float3 Normal : TEXCOORD1;
	float4 Color : COLOR0;
};

VS_OUT vs( float3 pos : POSITION, float2 uv : TEXCOORD0, float3 n : NORMAL )
{
	VS_OUT o;
	o.Position = mul( float4( pos, 1 ), WorldViewProjection );
	o.TexCoord = uv;
	o.Normal = n;
	o.Color = Tint;
	return o;
}

float4 ps( VS_OUT i ) : COLOR0
{
	return tex2D( DiffuseSampler, i.TexCoord ) * i.Color * Exposure * saturate( i.Normal.z );
}

float4 psColorOnly( float4 color : COLOR0 ) : COLOR0
{
	return color;
}

technique Main
{
	pass Textured
	{
		ZEnable = true;
		CullMode = CCW;
		vertexshader = compile vs_3_0 vs();
		pixelshader = compile ps_3_0 ps();
	}
	pass ColorOnly
	{
		AlphaBlendEnable = true;
		vertexshader = compile vs_3_0 vs();
		pixelshader = compile ps_3_0 psColorOnly();
	}
}
)SRC";

std::string StageBytecode( const StageInput& stage )
{
	return std::string( g_stringTable.GetString( stage.shaderDataStr ), stage.shaderSize );
}

// Binding a register lands on under the Vulkan compiler's convention (see EffectCompilerVulkan.h).
uint32_t ExpectedBinding( const RegisterInputDescription& reg )
{
	if( reg.registerType == RT_CONSTANT_BUFFER )
	{
		return reg.registerIndex;
	}
	if( reg.registerType == RT_SAMPLER )
	{
		return 32 + reg.registerIndex;
	}
	if( reg.registerType & RT_UAV_BUFFER )
	{
		return 128 + reg.registerIndex;
	}
	return 64 + reg.registerIndex; // SRVs
}

bool HaveTool( const char* tool )
{
	return std::system( ( std::string( "command -v " ) + tool + " >/dev/null 2>&1" ).c_str() ) == 0;
}

std::string RunTool( const std::string& command )
{
	std::string output;
	if( FILE* pipe = popen( command.c_str(), "r" ) )
	{
		char buffer[4096];
		size_t n;
		while( ( n = fread( buffer, 1, sizeof( buffer ), pipe ) ) > 0 )
		{
			output.append( buffer, n );
		}
		pclose( pipe );
	}
	return output;
}

} // namespace


TEST( VulkanCompiler, CompilesLegacyEffectToSpirv )
{
	auto data = Compile<EffectCompilerVulkan>( s_texturedEffect );
	ASSERT_EQ( data.techniques.size(), 1u );
	ASSERT_EQ( data.techniques[0].passes.size(), 2u );
	for( auto& pass : data.techniques[0].passes )
	{
		ASSERT_EQ( pass.stages.size(), 2u );
		for( auto& stage : pass.stages )
		{
			auto code = StageBytecode( stage );
			ASSERT_GE( code.size(), 20u );
			uint32_t magic;
			memcpy( &magic, code.data(), 4 );
			EXPECT_EQ( magic, 0x07230203u ) << "stage bytecode is not SPIR-V";
		}
	}
	EXPECT_FALSE( data.techniques[0].passes[0].states.empty() ) << "render states were not recorded";
}

TEST( VulkanCompiler, FillsSignatureFromReflection )
{
	auto data = Compile<EffectCompilerVulkan>( s_texturedEffect );
	auto& pass = data.techniques[0].passes[0];
	auto& vs = pass.stages[0];
	auto& ps = pass.stages[1];

	bool hasPerObject = false;
	for( auto& reg : vs.registerInputs )
	{
		hasPerObject |= reg.registerType == RT_CONSTANT_BUFFER && reg.registerIndex == 3;
	}
	EXPECT_TRUE( hasPerObject ) << "vertex shader signature is missing cbuffer PerObject at b3";

	EXPECT_FALSE( ps.textures.empty() ) << "pixel shader signature has no texture";
	// sampler_state samplers become static samplers, as on DX12 (immutable samplers in the Vulkan set layout).
	ASSERT_EQ( ps.staticSamplers.size(), 1u ) << "the sampler_state sampler should be a static sampler";
	EXPECT_EQ( ps.staticSamplers[0].registerIndex, 0u );

	bool hasExposure = false;
	for( auto& constant : ps.constants )
	{
		hasExposure |= strcmp( g_stringTable.GetString( constant.name ), "Exposure" ) == 0;
	}
	EXPECT_TRUE( hasExposure ) << "implicit global Exposure is not among the pixel shader constants";
}

TEST( VulkanCompiler, SpirvValidatesAndFollowsBindingConvention )
{
	if( !HaveTool( "spirv-val" ) || !HaveTool( "spirv-dis" ) )
	{
		GTEST_SKIP() << "spirv-val/spirv-dis (SPIRV-Tools) not on PATH";
	}
	auto data = Compile<EffectCompilerVulkan>( s_texturedEffect );
	std::regex bindingDecoration( "OpDecorate %([A-Za-z0-9_]+) Binding ([0-9]+)" );
	std::regex setDecoration( "OpDecorate %([A-Za-z0-9_]+) DescriptorSet ([0-9]+)" );
	int stageIndex = 0;
	for( auto& pass : data.techniques[0].passes )
	{
		for( auto& stage : pass.stages )
		{
			std::string path = "/tmp/vulkan_compiler_test_" + std::to_string( getpid() ) + "_" + std::to_string( stageIndex++ ) + ".spv";
			{
				std::ofstream file( path, std::ios::binary );
				auto code = StageBytecode( stage );
				file.write( code.data(), std::streamsize( code.size() ) );
			}
			auto validation = RunTool( "spirv-val --target-env vulkan1.3 " + path + " 2>&1; echo status=$?" );
			EXPECT_NE( validation.find( "status=0" ), std::string::npos ) << validation;

			auto disassembly = RunTool( "spirv-dis --raw-id " + path );
			std::remove( path.c_str() );

			std::map<std::string, uint32_t> bindings, sets;
			for( std::sregex_iterator it( disassembly.begin(), disassembly.end(), bindingDecoration ), end; it != end; ++it )
			{
				bindings[( *it )[1]] = uint32_t( std::stoul( ( *it )[2] ) );
			}
			for( std::sregex_iterator it( disassembly.begin(), disassembly.end(), setDecoration ), end; it != end; ++it )
			{
				sets[( *it )[1]] = uint32_t( std::stoul( ( *it )[2] ) );
			}
			std::set<std::pair<uint32_t, uint32_t>> actual;
			for( auto& [id, binding] : bindings )
			{
				actual.insert( { sets[id], binding } );
			}
			std::set<std::pair<uint32_t, uint32_t>> expected;
			for( auto& reg : stage.registerInputs )
			{
				expected.insert( { reg.registerSpace, ExpectedBinding( reg ) } );
			}
			for( auto& sampler : stage.staticSamplers ) // immutable samplers still occupy a binding
			{
				expected.insert( { sampler.registerSpace, 32u + sampler.registerIndex } );
			}
			EXPECT_EQ( actual, expected ) << "descriptor (set, binding) pairs in SPIR-V differ from the signature's registers";
		}
	}
}

TEST( VulkanCompiler, RejectsStageInterfacesWhoseLocationsWouldDiffer )
{
	// The pixel shader reads only TEXCOORD1, which is the third of the vertex outputs alphabetically: its location would
	// be 0 in the pixel shader but 2 in the vertex shader.
	const char* src = R"SRC(
struct VS_OUT
{
	float4 Position : POSITION;
	float2 TexCoord : TEXCOORD0;
	float3 Normal : TEXCOORD1;
	float4 Color : COLOR0;
};

VS_OUT vs( float3 pos : POSITION )
{
	VS_OUT o = (VS_OUT)0;
	o.Position = float4( pos, 1 );
	return o;
}

float4 ps( float3 n : TEXCOORD1 ) : COLOR0
{
	return float4( n, 1 );
}

technique t0
{
	pass p0
	{
		vertexshader = compile vs_3_0 vs();
		pixelshader = compile ps_3_0 ps();
	}
}
)SRC";
	EXPECT_FALSE( Compiles<EffectCompilerVulkan>( src ) );
}

TEST( VulkanCompiler, CompilesComputeShaders )
{
	const char* src = R"SRC(
RWStructuredBuffer<float> Output : register( u0 );
StructuredBuffer<float> Input : register( t1 );

[numthreads( 64, 1, 1 )]
void cs( uint3 id : SV_DispatchThreadID )
{
	Output[id.x] = Input[id.x] * 2;
}

technique t0
{
	pass p0
	{
		computeshader = compile cs_5_0 cs();
	}
}
)SRC";
	auto data = Compile<EffectCompilerVulkan>( src );
	auto& stage = data.techniques[0].passes[0].stages[0];
	EXPECT_EQ( stage.threadGroupSize[0], 64u );
	EXPECT_EQ( stage.threadGroupSize[1], 1u );
	EXPECT_FALSE( stage.uavs.empty() );
}

#endif
