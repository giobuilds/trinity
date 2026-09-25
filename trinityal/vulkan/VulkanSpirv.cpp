// Copyright © 2026 CCP ehf.

#include "StdAfx.h"

#if ( TRINITY_PLATFORM == TRINITY_VULKAN )

#include "VulkanSpirv.h"

#include <algorithm>
#include <cctype>
#include <cstring>
#include <unordered_map>

namespace
{

enum : uint32_t
{
	SPIRV_MAGIC = 0x07230203,
	HEADER_WORDS = 5,

	OP_NAME = 5,
	OP_TYPE_INT = 21,
	OP_TYPE_FLOAT = 22,
	OP_TYPE_VECTOR = 23,
	OP_TYPE_IMAGE = 25,
	OP_TYPE_SAMPLER = 26,
	OP_TYPE_SAMPLED_IMAGE = 27,
	OP_TYPE_ARRAY = 28,
	OP_TYPE_RUNTIME_ARRAY = 29,
	OP_TYPE_STRUCT = 30,
	OP_TYPE_POINTER = 32,
	OP_CONSTANT = 43,
	OP_VARIABLE = 59,
	OP_DECORATE = 71,

	DECORATION_BLOCK = 2,
	DECORATION_BUFFER_BLOCK = 3,
	DECORATION_BUILT_IN = 11,
	DECORATION_LOCATION = 30,
	DECORATION_BINDING = 33,
	DECORATION_DESCRIPTOR_SET = 34,

	STORAGE_UNIFORM_CONSTANT = 0,
	STORAGE_INPUT = 1,
	STORAGE_UNIFORM = 2,
	STORAGE_STORAGE_BUFFER = 12,

	DIM_1D = 0,
	DIM_2D = 1,
	DIM_3D = 2,
	DIM_CUBE = 3,
	DIM_BUFFER = 5,
};

struct Instruction
{
	uint32_t opcode;
	const uint32_t* operands; // words after the opcode word
	uint32_t operandCount;
};

// Calls f(instruction, offsetOfItsFirstWord) for every instruction; false on a malformed stream.
template <typename F>
bool ForEachInstruction( const std::vector<uint32_t>& words, F f )
{
	size_t i = HEADER_WORDS;
	while( i < words.size() )
	{
		uint32_t count = words[i] >> 16;
		if( count == 0 || i + count > words.size() )
		{
			return false;
		}
		f( Instruction{ words[i] & 0xffff, words.data() + i + 1, count - 1 }, i );
		i += count;
	}
	return true;
}

bool CheckHeader( const std::vector<uint32_t>& words, std::string& error )
{
	if( words.size() < HEADER_WORDS || words[0] != SPIRV_MAGIC )
	{
		error = "not a SPIR-V module";
		return false;
	}
	return true;
}

std::string ReadString( const uint32_t* operands, uint32_t count )
{
	const char* text = reinterpret_cast<const char*>( operands );
	return std::string( text, strnlen( text, count * 4 ) );
}

}

