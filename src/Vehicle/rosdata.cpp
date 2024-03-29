#include "rosdata.h"
#include "vehicle.h"


#include <QDataStream>
#include <QMutexLocker>
#include <QMetaEnum>
#include <qmath.h>
#include <QDateTime>
#include <QVector3D>
#include <QThread>
#include <QString>

#include <QPixmap>
#include <QImage>

#include <QFile>

#include <math.h>

using std::placeholders::_1;

#define RAD2DEG		(57.0)
#define DEG2RAD     (0.0174533)

CROSData::CROSData(IVehicle* agent, QObject* parent)
    : QObject(parent), mAgent(agent)
{
    mSeq = 0;
    mRecvTime = 0;
    mRecvTime_LocalPos = 0;
    mRecvTime_Monitoring = 0;
    mStrNavStateList << "MANUAL(0)" << "ALTCTL(1)" << "POSCTL(2)" << "AUTO_MISSION(3)" << "AUTO_LOITER(4)" << "AUTO_RTL(5)" << "AUTO_RCRECOVER(6)" << "AUTO_RTGS" << "AUTO_LANDENGFAIL" << "AUTO_LANDGPSFAIL"
                     << "ACRO(10)" << "UNUSED" << "DESCEND" << "TERMINATION" << "OFFBOARD(14)" << "STAB(15)" << "RATTITUDE" << "AUTO_TAKEOFF(17)" << "AUTO_LAND" << "AUTO_FOLLOW_TARGET" << "AUTO_PRECLAND"
                     << "ORBIT" << "MAX";
    mStrArmingStateList << "" << "INIT" << "STANDBY" << "ARMED" << "STANDBY_ERROR" << "SHUTDOWN" << "IN_AIR_RESTORE" << "MAX";
    
    mTargetX = 0;
    mTargetY = 0;
    mTargetZ = 0;

    initSubscription();
}

CROSData::~CROSData()
{
}

