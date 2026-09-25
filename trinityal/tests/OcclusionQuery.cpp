// Copyright © 2023 CCP ehf.

#include "StdAfx.h"
#include "WithValidRenderContextFixture.h"
#include "WithRenderContextFixture.h"

#include <map>
#include <string>

using namespace Tr2RenderContextEnum;

struct OcclusionQuery : public WithValidRenderContext
{
};


TEST_F( OcclusionQuery, OcclusionQueryIsInvalidBeforeCreation )
{
	Tr2OcclusionQueryAL query;
	EXPECT_FALSE( query.IsValid() );
}

TEST_F( WithRenderContext, CreatingOcclusionQueryWithoutRenderContextFails )
{
	Tr2OcclusionQueryAL query;
	ASSERT_HRESULT_FAILED( query.Create( *renderContext ) );
	EXPECT_FALSE( query.IsValid() );
}

TEST_F( OcclusionQuery, OcclusionQueryIsValidAfterCreation )
{
	ENSURE_GPU_OR_SKIP
	Tr2OcclusionQueryAL query;
	ASSERT_HRESULT_SUCCEEDED( query.Create( *renderContext ) );
	EXPECT_TRUE( query.IsValid() );
}

TEST_F( OcclusionQuery, RuningInvalidOcclusionQueryFails )
{
	Tr2OcclusionQueryAL query;

	ASSERT_HRESULT_FAILED( query.Begin( *renderContext ) );
	ASSERT_HRESULT_FAILED( query.End( *renderContext ) );
	uint32_t count = 0;
	ASSERT_HRESULT_FAILED( query.GetPixelCount( *renderContext, count, Tr2OcclusionQueryAL::WAIT ) );
}

TEST_F( OcclusionQuery, CanRunOcclusionQuery )
{
	ENSURE_GPU_OR_SKIP
	Tr2OcclusionQueryAL query;
	ASSERT_HRESULT_SUCCEEDED( query.Create( *renderContext ) );
	ASSERT_TRUE( query.IsValid() );

	ASSERT_HRESULT_SUCCEEDED( query.Begin( *renderContext ) );
	ASSERT_HRESULT_SUCCEEDED( query.End( *renderContext ) );
}

TEST_F( OcclusionQuery, CanGetOcclusionQueryResultSynchronously )
{
	ENSURE_GPU_OR_SKIP
	Tr2OcclusionQueryAL query;
	ASSERT_HRESULT_SUCCEEDED( query.Create( *renderContext ) );
	ASSERT_TRUE( query.IsValid() );

	ASSERT_HRESULT_SUCCEEDED( query.Begin( *renderContext ) );
	ASSERT_HRESULT_SUCCEEDED( query.End( *renderContext ) );

	uint32_t count = 0;
	ASSERT_HRESULT_SUCCEEDED( query.GetPixelCount( *renderContext, count, Tr2OcclusionQueryAL::WAIT ) );
	EXPECT_EQ( 0, count );
}

TEST_F( OcclusionQuery, OcclusionQueryEqualsItself )
{
	ENSURE_GPU_OR_SKIP
	Tr2OcclusionQueryAL query;
	ASSERT_HRESULT_SUCCEEDED( query.Create( *renderContext ) );
	EXPECT_TRUE( query == query );
}

TEST_F( OcclusionQuery, DifferentOcclusionQueriesAreNotEqual )
{
	ENSURE_GPU_OR_SKIP
	Tr2OcclusionQueryAL query1;
	ASSERT_HRESULT_SUCCEEDED( query1.Create( *renderContext ) );
	Tr2OcclusionQueryAL query2;
	ASSERT_HRESULT_SUCCEEDED( query2.Create( *renderContext ) );
	EXPECT_FALSE( query1 == query2 );
}

TEST_F( OcclusionQuery, OcclusionQueryHasMemoryClass )
{
	ENSURE_GPU_OR_SKIP
	Tr2OcclusionQueryAL query;
	ASSERT_HRESULT_SUCCEEDED( query.Create( *renderContext ) );
	auto memoryClass = query.GetMemoryClass();
	EXPECT_TRUE( memoryClass == AL_MEMORY_VIDEO || memoryClass == AL_MEMORY_MANAGED );
}

