// Copyright © 2023 CCP ehf.

#include "StdAfx.h"
#include "WithValidRenderContextFixture.h"
#include "WithRenderContextFixture.h"

#include <cstring>
#include <vector>

using namespace Tr2RenderContextEnum;

struct Texture : public WithValidRenderContext
{
};

TEST_F( Texture, TextureIsInvalidBeforeCreation )
{
	Tr2TextureAL tex;
	EXPECT_FALSE( tex.IsValid() );
}

TEST_F( WithRenderContext, Creating2DTextureWithoutRenderContextFails )
{
	Tr2TextureAL tex;
	ASSERT_HRESULT_FAILED( tex.Create( Tr2BitmapDimensions( 128, 128, 1, PIXEL_FORMAT_B8G8R8A8_UNORM ), Tr2GpuUsage::SHADER_RESOURCE, Tr2CpuUsage::WRITE, *renderContext ) );
}

TEST_F( WithRenderContext, CreatingCubeTextureWithoutRenderContextFails )
{
	Tr2TextureAL tex;
	ASSERT_HRESULT_FAILED( tex.Create( Tr2BitmapDimensions( TEX_TYPE_CUBE, PIXEL_FORMAT_B8G8R8A8_UNORM, 128, 128, 1, 1 ), Tr2GpuUsage::SHADER_RESOURCE, Tr2CpuUsage::WRITE, *renderContext ) );
}

TEST_F( WithRenderContext, CreatingVolumeTextureWithoutRenderContextFails )
{
	uint32_t pixels[4 * 4 * 4] = { 0 };
	Tr2SubresourceData initialData;
	initialData.m_sysMemPitch = 4 * 4;
	initialData.m_sysMemSlicePitch = 4 * 4 * 4;
	initialData.m_sysMem = pixels;

	Tr2TextureAL tex;
	ASSERT_HRESULT_FAILED( tex.Create( Tr2BitmapDimensions( TEX_TYPE_3D, PIXEL_FORMAT_B8G8R8A8_UNORM, 1, 1, 1, 1 ), Tr2GpuUsage::SHADER_RESOURCE, &initialData, *renderContext ) );
}

TEST_F( Texture, CreatingImmutable2DTextureWithoutInitialDataFails )
{
	Tr2TextureAL tex;
	ASSERT_HRESULT_FAILED( tex.Create( Tr2BitmapDimensions( 128, 128, 1, PIXEL_FORMAT_B8G8R8A8_UNORM ), Tr2GpuUsage::SHADER_RESOURCE, nullptr, *renderContext ) );
}

TEST_F( Texture, CreatingImmutableCubeTextureWithoutInitialDataFails )
{
	Tr2TextureAL tex;
	ASSERT_HRESULT_FAILED( tex.Create( Tr2BitmapDimensions( TEX_TYPE_CUBE, PIXEL_FORMAT_B8G8R8A8_UNORM, 128, 128, 1, 1 ), Tr2GpuUsage::SHADER_RESOURCE, Tr2CpuUsage::NONE, *renderContext ) );
}

TEST_F( Texture, CreatingVolumeTextureWithoutInitialDataFails )
{
	Tr2TextureAL tex;
	ASSERT_HRESULT_FAILED( tex.Create( Tr2BitmapDimensions( TEX_TYPE_3D, PIXEL_FORMAT_B8G8R8A8_UNORM, 128, 128, 128, 1 ), Tr2GpuUsage::SHADER_RESOURCE, nullptr, *renderContext ) );
}

TEST_F( Texture, Texture2DIsValidAfterCreation )
{
	ENSURE_GPU_OR_SKIP
	Tr2TextureAL tex;
	ASSERT_HRESULT_SUCCEEDED( tex.Create( Tr2BitmapDimensions( 128, 128, 1, PIXEL_FORMAT_B8G8R8A8_UNORM ), Tr2GpuUsage::SHADER_RESOURCE, Tr2CpuUsage::WRITE, *renderContext ) );
	EXPECT_TRUE( tex.IsValid() );
	EXPECT_EQ( TEX_TYPE_2D, tex.GetType() );
}

