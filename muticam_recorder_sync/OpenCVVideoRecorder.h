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
#include <ctime>
#include <stdio.h>
#include <iostream>
#include <queue>

#define num_camera 2

extern std::queue<std::pair<double, int>> timequeue;
extern FILE* pfile[num_camera];
extern std::string file[num_camera];


void push(double a, int id);
//
// Base exception
//
class BaseException: public std::exception
{
    QString m_Function;
    QString m_Message;
public:
	BaseException(const char*fun, const char* msg);
	~BaseException() throw();
	const QString& Function() const;
	const QString& Message() const;
};
//
// Exception for video recorder
class VideoRecorderException: public BaseException
{
public:
	VideoRecorderException(const char* fun, const char*msg);
	~VideoRecorderException() throw();
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

	VmbUint32_t maxQueueElements() const;
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
			: m_Data(pBuffer, pBuffer + BufferByteSize)
			, m_Width(Width)
			, m_Height(Height)
			, m_PixelFormat(PixelFormat)
		{
		}
		//
		// Method: equal
		//
		// Purpose: compare frame store to frame dimensions
		//
		bool equal(VmbUint32_t Width, VmbUint32_t Height, VmbPixelFormat_t PixelFormat) const
		{
			return      m_Width == Width
				&& m_Height == Height
				&& m_PixelFormat == PixelFormat;
		}
		//
		// Method: setData
		//
		// Purpose: copy data into frame store from matching source
		//
		// Returns: false if data size not equal to internal buffer size
		//
		bool setData(const VmbUchar_t *Buffer, VmbUint32_t BufferSize)
		{
			if (BufferSize == dataSize())
			{
				std::copy(Buffer, Buffer + BufferSize, m_Data.begin());
				return true;
			}
			return false;
		}
		//
		// Methode: PixelFormat()
		//
		// Purpose: get pixel format of internal buffer.
		//
		VmbPixelFormat_t    pixelFormat()   const { return m_PixelFormat; }
		//
		// Methode: Width()
		//
		// Purpose: get image width.
		//
		VmbUint32_t         width()         const { return m_Width; }
		//
		// Methode: Height()
		//
		// Purpose: get image height
		//
		VmbUint32_t         height()        const { return m_Height; }
		//
		// Methode: dataSize()
		//
		// Purpose: get buffer size of internal data.
		//
		VmbUint32_t         dataSize()      const { return static_cast<VmbUint32_t>(m_Data.size()); }
		//
		// Methode: data()
		//
		// Purpose: get constant internal data pointer.
		//
		const VmbUchar_t*   data()          const { return &*m_Data.begin(); }
		//
		// Methode: data()
		//
		// Purpose: get internal data pointer.
		//
		VmbUchar_t*         data() { return &*m_Data.begin(); }
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

	void run();
	bool convertImage(frame_store &frame);
public:
	int cam_id = -1;

	OpenCVRecorder(const QString &fileName, VmbFloat_t fps, VmbUint32_t Width, VmbUint32_t Height);
	virtual ~OpenCVRecorder();
	void stopThread();
	bool enqueueFrame(const AVT::VmbAPI::Frame &frame);
	int m_framequeue_size();
};

#endif