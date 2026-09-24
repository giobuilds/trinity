// Copyright © 2026 CCP ehf.

#pragma once

// HLSL pass helpers shared by the HLSL-emitting compilers (DX11/DX12 and Vulkan), moved unchanged from
// EffectCompilerDX11.cpp.

#include "EffectData.h"
#include "ParserUtils.h"

class ASTNode;
class ParserState;
class CodeStream;
struct Symbol;
struct InlineString;

bool FindOutputBySemantics( ASTNode* node, const char** semantics, std::vector<Symbol*>* path );
void PrintValuePath( std::ostream& os, const std::vector<Symbol*>& path, const char* functionSymbol );
void PatchSemantics( InputStageType shaderStage, ASTNode* callNode );
PatchAction PatchShader( InputStageType shaderStage, ASTNode* callNode, ParserState& state, CodeStream& os, std::string& entryPointName );
std::string SanitizeCode( const std::string& src );
bool ParseShaderName( const InlineString& name, InputStageType& type );
