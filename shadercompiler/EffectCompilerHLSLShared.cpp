// Copyright © 2023 CCP ehf.

// HLSL pass helpers shared by the HLSL-emitting compilers (DX11/DX12 and Vulkan), moved unchanged from
// EffectCompilerDX11.cpp (PatchSemantics and PatchShader lost their `static`).

#include "stdafx.h"
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
#include "OutputHLSL.h"

extern CompileMessageQueue g_messages;

static bool FindParameterBySemantics( ASTNode* node, const char** semantics, std::vector<Symbol*>* path, bool outParameter = false )
{
	for( unsigned j = 0; j < node->GetChildrenCount(); ++j )
	{
		Symbol* symbol = node->GetChild( j )->GetSymbol();
		if( symbol == nullptr )
		{
			continue;
		}
		if( outParameter )
		{
			if( node->GetChild( j )->GetToken() == 0 || node->GetChild( j )->GetToken()->type == OP_IN )
			{
				continue;
			}
		}
		for( unsigned k = 0; semantics[k]; ++k )
		{
			if( _stricmp( ToString( symbol->semantic ).c_str(), semantics[k] ) == 0 )
			{
				if( path )
				{
					path->clear();
					path->push_back( symbol );
				}
				return true;
			}
		}
		if( node->GetChild( j )->GetType().symbol && node->GetChild( j )->GetType().symbol->definition )
		{
			for( unsigned i = 0; i < node->GetChild( j )->GetType().symbol->definition->GetChildrenCount(); ++i )
			{
				if( FindParameterBySemantics( node->GetChild( j )->GetType().symbol->definition->GetChild( i ), semantics, path ) )
				{
					if( path )
					{
						path->insert( path->begin(), symbol );
					}
					return true;
				}
			}
		}
	}
	return false;
}

bool FindOutputBySemantics( ASTNode* node, const char** semantics, std::vector<Symbol*>* path )
{
	// check "out" function arguments
	if( FindParameterBySemantics( node, semantics, path, true ) )
	{
		return true;
	}
	// check return value if it has semantics
	if( node->GetSymbol() && node->GetSymbol()->semantic.start )
	{
		std::string sematic = ToString( node->GetSymbol()->semantic );
		for( unsigned i = 0; semantics[i]; ++i )
		{
			if( _stricmp( sematic.c_str(), semantics[i] ) == 0 )
			{
				if( path )
				{
					path->push_back( nullptr );
				}
				return true;
			}
		}
	}
	// finally check return value as a structure
	if( node->GetType().symbol &&
		node->GetType().symbol->definition )
	{
		for( unsigned i = 0; i < node->GetType().symbol->definition->GetChildrenCount(); ++i )
		{
			if( FindParameterBySemantics( node->GetType().symbol->definition->GetChild( i ), semantics, path ) )
			{
				if( path )
				{
					path->insert( path->begin(), nullptr );
				}
				return true;
			}
		}
	}
	return false;
}

void PrintValuePath( std::ostream& os, const std::vector<Symbol*>& path, const char* functionSymbol )
{
	bool first = true;
	for( auto it = path.begin(); it != path.end(); ++it )
	{
		if( first )
		{
			first = false;
		}
		else
		{
			os << ".";
		}
		if( *it )
		{
			os << ( *it )->name;
		}
		else
		{
			os << functionSymbol;
		}
	}
}

void PatchSemantics( InputStageType shaderStage, ASTNode* callNode )
{
	Symbol* entryPointSymbol = callNode->GetSymbol();
	if( !entryPointSymbol )
	{
		return;
	}
	ASTNode* functionHeader = entryPointSymbol->definition->GetChildOrNull( 0 );
	if( functionHeader == nullptr )
	{
		return;
	}
	std::vector<Symbol*> targetPath;
	switch( shaderStage )
	{
	case VERTEX_STAGE: {
		const char* semantics[] = { "position", nullptr };
		if( FindOutputBySemantics( functionHeader, semantics, &targetPath ) )
		{
			if( targetPath.back() )
			{
				targetPath.back()->semantic = MakeInlineString( "SV_Position" );
			}
			else
			{
				entryPointSymbol->semantic = MakeInlineString( "SV_Position" );
			}
		}
		break;
	}
	case PIXEL_STAGE: {
		{
			const char* semantics[] = { "color", "color0", nullptr };
			if( FindOutputBySemantics( functionHeader, semantics, &targetPath ) )
			{
				if( targetPath.back() )
				{
					targetPath.back()->semantic = MakeInlineString( "SV_Target0" );
				}
				else
				{
					entryPointSymbol->semantic = MakeInlineString( "SV_Target0" );
				}
			}
		}
		{
			const char* semantics[] = { "color1", nullptr };
			if( FindOutputBySemantics( functionHeader, semantics, &targetPath ) )
			{
				if( targetPath.back() )
				{
					targetPath.back()->semantic = MakeInlineString( "SV_Target1" );
				}
				else
				{
					entryPointSymbol->semantic = MakeInlineString( "SV_Target1" );
				}
			}
		}
		break;
	}
	default: // other stages need no semantic patching
		break;
	}
}

