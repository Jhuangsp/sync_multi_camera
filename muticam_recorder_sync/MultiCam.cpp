//#include "MultiCam.h"
//
//MultiCam::MultiCam(QWidget *parent)
//    : QMainWindow(parent)
//{
//    ui.setupUi(this);
//}


#include <sstream>
#include <iostream>
#include <ctime>
#include <iomanip>
#include "MultiCam.h"
#include "VimbaImageTransform/Include/VmbTransform.h"
#define NUM_COLORS 3
#define BIT_DEPTH 8

using AVT::VmbAPI::FramePtr;
using AVT::VmbAPI::FeaturePtr;
using AVT::VmbAPI::CameraPtrVector;
// class func
MultiCam::MultiCam(QWidget *parent, Qt::WindowFlags flags)
    : QMainWindow(parent, flags)
    , m_bIsStreaming(false)
{
    ui.setupUi(this);
    ui.m_LabelStream_1->setAlignment(Qt::AlignCenter);
    ui.m_LabelStream_2->setAlignment(Qt::AlignCenter);
    // Connect GUI events with event handlers
    QObject::connect(ui.m_ButtonStartStop, SIGNAL(clicked()), this, SLOT(OnBnClickedButtonStartstop()));

    // Start Vimba
    VmbErrorType err = m_ApiController.StartUp();
    setWindowTitle(QString("Asynchronous OpenCV Recorder Vimba C++ API Version ") + QString::fromStdString(m_ApiController.GetVersion()));
    Log("Starting Vimba", err);

    if (VmbErrorSuccess == err)
    {
        // Connect new camera found event with event handler
        QObject::connect(m_ApiController.GetCameraObserver(), SIGNAL(CameraListChangedSignal(int)), this, SLOT(OnCameraListChanged(int)));
        // Connect GUI events with event handlers
        //QObject::connect( this, SIGNAL(StartActionCommandSignal()), this, SLOT( AcquisitionLoop() ), Qt::QueuedConnection);

        // Initially get all connected cameras
        UpdateCameraListBox();
        std::stringstream strMsg;
        strMsg << "Cameras found..." << m_cameras.size();
        Log(strMsg.str());
    }
    // open timer
    actt = ActionTimer();
}

MultiCam::~MultiCam()
{
    // if we are streaming stop streaming
    if (true == m_bIsStreaming)
        OnBnClickedButtonStartstop();

    // Before we close the application we stop Vimba
    m_ApiController.ShutDown();

}

void MultiCam::AcquisitionLoop()
{
    // Send Action Command
    char key = 0;
    int count = 0;
    std::cout << "AcquisitionLoop" << std::endl;
    while (m_bIsStreaming)
    {
        // Set Action Command to Vimba API
        int deviceKey = 11, groupKey = 22, groupMask = 33;
        FeaturePtr      pFeature;
        m_ApiController.m_system.GetFeatureByName("ActionDeviceKey", pFeature);
        pFeature->SetValue(deviceKey);
        m_ApiController.m_system.GetFeatureByName("ActionGroupKey", pFeature);
        pFeature->SetValue(groupKey);
        m_ApiController.m_system.GetFeatureByName("ActionGroupMask", pFeature);
        pFeature->SetValue(groupMask);
        m_ApiController.m_system.GetFeatureByName("ActionCommand", pFeature);
        VmbErrorType lError = pFeature->RunCommand();
        if (VmbErrorSuccess != lError) std::cout << "[F]...Could not send Action Command. Reason: " << lError << std::endl;
        Sleep(1000 / 5);
        if (count > 45)
            break;
        else
            count++;
    }
}