#if TRINITY_PLATFORM_SUPPORTS_TEXTURE_ARRAYS
TEST_F( Texture, Texture2DArrayIsValidAfterCreation )
{
	ENSURE_GPU_OR_SKIP
	Tr2TextureAL tex;
	ASSERT_HRESULT_SUCCEEDED( tex.Create( Tr2BitmapDimensions( TEX_TYPE_2D, PIXEL_FORMAT_B8G8R8A8_UNORM, 128, 128, 1, 1, 2 ), Tr2GpuUsage::SHADER_RESOURCE, Tr2CpuUsage::WRITE, *renderContext ) );
	EXPECT_TRUE( tex.IsValid() );
	EXPECT_EQ( TEX_TYPE_2D, tex.GetType() );
	EXPECT_EQ( 2, tex.GetArraySize() );
}
#else
TEST_F( Texture, Texture2DArrayFailsOnUnsupportingPlatforms )
{
	Tr2TextureAL tex;
	ASSERT_HRESULT_FAILED( tex.Create( Tr2BitmapDimensions( TEX_TYPE_2D, PIXEL_FORMAT_B8G8R8A8_UNORM, 128, 128, 1, 1, 2 ), Tr2GpuUsage::SHADER_RESOURCE, Tr2CpuUsage::WRITE, *renderContext ) );
}
#endif

TEST_F( Texture, TextureCubeIsValidAfterCreation )
{
	ENSURE_GPU_OR_SKIP
	Tr2TextureAL tex;
	ASSERT_HRESULT_SUCCEEDED( tex.Create( Tr2BitmapDimensions( TEX_TYPE_CUBE, PIXEL_FORMAT_B8G8R8A8_UNORM, 128, 128, 1, 1 ), Tr2GpuUsage::SHADER_RESOURCE, Tr2CpuUsage::WRITE, *renderContext ) );
	EXPECT_TRUE( tex.IsValid() );
	EXPECT_EQ( TEX_TYPE_CUBE, tex.GetType() );
}

TEST_F( Texture, TextureVolumeIsValidAfterCreation )
{
	ENSURE_GPU_OR_SKIP
	uint32_t pixels[4 * 4 * 4 * 4] = { 0 };
	Tr2SubresourceData initialData;
	initialData.m_sysMemPitch = 4 * 4;
	initialData.m_sysMemSlicePitch = 4 * 4 * 4;
	initialData.m_sysMem = pixels;

	Tr2TextureAL tex;
	ASSERT_HRESULT_SUCCEEDED( tex.Create( Tr2BitmapDimensions( TEX_TYPE_3D, PIXEL_FORMAT_B8G8R8A8_UNORM, 4, 4, 4, 1 ), Tr2GpuUsage::SHADER_RESOURCE, &initialData, *renderContext ) );
	EXPECT_TRUE( tex.IsValid() );
	EXPECT_EQ( TEX_TYPE_3D, tex.GetType() );
}

TEST_F( Texture, CanCreateMipMapped2DTexture )
{
	ENSURE_GPU_OR_SKIP
	Tr2TextureAL tex;
	ASSERT_HRESULT_SUCCEEDED( tex.Create( Tr2BitmapDimensions( 128, 128, 0, PIXEL_FORMAT_B8G8R8A8_UNORM ), Tr2GpuUsage::SHADER_RESOURCE, Tr2CpuUsage::WRITE, *renderContext ) );
	EXPECT_TRUE( tex.IsValid() );
	EXPECT_EQ( 8, tex.GetTrueMipCount() );
}

TEST_F( Texture, CanCreateMipMappedCubeTexture )
{
	ENSURE_GPU_OR_SKIP
	Tr2TextureAL tex;
	ASSERT_HRESULT_SUCCEEDED( tex.Create( Tr2BitmapDimensions( TEX_TYPE_CUBE, PIXEL_FORMAT_B8G8R8A8_UNORM, 128, 128, 1, 0 ), Tr2GpuUsage::SHADER_RESOURCE, Tr2CpuUsage::WRITE, *renderContext ) );
	EXPECT_TRUE( tex.IsValid() );
	EXPECT_EQ( 8, tex.GetTrueMipCount() );
}