void CROSData::initSubscription()
{
    mQHAC3Node = rclcpp::Node::make_shared(("agent_" + std::to_string(mAgent->data("SYSID").toInt()) + "_qhac3_node"));

    /** For iris **/
    rclcpp::QoS qos = rclcpp::SystemDefaultsQoS();
    qos.best_effort();
    
    rclcpp::QoS qos2 = rclcpp::QoS(10).reliability(RMW_QOS_POLICY_RELIABILITY_BEST_EFFORT);

    std::string topic_prefix = ros2Header + std::to_string(mAgent->data("SYSID").toInt());

    // Subscribers
    QString topic = nullptr;
    int sysid = mAgent->data("SYSID").toInt();
    topic = QString("/vehicle%1/fmu/out/vehicle_status").arg(sysid); 
    mVehicleStatusSub_ = mQHAC3Node->create_subscription<px4_msgs::msg::VehicleStatus>(topic.toStdString().c_str(), qos, std::bind(&CROSData::updateVehicleStatus, this, _1));
    // topic = QString("/vehicle%1/out/VehicleLocalPosition").arg(sysid); 
    // mVehicleLocalPositionSub_ = mQHAC3Node->create_subscription<px4_msgs::msg::VehicleLocalPosition>(topic.toStdString().c_str(), qos, std::bind(&CROSData::updateVehicleLocalPosition, this, _1));
    // topic = QString("/vehicle%1/out/VehicleGlobalPosition").arg(sysid); 
    // mVehicleGlobalPositionSub_ = mQHAC3Node->create_subscription<px4_msgs::msg::VehicleGlobalPosition>(topic.toStdString().c_str(), qos, std::bind(&CROSData::updateVehicleGlobalPosition, this, _1));
    //topic = QString("/vehicle%1/out/BatteryStatus").arg(sysid); 
    //mBatteryStatusSub_ = mQHAC3Node->create_subscription<px4_msgs::msg::BatteryStatus>(topic.toStdString().c_str(), qos, std::bind(&CROSData::updateBatteryStatus, this, _1));
    topic = QString("/vehicle%1/fmu/out/monitoring").arg(sysid); 
    mMonitoringSub_ = mQHAC3Node->create_subscription<px4_msgs::msg::Monitoring>(topic.toStdString().c_str(), qos, std::bind(&CROSData::updateMonitoring, this, _1));
    /*
    topic = QString("/vehicle%1/out/Mission").arg(sysid); 
    mMissionSub_ = mQHAC3Node->create_subscription<px4_msgs::msg::Mission>(topic.toStdString().c_str(), qos, std::bind(&CROSData::updateMission, this, _1));
    topic = QString("/vehicle%1/out/MissionResult").arg(sysid);
    mMissionResultSub_ = mQHAC3Node->create_subscription<px4_msgs::msg::MissionResult>(topic.toStdString().c_str(), qos2, std::bind(&CROSData::updateMissionResult, this, _1));
    topic = QString("/vehicle%1/out/NavigatorMissionItem").arg(sysid); 
    mMissionItemSub_ = mQHAC3Node->create_subscription<px4_msgs::msg::NavigatorMissionItem>(topic.toStdString().c_str(), qos, std::bind(&CROSData::updateMissionItem, this, _1));
    topic = QString("/vehicle%1/fpv_camera/image_raw").arg(sysid - 1);
    mFpvCameraImageSub_ = mQHAC3Node->create_subscription<sensor_msgs::msg::Image>(topic.toStdString().c_str(), qos2, std::bind(&CROSData::updateFpvCamera, this, _1));
    topic = QString("/vehicle%1/follow_camera/image_raw").arg(sysid - 1);
    mFollowCameraImageSub_ = mQHAC3Node->create_subscription<sensor_msgs::msg::Image>(topic.toStdString().c_str(), qos2, std::bind(&CROSData::updateFollowCamera, this, _1));
    */
   // mVehicleCommandAckSub_ = mQHAC3Node->create_subscription<px4_msgs::msg::VehicleCommandAck>(topic_prefix + "/vehicle_command_ack", qos, std::bind(&CROSData::updateVehicleCommandAck, this, _1));
    //mLogMessageSub_ = mQHAC3Node->create_subscription<px4_msgs::msg::LogMessage>(topic_prefix + "/log_message", qos, std::bind(&CROSData::updateLogMessage, this, _1));
    // mUavcanParameterValueSub_ = mQHAC3Node->create_subscription<px4_msgs::msg::UavcanParameterValue>(topic_prefix + "/uavcan_parameter_value", qos, std::bind(&CROSData::parameterValueCallback, this, _1));

    // Publishers
    rclcpp::QoS qos_cmd = rclcpp::SystemDefaultsQoS();
    qos_cmd.reliable();
    topic = QString("/vehicle%1/fmu/in/vehicle_command").arg(sysid); 
    mCommandQHACPub_ = mQHAC3Node->create_publisher<px4_msgs::msg::VehicleCommand>(topic.toStdString().c_str(), qos_cmd);
    // mUavcanParameterRequestQHACPub_ = mQHAC3Node->create_publisher<px4_msgs::msg::UavcanParameterRequest>(topic_prefix + "/uavcan_parameter_request", rclcpp::SystemDefaultsQoS());

    // Agent Manager
    mGstRunning = false;

    init_pos_x = mAgent->info("init_pos_x").toDouble();
    init_pos_y = mAgent->info("init_pos_y").toDouble();
    mTargetX = init_pos_y;
    mTargetY = init_pos_x;

    QThread* thread = new QThread();
    SpinWorker* worker = new SpinWorker();
    worker->mNodePtr = mQHAC3Node;
    worker->moveToThread(thread);
    connect(thread, SIGNAL(started()), worker, SLOT (process()));
    thread->start();
}

void CROSData::show()
{
    QMutexLocker locker(&mMutex);

}

QString CROSData::log() const
{
    return QString("");
}

