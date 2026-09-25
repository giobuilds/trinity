// Copyright © 2023 CCP ehf.

#include "StdAfx.h"
#include "WithValidRenderContextFixture.h"
#include "WithRenderContextFixture.h"

using namespace Tr2RenderContextEnum;

struct Fence : public WithValidRenderContext
{
};


TEST_F( Fence, FenceIsInvalidBeforeCreation )
{
	Tr2FenceAL fence;
	EXPECT_FALSE( fence.IsValid() );
}

TEST_F( WithRenderContext, CreatingFenceWithoutRenderContextFails )
{
	Tr2FenceAL fence;
	ASSERT_HRESULT_FAILED( fence.Create( *renderContext ) );
	EXPECT_FALSE( fence.IsValid() );
}

TEST_F( Fence, FenceIsValidAfterCreation )
{
	ENSURE_GPU_OR_SKIP
	Tr2FenceAL fence;
	ASSERT_HRESULT_SUCCEEDED( fence.Create( *renderContext ) );
	EXPECT_TRUE( fence.IsValid() );
}

TEST_F( Fence, UsingInvalidFenceFails )
{
	Tr2FenceAL fence;

	ASSERT_HRESULT_FAILED( fence.PutFence( *renderContext ) );

	bool reached = false;
	ASSERT_HRESULT_FAILED( fence.IsReached( reached, *renderContext ) );
}

TEST_F( Fence, CanUseFence )
{
	ENSURE_GPU_OR_SKIP
	Tr2FenceAL fence;
	ASSERT_HRESULT_SUCCEEDED( fence.Create( *renderContext ) );
	ASSERT_TRUE( fence.IsValid() );

	ASSERT_HRESULT_SUCCEEDED( fence.PutFence( *renderContext ) );
	bool reached = false;
	ASSERT_HRESULT_SUCCEEDED( fence.IsReached( reached, *renderContext ) );
}

TEST_F( Fence, FenceEqualsItself )
{
	ENSURE_GPU_OR_SKIP
	Tr2FenceAL fence;
	ASSERT_HRESULT_SUCCEEDED( fence.Create( *renderContext ) );
	EXPECT_TRUE( fence == fence );
}

TEST_F( Fence, DifferentFencesAreNotEqual )
{
	ENSURE_GPU_OR_SKIP
	Tr2FenceAL fence1;
	ASSERT_HRESULT_SUCCEEDED( fence1.Create( *renderContext ) );
	Tr2FenceAL fence2;
	ASSERT_HRESULT_SUCCEEDED( fence2.Create( *renderContext ) );
	EXPECT_FALSE( fence1 == fence2 );
}

TEST_F( Fence, FenceHasMemoryClass )
{
	ENSURE_GPU_OR_SKIP
	Tr2FenceAL fence;
	ASSERT_HRESULT_SUCCEEDED( fence.Create( *renderContext ) );
	auto memoryClass = fence.GetMemoryClass();
	EXPECT_TRUE( memoryClass == AL_MEMORY_VIDEO || memoryClass == AL_MEMORY_MANAGED );
}

#if TRINITY_PLATFORM == TRINITY_VULKAN
TEST_F( Fence, FenceIsReachedOnceTheGpuHasFinished )
{
	ENSURE_GPU_OR_SKIP
	Tr2FenceAL fence;
	ASSERT_HRESULT_SUCCEEDED( fence.Create( *renderContext ) );
	bool reached = true;
	EXPECT_HRESULT_FAILED( fence.IsReached( reached, *renderContext ) ); // not put yet

	ASSERT_HRESULT_SUCCEEDED( renderContext->Clear( Tr2RenderContextEnum::CLEARFLAGS_TARGET, 0xff000000, 1.0f ) );
	ASSERT_HRESULT_SUCCEEDED( fence.PutFence( *renderContext ) );
	ASSERT_HRESULT_SUCCEEDED( fence.IsReached( reached, *renderContext ) );
	EXPECT_FALSE( reached ); // the clear is recorded but not submitted

	ASSERT_HRESULT_SUCCEEDED( fence.Wait( *renderContext ) );
	ASSERT_HRESULT_SUCCEEDED( fence.IsReached( reached, *renderContext ) );
	EXPECT_TRUE( reached );
}

TEST_F( Fence, RenderedFrameNumberFollowsTheGpu )
{
	ENSURE_GPU_OR_SKIP
	ASSERT_HRESULT_SUCCEEDED( renderContext->Clear( Tr2RenderContextEnum::CLEARFLAGS_TARGET, 0xff000000, 1.0f ) );
	const uint64_t frame = renderContext->GetRecordingFrameNumber();
	ASSERT_HRESULT_SUCCEEDED( renderContext->Present() );
	EXPECT_EQ( frame + 1, renderContext->GetRecordingFrameNumber() );

	Tr2FenceAL fence;
	ASSERT_HRESULT_SUCCEEDED( fence.Create( *renderContext ) );
	ASSERT_HRESULT_SUCCEEDED( renderContext->Clear( Tr2RenderContextEnum::CLEARFLAGS_TARGET, 0xff000000, 1.0f ) );
	ASSERT_HRESULT_SUCCEEDED( fence.PutFence( *renderContext ) );
	ASSERT_HRESULT_SUCCEEDED( fence.Wait( *renderContext ) );
	EXPECT_EQ( frame, renderContext->GetRenderedFrameNumber() );
}
#endif