TEST_F( Texture, TextureEqualsItself )
{
	ENSURE_GPU_OR_SKIP
	Tr2TextureAL tex;
	ASSERT_HRESULT_SUCCEEDED( tex.Create( Tr2BitmapDimensions( 128, 128, 1, PIXEL_FORMAT_B8G8R8A8_UNORM ), Tr2GpuUsage::SHADER_RESOURCE, Tr2CpuUsage::WRITE, *renderContext ) );
	EXPECT_TRUE( tex == tex );
}

TEST_F( Texture, DifferentTexturesAreNotEqual )
{
	ENSURE_GPU_OR_SKIP
	Tr2TextureAL tex1;
	ASSERT_HRESULT_SUCCEEDED( tex1.Create( Tr2BitmapDimensions( 128, 128, 1, PIXEL_FORMAT_B8G8R8A8_UNORM ), Tr2GpuUsage::SHADER_RESOURCE, Tr2CpuUsage::WRITE, *renderContext ) );
	Tr2TextureAL tex2;
	ASSERT_HRESULT_SUCCEEDED( tex2.Create( Tr2BitmapDimensions( 128, 128, 1, PIXEL_FORMAT_B8G8R8A8_UNORM ), Tr2GpuUsage::SHADER_RESOURCE, Tr2CpuUsage::WRITE, *renderContext ) );
	EXPECT_FALSE( tex1 == tex2 );
}

TEST_F( Texture, LockingInvalidTextureFails )
{
	Tr2TextureAL tex;
	const void* constData;
	void* data;
	uint32_t pitch;
	ASSERT_HRESULT_FAILED( tex.MapForReading( Tr2TextureSubresource( 0 ), constData, pitch, *renderContext ) );
	ASSERT_HRESULT_FAILED( tex.MapForWriting( Tr2TextureSubresource( 0 ), data, pitch, *renderContext ) );
}

TEST_F( Texture, TextureHasMemoryClass )
{
	ENSURE_GPU_OR_SKIP
	Tr2TextureAL tex;
	ASSERT_HRESULT_SUCCEEDED( tex.Create( Tr2BitmapDimensions( 128, 128, 1, PIXEL_FORMAT_B8G8R8A8_UNORM ), Tr2GpuUsage::SHADER_RESOURCE, Tr2CpuUsage::WRITE, *renderContext ) );
	auto memoryClass = tex.GetMemoryClass();
	EXPECT_TRUE( memoryClass == AL_MEMORY_VIDEO || memoryClass == AL_MEMORY_MANAGED );
}

TEST_F( Texture, CanCreateCompressed2DTexture )
{
	ENSURE_GPU_OR_SKIP
	Tr2TextureAL tex;
	ASSERT_HRESULT_SUCCEEDED( tex.Create( Tr2BitmapDimensions( 128, 128, 1, PIXEL_FORMAT_BC1_UNORM ), Tr2GpuUsage::SHADER_RESOURCE, Tr2CpuUsage::WRITE, *renderContext ) );
	EXPECT_TRUE( tex.IsValid() );
	EXPECT_EQ( TEX_TYPE_2D, tex.GetType() );
	EXPECT_EQ( PIXEL_FORMAT_BC1_UNORM, tex.GetFormat() );
}

TEST_F( Texture, CanCreateCompressedCubeTexture )
{
	ENSURE_GPU_OR_SKIP
	Tr2TextureAL tex;
	ASSERT_HRESULT_SUCCEEDED( tex.Create( Tr2BitmapDimensions( TEX_TYPE_CUBE, PIXEL_FORMAT_BC1_UNORM, 128, 128, 1, 1 ), Tr2GpuUsage::SHADER_RESOURCE, Tr2CpuUsage::WRITE, *renderContext ) );
	EXPECT_TRUE( tex.IsValid() );
	EXPECT_EQ( TEX_TYPE_CUBE, tex.GetType() );
	EXPECT_EQ( PIXEL_FORMAT_BC1_UNORM, tex.GetFormat() );
}

