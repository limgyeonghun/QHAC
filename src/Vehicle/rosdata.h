#ifndef CROSDATA_H
#define CROSDATA_H

#include <QObject>
#include <QByteArray>
#include <QMutex>
#include <QStringList>
#include <QMap>
#include <QVariant>

#include <rclcpp/rclcpp.hpp>
#include <px4_msgs/msg/vehicle_status.hpp>
#include <px4_msgs/msg/vehicle_local_position.hpp>
#include <px4_msgs/msg/vehicle_global_position.hpp>
#include <px4_msgs/msg/vehicle_command.hpp>
#include <px4_msgs/msg/vehicle_command_ack.hpp>
#include <px4_msgs/msg/uavcan_parameter_request.hpp>
#include <px4_msgs/msg/uavcan_parameter_value.hpp>
#include <px4_msgs/msg/log_message.hpp>

#include <px4_msgs/msg/log_message.hpp>

#include <spinworker.h>

class IVehicle;
class FileManager;
class CBModelAgent;

class CROSData : public QObject
{
    Q_OBJECT

public:
    enum MonitoringFlagType {
        SAFETY_LOCK_STATUS = 0,
        ARM_STATUS,
        OFFBOARD_MODE,
        MANUAL_MODE,

        AUTO_MODE,
        FAIL_SAFE_MODE,
        BATTERY_PROBLEM,
        RTKGPS_CONNECTION,

        RTKGPS_BASE_RECV,
        RTKGPS_FIXED_MODE,
        RTKGPS_OFFSET,
        COMM_PROBLEM,

        INIT_PITCH_PROBLEM,
        INIT_ROLL_PROBLEM,
        INIT_VELX_PROBLEM,
        INIT_VELY_PROBLEM,

        INIT_VELZ_PROBLEM,
        INIT_EMBEDDED_SC_OFFSET,
        INIT_EMBEDDED_SC_FILE,
        INIT_EMBEDDED_SC_START_TIME,

        PERI_5V_POWER_PROBLEM,
        TEMPERATURE_PROBLEM,
		INAV_BIAS_XY_PROBLEM,
		INAV_BIAS_Z_PROBLEM,

		MAG_INCONSISTENT_PROBLEM,
		ACC_INCONSISTENT_PROBLEM,
		GYR_INCONSISTENT_PROBLEM,
		AGE_CORR_LV1_PROBLEM,

		AGE_CORR_LV2_PROBLEM,
        STATUS_LANDED,

        NUM_OF_MONITORING
    };
    Q_ENUM(MonitoringFlagType);

public:
    explicit CROSData(IVehicle* agent, QObject* parent = 0);
    virtual ~CROSData();

public:
    void initSubscription();

public Q_SLOTS:
    void onTimeout();

public:
    void setAgent(IVehicle* aAgent) {mAgent = aAgent;}
    void setFileManager(FileManager* aFTP) {mFTP = aFTP;}
    bool updateData(const QByteArray& aByteArray);
    void saveParam(const QString aName, QVariant value);
    QVariant param(const QString aName);
    void resetParams();

    void resetAck();
    void setAckForROS(const uint16_t aCmd, uint8_t result);
    int checkAck(const uint16_t aCmd);
    bool monitoringFlag(uint32_t aValue, uint aBit);
    QString strMonitoringEnum(MonitoringFlagType aType);
    QString strMonitoringStatus();
    void show();
    QString log() const;

    void setAgentBaseDiffAlt(double alt);

    void updateTarget(float x, float y, float z, float H);
    void updateTargetGlobal(double lat, double lng, double altitude, double yaw);
    void publishCommand(px4_msgs::msg::VehicleCommand command);
    // void publishRequestParam(px4_msgs::msg::UavcanParameterRequest req);
    void updateLogMessage(const px4_msgs::msg::LogMessage::SharedPtr msg);
    void updateVehicleStatus(const px4_msgs::msg::VehicleStatus::SharedPtr msg);
    void updateVehicleCommandAck(const px4_msgs::msg::VehicleCommandAck::SharedPtr msg);
    void updateVehicleLocalPosition(const px4_msgs::msg::VehicleLocalPosition::SharedPtr msg);
    void updateVehicleGlobalPosition(const px4_msgs::msg::VehicleGlobalPosition::SharedPtr msg);


    // void parameterValueCallback(const px4_msgs::msg::UavcanParameterValue::SharedPtr msg);
    QList<QString> getParamRequested();


public:
    virtual int                 battery()     {return 0;}
    virtual float               posX()        {return 0;}
    virtual float               posY()        {return 0;}
    virtual float               posZ()        {return 0;}
    virtual float               heading()     {return 0;}

    QVariant data(const QString& aItem);

private:
    uint toUInt(const QByteArray& aBuffer);
    ushort toUShort(const QByteArray& aBuffer);

private:
    IVehicle*                     mAgent;
    FileManager*                mFTP;
    uint                        mSeq;
    QMutex                      mMutex;
    QStringList                 mStrNavStateList;
    QStringList                 mStrArmingStateList;
    QStringList                 mStrStateList;
    qint64                      mRecvTime;
    qint64                      mRecvTime_LocalPos = 0;
    qint64                      mRecvTime_Monitoring = 0;
    bool                        mCommFlag = false;
    
    double                      mRoll = 0, mPitch = 0, mYaw = 0;
    double                      mTargetX, mTargetY, mTargetZ, mTargetH;
    double                      mTargetLat, mTargetLng, mTargetAlt, mTargetYaw;
    double                      agentBaseAltDiff = 0;

    QMap< QString, QVariant >           mParams;
    px4_msgs::msg::VehicleStatus                mVehicleStatus;
    px4_msgs::msg::VehicleLocalPosition         mVehicleLocalPosition;
    px4_msgs::msg::VehicleGlobalPosition        mVehicleGlobalPosition;
    px4_msgs::msg::VehicleCommandAck            mVehicleCommandAck;
    bool                                        mGstRunning;

    QList<QString>                      param_requested;

    rclcpp::Node::SharedPtr             mQHAC3Node;
    rclcpp::Subscription<px4_msgs::msg::VehicleStatus>::SharedPtr mVehicleStatusSub_;
    rclcpp::Subscription<px4_msgs::msg::VehicleLocalPosition>::SharedPtr mVehicleLocalPositionSub_;
    rclcpp::Subscription<px4_msgs::msg::VehicleGlobalPosition>::SharedPtr mVehicleGlobalPositionSub_;


    rclcpp::Subscription<px4_msgs::msg::VehicleCommandAck>::SharedPtr mVehicleCommandAckSub_;
    rclcpp::Subscription<px4_msgs::msg::LogMessage>::SharedPtr mLogMessageSub_;
    // rclcpp::Subscription<px4_msgs::msg::UavcanParameterValue>::SharedPtr mUavcanParameterValueSub_;

    rclcpp::Publisher<px4_msgs::msg::VehicleCommand>::SharedPtr mCommandQHACPub_;
    // rclcpp::Publisher<px4_msgs::msg::UavcanParameterRequest>::SharedPtr mUavcanParameterRequestQHACPub_;

    rclcpp::executors::SingleThreadedExecutor::SharedPtr mROS2Executor;


     std::string                 ros2Header = "/vehicle";

     double                     init_pos_x = 0, init_pos_y = 0;
};

#endif // CROSDATA_H