QPixmap CROSData::getCamera()
{
    cv::Mat image_mat = cv::imdecode(cv::Mat(mCameraImage.data), cv::IMREAD_COLOR);
//    qDebug() << "image_mat" << image_mat;
    std::cout << "image_mat" << image_mat << endl;
    qDebug() << "data" << image_mat.data;
    qDebug() << "cols" << image_mat.cols;
    qDebug() << "rows" << image_mat.rows;

    return QPixmap::fromImage(
        QImage(
            (unsigned char*) image_mat.data, 
            image_mat.cols, 
            image_mat.rows, 
            QImage::Format_RGB888
        )
    );
}

QVariant CROSData::data(const QString &aItem)
{
    QString item = aItem.toUpper().trimmed();

    if  ( item == "LOCALPOS" ) {
        return "";
    }
    else if ( item == "LOCALVEL" ) {
        return "";
    }
	else if ( item == "VEL_X") {
        return mVehicleLocalPosition.vx;
	}
	else if ( item == "VEL_Y") {
        return mVehicleLocalPosition.vy;
	}
	else if ( item == "VEL_Z") {
        return mVehicleLocalPosition.vz;
	}
    else if ( item == "STATE" ) {
        return "";
    }
    else if ( item == "MODE") {
        int state = mVehicleStatus.nav_state;
        QString mode = "";
        switch (state)
        {
        case 0:
            mode = "Manual mode";
            break;
        case 3:
            mode = "Auto mission mode";
            break;
        case 4:
            mode = "Auto loiter mode";
            break;
        case 5:
            mode = "Auto return to launch mode";
            break;
        case 14:
            mode = "Offboard";
            break;
        case 17:
            mode = "Takeoff";
            break;
        case 18:
            mode = "Land";
            break;
        default:
            mode = "etc";
            break;
        }
        return mode;
    }
    else if ( item == "ISARMED") {
        if (mVehicleStatus.arming_state == 2) return "ARM";
        else return "DISARM";
    }
    else if ( item == "IS_GST_RUNNING") {
        return mGstRunning;
    }
    else if ( item == "BATTERY") {
        return QString("%1 %").arg(mMonitoring.battery, 6, 'f', 2);
    }
    else if ( item == "DISTANCE") {
        return 0;
    }
    else if ( item == "GPS_STATUS") {
        return "";
    }
    else if ( item == "POSX" ) {
        return mVehicleLocalPosition.x;
    }
    else if ( item == "POSY" ) {
        return mVehicleLocalPosition.y;
    }
    else if ( item == "POSZ" ) {
        return mVehicleLocalPosition.z;
    }
    else if ( item == "POS" ) {
        return QVector3D(mVehicleLocalPosition.x, mVehicleLocalPosition.y, mVehicleLocalPosition.z);
    }
    else if (item == "LPOS_STR" ) {
		return QString("(%1, %2, %3) (%4)")
				.arg(mVehicleLocalPosition.x,6,'f',6)
				.arg(mVehicleLocalPosition.y,6,'f',6)
				.arg(mVehicleLocalPosition.z,6,'f',6)
                .arg(mVehicleLocalPosition.vz,6,'f',6);        
    }
    else if ( item == "POSVX" ) {
        return mVehicleLocalPosition.vx;
    }
    else if ( item == "POSVY" ) {
        return mVehicleLocalPosition.vy;
    }
    else if ( item == "POSVZ" ) {
        return mVehicleLocalPosition.vz;
    }
    else if ( item == "LPSP" ) {
		return QString("X:%1, Y:%2, Z:%3")
				.arg(mTargetX,6,'f',2)
				.arg(mTargetY,6,'f',2)
				.arg(mTargetZ,6,'f',2);
	}
	else if ( item == "LPSP_X") {
		return mTargetX;
	}
	else if ( item == "LPSP_Y") {
		return mTargetY;
	}
    else if ( item == "LPSP_Z") {
		return mTargetZ;
	}
    else if ( item == "HEADING") {
        return mVehicleLocalPosition.heading;
    }
	else if ( item == "STATUSTEXT") {
        return "";
        // if (!mLogMessageQueue.isEmpty()){
        //     auto tmpLogMessage = mLogMessageQueue.dequeue();
        //     std::string statustext(tmpLogMessage.text.begin(), tmpLogMessage.text.end());
        //     return statustext.c_str();
        // }else{
        //     return "";
        // }
	}
	else if ( item == "ATTITUDE") {
        return "";
	}
	else if (item == "ROLL" ) {
        return "";
	}
	else if (item == "PITCH" ) {
        return "";
	}
	else if (item == "YAW" ) {
        return "";
	}
    else if (item == "GLOBAL_LAT") {
        return mVehicleGlobalPosition.lat;
    }
    else if (item == "GLOBAL_LON") {
        return mVehicleGlobalPosition.lon;
    }
    else if (item == "GLOBAL_ALT") {
        return mVehicleGlobalPosition.alt;
    }
    else if (item == "LLH" ) {
        return QVector3D(mVehicleGlobalPosition.lat, mVehicleGlobalPosition.lon, mVehicleGlobalPosition.alt);
    }
    else if (item == "REF_ALT") {
        return mVehicleLocalPosition.ref_alt;
    }
    else if (item == "REF_LLH") {
        return QVector3D(mVehicleLocalPosition.ref_lat, mVehicleLocalPosition.ref_lon, mVehicleLocalPosition.ref_alt);
    }
    else if (item == "LLH_STR" ) {
		return QString("(%1, %2, %3)")
				.arg(mVehicleGlobalPosition.lat,6,'f',6)
				.arg(mVehicleGlobalPosition.lon,6,'f',6)
				.arg(mVehicleGlobalPosition.alt,6,'f',6);        
    }
    else if (item == "MISSION" ) {
    
        QVariantList varLst;

        for (auto v: mMissions){
            varLst.append(QVariant::fromValue(v));
        }
        return varLst;
    }
    else if (item == "MISSION_ITEM") {
        return QVector3D(mMissionItem.latitude, mMissionItem.longitude, mMissionItem.altitude);
    }
    else if ( item == "IS_FPV_CAMERA"){
        return is_fpv_cam;
    }
    else if ( item == "IS_FOLLOW_CAMERA"){
        return is_follow_cam;
    }
    else if (item == "FPV_CAMERA") {
        cv::Mat image_mat = mFpv_Cv_ptr->image;

        QPixmap camera = QPixmap::fromImage(
                    QImage(
                            (unsigned char*) image_mat.data,
                            image_mat.cols,
                            image_mat.rows,
                            QImage::Format_RGB888
                    )
                );
        return camera;
    }
    else if (item == "FOLLOW_CAMERA") {
        cv::Mat image_mat = mFollow_Cv_ptr->image;
    
        QPixmap camera = QPixmap::fromImage(
                QImage(
                        (unsigned char*) image_mat.data,
                        image_mat.cols,
                        image_mat.rows,
                        QImage::Format_RGB888
                )
        );
        return camera;
    }
    else if ( item == "MSG_INTERVAL_TIME") {
        qint64 t = QDateTime::currentMSecsSinceEpoch();
        if ((t - mRecvTime_Monitoring) > 10000 ) {
            mCommFlag = false;
        }
        return t - mRecvTime_Monitoring;
    }
    else if ( item == "AGENT_BASE_ALT_DIFF") {
        return agentBaseAltDiff;
    }
    else if (item == "OFFLINE"){
        return calculatedist();
    }
    else if (item == "PREFLIGHT_CHECK"){
        bool valid_position = mVehicleLocalPosition.xy_valid&mVehicleLocalPosition.z_valid&mVehicleLocalPosition.v_xy_valid&mVehicleLocalPosition.v_z_valid;
        return (valid_position ? "READY":"NOT READY");
    }
    else if (item == "POS_"){
        return QString("(%1, %2, %3)")
				.arg(mMonitoring.pos_x,6,'f',6)
				.arg(mMonitoring.pos_y,6,'f',6)
				.arg(mMonitoring.pos_z,6,'f',6);   
    }
    else if (item == "RTK"){
        return QString("(%1, %2, %3)")
				.arg(mMonitoring.rtk_n,6,'f',6)
				.arg(mMonitoring.rtk_e,6,'f',6)
				.arg(mMonitoring.rtk_d,6,'f',6);   
    }
    else if (item == "FIXED_CHECK"){
        const uint32_t RTKGPS_FIXED_MODE_MASK = 1 << 9; // 10번째 비트에 해당하는 비트 마스크
        bool fixed_check = mMonitoring.status1 & RTKGPS_FIXED_MODE_MASK;
        return (fixed_check ? "FIXED":"NOT FIXED");
    }
    else if (item == "TIMESTAMP"){

        qulonglong timestamp = mVehicleStatus.timestamp;
        return timestamp;
    }
    else {
        return QString("--");
    }
}