#if TRINITY_PLATFORM == TRINITY_VULKAN
// Data round trips through the Vulkan texture's staging paths: initial data (with padded source pitches), CPU maps with
// boxes, UpdateSubresource, image-to-image copies, cube faces, compressed mips smaller than a block and mip generation.
// (Other backends have their own constraints on which of these combinations are allowed, so these stay Vulkan-only.)
namespace
{

uint32_t Pixel( uint32_t layer, uint32_t mip, uint32_t x, uint32_t y )
{
	return ( layer << 28 ) | ( mip << 24 ) | ( y << 12 ) | x;
}

struct TexelSource
{
	std::vector<std::vector<uint32_t>> storage;
	std::vector<Tr2SubresourceData> data;
};

// One subresource per (layer, mip) in D3D order, rows padded by `padding` texels to check that pitches are honoured.
TexelSource MakeTexels( const Tr2BitmapDimensions& desc, uint32_t padding )
{
	TexelSource source;
	for( uint32_t layer = 0; layer < desc.GetArraySize(); ++layer )
	{
		for( uint32_t mip = 0; mip < desc.GetTrueMipCount(); ++mip )
		{
			const uint32_t width = desc.GetMipWidth( mip ), height = desc.GetMipHeight( mip ), pitch = width + padding;
			std::vector<uint32_t> texels( pitch * height, 0xdeadbeef );
			for( uint32_t y = 0; y < height; ++y )
			{
				for( uint32_t x = 0; x < width; ++x )
				{
					texels[y * pitch + x] = Pixel( layer, mip, x, y );
				}
			}
			source.storage.push_back( std::move( texels ) );
		}
	}
	for( auto& texels : source.storage )
	{
		Tr2SubresourceData data{};
		data.m_sysMem = texels.data();
		source.data.push_back( data );
	}
	uint32_t index = 0;
	for( uint32_t layer = 0; layer < desc.GetArraySize(); ++layer )
	{
		for( uint32_t mip = 0; mip < desc.GetTrueMipCount(); ++mip, ++index )
		{
			source.data[index].m_sysMemPitch = ( desc.GetMipWidth( mip ) + padding ) * 4;
			source.data[index].m_sysMemSlicePitch = source.data[index].m_sysMemPitch * desc.GetMipHeight( mip );
		}
	}
	return source;
}

uint32_t ReadTexel( const void* data, uint32_t pitch, uint32_t x, uint32_t y )
{
	return static_cast<const uint32_t*>( static_cast<const void*>( static_cast<const uint8_t*>( data ) + size_t( y ) * pitch ) )[x];
}

}

TEST_F( Texture, InitialDataOfEveryMipCanBeReadBack )
{
	ENSURE_GPU_OR_SKIP
	const Tr2BitmapDimensions desc( 16, 8, 0, PIXEL_FORMAT_B8G8R8A8_UNORM );
	TexelSource source = MakeTexels( desc, 3 );
	Tr2TextureAL tex;
	ASSERT_HRESULT_SUCCEEDED( tex.Create( desc, Tr2GpuUsage::SHADER_RESOURCE, Tr2CpuUsage::READ, source.data.data(), *renderContext ) );
	ASSERT_EQ( 5u, tex.GetTrueMipCount() );

	for( uint32_t mip = 0; mip < tex.GetTrueMipCount(); ++mip )
	{
		const void* data = nullptr;
		uint32_t pitch = 0;
		ASSERT_HRESULT_SUCCEEDED( tex.MapForReading( Tr2TextureSubresource( mip ), data, pitch, *renderContext ) );
		EXPECT_EQ( desc.GetMipPitch( mip ), pitch );
		for( uint32_t y = 0; y < desc.GetMipHeight( mip ); ++y )
		{
			for( uint32_t x = 0; x < desc.GetMipWidth( mip ); ++x )
			{
				ASSERT_EQ( Pixel( 0, mip, x, y ), ReadTexel( data, pitch, x, y ) ) << "mip " << mip << " at " << x << "," << y;
			}
		}
		tex.UnmapForReading( *renderContext );
	}
}

