/*=============================================================================
  Copyright (C) 2015 Allied Vision Technologies.  All Rights Reserved.

  Redistribution of this file, in original or modified form, without
  prior written consent of Allied Vision Technologies is prohibited.

-------------------------------------------------------------------------------

  File:        OpenCVVideoRecorder.h

  Description: class to save videos with OpenCV.

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

/*===========================================================================
<OpenCV License>
By downloading, copying, installing or using the software you agree to this license.
If you do not agree to this license, do not download, install,
copy or use the software.


                          License Agreement
               For Open Source Computer Vision Library
                       (3-clause BSD License)

Copyright (C) 2000-2015, Intel Corporation, all rights reserved.
Copyright (C) 2009-2011, Willow Garage Inc., all rights reserved.
Copyright (C) 2009-2015, NVIDIA Corporation, all rights reserved.
Copyright (C) 2010-2013, Advanced Micro Devices, Inc., all rights reserved.
Copyright (C) 2015, OpenCV Foundation, all rights reserved.
Copyright (C) 2015, Itseez Inc., all rights reserved.
Third party copyrights are property of their respective owners.

Redistribution and use in source and binary forms, with or without modification,
are permitted provided that the following conditions are met:

  * Redistributions of source code must retain the above copyright notice,
    this list of conditions and the following disclaimer.

  * Redistributions in binary form must reproduce the above copyright notice,
    this list of conditions and the following disclaimer in the documentation
    and/or other materials provided with the distribution.

  * Neither the names of the copyright holders nor the names of the contributors
    may be used to endorse or promote products derived from this software
    without specific prior written permission.

This software is provided by the copyright holders and contributors "as is" and
any express or implied warranties, including, but not limited to, the implied
warranties of merchantability and fitness for a particular purpose are disclaimed.
In no event shall copyright holders or contributors be liable for any direct,
indirect, incidental, special, exemplary, or consequential damages
(including, but not limited to, procurement of substitute goods or services;
loss of use, data, or profits; or business interruption) however caused
and on any theory of liability, whether in contract, strict liability,
or tort (including negligence or otherwise) arising in any way out of
the use of this software, even if advised of the possibility of such damage.
</OpenCV License>
=============================================================================*/
#ifndef OPEN_CV_VIDEO_RECORDER_H_
#define OPEN_CV_VIDEO_RECORDER_H_
// open cv include
#include "opencv2/opencv.hpp"
//qt include
#include "QtCore/QSharedPointer"
#include "QtCore/QList"
#include "QtCore/QMutex"
#include "QtCore/QWaitCondition"
#include "QtCore/QThread"
// std include
#include <vector>
#include <algorithm>
#include <exception>
// allied vision image transform include
#include "VimbaImageTransform/Include/VmbTransform.h"
#include <VimbaCPP/Include/VimbaCPP.h>