bool CROSData::monitoringFlag(uint32_t aValue, uint aBit)
{
    return (aValue & (1<<aBit)) > 0;
}

QString CROSData::strMonitoringEnum(CROSData::MonitoringFlagType aType)
{
    const QMetaObject metaObject = CROSData::staticMetaObject;
    int enumIndex = metaObject.indexOfEnumerator("MonitoringFlagType");
    if(enumIndex == -1) {
        /* The enum does not contain the specified enum */
        return "";
    }
    QMetaEnum en = metaObject.enumerator(enumIndex);
    return QString(en.valueToKey(aType));
}

QString CROSData::strMonitoringStatus()
{
    QString strStatus = "";
    return strStatus;
}

bool CROSData::updateData(const QByteArray &aByteArray)
{
    bool result = false;

    return result;
}

void CROSData::updateTarget(float x, float y, float z, float h)
{
    mTargetX = x;
    mTargetY = y;
    mTargetZ = z;
    mTargetH = h;
}

void CROSData::updateTargetGlobal(double lat, double lng, double altitude, double yaw)
{
    mTargetLat = lat;
    mTargetLng = lng;
    mTargetAlt = altitude;
    mTargetYaw = yaw;
}


void CROSData::saveParam(const QString aName, QVariant value)
{
    mParams[aName] = value;
}