TEST_F( Texture, CubeFacesKeepTheirOwnData )
{
	ENSURE_GPU_OR_SKIP
	const Tr2BitmapDimensions desc( TEX_TYPE_CUBE, PIXEL_FORMAT_B8G8R8A8_UNORM, 8, 8, 1, 2 );
	TexelSource source = MakeTexels( desc, 0 );
	Tr2TextureAL tex;
	ASSERT_HRESULT_SUCCEEDED( tex.Create( desc, Tr2GpuUsage::SHADER_RESOURCE, Tr2CpuUsage::READ, source.data.data(), *renderContext ) );

	for( uint32_t face = 0; face < 6; ++face )
	{
		const void* data = nullptr;
		uint32_t pitch = 0;
		ASSERT_HRESULT_SUCCEEDED( tex.MapForReading( Tr2TextureSubresource( face, 1 ), data, pitch, *renderContext ) );
		EXPECT_EQ( Pixel( face, 1, 0, 0 ), ReadTexel( data, pitch, 0, 0 ) );
		EXPECT_EQ( Pixel( face, 1, 3, 2 ), ReadTexel( data, pitch, 3, 2 ) );
		tex.UnmapForReading( *renderContext );
	}
}

TEST_F( Texture, UninitializedTextureReadsAsZero )
{
	ENSURE_GPU_OR_SKIP
	Tr2TextureAL tex;
	ASSERT_HRESULT_SUCCEEDED( tex.Create( Tr2BitmapDimensions( 8, 8, 1, PIXEL_FORMAT_R16G16B16A16_FLOAT ), Tr2GpuUsage::RENDER_TARGET, Tr2CpuUsage::READ, *renderContext ) );
	const void* data = nullptr;
	uint32_t pitch = 0;
	ASSERT_HRESULT_SUCCEEDED( tex.MapForReading( Tr2TextureSubresource( 0 ), data, pitch, *renderContext ) );
	for( uint32_t i = 0; i < 8 * 8 * 8; ++i )
	{
		ASSERT_EQ( 0, static_cast<const uint8_t*>( data )[i] );
	}
	tex.UnmapForReading( *renderContext );
}

TEST_F( Texture, WritingABoxChangesOnlyThatBox )
{
	ENSURE_GPU_OR_SKIP
	Tr2TextureAL tex;
	ASSERT_HRESULT_SUCCEEDED( tex.Create( Tr2BitmapDimensions( 32, 16, 1, PIXEL_FORMAT_B8G8R8A8_UNORM ), Tr2GpuUsage::SHADER_RESOURCE, Tr2CpuUsage::READ | Tr2CpuUsage::WRITE, *renderContext ) );

	const uint32_t ltrb[] = { 4, 2, 12, 7 };
	void* target = nullptr;
	uint32_t pitch = 0;
	ASSERT_HRESULT_SUCCEEDED( tex.MapForWriting( Tr2TextureSubresource( 0 ).SetRect( ltrb ), target, pitch, *renderContext ) );
	ASSERT_LE( 8 * 4u, pitch );
	for( uint32_t y = 0; y < 5; ++y )
	{
		for( uint32_t x = 0; x < 8; ++x )
		{
			reinterpret_cast<uint32_t*>( static_cast<uint8_t*>( target ) + y * pitch )[x] = Pixel( 0, 0, x + 4, y + 2 );
		}
	}
	tex.UnmapForWriting( *renderContext );

	const void* data = nullptr;
	ASSERT_HRESULT_SUCCEEDED( tex.MapForReading( Tr2TextureSubresource( 0 ), data, pitch, *renderContext ) );
	for( uint32_t y = 0; y < 16; ++y )
	{
		for( uint32_t x = 0; x < 32; ++x )
		{
			const bool inside = x >= 4 && x < 12 && y >= 2 && y < 7;
			ASSERT_EQ( inside ? Pixel( 0, 0, x, y ) : 0u, ReadTexel( data, pitch, x, y ) ) << x << "," << y;
		}
	}
	tex.UnmapForReading( *renderContext );
}

