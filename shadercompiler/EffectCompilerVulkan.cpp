// Copyright © 2026 CCP ehf.

#include "stdafx.h"
#if SHADERCOMPILER_WITH_DXC

// IID_ID3D12ShaderReflection is defined (not just declared) in this translation unit.
#define INITGUID
#include "DxReflection.h"

#include "EffectCompilerVulkan.h"
#include "EffectCompilerHLSLShared.h"
#include "CompileMessageQueue.h"
#include "StringTable.h"
#include "EffectData.h"
#include "SymbolTable.h"
#include "ASTNode.h"
#include "ParserUtils.h"
#include "HLSLParser.h"
#include "ParserState.h"
#include "FXAnalyzer.h"
#include "TextureFunctionConversionDX11.h"
#include "Macro.h"
#include "OutputHLSL.h"
#include "Platforms.h"
#include <WorkQueue.h>

#include <algorithm>

extern CompileMessageQueue g_messages;
extern StringTable g_stringTable;
extern bool g_skipOptimization;

namespace
{

std::wstring Widen( const std::string& s )
{
	return std::wstring( s.begin(), s.end() );
}

// Report dxc's diagnostics; returns true when there was any text.
bool ReportErrors( IDxcResult* result )
{
	CComPtr<IDxcBlobUtf8> errors;
	if( SUCCEEDED( result->GetOutput( DXC_OUT_ERRORS, IID_PPV_ARGS( &errors ), nullptr ) ) && errors && errors->GetStringLength() )
	{
		g_messages.AddMessage( "%s", errors->GetStringPointer() );
		return true;
	}
	return false;
}

CComPtr<IDxcResult> RunDxc( const std::string& code, const std::vector<std::wstring>& arguments )
{
	CComPtr<IDxcCompiler3> compiler;
	if( FAILED( DxcCreateInstance( CLSID_DxcCompiler, IID_PPV_ARGS( &compiler ) ) ) )
	{
		g_messages.AddMessage( "\\memory(0): error X0000: could not create the dxc compiler" );
		return nullptr;
	}
	std::vector<LPCWSTR> args;
	args.reserve( arguments.size() );
	for( auto& a : arguments )
	{
		args.push_back( a.c_str() );
	}
	DxcBuffer source{ code.data(), code.size(), DXC_CP_UTF8 };
	CComPtr<IDxcResult> result;
	if( FAILED( compiler->Compile( &source, args.data(), UINT32( args.size() ), nullptr, IID_PPV_ARGS( &result ) ) ) )
	{
		g_messages.AddMessage( "\\memory(0): error X0000: dxc failed to run" );
		return nullptr;
	}
	return result;
}

// SPIR-V links stage interfaces by location. dxc numbers each stage's user inputs/outputs alphabetically by semantic
// (-fvk-stage-io-order=alpha), so two stages agree only if the consumer reads a prefix of the producer's sorted outputs.
// Anything else would silently mismatch, so it is rejected here until explicit per-semantic locations are emitted.
std::vector<std::string> SortedUserSemantics( ID3D12ShaderReflection* reflection, bool outputs )
{
	D3D12_SHADER_DESC desc;
	reflection->GetDesc( &desc );
	std::vector<std::string> semantics;
	UINT count = outputs ? desc.OutputParameters : desc.InputParameters;
	for( UINT i = 0; i < count; ++i )
	{
		D3D12_SIGNATURE_PARAMETER_DESC p;
		if( outputs ? FAILED( reflection->GetOutputParameterDesc( i, &p ) ) : FAILED( reflection->GetInputParameterDesc( i, &p ) ) )
		{
			continue;
		}
		if( p.SystemValueType != D3D_NAME_UNDEFINED )
		{
			continue;
		}
		std::string name = p.SemanticName;
		std::transform( name.begin(), name.end(), name.begin(), ::toupper );
		semantics.push_back( name + std::to_string( p.SemanticIndex ) );
	}
	std::sort( semantics.begin(), semantics.end() );
	return semantics;
}

bool MatchStageLocations( ID3D12ShaderReflection* producer, ID3D12ShaderReflection* consumer )
{
	auto outputs = SortedUserSemantics( producer, true );
	auto inputs = SortedUserSemantics( consumer, false );
	for( size_t i = 0; i < inputs.size(); ++i )
	{
		if( i >= outputs.size() || outputs[i] != inputs[i] )
		{
			g_messages.AddMessage(
				"\\memory(0): error X0000: Vulkan: the next stage reads %s, which gets a different location than the previous stage "
				"writes it at; the consumer's inputs must be the first (alphabetically) of the producer's outputs until explicit "
				"locations are supported",
				inputs[i].c_str() );
			return false;
		}
	}
	return true;
}

} // namespace


