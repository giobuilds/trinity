// Copyright © 2023 CCP ehf.

#include "StdAfx.h"
#include "WithValidRenderContextFixture.h"
#include "WithRenderContextFixture.h"

using namespace Tr2RenderContextEnum;

struct GpuTimer : public WithValidRenderContext
{
};


TEST_F( GpuTimer, TimerIsInvalidBeforeCreation )
{
	Tr2GpuTimerAL timer;
	EXPECT_FALSE( timer.IsValid() );
}

TEST_F( WithRenderContext, CreatingWithoutRenderContext )
{
	Tr2GpuTimerAL timer;
	auto hrResult = timer.Create( *renderContext );
	ASSERT_HRESULT_FAILED( hrResult );
}

// GPU timers are disabled for now on Metal because of stability issues
#if TRINITY_PLATFORM != TRINITY_METAL

TEST_F( GpuTimer, IsValidAfterCreation )
{
	ENSURE_GPU_OR_SKIP
	Tr2GpuTimerAL timer;
	auto hrResult = timer.Create( *renderContext );
	ASSERT_HRESULT_SUCCEEDED( hrResult );
	EXPECT_TRUE( timer.IsValid() );
}

TEST_F( GpuTimer, Begins )
{
	ENSURE_GPU_OR_SKIP
	Tr2GpuTimerAL timer;
	const auto hrResult = timer.Create( *renderContext );
	ASSERT_HRESULT_SUCCEEDED( hrResult );
	const auto begins = timer.Begin( *renderContext );
	EXPECT_TRUE( begins );
}

TEST_F( GpuTimer, ValidAfterStopping )
{
	ENSURE_GPU_OR_SKIP
	Tr2GpuTimerAL timer;
	const auto hrResult = timer.Create( *renderContext );
	ASSERT_HRESULT_SUCCEEDED( hrResult );
	timer.Begin( *renderContext );
	timer.End( *renderContext );
	EXPECT_TRUE( timer.IsValid() );
}

#endif

#if TRINITY_PLATFORM == TRINITY_VULKAN
TEST_F( GpuTimer, MeasuresElapsedTime )
{
	ENSURE_GPU_OR_SKIP
	Tr2GpuTimerAL timer;
	ASSERT_HRESULT_SUCCEEDED( timer.Create( *renderContext ) );
	EXPECT_EQ( -1.0f, timer.GetTime( *renderContext ) ); // nothing measured yet
	ASSERT_TRUE( timer.Begin( *renderContext ) );
	EXPECT_FALSE( timer.Begin( *renderContext ) ); // one interval at a time
	for( int i = 0; i < 16; ++i )
	{
		ASSERT_HRESULT_SUCCEEDED( renderContext->Clear( Tr2RenderContextEnum::CLEARFLAGS_TARGET, 0xff000000 | i, 1.0f ) );
	}
	timer.End( *renderContext );

	Tr2FenceAL fence;
	ASSERT_HRESULT_SUCCEEDED( fence.Create( *renderContext ) );
	ASSERT_HRESULT_SUCCEEDED( fence.PutFence( *renderContext ) );
	ASSERT_HRESULT_SUCCEEDED( fence.Wait( *renderContext ) );

	const float seconds = timer.GetTime( *renderContext );
	EXPECT_GE( seconds, 0.0f );
	EXPECT_LT( seconds, 1.0f );
	EXPECT_TRUE( timer.Begin( *renderContext ) ); // read, so it can measure again
	timer.End( *renderContext );
}
#endif