TEST_F( Texture, UpdateSubresourceWritesABox )
{
	ENSURE_GPU_OR_SKIP
	Tr2TextureAL tex;
	ASSERT_HRESULT_SUCCEEDED( tex.Create( Tr2BitmapDimensions( 16, 16, 1, PIXEL_FORMAT_B8G8R8A8_UNORM ), Tr2GpuUsage::RENDER_TARGET | Tr2GpuUsage::SHADER_RESOURCE, Tr2CpuUsage::READ, *renderContext ) );

	uint32_t texels[3 * 5]; // 2x3 region in rows of 5 texels
	for( uint32_t y = 0; y < 3; ++y )
	{
		for( uint32_t x = 0; x < 5; ++x )
		{
			texels[y * 5 + x] = Pixel( 0, 0, x + 9, y + 1 );
		}
	}
	const uint32_t ltrb[] = { 9, 1, 11, 4 };
	ASSERT_HRESULT_SUCCEEDED( tex.UpdateSubresource( Tr2TextureSubresource( 0 ).SetRect( ltrb ), texels, 5 * 4, 0, *renderContext ) );

	const void* data = nullptr;
	uint32_t pitch = 0;
	ASSERT_HRESULT_SUCCEEDED( tex.MapForReading( Tr2TextureSubresource( 0 ), data, pitch, *renderContext ) );
	for( uint32_t y = 0; y < 16; ++y )
	{
		for( uint32_t x = 0; x < 16; ++x )
		{
			const bool inside = x >= 9 && x < 11 && y >= 1 && y < 4;
			ASSERT_EQ( inside ? Pixel( 0, 0, x, y ) : 0u, ReadTexel( data, pitch, x, y ) ) << x << "," << y;
		}
	}
	tex.UnmapForReading( *renderContext );
}

TEST_F( Texture, CopySubresourceRegionMovesARectangle )
{
	ENSURE_GPU_OR_SKIP
	const Tr2BitmapDimensions srcDesc( 16, 16, 1, PIXEL_FORMAT_B8G8R8A8_UNORM );
	TexelSource source = MakeTexels( srcDesc, 0 );
	Tr2TextureAL src;
	ASSERT_HRESULT_SUCCEEDED( src.Create( srcDesc, Tr2GpuUsage::SHADER_RESOURCE, source.data.data(), *renderContext ) );
	Tr2TextureAL dst;
	ASSERT_HRESULT_SUCCEEDED( dst.Create( Tr2BitmapDimensions( 8, 8, 1, PIXEL_FORMAT_B8G8R8A8_UNORM ), Tr2GpuUsage::RENDER_TARGET, Tr2CpuUsage::READ, *renderContext ) );

	const uint32_t srcRect[] = { 5, 6, 9, 8 };
	const uint32_t dstRect[] = { 2, 3, 8, 8 };
	ASSERT_HRESULT_SUCCEEDED( dst.CopySubresourceRegion( Tr2TextureSubresource( 0 ).SetRect( dstRect ), src, Tr2TextureSubresource( 0 ).SetRect( srcRect ), *renderContext ) );

	const void* data = nullptr;
	uint32_t pitch = 0;
	ASSERT_HRESULT_SUCCEEDED( dst.MapForReading( Tr2TextureSubresource( 0 ), data, pitch, *renderContext ) );
	for( uint32_t y = 0; y < 8; ++y )
	{
		for( uint32_t x = 0; x < 8; ++x )
		{
			// The copy is cut to the smaller of the two rectangles: 4x2 texels from (5,6) land at (2,3).
			const bool inside = x >= 2 && x < 6 && y >= 3 && y < 5;
			ASSERT_EQ( inside ? Pixel( 0, 0, x + 3, y + 3 ) : 0u, ReadTexel( data, pitch, x, y ) ) << x << "," << y;
		}
	}
	dst.UnmapForReading( *renderContext );
}