bool EffectCompilerVulkan::Create()
{
	ZoneScoped;
	CComPtr<IDxcCompiler3> compiler;
	if( FAILED( DxcCreateInstance( CLSID_DxcCompiler, IID_PPV_ARGS( &compiler ) ) ) )
	{
		g_messages.AddMessage( "\\memory(0): error X0000: Could not load dxc (libdxcompiler.so)" );
		return false;
	}
	return true;
}

std::vector<std::wstring> EffectCompilerVulkan::CommonArguments( const std::string& entryPoint, const std::string& profile )
{
	return {
		L"-T",
		Widen( profile ),
		L"-E",
		Widen( entryPoint ),
		L"-Zpc", // column-major matrix packing, as fxc is invoked for DX11/DX12 (D3DCOMPILE_PACK_MATRIX_COLUMN_MAJOR)
		L"-HV",
		L"2018", // the effect sources are written for fxc; HLSL 2021 changes semantics they rely on
		g_skipOptimization ? L"-Od" : L"-O3",
	};
}

std::vector<std::wstring> EffectCompilerVulkan::SpirvArguments()
{
	return {
		L"-spirv",
		L"-fspv-target-env=vulkan1.3",
		L"-fvk-use-dx-layout", // constant buffer offsets identical to D3D, so the DXIL reflection describes the SPIR-V
		L"-fvk-stage-io-order=alpha", // stage interfaces numbered by semantic; see MatchStageLocations
		L"-fvk-b-shift",
		L"0",
		L"all",
		L"-fvk-s-shift",
		L"32",
		L"all",
		L"-fvk-t-shift",
		L"64",
		L"all",
		L"-fvk-u-shift",
		L"128",
		L"all",
	};
}

bool EffectCompilerVulkan::CompileStage( const std::string& code, const std::string& entryPoint, const std::string& profile, CompiledStage& out )
{
	ZoneScoped;
	auto common = CommonArguments( entryPoint, profile );

	// SPIR-V: the shipped bytecode.
	auto spirvArgs = common;
	auto extra = SpirvArguments();
	spirvArgs.insert( spirvArgs.end(), extra.begin(), extra.end() );
	auto spirvResult = RunDxc( code, spirvArgs );
	if( !spirvResult )
	{
		return false;
	}
	HRESULT status = E_FAIL;
	spirvResult->GetStatus( &status );
	if( FAILED( status ) )
	{
		ReportErrors( spirvResult );
		return false;
	}
	CComPtr<IDxcBlob> spirv;
	if( FAILED( spirvResult->GetOutput( DXC_OUT_OBJECT, IID_PPV_ARGS( &spirv ), nullptr ) ) || !spirv )
	{
		g_messages.AddMessage( "\\memory(0): error X0000: dxc produced no SPIR-V" );
		return false;
	}
	out.spirv.assign( static_cast<const char*>( spirv->GetBufferPointer() ), spirv->GetBufferSize() );

	// DXIL twin of the same source: only its D3D12 reflection is used.
	auto dxilResult = RunDxc( code, common );
	if( !dxilResult )
	{
		return false;
	}
	dxilResult->GetStatus( &status );
	if( FAILED( status ) )
	{
		ReportErrors( dxilResult );
		return false;
	}
	CComPtr<IDxcBlob> reflectionBlob;
	if( FAILED( dxilResult->GetOutput( DXC_OUT_REFLECTION, IID_PPV_ARGS( &reflectionBlob ), nullptr ) ) || !reflectionBlob )
	{
		g_messages.AddMessage( "\\memory(0): error X0000: dxc produced no reflection data" );
		return false;
	}
	CComPtr<IDxcUtils> utils;
	if( FAILED( DxcCreateInstance( CLSID_DxcUtils, IID_PPV_ARGS( &utils ) ) ) )
	{
		g_messages.AddMessage( "\\memory(0): error X0000: could not create dxc utils" );
		return false;
	}
	DxcBuffer reflectionBuffer{ reflectionBlob->GetBufferPointer(), reflectionBlob->GetBufferSize(), 0 };
	if( FAILED( utils->CreateReflection( &reflectionBuffer, IID_ID3D12ShaderReflection, reinterpret_cast<void**>( &out.reflection ) ) ) )
	{
		g_messages.AddMessage( "\\memory(0): error X0000: Could not get shader reflection" );
		return false;
	}
	return true;
}

