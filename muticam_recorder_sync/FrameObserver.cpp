#include <FrameObserver.h>
#include <OpenCVVideoRecorder.h>
#include <iostream>

namespace AVT {
namespace VmbAPI {
namespace Examples {



//
// This is our callback routine that will be executed on every received frame.
// Triggered by the API.
//
// Parameters:
//  [in]    pFrame          The frame returned from the API
//
void FrameObserver::FrameReceived( const FramePtr pFrame )
{
    bool bQueueDirectly = true;
    VmbFrameStatusType eReceiveStatus;

    if(     0 != receivers(SIGNAL(FrameReceivedSignal(int)) ) 
        &&  VmbErrorSuccess == pFrame->GetReceiveStatus( eReceiveStatus ) )
    {
        // Lock the frame queue
        m_FramesMutex.lock();

        // Add frame to queue
        m_Frames.push( pFrame );
		push((double)clock(), observer_id);
		//std::cout << "FrameObserver timeq address : " << &timequeue << std::endl;
        // Unlock frame queue
        m_FramesMutex.unlock();
        // Emit the frame received signal


        emit FrameReceivedSignal(eReceiveStatus * 10 + observer_id );
        bQueueDirectly = false;
    }

    // If any error occurred we queue the frame without notification
    if( true == bQueueDirectly )
    {
        m_pCamera->QueueFrame( pFrame );
    }
}

//
// After the view has been notified about a new frame it can pick it up.
// It is then removed from the internal queue
//
// Returns:
//  A shared pointer to the latest frame
//
FramePtr FrameObserver::GetFrame()
{
    // Lock the frame queue
    m_FramesMutex.lock();
    // Pop frame from queue
    FramePtr res;
    if( !m_Frames.empty() )
    {
        res = m_Frames.front();
        m_Frames.pop();
    }
    // Unlock frame queue
    m_FramesMutex.unlock();
    return res;
}

//
// Clears the internal (double buffering) frame queue
//
void FrameObserver::ClearFrameQueue()
{
    // Lock the frame queue
    m_FramesMutex.lock();
    // Clear the frame queue and release the memory
    std::queue<FramePtr> empty;
    std::swap( m_Frames, empty );
    // Unlock the frame queue
    m_FramesMutex.unlock();
}

}}} // namespace AVT::VmbAPI::Examples