TEST_F( Texture, CompressedMipsSmallerThanABlockRoundTrip )
{
	ENSURE_GPU_OR_SKIP
	const Tr2BitmapDimensions desc( 8, 8, 0, PIXEL_FORMAT_BC1_UNORM );
	ASSERT_EQ( 4u, desc.GetTrueMipCount() );
	std::vector<std::vector<uint8_t>> blocks;
	std::vector<Tr2SubresourceData> initial;
	for( uint32_t mip = 0; mip < desc.GetTrueMipCount(); ++mip )
	{
		std::vector<uint8_t> bytes( desc.GetMipSize( mip ) );
		for( size_t i = 0; i < bytes.size(); ++i )
		{
			bytes[i] = uint8_t( mip * 64 + i * 7 + 1 );
		}
		blocks.push_back( std::move( bytes ) );
	}
	for( uint32_t mip = 0; mip < desc.GetTrueMipCount(); ++mip )
	{
		Tr2SubresourceData data{};
		data.m_sysMem = blocks[mip].data();
		data.m_sysMemPitch = desc.GetMipPitch( mip );
		initial.push_back( data );
	}
	Tr2TextureAL tex;
	ASSERT_HRESULT_SUCCEEDED( tex.Create( desc, Tr2GpuUsage::SHADER_RESOURCE, Tr2CpuUsage::READ, initial.data(), *renderContext ) );

	for( uint32_t mip = 0; mip < desc.GetTrueMipCount(); ++mip )
	{
		const void* data = nullptr;
		uint32_t pitch = 0;
		ASSERT_HRESULT_SUCCEEDED( tex.MapForReading( Tr2TextureSubresource( mip ), data, pitch, *renderContext ) );
		ASSERT_EQ( desc.GetMipPitch( mip ), pitch );
		EXPECT_EQ( 0, memcmp( blocks[mip].data(), data, blocks[mip].size() ) ) << "mip " << mip;
		tex.UnmapForReading( *renderContext );
	}
}

TEST_F( Texture, GeneratedMipsOfASolidColourAreThatColour )
{
	ENSURE_GPU_OR_SKIP
	const Tr2BitmapDimensions desc( 16, 16, 0, PIXEL_FORMAT_R8G8B8A8_UNORM );
	std::vector<uint32_t> solid( 16 * 16, 0xff40a0c0u );
	std::vector<Tr2SubresourceData> initial( desc.GetTrueMipCount() );
	initial[0].m_sysMem = solid.data();
	initial[0].m_sysMemPitch = 16 * 4;
	Tr2TextureAL tex;
	ASSERT_HRESULT_SUCCEEDED( tex.Create( desc, Tr2MsaaDesc(), Tr2GpuUsage::RENDER_TARGET | Tr2GpuUsage::SHADER_RESOURCE, Tr2CpuUsage::READ, initial.data(), *renderContext ) );
	ASSERT_HRESULT_SUCCEEDED( tex.GenerateMipMaps( *renderContext ) );

	for( uint32_t mip = 1; mip < desc.GetTrueMipCount(); ++mip )
	{
		const void* data = nullptr;
		uint32_t pitch = 0;
		ASSERT_HRESULT_SUCCEEDED( tex.MapForReading( Tr2TextureSubresource( mip ), data, pitch, *renderContext ) );
		EXPECT_EQ( 0xff40a0c0u, ReadTexel( data, pitch, 0, 0 ) ) << "mip " << mip;
		EXPECT_EQ( 0xff40a0c0u, ReadTexel( data, pitch, desc.GetMipWidth( mip ) - 1, desc.GetMipHeight( mip ) - 1 ) ) << "mip " << mip;
		tex.UnmapForReading( *renderContext );
	}
}
#endif