void MultiCam::OnBnClickedButtonStartstop()
{
    VmbErrorType err;
    QList<QListWidgetItem*> selection = ui.m_ListBoxCameras->selectedItems();
    int num_cam = selection.size();
    bool flag = true;
    for (int i = 0; i < num_cam; i++) {
        m_selected_cameras.push_back(m_cameras[ui.m_ListBoxCameras->row(selection[i])]);
        if (-1 >= ui.m_ListBoxCameras->row(selection[i])) flag = false;
    }
    std::cout << "Select " << num_cam << " camera(s)" << std::endl;
    std::cout << flag << std::endl;


    if (flag)
    {
        std::cout << "aaaaaaaaaa\n";
        if (false == m_bIsStreaming)
        {
            std::cout << "bbbbbbbbb\n";
            // Start acquisition
            err = m_ApiController.StartContinuousImageAcquisition(m_selected_cameras);
            time_t now = time(0);
            tm *ltm = localtime(&now);
            std::stringstream date;
            date << 1900 + ltm->tm_year << "-"
                << std::setw(2) << std::setfill('0') << 1 + ltm->tm_mon << "-"
                << std::setw(2) << std::setfill('0') << ltm->tm_mday << "_"
                << std::setw(2) << std::setfill('0') << ltm->tm_hour
                << std::setw(2) << std::setfill('0') << ltm->tm_min
                << std::setw(2) << std::setfill('0') << ltm->tm_sec;
            // Set up Qt image
            VmbUint32_t         Width = m_ApiController.GetWidth();
            VmbUint32_t         Height = m_ApiController.GetHeight();
            double              FPS = m_ApiController.GetFPS();
            if (VmbErrorSuccess == err)
            {
                try
                {
                    for (int i = 0; i < num_cam; i++) {
                        std::stringstream vid_name;
                        vid_name << date.str() << "_cam" << std::setw(2) << std::setfill('0') << i << ".avi";
                        OpenCVRecorderPtr m_pVideoRecorder = OpenCVRecorderPtr(new OpenCVRecorder(vid_name.str().c_str(), FPS, Width, Height));
                        m_pVideoRecorders.push_back(m_pVideoRecorder);
                        m_pVideoRecorders[i]->start();
                    }
                }
                catch (const BaseException &bex)
                {
                    Log((bex.Function() + " :" + bex.Message()).toStdString());
                }

                for (int i = 0; i < num_cam; i++) {
                    QImage m_Image = QImage(Width, Height, QImage::Format_RGB888);
                    m_Images.push_back(m_Image);
                    std::cout << "         Connect OnFrameReady         " << std::endl;

                    // QueuedConnection: The slot is invoked when control returns to the event loop of the receiver's thread. The slot is executed in the receiver's thread.
                    //QObject::connect(m_ApiController.GetFrameObserver(i), SIGNAL(FrameReceivedSignal(int)), this, SLOT(OnFrameReady(int)), Qt::QueuedConnection);
                    QObject::connect(m_ApiController.GetFrameObserver(i), SIGNAL(FrameReceivedSignal(int)), this, SLOT(OnFrameReady(int)), Qt::DirectConnection);
                }
            }
            else {
                std::cout << "StartContinuousImageAcquisition failed" << std::endl;
            }
            Log("Starting Acquisition", err);
            m_bIsStreaming = VmbErrorSuccess == err;

            // Start sending command
            actt.setInterval([&]() {
                if (m_bIsStreaming) {
                    int deviceKey = 11, groupKey = 22, groupMask = 33;
                    FeaturePtr      pFeature;
                    m_ApiController.m_system.GetFeatureByName("ActionDeviceKey", pFeature);
                    pFeature->SetValue(deviceKey);
                    m_ApiController.m_system.GetFeatureByName("ActionGroupKey", pFeature);
                    pFeature->SetValue(groupKey);
                    m_ApiController.m_system.GetFeatureByName("ActionGroupMask", pFeature);
                    pFeature->SetValue(groupMask);
                    m_ApiController.m_system.GetFeatureByName("ActionCommand", pFeature);
                    VmbErrorType lError = pFeature->RunCommand();
                    if (VmbErrorSuccess != lError) std::cout << "[F]...Could not send Action Command. Reason: " << lError << std::endl;
                }
                else {
                    std::cout << "Not streaming" << std::endl;
                }
            }, 1000 / FPS);

            //actt.setTimeout([&]() {
            //	std::cout << "After 8s. Time out!" << std::endl;
            //	actt.stopInterval();
            //}, 8000);

        }
        else
        {
            actt.stopInterval();

            std::cout << "dddddd\n";
            bool m_framequeue_empty = ((*m_pVideoRecorders[0]).m_framequeue_size() == 0);
            for (int i = 1; i < num_cam; i++) {
                m_framequeue_empty = m_framequeue_empty && ((*m_pVideoRecorders[i]).m_framequeue_size() == 0);
            }
            if (m_framequeue_empty) {
                std::cout << "ccccccccccccc\n";
                for (int i = 0; i < num_cam; i++) {
                    std::cout << i << " framequeue size is " << (*m_pVideoRecorders[i]).m_framequeue_size() << std::endl;
                    if (!m_pVideoRecorders[i].isNull())
                    {
                        m_pVideoRecorders[i]->stopThread();
                        if (!m_pVideoRecorders[i]->wait(1000))
                        {

                            m_pVideoRecorders[i]->terminate();
                        }
                        m_pVideoRecorders[i].clear();
                    }
                }
                while (!timequeue.empty()) {
                    double tmp = timequeue.front().first;
                    int id = timequeue.front().second;

                    fputs((std::to_string(tmp) + "\n").c_str(), pfile[id]);//check
                    timequeue.pop();
                }
                for (int i = 0; i < num_cam; i++) {
                    fclose(pfile[i]);
                }

                m_bIsStreaming = false;
                // Stop acquisition
                err = m_ApiController.StopContinuousImageAcquisition();
                // Clear all frames that we have not picked up so far
                m_ApiController.ClearFrameQueue();
                for (int i = 0; i < num_cam; i++) m_Images[i] = QImage();

                Log("Stopping Acquisition", err);
            }
        }

        if (false == m_bIsStreaming)
        {
            ui.m_ButtonStartStop->setText(QString("Start Image Acquisition"));
        }
        else
        {
            ui.m_ButtonStartStop->setText(QString("Stop Image Acquisition"));
        }
        //emit StartActionCommandSignal();
    }
}