namespace TrinityALImpl
{

bool RemapSpirvBindings( std::vector<uint32_t>& words, Tr2RenderContextEnum::ShaderType stage, std::string& error )
{
	if( !CheckHeader( words, error ) )
	{
		return false;
	}
	struct Target
	{
		size_t setWord = 0; // index of the DescriptorSet literal
		size_t bindingWord = 0;
	};
	std::unordered_map<uint32_t, Target> targets;
	bool ok = ForEachInstruction( words, [&]( const Instruction& in, size_t at ) {
		if( in.opcode == OP_DECORATE && in.operandCount >= 3 )
		{
			if( in.operands[1] == DECORATION_DESCRIPTOR_SET )
			{
				targets[in.operands[0]].setWord = at + 3;
			}
			else if( in.operands[1] == DECORATION_BINDING )
			{
				targets[in.operands[0]].bindingWord = at + 3;
			}
		}
	} );
	if( !ok )
	{
		error = "malformed SPIR-V instruction stream";
		return false;
	}
	for( auto& target : targets )
	{
		if( !target.second.setWord || !target.second.bindingWord )
		{
			continue;
		}
		uint32_t space = words[target.second.setWord];
		uint32_t binding = words[target.second.bindingWord];
		if( space >= SpirvBinding::STAGE_BINDING_STRIDE / SpirvBinding::SPACE_BINDING_STRIDE || binding >= SpirvBinding::SPACE_BINDING_STRIDE )
		{
			error = "descriptor (set " + std::to_string( space ) + ", binding " + std::to_string( binding ) + ") is outside the register convention";
			return false;
		}
		words[target.second.setWord] = 0;
		words[target.second.bindingWord] = SpirvBinding::Make( stage, space, binding );
	}
	return true;
}

bool ReflectSpirv( const std::vector<uint32_t>& words, SpirvReflection& reflection, std::string& error )
{
	reflection = SpirvReflection();
	if( !CheckHeader( words, error ) )
	{
		return false;
	}

	struct Type
	{
		uint32_t opcode = 0;
		std::vector<uint32_t> operands; // without the result id
	};
	std::unordered_map<uint32_t, Type> types;
	std::unordered_map<uint32_t, uint32_t> constants; // 32-bit integer constants, for array lengths
	std::unordered_map<uint32_t, std::string> names;
	std::unordered_map<uint32_t, uint32_t> bindings, locations;
	std::unordered_map<uint32_t, bool> blocks, bufferBlocks, builtIns;
	struct Variable
	{
		uint32_t id, pointerType, storageClass;
	};
	std::vector<Variable> variables;

	bool ok = ForEachInstruction( words, [&]( const Instruction& in, size_t ) {
		switch( in.opcode )
		{
		case OP_NAME:
			if( in.operandCount >= 2 )
			{
				names[in.operands[0]] = ReadString( in.operands + 1, in.operandCount - 1 );
			}
			break;
		case OP_DECORATE:
			if( in.operandCount >= 2 )
			{
				uint32_t target = in.operands[0];
				switch( in.operands[1] )
				{
				case DECORATION_BLOCK:
					blocks[target] = true;
					break;
				case DECORATION_BUFFER_BLOCK:
					bufferBlocks[target] = true;
					break;
				case DECORATION_BUILT_IN:
					builtIns[target] = true;
					break;
				case DECORATION_BINDING:
					if( in.operandCount >= 3 )
					{
						bindings[target] = in.operands[2];
					}
					break;
				case DECORATION_LOCATION:
					if( in.operandCount >= 3 )
					{
						locations[target] = in.operands[2];
					}
					break;
				}
			}
			break;
		case OP_TYPE_INT:
		case OP_TYPE_FLOAT:
		case OP_TYPE_VECTOR:
		case OP_TYPE_IMAGE:
		case OP_TYPE_SAMPLER:
		case OP_TYPE_SAMPLED_IMAGE:
		case OP_TYPE_ARRAY:
		case OP_TYPE_RUNTIME_ARRAY:
		case OP_TYPE_STRUCT:
		case OP_TYPE_POINTER:
			if( in.operandCount >= 1 )
			{
				Type& type = types[in.operands[0]];
				type.opcode = in.opcode;
				type.operands.assign( in.operands + 1, in.operands + in.operandCount );
			}
			break;
		case OP_CONSTANT:
			if( in.operandCount >= 3 )
			{
				constants[in.operands[1]] = in.operands[2];
			}
			break;
		case OP_VARIABLE:
			if( in.operandCount >= 3 )
			{
				variables.push_back( { in.operands[1], in.operands[0], in.operands[2] } );
			}
			break;
		}
	} );
	if( !ok )
	{
		error = "malformed SPIR-V instruction stream";
		return false;
	}

	auto typeOf = [&]( uint32_t id ) -> const Type* {
		auto found = types.find( id );
		return found == types.end() ? nullptr : &found->second;
	};

	for( const Variable& variable : variables )
	{
		const Type* pointer = typeOf( variable.pointerType );
		if( !pointer || pointer->opcode != OP_TYPE_POINTER || pointer->operands.size() < 2 )
		{
			continue;
		}
		uint32_t pointeeId = pointer->operands[1];
		const Type* pointee = typeOf( pointeeId );

		if( variable.storageClass == STORAGE_INPUT )
		{
			if( builtIns.count( variable.id ) || !locations.count( variable.id ) )
			{
				continue;
			}
			std::string name = names.count( variable.id ) ? names[variable.id] : std::string();
			static const char prefix[] = "in.var.";
			if( name.compare( 0, sizeof( prefix ) - 1, prefix ) != 0 )
			{
				continue; // not a dxc stage input; cannot be matched by semantic
			}
			name = name.substr( sizeof( prefix ) - 1 );
			SpirvVertexInput input;
			size_t digits = name.size();
			while( digits > 0 && isdigit( static_cast<unsigned char>( name[digits - 1] ) ) )
			{
				--digits;
			}
			input.semanticIndex = digits < name.size() ? uint32_t( std::stoul( name.substr( digits ) ) ) : 0;
			input.semantic = name.substr( 0, digits );
			std::transform( input.semantic.begin(), input.semantic.end(), input.semantic.begin(), []( unsigned char c ) { return char( toupper( c ) ); } );
			input.location = locations[variable.id];
			const Type* scalar = pointee;
			if( scalar && scalar->opcode == OP_TYPE_VECTOR && !scalar->operands.empty() )
			{
				scalar = typeOf( scalar->operands[0] );
			}
			if( scalar && scalar->opcode == OP_TYPE_INT && scalar->operands.size() >= 2 )
			{
				input.numericType = scalar->operands[1] ? SpirvNumericType::SINT : SpirvNumericType::UINT;
			}
			reflection.vertexInputs.push_back( input );
			continue;
		}

		if( variable.storageClass != STORAGE_UNIFORM_CONSTANT && variable.storageClass != STORAGE_UNIFORM && variable.storageClass != STORAGE_STORAGE_BUFFER )
		{
			continue;
		}
		if( !bindings.count( variable.id ) )
		{
			continue;
		}
		SpirvResource resource;
		resource.binding = bindings[variable.id];

		// Arrays of descriptors.
		if( pointee && pointee->opcode == OP_TYPE_ARRAY && pointee->operands.size() >= 2 )
		{
			resource.count = constants.count( pointee->operands[1] ) ? constants[pointee->operands[1]] : 1;
			pointeeId = pointee->operands[0];
			pointee = typeOf( pointeeId );
		}
		else if( pointee && pointee->opcode == OP_TYPE_RUNTIME_ARRAY && !pointee->operands.empty() )
		{
			resource.count = 0;
			pointeeId = pointee->operands[0];
			pointee = typeOf( pointeeId );
		}
		if( !pointee )
		{
			continue;
		}

		switch( pointee->opcode )
		{
		case OP_TYPE_SAMPLER:
			resource.type = VK_DESCRIPTOR_TYPE_SAMPLER;
			break;
		case OP_TYPE_SAMPLED_IMAGE:
			resource.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
			break;
		case OP_TYPE_IMAGE: {
			// OpTypeImage operands: sampled type, Dim, Depth, Arrayed, MS, Sampled, Format
			if( pointee->operands.size() < 6 )
			{
				continue;
			}
			uint32_t dim = pointee->operands[1];
			bool arrayed = pointee->operands[3] != 0;
			resource.multisampled = pointee->operands[4] != 0;
			bool storage = pointee->operands[5] == 2;
			if( dim == DIM_BUFFER )
			{
				resource.type = storage ? VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER : VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER;
				break;
			}
			resource.type = storage ? VK_DESCRIPTOR_TYPE_STORAGE_IMAGE : VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
			switch( dim )
			{
			case DIM_1D:
				resource.viewType = arrayed ? VK_IMAGE_VIEW_TYPE_1D_ARRAY : VK_IMAGE_VIEW_TYPE_1D;
				break;
			case DIM_2D:
				resource.viewType = arrayed ? VK_IMAGE_VIEW_TYPE_2D_ARRAY : VK_IMAGE_VIEW_TYPE_2D;
				break;
			case DIM_3D:
				resource.viewType = VK_IMAGE_VIEW_TYPE_3D;
				break;
			case DIM_CUBE:
				resource.viewType = arrayed ? VK_IMAGE_VIEW_TYPE_CUBE_ARRAY : VK_IMAGE_VIEW_TYPE_CUBE;
				break;
			default:
				continue; // subpass data, rect: not produced from HLSL
			}
			break;
		}
		case OP_TYPE_STRUCT:
			if( variable.storageClass == STORAGE_STORAGE_BUFFER || bufferBlocks.count( pointeeId ) )
			{
				resource.type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
			}
			else if( blocks.count( pointeeId ) )
			{
				resource.type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
			}
			else
			{
				continue;
			}
			break;
		default:
			continue; // e.g. acceleration structures
		}
		reflection.resources.push_back( resource );
	}

	std::sort( reflection.resources.begin(), reflection.resources.end(), []( const SpirvResource& a, const SpirvResource& b ) { return a.binding < b.binding; } );
	return true;
}

}

#endif
