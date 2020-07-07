#include <sstream>

#include "MutiCam.h"
#include "VimbaImageTransform/Include/VmbTransform.h"
#define NUM_COLORS 3
#define BIT_DEPTH 8

using AVT::VmbAPI::FramePtr;
using AVT::VmbAPI::CameraPtrVector;

MutiCam::MutiCam( QWidget *parent, Qt::WindowFlags flags )
    : QMainWindow( parent, flags )
    , m_bIsStreaming( false )
{
    ui.setupUi( this );
    ui.m_LabelStream->setAlignment(Qt::AlignCenter );
    // Connect GUI events with event handlers
    QObject::connect( ui.m_ButtonStartStop, SIGNAL( clicked() ), this, SLOT( OnBnClickedButtonStartstop() ) );

    // Start Vimba
    VmbErrorType err = m_ApiController.StartUp();
    setWindowTitle( QString( "Asynchronous OpenCV Recorder Vimba C++ API Version " )+ QString::fromStdString( m_ApiController.GetVersion() ) );
    Log( "Starting Vimba", err );

    if( VmbErrorSuccess == err )
    {
        // Connect new camera found event with event handler
        QObject::connect( m_ApiController.GetCameraObserver(), SIGNAL( CameraListChangedSignal(int) ), this, SLOT( OnCameraListChanged(int) ) );

        // Initially get all connected cameras
        UpdateCameraListBox();
        std::stringstream strMsg;
        strMsg << "Cameras found..." << m_cameras.size();
        Log(strMsg.str() );
    }
}

MutiCam::~MutiCam()
{
    // if we are streaming stop streaming
    if( true == m_bIsStreaming )
        OnBnClickedButtonStartstop();

    // Before we close the application we stop Vimba
    m_ApiController.ShutDown();

}

void MutiCam::OnBnClickedButtonStartstop()
{
    VmbErrorType err;
    int nRow = ui.m_ListBoxCameras->currentRow();

    if( -1 < nRow )
    {
        if( false == m_bIsStreaming )
        {
            // Start acquisition
            err = m_ApiController.StartContinuousImageAcquisition( m_cameras[nRow] );
            // Set up Qt image
            if (    VmbErrorSuccess == err)
            {
                VmbUint32_t         Width       = m_ApiController.GetWidth();
                VmbUint32_t         Height      = m_ApiController.GetHeight();
                double              FPS         = m_ApiController.GetFPS();
                try
                {
                    m_pVideoRecorder = OpenCVRecorderPtr( new OpenCVRecorder("MutiCam.avi", FPS, Width,Height) );
                    m_pVideoRecorder->start();
                }
                catch(const BaseException &bex)
                {
                    Log( (bex.Function()+" :" + bex.Message() ).toStdString() );
                }
                m_Image = QImage( Width, Height, QImage::Format_RGB888 );

                QObject::connect( m_ApiController.GetFrameObserver(), SIGNAL( FrameReceivedSignal(int) ), this, SLOT( OnFrameReady(int) ) );
            }
            Log( "Starting Acquisition", err );
            m_bIsStreaming = VmbErrorSuccess == err;
        }
        else
        {
            if( !m_pVideoRecorder.isNull() )
            {
                m_pVideoRecorder->stopThread();
                if( ! m_pVideoRecorder->wait( 1000 ) )
                {
                    m_pVideoRecorder->terminate();
                }
                m_pVideoRecorder.clear();
            }
            m_bIsStreaming = false;
            // Stop acquisition
            err = m_ApiController.StopContinuousImageAcquisition();
            // Clear all frames that we have not picked up so far
            m_ApiController.ClearFrameQueue();
            m_Image = QImage();
            Log( "Stopping Acquisition", err );
        }

        if( false == m_bIsStreaming )
        {
            ui.m_ButtonStartStop->setText( QString( "Start Image Acquisition" ) );
        }
        else
        {
            ui.m_ButtonStartStop->setText( QString( "Stop Image Acquisition" ) );
        }
    }
}