//
// This event handler (Qt slot) is triggered through a Qt signal posted by the frame observer
//
// Parameters:
//  [in]    status          The frame receive status (complete, incomplete, ...)
//
void MultiCam::OnFrameReady(int msg)
{
    int status = msg / 10;
    int cam_index = msg % 10;
    if (true == m_bIsStreaming)
    {
        // Pick up frame
        FramePtr pFrame = m_ApiController.GetFrame(cam_index);
        if (SP_ISNULL(pFrame))
        {
            Log("frame pointer is NULL, late frame ready message");
            return;
        }
        // See if it is not corrupt
        if (VmbFrameStatusComplete == status)
        {
            if (!m_pVideoRecorders[cam_index].isNull())
            {
                m_pVideoRecorders[cam_index]->enqueueFrame(*pFrame);

            }
            VmbUchar_t *pBuffer;
            VmbErrorType err = SP_ACCESS(pFrame)->GetImage(pBuffer);
            if (VmbErrorSuccess == err)
            {
                VmbUint32_t nSize;
                err = SP_ACCESS(pFrame)->GetImageSize(nSize);
                if (VmbErrorSuccess == err)
                {
                    VmbPixelFormatType ePixelFormat = m_ApiController.GetPixelFormat();
                    if (!m_Images[cam_index].isNull())
                    {
                        // Copy it
                        // We need that because Qt might repaint the view after we have released the frame already
                        if (ui.m_ColorProcessingCheckBox->checkState() == Qt::Checked)
                        {
                            static const VmbFloat_t Matrix[] = { 8.0f, 0.1f, 0.1f, // this matrix just makes a quick color to mono conversion
                                                                    0.1f, 0.8f, 0.1f,
                                                                    0.0f, 0.0f, 1.0f };

                            if (VmbErrorSuccess != CopyToImage(pBuffer, ePixelFormat, m_Images[cam_index], Matrix))
                            {
                                ui.m_ColorProcessingCheckBox->setChecked(false);
                            }

                        }
                        else
                        {
                            CopyToImage(pBuffer, ePixelFormat, m_Images[cam_index]);
                        }

                        // Display it
                        QSize s;
                        switch (cam_index)
                        {
                        case 0:
                            s = ui.m_LabelStream_1->size();
                            ui.m_LabelStream_1->setPixmap(QPixmap::fromImage(m_Images[cam_index]).scaled(s, Qt::KeepAspectRatio));
                            break;
                        case 1:
                            s = ui.m_LabelStream_2->size();
                            ui.m_LabelStream_2->setPixmap(QPixmap::fromImage(m_Images[cam_index]).scaled(s, Qt::KeepAspectRatio));
                            break;
                        default:
                            break;
                        }
                        //const QSize s = ui.m_LabelStream_2->size() ;
                        //ui.m_LabelStream_2->setPixmap( QPixmap::fromImage( m_Images[cam_index] ).scaled(s,Qt::KeepAspectRatio ) );
                    }
                }
            }
            else
            {
                std::cout << "[ERROR] Get image failed..." << std::endl;
            }
        }
        else
        {
            // If we receive an incomplete image we do nothing but logging
            Log("Failure in receiving image", VmbErrorOther);
        }

        // And queue it to continue streaming
        m_ApiController.QueueFrame(pFrame, cam_index);
    }
}

//
// This event handler (Qt slot) is triggered through a Qt signal posted by the camera observer
//
// Parameters:
//  [in]    reason          The reason why the callback of the observer was triggered (plug-in, plug-out, ...)
//
void MultiCam::OnCameraListChanged(int reason)
{
    bool bUpdateList = false;

    // We only react on new cameras being found and known cameras being unplugged
    if (AVT::VmbAPI::UpdateTriggerPluggedIn == reason)
    {
        Log("Camera list changed. A new camera was discovered by Vimba.");
        bUpdateList = true;
    }
    else if (AVT::VmbAPI::UpdateTriggerPluggedOut == reason)
    {
        Log("Camera list changed. A camera was disconnected from Vimba.");
        if (true == m_bIsStreaming)
        {
            OnBnClickedButtonStartstop();
        }
        bUpdateList = true;
    }

    if (true == bUpdateList)
    {
        UpdateCameraListBox();
    }

    ui.m_ButtonStartStop->setEnabled(0 < m_cameras.size() || m_bIsStreaming);
}