//
// Base exception
//
class BaseException: public std::exception
{
    QString m_Function;
    QString m_Message;
public:
    BaseException( const char*fun, const char* msg)
    {
        try             { if( NULL != fun )   {   m_Function = QString( fun );}}
        catch(...)      {}
        try             { if( NULL != msg )   {   m_Message = QString( msg ); }
        } catch(...)    {}
    }
    ~BaseException() throw() {}
    const QString& Function() const { return m_Function; }
    const QString& Message() const { return m_Message; }
};
//
// Exception for video recorder
class VideoRecorderException: public BaseException
{
public:
    VideoRecorderException( const char* fun, const char*msg)
        : BaseException( fun, msg )
    {}
    ~VideoRecorderException() throw() {}
};
//
// Video recorder using open cv VideoWriter 
//
class OpenCVRecorder: public QThread
{
    Q_OBJECT;
    //
    //  Example FOURCC codes that can be used with the OpenCVRecorder
    //
    VmbUint32_t maxQueueElements() const { return 3; }
    enum
    {
        FOURCC_USER_SELECT  = CV_FOURCC_PROMPT,
        FOURCC_DEFAULT      = CV_FOURCC_MACRO('I','Y','U','V'),
        FOURCC_MPEG1        = CV_FOURCC_MACRO('P','I','M','1'),
        FOURCC_MJPEG        = CV_FOURCC_MACRO('M','J','P','G'),
        FOURCC_MPEG42       = CV_FOURCC_MACRO('M','P','4','2'),
        FOURCC_MPEG43       = CV_FOURCC_MACRO('M','P','4','3'),
        FOURCC_DIVX         = CV_FOURCC_MACRO('D','I','V','X'),
        FOURCC_X264         = CV_FOURCC_MACRO('X','2','6','4'),
    };
    //
    // frame data temporary storage
    //
    struct frame_store
    {
    private:
        typedef std::vector<VmbUchar_t> data_vector;
        data_vector                 m_Data;             // Frame data
        VmbUint32_t                 m_Width;            // frame width
        VmbUint32_t                 m_Height;           // frame height
        VmbPixelFormat_t            m_PixelFormat;      // frame pixel format
    public:
        //
        // Method: frame_store()
        //
        // Purpose: default constructing frame store from data pointer and dimensions
        //
        frame_store(const VmbUchar_t *pBuffer, VmbUint32_t BufferByteSize, VmbUint32_t Width, VmbUint32_t Height, VmbPixelFormatType PixelFormat)
            : m_Data ( pBuffer, pBuffer + BufferByteSize )
            , m_Width( Width )
            , m_Height( Height )
            , m_PixelFormat( PixelFormat )
        {
        }
        //
        // Method: equal
        //
        // Purpose: compare frame store to frame dimensions
        //
        bool equal( VmbUint32_t Width, VmbUint32_t Height, VmbPixelFormat_t PixelFormat) const
        {
            return      m_Width       == Width
                    &&  m_Height      == Height
                    &&  m_PixelFormat == PixelFormat;
        }
        //
        // Method: setData
        //
        // Purpose: copy data into frame store from matching source
        //
        // Returns: false if data size not equal to internal buffer size
        //
        bool setData( const VmbUchar_t *Buffer, VmbUint32_t BufferSize)
        {
            if( BufferSize == dataSize() )
            {
                std::copy( Buffer, Buffer+BufferSize, m_Data.begin() );
                return true;
            }
            return false;
        }
        //
        // Methode: PixelFormat()
        //
        // Purpose: get pixel format of internal buffer.
        //
        VmbPixelFormat_t    pixelFormat()   const   { return m_PixelFormat; }
        //
        // Methode: Width()
        //
        // Purpose: get image width.
        //
        VmbUint32_t         width()         const   { return m_Width; }
        //
        // Methode: Height()
        //
        // Purpose: get image height
        //
        VmbUint32_t         height()        const   { return m_Height; }
        //
        // Methode: dataSize()
        //
        // Purpose: get buffer size of internal data.
        //
        VmbUint32_t         dataSize()      const   { return static_cast<VmbUint32_t>( m_Data.size() ); }
        //
        // Methode: data()
        //
        // Purpose: get constant internal data pointer.
        //
        const VmbUchar_t*   data()          const   { return &*m_Data.begin();}
        //
        // Methode: data()
        //
        // Purpose: get internal data pointer.
        //
        VmbUchar_t*         data()                  { return &*m_Data.begin();}
    };
    typedef QSharedPointer<frame_store> FrameStorePtr;  // shared pointer to frame store data
    typedef QList<FrameStorePtr>        FrameQueue;     // queue of frames tore pointers

    cv::VideoWriter         m_VideoWriter;              // OpenCV VideoWriter

    cv::Mat                 m_ConvertImage;             // storage for converted image, data should only be accessed inside run
                                                        // size and format are const while thread runs

    FrameQueue              m_FrameQueue;               // frame data queue for frames that are to be saved into video stream
    bool                    m_StopThread;               // flag to signal that the thread has to finish
    QMutex                  m_ClassLock;                // shared data lock
    QWaitCondition          m_FramesAvailable;          // queue frame available condition