bool EffectCompilerVulkan::CompileEffect( const char* source, size_t sourceLength, const std::vector<Macro>& defines, EffectData& result, IWorkQueue* workQueue )
{
	ZoneScoped;

	std::vector<Macro> platformDefines = defines;
	if( auto define = FindMacro( platformDefines, "PLATFORM" ) )
	{
		define->value = GetPlatformIdString( PLATFORM_VULKAN );
	}
	else
	{
		platformDefines.push_back( { "PLATFORM", GetPlatformIdString( PLATFORM_VULKAN ) } );
	}

	ParserState state( MakeInlineString( source, source + sourceLength ) );
	for( auto it = begin( platformDefines ); it != end( platformDefines ); ++it )
	{
		PreprocessorDefine d;
		d.location.fileName = MakeInlineString( "" );
		d.location.lineNumber = 0;
		d.value = MakeInlineString( it->value.c_str() );
		state.m_defines[MakeInlineString( it->name.c_str() )] = d;
	}

	if( !state.Parse() )
	{
		return false;
	}

	PatchCBuffers( state );
	TransferSRGBToTexturesDX11( state );
	ConvertTextureFunctionsDX11( state );
	MergeSamplers( state );

	std::vector<ASTNode*> techniqueNodes;
	state.GetTree()->FindNodes( NT_TECHNIQUE, techniqueNodes );
	if( techniqueNodes.empty() )
	{
		g_messages.AddMessage( "\\memory(0): error X0000: No technique or libraries found" );
		return false;
	}

	for( auto techniqueNode : techniqueNodes )
	{
		Technique technique;
		technique.name = g_stringTable.AddString( ToString( techniqueNode->GetToken()->stringValue ).c_str() );

		for( size_t passIx = 0; passIx < techniqueNode->GetChildrenCount(); ++passIx )
		{
			ASTNode* passNode = techniqueNode->GetChild( passIx );
			if( passNode->GetNodeType() == NT_LIBRARY )
			{
				state.ShowMessage( passNode->GetToken()->fileLocation, EC_INVALID_STATE, "library (raytracing is not supported by the Vulkan compiler yet)" );
				return false;
			}
			if( passNode->GetNodeType() != NT_PASS )
			{
				continue;
			}

			Pass outPass;
			CComPtr<ID3D12ShaderReflection> reflections[6];
			for( size_t stateIx = 0; stateIx < passNode->GetChildrenCount(); ++stateIx )
			{
				if( passNode->GetChild( stateIx )->GetNodeType() == NT_STATE_ASSIGNMENT )
				{
					DWORD stateCode = 0;
					DWORD value = 0;
					if( ParseStateAssignment( state, passNode->GetChild( stateIx ), g_renderStates, &value ) )
					{
						std::string name = ToString( passNode->GetChild( stateIx )->GetToken()->stringValue );
						for( int i = 0; g_renderStateNames[i].name; ++i )
						{
							if( _stricmp( name.c_str(), g_renderStateNames[i].name ) == 0 )
							{
								stateCode = g_renderStateNames[i].value;
							}
						}
						switch( int( stateCode ) )
						{
						case -1:
						case -2:
						case -3:
						case -4:
						case -5:
						case -6:
							if( value != 0 )
							{
								state.ShowMessage( passNode->GetChild( stateIx )->GetLocation(), EC_INVALID_STATE_VALUE, name.c_str() );
							}
							break;
						case 0:
							state.ShowMessage( passNode->GetChild( stateIx )->GetLocation(), EC_STATE_DEPRECATED, name.c_str() );
							break;
						default:
							outPass.states[stateCode] = value;
						}
					}
					continue;
				}

				if( passNode->GetChild( stateIx )->GetNodeType() != NT_SHADER_ASSIGNMENT )
				{
					continue;
				}

				ASTNode* shaderNode = passNode->GetChild( stateIx );

				StageInput stage;
				if( !ParseShaderName( shaderNode->GetToken()->stringValue, stage.type ) )
				{
					state.ShowMessage( shaderNode->GetToken()->fileLocation, EC_INVALID_STATE, ToString( shaderNode->GetToken()->stringValue ).c_str() );
					return false;
				}
				if( shaderNode->GetChild( 1 )->GetSymbol() == nullptr )
				{
					return false;
				}

				// Shader model 6.0 is dxc's minimum; the stage letter comes from the effect (vs_3_0 -> vs_6_0).
				std::string profile = ToString( shaderNode->GetChild( 0 )->GetToken()->stringValue ).substr( 0, 3 ) + "6_0";

				PatchSemantics( stage.type, shaderNode->GetChild( 1 ) );

				state.GetSymbolTable().ResetUsedFlag();
				MarkUsedSymbols( shaderNode->GetChild( 1 ), state );

				CreateGlobalsCB( state );
				AssignRegisters( state.GetTree(), stage.type );

				CompilerInputStream os( state, ShadingLanguage::HLSL );
				os << HLSL{ state.GetTree(), &state.GetSymbolTable() };

				std::string patchEntryPoint = ToString( shaderNode->GetChild( 1 )->GetSymbol()->name );
				if( PatchShader( stage.type, shaderNode->GetChild( 1 ), state, os, patchEntryPoint ) == PATCH_ERROR )
				{
					return false;
				}
				state.ResetPragmaUsage();
				std::string code = os.str();

				// Permutations often produce identical stage code: compile each distinct one once, as the DX11 compiler does.
				std::string cacheKey = profile + '\n' + patchEntryPoint + '\n' + code;
				bool needsToCompile = false;
				std::shared_ptr<CompiledStage> compiled;
				{
					std::lock_guard scope( m_compiledCS );
					auto found = m_compiled.find( cacheKey );
					if( found == end( m_compiled ) )
					{
						compiled = std::make_shared<CompiledStage>();
						m_compiled[cacheKey] = compiled;
						needsToCompile = true;
					}
					else
					{
						compiled = found->second;
					}
				}
				if( needsToCompile )
				{
					bool succeeded = CompileStage( code, patchEntryPoint, profile, *compiled );
					{
						std::lock_guard scope( compiled->mutex );
						compiled->succeeded = succeeded;
						compiled->done = true;
					}
					compiled->conditionVariable.notify_all();
				}
				else
				{
					if( workQueue )
					{
						workQueue->OnBlocked();
					}
					{
						std::unique_lock<std::mutex> lock( compiled->mutex );
						compiled->conditionVariable.wait( lock, [&compiled] { return compiled->done; } );
					}
					if( workQueue )
					{
						workQueue->OnUnblocked();
					}
				}
				if( !compiled->succeeded )
				{
					return false;
				}

				stage.shaderSize = uint32_t( compiled->spirv.size() );
				stage.shaderDataStr = g_stringTable.AddString( compiled->spirv.data(), compiled->spirv.size() );
				stage.source = code;

				// The engine uses static samplers where the effect declares them, as on DX12.
				if( !DxReflection::ProcessReflection<DxReflection::ReflectionDx12>( state, compiled->reflection.p, true, stage, result.annotations ) )
				{
					return false;
				}
				{
					stage.annotations.annotations.clear();
					Symbol* symbol = shaderNode->GetChild( 1 )->GetSymbol();
					if( symbol && symbol->annotations )
					{
						for( auto a = symbol->annotations->begin(); a != symbol->annotations->end(); ++a )
						{
							Annotation symbolAnnotation;
							bool isSrgb, isAutoregister;
							if( DxReflection::MakeEffectAnnotationFromSymbolAnnotation( *a, symbolAnnotation, isSrgb, isAutoregister ) )
							{
								stage.annotations.annotations[g_stringTable.AddString( ToString( a->name ).c_str() )] = symbolAnnotation;
							}
						}
					}
				}

				if( !stage.defaultValues.empty() )
				{
					stage.defaultValuesStr = g_stringTable.AddString( &stage.defaultValues[0], stage.defaultValues.size() );
				}
				else
				{
					stage.defaultValuesStr = INVALID_REFERENCE;
				}

				reflections[stage.type] = compiled->reflection;
				outPass.stages.push_back( stage );
			}

			// Each graphics stage must read its inputs at the locations the previous stage writes them.
			InputStageType pipelineStages[] = { VERTEX_STAGE, HULL_STAGE, DOMAIN_STAGE, GEOMETRY_STAGE, PIXEL_STAGE };
			ID3D12ShaderReflection* previous = nullptr;
			for( auto stageType : pipelineStages )
			{
				if( auto current = reflections[stageType].p )
				{
					if( previous && !MatchStageLocations( previous, current ) )
					{
						return false;
					}
					previous = current;
				}
			}

			technique.passes.push_back( outPass );
		}

		result.techniques.push_back( technique );
	}

	return true;
}

#endif