PatchAction PatchShader( InputStageType shaderStage, ASTNode* callNode, ParserState& state, CodeStream& os, std::string& entryPointName )
{
	ZoneScoped;

	// 1. wrap uniforms
	// 2. fix VPOS

	bool wrapUniforms = false;
	bool fixVPOS = false;
	bool fixVPOSType = false;

	Symbol* entryPointSymbol = callNode->GetSymbol();

	if( entryPointSymbol == nullptr || entryPointSymbol->definition == nullptr )
	{
		return PATCH_ERROR;
	}

	ASTNode* functionHeader = entryPointSymbol->definition->GetChildOrNull( 0 );
	if( functionHeader == nullptr )
	{
		return PATCH_ERROR;
	}

	std::vector<Symbol*> targetPath;
	std::vector<Symbol*> outPositionPath;
	std::vector<Symbol*> vfacePath;

	if( shaderStage == VERTEX_STAGE )
	{
		const char* position[] = { "position", "sv_position", nullptr };
		FindOutputBySemantics( functionHeader, position, &outPositionPath );
	}


	// check for uniform parameters
	if( callNode->GetChildrenCount() )
	{
		unsigned count = 0;
		for( unsigned i = 0; i < functionHeader->GetChildrenCount(); ++i )
		{
			if( IsUniformInputArgument( functionHeader->GetChild( i ) ) )
			{
				++count;
			}
		}
		if( count != callNode->GetChildrenCount() )
		{
			state.ShowMessage( callNode->GetLocation(), EC_NO_OVERRIDE, ToString( functionHeader->GetToken()->stringValue ).c_str(), "" );
			return PATCH_ERROR;
		}
		wrapUniforms = true;
	}

	// find VPOS+POSITION parameters
	std::vector<Symbol*> positionPath;
	std::vector<Symbol*> vposPath;
	const char* vpos[] = { "vpos", nullptr };
	fixVPOSType = fixVPOS = FindParameterBySemantics( functionHeader, vpos, &vposPath );
	if( fixVPOS )
	{
		const char* position[] = { "position", "sv_position", nullptr };
		bool foundPosition = FindParameterBySemantics( functionHeader, position, &positionPath );
		fixVPOS &= foundPosition;
	}

	if( fixVPOSType )
	{
		if( vposPath.back() )
		{
			if( vposPath.back()->type.symbol ||
				( vposPath.back()->type.builtInType == OP_FLOAT && vposPath.back()->type.width == 4 && vposPath.back()->type.height == 1 ) )
			{
				fixVPOSType = false;
			}
		}
	}

	if( !wrapUniforms && !fixVPOS && !fixVPOSType && ( shaderStage != VERTEX_STAGE || outPositionPath.empty() ) )
	{
		entryPointName = ToString( entryPointSymbol->name );
		return PATCH_USE;
	}

	if( entryPointSymbol->definition->GetChildOrNull( 2 ) )
	{
		os << HLSL{ entryPointSymbol->definition->GetChildOrNull( 2 ), nullptr };
	}
	// return type
	os << functionHeader->GetType();
	// name
	InlineString name = state.AllocateName();
	os << ' ' << name << "( ";
	bool first = true;

	for( size_t i = 0; i < functionHeader->GetChildrenCount(); ++i )
	{
		if( IsUniformInputArgument( functionHeader->GetChild( i ) ) )
		{
			continue;
		}

		Symbol* symbol = functionHeader->GetChild( i )->GetSymbol();
		if( symbol == nullptr )
		{
			return PATCH_ERROR;
		}
		if( fixVPOS && symbol->semantic.start && _stricmp( ToString( symbol->semantic ).c_str(), "vpos" ) == 0 )
		{
			continue;
		}
		if( !first )
		{
			os << ", ";
		}
		else
		{
			first = false;
		}
		if( fixVPOSType && symbol->semantic.start && _stricmp( ToString( symbol->semantic ).c_str(), "vpos" ) == 0 )
		{
			os << "float4 " << functionHeader->GetChild( i )->GetSymbol()->name << ": VPOS";
		}
		else
		{
			os << HLSL{ functionHeader->GetChild( i ), nullptr };
		}
	}
	os << " )";
	// semantics
	if( functionHeader->GetSymbol()->semantic.start )
	{
		os << " : " << functionHeader->GetSymbol()->semantic;
	}
	os << "\n{\n";
	unsigned uniformIndex = 0;
	std::map<size_t, InlineString> defaultUniformArguments;
	for( size_t i = 0; i < functionHeader->GetChildrenCount(); ++i )
	{
		if( IsUniformInputArgument( functionHeader->GetChild( i ) ) )
		{
			if( callNode->GetChildrenCount() <= uniformIndex && functionHeader->GetChild( i )->GetChildOrNull( 1 ) != nullptr )
			{
				InlineString uniformName = state.AllocateName();
				Type type = functionHeader->GetChild( i )->GetType();
				type.storageClass = 0;
				os << type << ' ' << uniformName << " = " << HLSL{ functionHeader->GetChild( i )->GetChildOrNull( 1 ), nullptr };
				os << ";\n";
				defaultUniformArguments[i] = uniformName;
			}
			uniformIndex++;
		}
	}

	if( functionHeader->GetSymbol()->type.symbol || functionHeader->GetSymbol()->type.builtInType != OP_VOID )
	{
		os << functionHeader->GetType() << " __returnValue = ";
	}
	// call original function
	uniformIndex = 0;
	os << entryPointSymbol->name << "(";
	for( size_t i = 0; i < functionHeader->GetChildrenCount(); ++i )
	{
		if( i > 0 )
		{
			os << ", ";
		}
		if( IsUniformInputArgument( functionHeader->GetChild( i ) ) )
		{
			if( callNode->GetChildrenCount() > uniformIndex )
			{
				os << HLSL{ callNode->GetChild( uniformIndex ), nullptr };
			}
			else if( functionHeader->GetChild( i )->GetChildOrNull( 1 ) != nullptr )
			{
				os << defaultUniformArguments[i];
			}
			uniformIndex++;
		}
		else
		{
			Symbol* symbol = functionHeader->GetChild( i )->GetSymbol();
			if( symbol == nullptr )
			{
				return PATCH_ERROR;
			}
			if( fixVPOS && symbol->semantic.start && _stricmp( ToString( symbol->semantic ).c_str(), "vpos" ) == 0 )
			{
				// pass VPOS value from POSITION
				PrintValuePath( os, positionPath, "" );
				if( symbol->type.symbol == nullptr && symbol->type.builtInType == OP_FLOAT && symbol->type.height == 1 &&
					positionPath.back()->type.symbol == nullptr && positionPath.back()->type.builtInType == OP_FLOAT &&
					positionPath.back()->type.height == 1 && positionPath.back()->type.width != symbol->type.width )
				{
					const char* swizzle = "xyzw";
					os << ".";
					for( int k = 0; k < symbol->type.width; ++k )
					{
						os << swizzle[std::min( k, positionPath.back()->type.width - 1 )];
					}
				}
			}
			else if( fixVPOSType && symbol->semantic.start && _stricmp( ToString( symbol->semantic ).c_str(), "vpos" ) == 0 )
			{
				Type type = symbol->type;
				os << type << "( " << symbol->name;
				if( type.width != 4 )
				{
					const char* swizzle = "xyzw";
					os << ".";
					for( int k = 0; k < symbol->type.width; ++k )
					{
						os << swizzle[std::min( k, symbol->type.width - 1 )];
					}
				}
				os << " )";
			}
			else
			{
				os << symbol->name;
			}
		}
	}
	os << ");\n";
	if( functionHeader->GetSymbol()->type.symbol || functionHeader->GetSymbol()->type.builtInType != OP_VOID )
	{
		os << "return __returnValue;\n";
	}
	os << "}\n";

	entryPointName = ToString( name );
	return PATCH_USE;
}

std::string SanitizeCode( const std::string& src )
{
	ZoneScoped;
	std::regex line( "#line[^\\n]*\\n?\\n" );
	return std::regex_replace( src, line, std::string( "" ) );
}


bool ParseShaderName( const InlineString& name, InputStageType& type )
{
	if( _stricmp( ToString( name ).c_str(), "vertexshader" ) == 0 )
	{
		type = VERTEX_STAGE;
	}
	else if( _stricmp( ToString( name ).c_str(), "pixelshader" ) == 0 )
	{
		type = PIXEL_STAGE;
	}
	else if( _stricmp( ToString( name ).c_str(), "computeshader" ) == 0 )
	{
		type = COMPUTE_STAGE;
	}
	else if( _stricmp( ToString( name ).c_str(), "geometryshader" ) == 0 )
	{
		type = GEOMETRY_STAGE;
	}
	else if( _stricmp( ToString( name ).c_str(), "hullshader" ) == 0 )
	{
		type = HULL_STAGE;
	}
	else if( _stricmp( ToString( name ).c_str(), "domainshader" ) == 0 )
	{
		type = DOMAIN_STAGE;
	}
	else
	{
		return false;
	}
	return true;
}