#if TRINITY_PLATFORM == TRINITY_VULKAN
namespace
{

// Draws a two-triangle quad over the left half of a 64x64 target (32 * 64 samples) with a depth buffer bound (depth test
// off), calling begin() just before and end() just after the draw, then restores the render context state it changed.
// The depth buffer is there because lavapipe (Mesa 26.0) counts no occlusion samples without a depth attachment, where
// RADV counts them as the spec says; the engine binds one whenever it uses occlusion queries anyway.
template <typename Begin, typename End>
void DrawLeftHalf( Tr2PrimaryRenderContextAL& renderContext, Begin begin, End end )
{
	uint8_t vsBytecode[] = {
#include INCLUDE_SHADER_CODE( PositionOnly.vs )
	};
	uint8_t psBytecode[] = {
#include INCLUDE_SHADER_CODE( ConstantColor.ps )
	};
	Tr2ShaderAL vs;
	ASSERT_HRESULT_SUCCEEDED( vs.Create( VERTEX_SHADER, vsBytecode, Tr2ShaderSignatureAL().Add( Tr2VertexDefinition::POSITION, 0, 0, Tr2ShaderPipelineInputAL::FLOAT, 3 ), "", renderContext ) );
	Tr2ShaderAL ps;
	ASSERT_HRESULT_SUCCEEDED( ps.Create( PIXEL_SHADER, psBytecode, Tr2ShaderSignatureAL(), "", renderContext ) );
	Tr2ShaderAL shaders[] = { vs, ps };
	Tr2ShaderProgramAL sp;
	ASSERT_HRESULT_SUCCEEDED( sp.Create( shaders, 2, renderContext ) );

	float vertices[] = { -1, -1, 0, -1, 1, 0, 0, -1, 0, 0, 1, 0 };
	Tr2BufferAL vb;
	ASSERT_HRESULT_SUCCEEDED( vb.Create( 12, 4, Tr2GpuUsage::VERTEX_BUFFER, Tr2CpuUsage::NONE, vertices, renderContext ) );
	Tr2VertexDefinition definition;
	definition.Add( Tr2VertexDefinition::FLOAT32_3, Tr2VertexDefinition::POSITION );
	Tr2VertexLayoutAL layout;
	ASSERT_HRESULT_SUCCEEDED( layout.Create( definition, renderContext ) );
	Tr2TextureAL target;
	ASSERT_HRESULT_SUCCEEDED( target.Create( Tr2BitmapDimensions( 64, 64, 1, PIXEL_FORMAT_B8G8R8A8_UNORM ), Tr2GpuUsage::RENDER_TARGET, renderContext ) );
	Tr2TextureAL depth;
	ASSERT_HRESULT_SUCCEEDED( depth.Create( Tr2BitmapDimensions( 64, 64, 1, PIXEL_FORMAT_D24_UNORM_S8_UINT ), Tr2GpuUsage::DEPTH_STENCIL, renderContext ) );

	Tr2Viewport previousViewport;
	renderContext.GetViewport( previousViewport );
	ASSERT_HRESULT_SUCCEEDED( renderContext.PushRenderTarget() );
	ASSERT_HRESULT_SUCCEEDED( renderContext.PushDepthStencil() );
	ASSERT_HRESULT_SUCCEEDED( renderContext.SetRenderTarget( target ) );
	ASSERT_HRESULT_SUCCEEDED( renderContext.SetDepthStencil( depth ) );
	ASSERT_HRESULT_SUCCEEDED( renderContext.SetViewport( Tr2Viewport( 64, 64 ) ) );
	ASSERT_HRESULT_SUCCEEDED( renderContext.Clear( CLEARFLAGS_TARGET | CLEARFLAGS_ZBUFFER, 0, 1.0f ) );
	ASSERT_HRESULT_SUCCEEDED( renderContext.SetStreamSource( 0, vb, 0, 12 ) );
	ASSERT_HRESULT_SUCCEEDED( renderContext.SetVertexLayout( layout ) );
	ASSERT_HRESULT_SUCCEEDED( renderContext.SetShaderProgram( sp ) );
	ASSERT_HRESULT_SUCCEEDED( renderContext.SetTopology( TOP_TRIANGLE_STRIP ) );
	ASSERT_HRESULT_SUCCEEDED( renderContext.SetRenderState( RS_ZENABLE, 0 ) );
	ASSERT_HRESULT_SUCCEEDED( renderContext.SetRenderState( RS_CULLMODE, CULLMODE_NONE ) );

	begin();
	ASSERT_HRESULT_SUCCEEDED( renderContext.DrawPrimitive( 0, 2 ) );
	end();

	ASSERT_HRESULT_SUCCEEDED( renderContext.PopDepthStencil() );
	ASSERT_HRESULT_SUCCEEDED( renderContext.PopRenderTarget() );
	ASSERT_HRESULT_SUCCEEDED( renderContext.SetViewport( previousViewport ) );
	ASSERT_HRESULT_SUCCEEDED( renderContext.SetTopology( TOP_TRIANGLES ) );
	ASSERT_HRESULT_SUCCEEDED( renderContext.SetStreamSource( 0, Tr2BufferAL(), 0, 0 ) );
	ASSERT_HRESULT_SUCCEEDED( renderContext.SetShaderProgram( Tr2ShaderProgramAL() ) );
}

}

