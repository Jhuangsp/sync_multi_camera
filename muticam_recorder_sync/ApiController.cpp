/*=============================================================================
  Copyright (C) 2012 - 2016 Allied Vision Technologies.  All Rights Reserved.

  Redistribution of this file, in original or modified form, without
  prior written consent of Allied Vision Technologies is prohibited.

-------------------------------------------------------------------------------

  File:        ApiController.cpp

  Description: Implementation file for the ApiController helper class that
               demonstrates how to implement an asynchronous, continuous image
               acquisition with VimbaCPP.

-------------------------------------------------------------------------------

  THIS SOFTWARE IS PROVIDED BY THE AUTHOR "AS IS" AND ANY EXPRESS OR IMPLIED
  WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF TITLE,
  NON-INFRINGEMENT, MERCHANTABILITY AND FITNESS FOR A PARTICULAR  PURPOSE ARE
  DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT,
  INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
  (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
  LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
  AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR
  TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
  OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

=============================================================================*/

#include "ApiController.h"
#include <sstream>
#include <iostream>
#include <vector>
#include "Common/StreamSystemInfo.h"
#include "Common/ErrorCodeToMessage.h"

namespace AVT {
namespace VmbAPI {
namespace Examples {

enum    { NUM_FRAMES=3, };

ApiController::ApiController()
    // Get a reference to the Vimba singleton
    : m_system( VimbaSystem::GetInstance() )
{
}

ApiController::~ApiController()
{
}

//
// Translates Vimba error codes to readable error messages
//
// Parameters:
//  [in]    eErr        The error code to be converted to string
//
// Returns:
//  A descriptive string representation of the error code
//
std::string ApiController::ErrorCodeToMessage( VmbErrorType eErr ) const
{
	return AVT::VmbAPI::Examples::ErrorCodeToMessage(eErr);
}

//
// Starts the Vimba API and loads all transport layers
//
// Returns:
//  An API status code
//
VmbErrorType ApiController::StartUp()
{
    VmbErrorType res;

    // Start Vimba
    res = m_system.Startup();
    if( VmbErrorSuccess == res )
    {
        // This will be wrapped in a shared_ptr so we don't delete it
        SP_SET( m_pCameraObserver , new CameraObserver() );
        // Register an observer whose callback routine gets triggered whenever a camera is plugged in or out
        res = m_system.RegisterCameraListObserver( m_pCameraObserver );
    }

    return res;
}

//
// Shuts down the API
//
void ApiController::ShutDown()
{
    // Release Vimba
    m_system.Shutdown();
}

/*** helper function to set image size to a value that is dividable by modulo 2.
\note this is needed because VimbaImageTransform does not support odd values for some input formats
*/
inline VmbErrorType SetValueIntMod2( const CameraPtr &camera, const std::string &featureName, VmbInt64_t &storage )
{
    VmbErrorType    res;
    FeaturePtr      pFeature;
    res = SP_ACCESS( camera )->GetFeatureByName( featureName.c_str(), pFeature );
    if( VmbErrorSuccess == res )
    {
        VmbInt64_t minValue,maxValue;
        res = SP_ACCESS( pFeature )->GetRange( minValue,maxValue );
        if( VmbErrorSuccess == res )
        {
            maxValue = ( maxValue>>1 )<<1; // mod 2 dividable
            res = SP_ACCESS( pFeature )->SetValue( maxValue );
            if( VmbErrorSuccess == res )
            {
                storage = maxValue;
            }
        }
    }
    return res;
}

//
// Opens the given camera
// Sets the maximum possible Ethernet packet size
// Adjusts the image format
// Sets up the observer that will be notified on every incoming frame
// Calls the API convenience function to start image acquisition
// Closes the camera in case of failure
//
// Parameters:
//  [in]    rStrCameraID    The ID of the camera to open as reported by Vimba
//
// Returns:
//  An API status code
//
VmbErrorType ApiController::StartContinuousImageAcquisition( const std::vector<std::string> &rStrCameraIDs )
{
    // Open the desired camera by its ID
	int num_cam = rStrCameraIDs.size();
	std::cout << "Starting " << num_cam << " cameras..." << std::endl;
	std::cout << "Open camera" << std::endl;
	VmbErrorType res;
	m_pCameras.clear();
	for (int i = 0; i < num_cam; i++) {
		std::cout << rStrCameraIDs[i].c_str() << std::endl;
		res = m_system.OpenCameraByID(rStrCameraIDs[i].c_str(), VmbAccessModeFull, m_pCamera);
		m_pCameras.push_back(m_pCamera);
		if (res != VmbErrorSuccess) break;
	}
    if( VmbErrorSuccess == res )
    {

        // Set the GeV packet size to the highest possible value
        // (In this example we do not test whether this cam actually is a GigE cam)
		std::cout << "Adjust Packet Size" << std::endl;
		for (int i = 0; i < num_cam; i++){
			FeaturePtr pCommandFeature;
			if (VmbErrorSuccess == SP_ACCESS(m_pCameras[i])->GetFeatureByName("GVSPAdjustPacketSize", pCommandFeature))
			{
				if (VmbErrorSuccess == SP_ACCESS(pCommandFeature)->RunCommand())
				{
					bool bIsCommandDone = false;
					do
					{
						if (VmbErrorSuccess != SP_ACCESS(pCommandFeature)->IsCommandDone(bIsCommandDone))
						{
							break;
						}
					} while (false == bIsCommandDone);
				}
			}
			res = SetValueIntMod2(m_pCameras[i], "Width", m_nWidth);
			if (res != VmbErrorSuccess) break;
			res = SetValueIntMod2(m_pCameras[i], "Height", m_nHeight);
			if (res != VmbErrorSuccess) break;
		}
        if( VmbErrorSuccess == res )
        {
			std::cout << "Adjust Frame Rate" << std::endl;
			for (int i = 0; i < num_cam; i++) {
				m_FPS = 15.0;
				FeaturePtr pFeatureFPS;  
				res = SP_ACCESS(m_pCameras[i])->GetFeatureByName("AcquisitionFrameRateAbs", pFeatureFPS);
				if (VmbErrorSuccess != res)
				{
					// lets try other
					res = SP_ACCESS(m_pCameras[i])->GetFeatureByName("AcquisitionFrameRate", pFeatureFPS);
				}
				if (VmbErrorSuccess == res)
				{
					res = SP_ACCESS(pFeatureFPS)->SetValue(m_FPS);
				}
			}
			FeaturePtr pFeature;       // Any camera feature
			VmbInt64_t nPLS0, nPLS1;   // The payload size of one frame
			std::vector<VmbInt64_t> nPLS(num_cam);

			// Open Camera
			//res = cameras[0]->Open(VmbAccessModeFull);
			//res = cameras[1]->Open(VmbAccessModeFull);

			// Set action trigger mode
			std::cout << "Set action trigger mode" << std::endl;
			for (int i = 0; i < num_cam; i++) {
				res = m_pCameras[i]->GetFeatureByName("TriggerSelector", pFeature);
				res = pFeature->SetValue("FrameStart");
				res = m_pCameras[i]->GetFeatureByName("TriggerSource", pFeature);
				res = pFeature->SetValue("Action0");
				res = m_pCameras[i]->GetFeatureByName("TriggerMode", pFeature);
				res = pFeature->SetValue("On");
			}

			// Set Action Command to camera
			std::cout << "Set Action Command to camera" << std::endl;
			for (int i = 0; i < num_cam; i++) {
				int deviceKey = 11, groupKey = 22, groupMask = 33;
				m_pCameras[i]->GetFeatureByName("ActionDeviceKey", pFeature);
				pFeature->SetValue(deviceKey);
				m_pCameras[i]->GetFeatureByName("ActionGroupKey", pFeature);
				pFeature->SetValue(groupKey);
				m_pCameras[i]->GetFeatureByName("ActionGroupMask", pFeature);
				pFeature->SetValue(groupMask);
			}

			//std::cout << "Set Payload Size" << std::endl;
			//for (int i = 0; i < num_cam; i++) {
			//	res = m_pCameras[i]->GetFeatureByName("PayloadSize", pFeature);
			//	res = pFeature->GetValue(nPLS[i]);
			//}

			std::cout << "Adjust Pixel Format" << std::endl;
			for (int i = 0; i < num_cam; i++) {
				// Store currently selected image format
				FeaturePtr pFormatFeature;
				res = SP_ACCESS(m_pCameras[i])->GetFeatureByName("PixelFormat", pFormatFeature);
				if (VmbErrorSuccess == res) {
					res = SP_ACCESS(pFormatFeature)->GetValue(m_nPixelFormat); // RGBBBBBBBBBBBBBBBBBB
				}
				else break;
			}
			if (VmbErrorSuccess == res)
			{
				std::cout << "Create Obvservers" << std::endl;
				for (int i = 0; i < num_cam; i++) {
					// Create a frame observer for this camera (This will be wrapped in a shared_ptr so we don't delete it)
					SP_SET(m_pFrameObserver, new FrameObserver(m_pCameras[i], i));
					m_pFrameObservers.push_back(m_pFrameObserver);
				}

				
				std::cout << "Start Continuous Image Acquisition" << std::endl;
				for (int i = 0; i < num_cam; i++) {
					// Start streaming
					res = SP_ACCESS(m_pCameras[i])->StartContinuousImageAcquisition(NUM_FRAMES, m_pFrameObservers[i]);
				}
				std::cout << "Done" << std::endl;
			}
        }
    }

    return res;
}

//
// Calls the API convenience function to stop image acquisition
// Closes the camera
//
// Returns:
//  An API status code
//
VmbErrorType ApiController::StopContinuousImageAcquisition()
{
    // Stop streaming
	std::cout << "stop : " << clock();
	VmbErrorType f_res = VmbErrorSuccess;
	for (int i = 0; i < m_pCameras.size(); i++)	{
		m_pCameras[i]->StopContinuousImageAcquisition();
		VmbErrorType res = m_pCameras[i]->Close();
		if (VmbErrorSuccess != res) {
			f_res = res;
		}
	}
	std::cout << "stop\n";
    //m_pCamera->StopContinuousImageAcquisition();

    // Close camera
    return  f_res;
}

//
// Gets all cameras known to Vimba
//
// Returns:
//  A vector of camera shared pointers
//
CameraPtrVector ApiController::GetCameraList()
{
    CameraPtrVector cameras;
    // Get all known cameras
    if( VmbErrorSuccess == m_system.GetCameras( cameras ) )
    {
        // And return them
        return cameras;
    }
    return CameraPtrVector();
}

//
// Gets the width of a frame
//
// Returns:
//  The width as integer
//
int ApiController::GetWidth() const
{
    return static_cast<int>(m_nWidth);
}

//
// Gets the height of a frame
//
// Returns:
//  The height as integer
//
int ApiController::GetHeight() const
{
    return static_cast<int>(m_nHeight);
}

//
// Gets the pixel format of a frame
//
// Returns:
//  The pixel format as enum
//
VmbPixelFormatType ApiController::GetPixelFormat() const
{
    return static_cast<VmbPixelFormatType>(m_nPixelFormat);
}

//
// Gets the current frames per second
//
// Returns:
//  Frame rate in Hertz
//
double ApiController::GetFPS() const
{
    return m_FPS;
}

//
// Gets the oldest frame that has not been picked up yet
//
// Returns:
//  A frame shared pointer
//
FramePtr ApiController::GetFrame(int cam_index)
{
    return SP_DYN_CAST( m_pFrameObservers[cam_index], FrameObserver )->GetFrame();
}

//
// Clears all remaining frames that have not been picked up
//
void ApiController::ClearFrameQueue()
{
	for (int i = 0; i < m_pFrameObservers.size(); i++) {
		SP_DYN_CAST(m_pFrameObservers[i], FrameObserver)->ClearFrameQueue();
	}
    //SP_DYN_CAST( m_pFrameObserver,FrameObserver )->ClearFrameQueue();
}

//
// Queues a given frame to be filled by the API
//
// Parameters:
//  [in]    pFrame          The frame to queue
//
// Returns:
//  An API status code
//
VmbErrorType ApiController::QueueFrame( FramePtr pFrame, int cam_index )
{
    return SP_ACCESS( m_pCameras[cam_index] )->QueueFrame( pFrame );
}

//
// Returns the camera observer as QObject pointer to connect their signals to the view's slots
//
QObject* ApiController::GetCameraObserver()
{
    return SP_DYN_CAST( m_pCameraObserver, CameraObserver ).get();
}

//
// Returns the frame observer as QObject pointer to connect their signals to the view's slots
//
QObject* ApiController::GetFrameObserver( const int camera_index )
{
    return SP_DYN_CAST( m_pFrameObservers[camera_index], FrameObserver ).get();
}

//
// Gets the version of the Vimba API
//
// Returns:
//  The version as string
//
std::string ApiController::GetVersion() const
{
    std::ostringstream os;
    os << m_system;
    return os.str();
}

}}} // namespace AVT::VmbAPI::Examples