//
// Copies the content of a byte buffer to a Qt image with respect to the image's alignment
//
// Parameters:
//  [in]    pInbuffer       The byte buffer as received from the cam
//  [in]    ePixelFormat    The pixel format of the frame
//  [out]   OutImage        The filled Qt image
//
VmbErrorType MultiCam::CopyToImage(VmbUchar_t *pInBuffer, VmbPixelFormat_t ePixelFormat, QImage &pOutImage, const float *Matrix /*= NULL */)
{
    const int           nHeight = m_ApiController.GetHeight();
    const int           nWidth = m_ApiController.GetWidth();

    VmbImage            SourceImage, DestImage;
    VmbError_t          Result;
    SourceImage.Size = sizeof(SourceImage);
    DestImage.Size = sizeof(DestImage);

    Result = VmbSetImageInfoFromPixelFormat(ePixelFormat, nWidth, nHeight, &SourceImage);
    if (VmbErrorSuccess != Result)
    {
        Log("Could not set source image info", static_cast<VmbErrorType>(Result));
        return static_cast<VmbErrorType>(Result);
    }
    QString             OutputFormat;
    const int           bytes_per_line = pOutImage.bytesPerLine();
    switch (pOutImage.format())
    {
    default:
        Log("unknown output format", VmbErrorBadParameter);
        return VmbErrorBadParameter;
    case QImage::Format_RGB888:
        if (nWidth * 3 != bytes_per_line)
        {
            Log("image transform does not support stride", VmbErrorWrongType);
            return VmbErrorWrongType;
        }
        OutputFormat = "RGB24";
        break;
    }
    Result = VmbSetImageInfoFromString(OutputFormat.toStdString().c_str(), OutputFormat.length(), nWidth, nHeight, &DestImage);
    if (VmbErrorSuccess != Result)
    {
        Log("could not set output image info", static_cast<VmbErrorType>(Result));
        return static_cast<VmbErrorType>(Result);
    }
    SourceImage.Data = pInBuffer;
    DestImage.Data = pOutImage.bits();
    // do color processing?
    if (NULL != Matrix)
    {
        VmbTransformInfo TransformParameter;
        Result = VmbSetColorCorrectionMatrix3x3(Matrix, &TransformParameter);
        if (VmbErrorSuccess == Result)
        {
            Result = VmbImageTransform(&SourceImage, &DestImage, &TransformParameter, 1);
        }
        else
        {
            Log("could not set matrix t o transform info ", static_cast<VmbErrorType>(Result));
            return static_cast<VmbErrorType>(Result);
        }
    }
    else
    {
        Result = VmbImageTransform(&SourceImage, &DestImage, NULL, 0);
    }
    if (VmbErrorSuccess != Result)
    {
        Log("could not transform image", static_cast<VmbErrorType>(Result));
        return static_cast<VmbErrorType>(Result);
    }
    return static_cast<VmbErrorType>(Result);
}

//
// Queries and lists all known camera
//
void MultiCam::UpdateCameraListBox()
{
    // Get all cameras currently connected to Vimba
    CameraPtrVector cameras = m_ApiController.GetCameraList();

    // Simply forget about all cameras known so far
    ui.m_ListBoxCameras->clear();
    m_cameras.clear();

    // And query the camera details again
    for (CameraPtrVector::const_iterator iter = cameras.begin();
        cameras.end() != iter;
        ++iter)
    {
        std::string strCameraName;
        std::string strCameraID;
        if (VmbErrorSuccess != (*iter)->GetName(strCameraName))
        {
            strCameraName = "[NoName]";
        }
        // If for any reason we cannot get the ID of a camera we skip it
        if (VmbErrorSuccess == (*iter)->GetID(strCameraID))
        {
            ui.m_ListBoxCameras->addItem(QString::fromStdString(strCameraName + " " + strCameraID));
            m_cameras.push_back(strCameraID);
        }
    }

    ui.m_ButtonStartStop->setEnabled(0 < m_cameras.size() || m_bIsStreaming);
}

//
// Prints out a given logging string, error code and the descriptive representation of that error code
//
// Parameters:
//  [in]    strMsg          A given message to be printed out
//  [in]    eErr            The API status code
//
void MultiCam::Log(std::string strMsg, VmbErrorType eErr)
{
    strMsg += "..." + m_ApiController.ErrorCodeToMessage(eErr);
    ui.m_ListLog->insertItem(0, QString::fromStdString(strMsg));
}

//
// Prints out a given logging string
//
// Parameters:
//  [in]    strMsg          A given message to be printed out
//
void MultiCam::Log(std::string strMsg)
{
    ui.m_ListLog->insertItem(0, QString::fromStdString(strMsg));
}