//
// This event handler (Qt slot) is triggered through a Qt signal posted by the frame observer
//
// Parameters:
//  [in]    status          The frame receive status (complete, incomplete, ...)
//
void MutiCam::OnFrameReady( int status )
{
    if( true == m_bIsStreaming )
    {
        // Pick up frame
        FramePtr pFrame = m_ApiController.GetFrame();
        if( SP_ISNULL( pFrame ) )
        {
            Log("frame pointer is NULL, late frame ready message");
            return;
        }
        // See if it is not corrupt
        if( VmbFrameStatusComplete == status )
        {
            if(! m_pVideoRecorder.isNull() )
            {
                m_pVideoRecorder->enqueueFrame( *pFrame);
            }
            VmbUchar_t *pBuffer;
            VmbErrorType err = SP_ACCESS( pFrame )->GetImage( pBuffer );
            if( VmbErrorSuccess == err )
            {
                VmbUint32_t nSize;
                err = SP_ACCESS( pFrame )->GetImageSize( nSize );
                if( VmbErrorSuccess == err )
                {
                    VmbPixelFormatType ePixelFormat = m_ApiController.GetPixelFormat();
                    if( ! m_Image.isNull() )
                    {
                        // Copy it
                        // We need that because Qt might repaint the view after we have released the frame already
                        if( ui.m_ColorProcessingCheckBox->checkState()==  Qt::Checked )
                        {
                            static const VmbFloat_t Matrix[] = {    8.0f, 0.1f, 0.1f, // this matrix just makes a quick color to mono conversion
                                                                    0.1f, 0.8f, 0.1f,
                                                                    0.0f, 0.0f, 1.0f };
                            if( VmbErrorSuccess != CopyToImage( pBuffer,ePixelFormat, m_Image, Matrix ) )
                            {
                                ui.m_ColorProcessingCheckBox->setChecked( false );
                            }
                        }
                        else
                        {
                            CopyToImage( pBuffer,ePixelFormat, m_Image );
                        }

                        // Display it
                        const QSize s = ui.m_LabelStream->size() ;
                        ui.m_LabelStream->setPixmap( QPixmap::fromImage( m_Image ).scaled(s,Qt::KeepAspectRatio ) );
                    }
                }
            }
        }
        else
        {
            // If we receive an incomplete image we do nothing but logging
            Log( "Failure in receiving image", VmbErrorOther );
        }

        // And queue it to continue streaming
        m_ApiController.QueueFrame( pFrame );
    }
}

//
// This event handler (Qt slot) is triggered through a Qt signal posted by the camera observer
//
// Parameters:
//  [in]    reason          The reason why the callback of the observer was triggered (plug-in, plug-out, ...)
//
void MutiCam::OnCameraListChanged( int reason )
{
    bool bUpdateList = false;

    // We only react on new cameras being found and known cameras being unplugged
    if( AVT::VmbAPI::UpdateTriggerPluggedIn == reason )
    {
        Log( "Camera list changed. A new camera was discovered by Vimba." );
        bUpdateList = true;
    }
    else if( AVT::VmbAPI::UpdateTriggerPluggedOut == reason )
    {
        Log( "Camera list changed. A camera was disconnected from Vimba." );
        if( true == m_bIsStreaming )
        {
            OnBnClickedButtonStartstop();
        }
        bUpdateList = true;
    }

    if( true == bUpdateList )
    {
        UpdateCameraListBox();
    }

    ui.m_ButtonStartStop->setEnabled( 0 < m_cameras.size() || m_bIsStreaming );
}