QVariant CROSData::param(const QString aName)
{
    return mParams[aName];
}

void CROSData::resetParams()
{
	mParams.clear();
}

void CROSData::resetAck()
{
	// memset(&mAck, 0, sizeof(mAck));
}

void CROSData::setAckForROS(const uint16_t aCmd, uint8_t result)
{
    // mAck.command = aCmd;
    // mAck.result = result;
}

int CROSData::checkAck(const uint16_t aCmd)
{
    return 0;
}

uint CROSData::toUInt(const QByteArray &aBuffer)
{
    uint result = 0;
    memcpy(&result, aBuffer.data(), 4);
    return result;
}

ushort CROSData::toUShort(const QByteArray &aBuffer)
{
    ushort result = 0;
    memcpy(&result, aBuffer.data(), 2);
	return result;
}

void CROSData::updateVehicleStatus(const px4_msgs::msg::VehicleStatus::SharedPtr msg)
{
    mVehicleStatus = *msg;
}

void CROSData::updateVehicleLocalPosition(const px4_msgs::msg::VehicleLocalPosition::SharedPtr msg)
{
    mVehicleLocalPosition = *msg;
}

void CROSData::updateVehicleGlobalPosition(const px4_msgs::msg::VehicleGlobalPosition::SharedPtr msg)
{
    mVehicleGlobalPosition = *msg;
}

void CROSData::updateMission(const px4_msgs::msg::Mission::SharedPtr msg)
{
    mMission = *msg;
    mMissions.clear();
    mMissionSize = 0;
    qDebug() << "updateMission......";
}