TEST_F( OcclusionQuery, CountsTheSamplesOfADraw )
{
	ENSURE_GPU_OR_SKIP
	Tr2OcclusionQueryAL query;
	ASSERT_HRESULT_SUCCEEDED( query.Create( *renderContext ) );
	DrawLeftHalf(
		*renderContext,
		[&] { ASSERT_HRESULT_SUCCEEDED( query.Begin( *renderContext ) ); },
		[&] { ASSERT_HRESULT_SUCCEEDED( query.End( *renderContext ) ); } );

	uint32_t count = 0;
	EXPECT_EQ( S_FALSE, query.GetPixelCount( *renderContext, count, Tr2OcclusionQueryAL::DO_NOT_WAIT ).GetResult() ); // not submitted yet
	ASSERT_EQ( S_OK, query.GetPixelCount( *renderContext, count, Tr2OcclusionQueryAL::WAIT ).GetResult() );
	EXPECT_EQ( 32u * 64u, count );
}

TEST_F( OcclusionQuery, PipelineStatisticsCountTheDraw )
{
	ENSURE_GPU_OR_SKIP
	Tr2PipelineStatsQueryAL query;
	if( FAILED( query.Create( *renderContext ) ) )
	{
		GTEST_SKIP() << "The device has no pipeline statistics queries.";
	}
	DrawLeftHalf(
		*renderContext,
		[&] { ASSERT_HRESULT_SUCCEEDED( query.Begin( *renderContext ) ); },
		[&] { ASSERT_HRESULT_SUCCEEDED( query.End( *renderContext ) ); } );

	Tr2FenceAL fence;
	ASSERT_HRESULT_SUCCEEDED( fence.Create( *renderContext ) );
	ASSERT_HRESULT_SUCCEEDED( fence.PutFence( *renderContext ) );
	ASSERT_HRESULT_SUCCEEDED( fence.Wait( *renderContext ) );

	Tr2PipelineStatsDataAL data;
	ASSERT_EQ( S_OK, query.GetStats( data, *renderContext ).GetResult() );
	ASSERT_EQ( 11u, Tr2PipelineStatsQueryAL::GetValueCount( data ) );
	std::map<std::string, uint64_t> values;
	for( size_t i = 0; i < Tr2PipelineStatsQueryAL::GetValueCount( data ); ++i )
	{
		values[Tr2PipelineStatsQueryAL::GetLabel( data, i )] = Tr2PipelineStatsQueryAL::GetValue( data, i );
	}
	EXPECT_EQ( 4u, values["IAVertices"] );
	EXPECT_EQ( 2u, values["IAPrimitives"] );
	EXPECT_GE( values["VSInvocations"], 4u );
	// Implementation-dependent granularity: lavapipe counts pixels (2048+), RADV counts 2x2 quads (544 here).
	EXPECT_GT( values["PSInvocations"], 0u );
	EXPECT_EQ( 0u, values["CSInvocations"] );
}
#endif