//
// Copies the content of a byte buffer to a Qt image with respect to the image's alignment
//
// Parameters:
//  [in]    pInbuffer       The byte buffer as received from the cam
//  [in]    ePixelFormat    The pixel format of the frame
//  [out]   OutImage        The filled Qt image
//
VmbErrorType MutiCam::CopyToImage( VmbUchar_t *pInBuffer, VmbPixelFormat_t ePixelFormat, QImage &pOutImage, const float *Matrix /*= NULL */ )
{
    const int           nHeight = m_ApiController.GetHeight();
    const int           nWidth  = m_ApiController.GetWidth();

    VmbImage            SourceImage,DestImage;
    VmbError_t          Result;
    SourceImage.Size    = sizeof( SourceImage );
    DestImage.Size      = sizeof( DestImage );

    Result = VmbSetImageInfoFromPixelFormat( ePixelFormat, nWidth, nHeight, & SourceImage );
    if( VmbErrorSuccess != Result )
    {
        Log( "Could not set source image info", static_cast<VmbErrorType>( Result ) );
        return static_cast<VmbErrorType>( Result );
    }
    QString             OutputFormat;
    const int           bytes_per_line = pOutImage.bytesPerLine();
    switch( pOutImage.format() )
    {
    default:
        Log( "unknown output format",VmbErrorBadParameter );
        return VmbErrorBadParameter;
    case QImage::Format_RGB888:
        if( nWidth*3 != bytes_per_line )
        {
            Log( "image transform does not support stride",VmbErrorWrongType );
            return VmbErrorWrongType;
        }
        OutputFormat = "RGB24";
        break;
    }
    Result = VmbSetImageInfoFromString( OutputFormat.toStdString().c_str(), OutputFormat.length(),nWidth,nHeight, &DestImage );
    if( VmbErrorSuccess != Result )
    {
        Log( "could not set output image info",static_cast<VmbErrorType>( Result ) );
        return static_cast<VmbErrorType>( Result );
    }
    SourceImage.Data    = pInBuffer;
    DestImage.Data      = pOutImage.bits();
    // do color processing?
    if( NULL != Matrix )
    {
        VmbTransformInfo TransformParameter;
        Result = VmbSetColorCorrectionMatrix3x3( Matrix, &TransformParameter );
        if( VmbErrorSuccess == Result )
        {
            Result = VmbImageTransform( &SourceImage, &DestImage, &TransformParameter,1 );
        }
        else
        {
            Log( "could not set matrix t o transform info ", static_cast<VmbErrorType>( Result ) );
            return static_cast<VmbErrorType>( Result );
        }
    }
    else
    {
        Result = VmbImageTransform( &SourceImage, &DestImage,NULL,0 );
    }
    if( VmbErrorSuccess != Result )
    {
        Log( "could not transform image", static_cast<VmbErrorType>( Result ) );
        return static_cast<VmbErrorType>( Result );
    }
    return static_cast<VmbErrorType>( Result );
}

//
// Queries and lists all known camera
//
void MutiCam::UpdateCameraListBox()
{
    // Get all cameras currently connected to Vimba
    CameraPtrVector cameras = m_ApiController.GetCameraList();

    // Simply forget about all cameras known so far
    ui.m_ListBoxCameras->clear();
    m_cameras.clear();

    // And query the camera details again
    for( CameraPtrVector::const_iterator iter = cameras.begin();
            cameras.end() != iter;
            ++iter )
    {
        std::string strCameraName;
        std::string strCameraID;
        if( VmbErrorSuccess != (*iter)->GetName( strCameraName ) )
        {
            strCameraName = "[NoName]";
        }
        // If for any reason we cannot get the ID of a camera we skip it
        if( VmbErrorSuccess == (*iter)->GetID( strCameraID ) )
        {
            ui.m_ListBoxCameras->addItem( QString::fromStdString( strCameraName + " " +strCameraID ) );
            m_cameras.push_back( strCameraID );
        }
    }

    ui.m_ButtonStartStop->setEnabled( 0 < m_cameras.size() || m_bIsStreaming );
}

//
// Prints out a given logging string, error code and the descriptive representation of that error code
//
// Parameters:
//  [in]    strMsg          A given message to be printed out
//  [in]    eErr            The API status code
//
void MutiCam::Log( std::string strMsg, VmbErrorType eErr )
{
    strMsg += "..." + m_ApiController.ErrorCodeToMessage( eErr );
    ui.m_ListLog->insertItem( 0, QString::fromStdString( strMsg ) );
}

//
// Prints out a given logging string
//
// Parameters:
//  [in]    strMsg          A given message to be printed out
//
void MutiCam::Log( std::string strMsg)
{
    ui.m_ListLog->insertItem( 0, QString::fromStdString( strMsg ) );
}