void CROSData::updateMissionItem(const px4_msgs::msg::NavigatorMissionItem::SharedPtr msg)
{
    // mMissionItem = *msg;
    
    // CROSData::MissionItem *item = new CROSData::MissionItem(
    //     mMissionItem.instance_count,
    //     mMissionItem.sequence_total,
    //     //mMissionItem.sequence_current,
    //     mMissionItem.latitude,
    //     mMissionItem.longitude,
    //     mMissionItem.altitude,
    //     mMissionItem.yaw
    // );
    
    // if (mMissionSize < item->seq_total && !std::isnan(mMissionItem.altitude)){
    //     mMissions.append(item);
    // } else {
    //     delete item;
    // }
    // mMissionSize ++;
}

void CROSData::updateBatteryStatus(const px4_msgs::msg::BatteryStatus::SharedPtr msg)
{
    mBatteryStatus = *msg;
}

void CROSData::updateMonitoring(const px4_msgs::msg::Monitoring::SharedPtr msg)
{
    mMonitoring = *msg;
}

void CROSData::updateFpvCamera(const sensor_msgs::msg::Image::SharedPtr msg)
{
    try
    {
        mFpv_Cv_ptr = cv_bridge::toCvCopy(*msg, sensor_msgs::image_encodings::BGR8);
        is_fpv_cam = true;
    }
    catch (cv_bridge::Exception& e) {
        qDebug() << "cv_bridge exception: " << e.what();
    }

}

void CROSData::updateFollowCamera(const sensor_msgs::msg::Image::SharedPtr msg)
{
    try
    {
        mFollow_Cv_ptr = cv_bridge::toCvCopy(*msg, sensor_msgs::image_encodings::BGR8);
        is_follow_cam = true;
    }
    catch (cv_bridge::Exception& e) {
        qDebug() << "cv_bridge exception: " << e.what();
    }

}

void CROSData::updateVehicleCommandAck(const px4_msgs::msg::VehicleCommandAck::SharedPtr msg)
{
    mVehicleCommandAck.command = msg->command;
    mVehicleCommandAck.result = msg->result;
    mVehicleCommandAck.result_param1 = msg->result_param1;
    mVehicleCommandAck.result_param2 = msg->result_param2;
}

void CROSData::updateLogMessage(const px4_msgs::msg::LogMessage::SharedPtr msg)
{
    px4_msgs::msg::LogMessage newLogMessage;
    newLogMessage.timestamp = msg->timestamp;
    newLogMessage.severity = msg->severity;
    newLogMessage.text = msg->text;

    // mLogMessageQueue.enqueue(newLogMessage);
}


QList<QString> CROSData::getParamRequested() {
    return param_requested;
}


void CROSData::publishCommand(px4_msgs::msg::VehicleCommand command) {
    mCommandQHACPub_->publish(command);
}

void CROSData::setAgentBaseDiffAlt(double alt) {
    this->agentBaseAltDiff = alt;
}

void CROSData::updateMissionResult(const px4_msgs::msg::MissionResult::SharedPtr msg){
    mMissionResult = *msg;
}

bool CROSData::calculatedist(){
    bool offline = false;
    qreal start_x, start_y, end_x, end_y, current_x, current_y, result;
    if (mMissionResult.seq_current <= mMissionResult.seq_total - 2 && mMissionResult.seq_current % 2 == 0 && mVehicleStatus.nav_state == 3) {
        int currentMissionindex = mMissionResult.seq_current / 2;
        if (mMissionResult.seq_current == 0) {
            start_x = mVehicleLocalPosition.ref_lat;
            start_y = mVehicleLocalPosition.ref_lon;
        } else {
            start_x = mMissions[currentMissionindex-1]->lat;
            start_y = mMissions[currentMissionindex-1]->lon;
        }
        end_x = mMissions[currentMissionindex]->lat;
        end_y = mMissions[currentMissionindex]->lon;
        current_x = mVehicleGlobalPosition.lat;
        current_y = mVehicleGlobalPosition.lon;
        qreal slope = (end_y - start_y) / (end_x - start_x);
        qreal intercept = start_y - (slope * start_x);
        result = abs(slope * current_x - current_y + intercept) / sqrt(pow(slope, 2) + 1);
        if (0.000005 < result)
            offline = true;
    }
    return offline;
}

void CROSData::onTimeout()
{
}