    //
    // Method:      run()
    //
    // Purpose:     QThread run() implementation.
    //
    // Details:     main thread processing function that will run while the object is alife.
    //
    //
    void run()
    {
        while( ! m_StopThread )
        {
            FrameStorePtr tmp;
            {
                // two class events unlock the queue
                // first if a frame arrives enqueueFrame wakes the condition
                // second if the thread is stopped we are woken up
                // the while loop is necessary because a condition can be woken up by the system
                QMutexLocker local_lock( &m_ClassLock );
                while(! m_StopThread && m_FrameQueue.empty() )
                {
                    m_FramesAvailable.wait( local_lock.mutex() );
                }
                if( ! m_StopThread)
                {
                    tmp = m_FrameQueue.front();
                    m_FrameQueue.pop_front();
                }
            }// scope for the lock, from now one we don't need the class lock
            if( ! m_StopThread)
            {
                convertImage( *tmp );
                m_VideoWriter << m_ConvertImage;
            }
        }
    }
    //
    // Method:      convertImage()
    //
    // Purpose:     converts frame_store data to internal openCV image for video encoding.
    //
    // Parameters:
    //
    //  [in]    frame   internal frame_store struct from queue to convert into m_ConvertImage
    //
    // Note:        access to m_Convert image will race the function is not thread safe and meant to be used as single writer to data.
    //
    bool convertImage(frame_store &frame)
    {
        VmbImage srcImage;
        VmbImage dstImage;
        srcImage.Size = sizeof( srcImage);
        dstImage.Size = sizeof( dstImage);
        VmbSetImageInfoFromPixelFormat( frame.pixelFormat(), frame.width(), frame.height(), & srcImage );
        VmbSetImageInfoFromPixelFormat( VmbPixelFormatBgr8, m_ConvertImage.cols, m_ConvertImage.rows, & dstImage);
        srcImage.Data = frame.data();
        dstImage.Data = m_ConvertImage.data;
        return VmbErrorSuccess == VmbImageTransform( &srcImage, &dstImage, NULL, 0 );

    }
public:
    OpenCVRecorder(const QString &fileName, VmbFloat_t fps, VmbUint32_t Width, VmbUint32_t Height)
        : m_StopThread( false )
#ifdef _MSC_VER // codec selection only supported by Windows
		//, m_VideoWriter(fileName.toStdString(), FOURCC_USER_SELECT, fps, cv::Size(Width, Height), true)
		, m_VideoWriter(fileName.toStdString(), FOURCC_MJPEG, fps, cv::Size(Width, Height), true)
#else
        , m_VideoWriter(fileName.toStdString(), FOURCC_X264, fps, cv::Size(Width,Height),true )
#endif
        , m_ConvertImage( Height, Width, CV_8UC3)
    {
        if( ! m_VideoWriter.isOpened() )
        {
            throw VideoRecorderException(__FUNCTION__, "could not open recorder");
        }
    }
    virtual ~OpenCVRecorder(){}
    //
    // Method:      stopthread()
    //
    // Purpose:     stops thread and ends video encoding.
    //
    // Details:     will stop any data processing and signal that events are to be handled.
    //
    void stopThread()
    {
        QMutexLocker local_lock( &m_ClassLock);
        m_StopThread = true;
        m_FrameQueue.clear();
        m_FramesAvailable.wakeOne();
    }
    //
    // Method:      enqueueFrame()
    //
    // Purpose:     copies frame data into internal queue for processing.
    //
    // Parameters:
    //
    //  [in]    frame   Vimba frame to add to queue, data will be copied from frame
    //
    // Returns:
    //
    //  - true if frame was inserted
    //  - false if frame has not expected size or the insert did not succeed for other reasons
    //
    // Details:     frame data will be added to frame queue, if the MAX_QUEUE_ELEMENTS are reached
    //              the oldest frame data is dropped and the new frame is enqueued.
    //
    // Note:        after return frame is not referenced by the class.
    //
    bool enqueueFrame( const AVT::VmbAPI::Frame &frame)
    {
        VmbUint32_t         Width;
        VmbUint32_t         Height;
        VmbUint32_t         BufferSize;
        VmbPixelFormatType  PixelFormat;
        const VmbUchar_t*   pBuffer(NULL);

        if(     VmbErrorSuccess == frame.GetPixelFormat( PixelFormat) 
            &&  VmbErrorSuccess == frame.GetWidth( Width )
            &&  VmbErrorSuccess == frame.GetHeight( Height )
            &&  VmbErrorSuccess == frame.GetBufferSize( BufferSize )
            &&  VmbErrorSuccess == frame.GetBuffer( pBuffer ) )
        {
            if(     m_ConvertImage.cols == Width 
                &&  m_ConvertImage.rows == Height )
            {
                QMutexLocker local_lock( &m_ClassLock);
                FrameStorePtr pFrame;
                // in case we reached the maximum number of queued frames
                // take of the oldest and reuse it to store the newly arriving frame
                if( m_FrameQueue.size() >= static_cast<FrameQueue::size_type>( maxQueueElements() ) )
                {
                    pFrame = m_FrameQueue.front();
                    m_FrameQueue.pop_front();
                    if ( ! pFrame->equal( Width, Height, PixelFormat ) )
                    {
                        pFrame.clear();
                    }
                }
                if( pFrame.isNull() )
                {
                    pFrame = FrameStorePtr( new frame_store( pBuffer, BufferSize, Width, Height, PixelFormat) );
                }
                else
                {
                    pFrame->setData( pBuffer, BufferSize );
                }
                m_FrameQueue.push_back( pFrame );
                m_FramesAvailable.wakeOne();
                return true;
            }
        }
        return false;
    }
};

#endif