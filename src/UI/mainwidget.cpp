#include "mainwidget.h"
#include "ui_mainwidget.h"
#include "manager.h"
#include "sleeper.h"
#include "rosdata.h"
#include "departurecontrol.h"

#include <QKeyEvent>
#include <QFileDialog>
#include <QSignalMapper>
#include <QMessageBox>
#include <QPixmap>
#include <QImage>
#include <QtMath>
#include <QList>

#include <opencv2/imgcodecs.hpp>
#include <opencv2/opencv.hpp>
#include <cv_bridge/cv_bridge.h>


#include <QSqlDatabase>
#include <QSqlQuery>

using std::placeholders::_1;

MainWidget::MainWidget(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::MainWidget)
{
    ui->setupUi(this);

    // init alarm
    mReadyAlarm = false;


    // mRemaingTimeLabel = new QLabel("--  ");
    // QFont font = mRemaingTimeLabel->font();
    // font.setPointSize(25);
    // font.setBold(true);
    // mRemaingTimeLabel->setFont(font);
    // mRemaingTimeLabel->setAlignment(Qt::AlignCenter| Qt::AlignRight);
    // mRemaingTimeLabel->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    // ui->mainToolBar->addWidget(mRemaingTimeLabel);

    mManager = new CManager();
    mParamDialog = new CParamDialog(mManager, this);
    mCalibDialog = new CCalibDialog(mManager, this);
    mMonitorDialog = new CMonitoringDialog(mManager, this);
    mEmScenarioDialog = new CEmScenarioDialog(mManager, this);
    mControlDialog = new CControlDialog(mManager, this);

    ui->mapView->init(mManager, 6, 6);

    // FIXME: dynamic change according to the drone position
    ui->mapView->moveByGPS(36.7721938,127.2696386, 15);

    mMapView = ui->mapView;
    mRubberBand = NULL;
    mRubberBandDrawing = false;
    mPolygonDrawing = false;

    mImageLabel = new QLabel(this);
    mInformationLabel = new QLabel(this);
    mMapSelectionLabel = new QLabel(this);
    mImageLabel->setAttribute(Qt::WA_TransparentForMouseEvents);
    mInformationLabel->setAttribute(Qt::WA_TransparentForMouseEvents);
    mMapSelectionLabel->setAttribute(Qt::WA_TransparentForMouseEvents);
    ui->gridLayout->addWidget(mImageLabel,0,0, Qt::AlignTop);
    ui->gridLayout->addWidget(mMapSelectionLabel,0,0, Qt::AlignTop);
    ui->gridLayout->addWidget(mInformationLabel,0,0, Qt::AlignTop);

    QPalette sample_palette;
    sample_palette.setColor(QPalette::WindowText, Qt::blue);

    mImageLabel->setPalette(sample_palette);

    this->subscribeROS2Topics();


    selectVehicleId = -1;
    ui->flightInfo->horizontalHeader()->setStretchLastSection(true);
    connect(ui->flightList, SIGNAL(itemClicked(QListWidgetItem*)), this, SLOT(on_sysList_itemClicked(QListWidgetItem*)));


    connect(&mTimer, SIGNAL(timeout()), this, SLOT(updateUI()));
    mTimer.setInterval(33);
    mTimer.start();

    //connect(&mRoadTimer, SIGNAL(timeout()), this, SLOT(updateMap()));
    //mRoadTimer.setInterval(200);
    //mRoadTimer.start();
}

MainWidget::~MainWidget()
{
    mManagerThread.quit();
    if(!mManagerThread.wait(3000)) //Wait until it actually has terminated (max. 3 sec)
    {
            mManagerThread.terminate(); //Thread didn't exit in time, probably deadlocked, terminate it!
            mManagerThread.wait(); //We have to wait again here!
    }

    delete mManager;
    delete ui;
    if ( mParamDialog !=  NULL )	delete mParamDialog;
    if ( mCalibDialog !=  NULL )	delete mCalibDialog;
    delete mRemaingTimeLabel;
    if ( mEmScenarioDialog !=  NULL )	delete mEmScenarioDialog;
}

//void MainWidget::showCameraPopup()
//{
//    cameraviews = new cameraview(this);
//    // cameraviews->move(QApplication::desktop()->screen()->rect().center() - cameraviews->rect().center());
//    cameraviews->show();
//}

void MainWidget::initManager()
{
    mManagerThread.setObjectName("Manager");
    connect(&mManagerThread, SIGNAL(started()), mManager, SLOT(onWork()));
    connect(&mManagerThread, SIGNAL(finished()), mManager, SLOT(onTerminated()));
    mManager->moveToThread(&mManagerThread);
    mManagerThread.start();
}

void MainWidget::subscribeROS2Topics()
{
    auto qos = rclcpp::QoS(10).reliability(RMW_QOS_POLICY_RELIABILITY_BEST_EFFORT);
    _ros2node = rclcpp::Node::make_shared("qhac3node");
}

void MainWidget::procInitTreeWidget()
{
    //List Add
    const QMap<int, IVehicle*> agentsMap = mManager->agents();
    QMap<int, IVehicle*>::const_iterator agentsIterator;
    QMap<int, QColor> colorList;
    
    qsrand(time(0));
    QString departureData{""};
    for (agentsIterator = agentsMap.begin(); agentsIterator != agentsMap.end(); ++agentsIterator){
        int sysid = agentsIterator.value()->data("SYSID").toInt();
        QColor color = QColor(qrand()%255, qrand()%255, qrand()%255);
        QString roadData{""};
        QString str = QString("ID : %1\tSYSID : %2").arg(agentsIterator.value()->id()).arg(sysid);
        QListWidgetItem* pItem =new QListWidgetItem(str);
        pItem->setForeground(color);
        ui->flightList->addItem(pItem);
        colorList[agentsIterator.value()->id()] = color;
        roadList[agentsIterator.value()->id()] = roadData;
        mMapView->updateColor(colorList);


        QString str2 = QString("%1\t%2").arg(agentsIterator.value()->id()).arg(sysid);
        departureData.append(str2);
        departureData.append("//");
    }

    ui->departureControl->initData(departureData);


    //Show table
    QStringList strItemList;
    strItemList //<< "ISARMED"
                //<< "Battery"
                //<< "LPOS_STR"
                //<< "LPOS_STR"
                //<< "PREFLIGHT_CHECK"
                //<< "TIMESTAMP"
                << "POS_"
                << "RTK"
                << "FIXED_CHECK"
                << "MODE";
    
    int numItem = strItemList.size();

    for (int i = 0; i < numItem ; i++ ) {
        ui->flightInfo->insertRow(ui->flightInfo->rowCount() );
        ui->flightInfo->setItem(i,0,new QTableWidgetItem(strItemList[i]));
    }
}

void MainWidget::updateVehicleData(){
    if(selectVehicleId != -1){
        const QMap<int, IVehicle*> agentsMap = mManager->agents();
        QMap<int, IVehicle*>::const_iterator agentsIterator;
        for (agentsIterator = agentsMap.begin(); agentsIterator != agentsMap.end(); ++agentsIterator){
            int agentId = agentsIterator.value()->id();
            QPixmap img;
            if(agentId == selectVehicleId){
                QString type;
                if(camera_type == 0){
                    type = "FPV_CAMERA";
                }else{
                    type = "FOLLOW_CAMERA";
                }
                QString is_camtype = QString("IS_%1").arg(type);

                if(agentsIterator.value()->data(is_camtype).toBool()){
                    img = agentsIterator.value()->data(type).value<QPixmap>();
                    img = img.scaled(ui->label->width(),ui->label->height(),Qt::KeepAspectRatio);
                    ui->label->setPixmap(img);
                }else{
                    ui->label->setText("<html><head/><body><p align='center'>No camera</p></body></html>");
                }

                for (int i = 0; i < ui->flightInfo->rowCount() ; i++ ) {
                
                    QString type = ui->flightInfo->item(i, 0)->text();
                    QString value;

                    if ( agentsIterator.value() == NULL )  {
                        qDebug("Error: agent == NULL");
                        continue;
                    }
                    value = QString("%1").arg((agentsIterator.value()->data(type)).toString());
                    
                    ui->flightInfo->setItem(i,1,new QTableWidgetItem(value));
                }
            }
        }
    }
}

void MainWidget::updateDronesInMap()
{
    QMap<int, IVehicle*> agentsMap = mManager->agents();
    QMap<int, IVehicle*>::iterator agentsIterator;
    for (agentsIterator = agentsMap.begin(); agentsIterator != agentsMap.end(); ++agentsIterator){
        IVehicle* agent = agentsIterator.value();
        QVector3D llh = agent->data("LLH").value<QVector3D>();
        float heading = agent->data("HEADING").value<qreal>();
        mMapView->updateDrone(agent->id(), llh.x(), llh.y(), heading);
    }
}

void MainWidget::updateStatusText()
{
	QMap<int, IVehicle*> agentsMap = mManager->agents();
    QMap<int, IVehicle*>::iterator agentsIterator;
    QMap<int, QString> warningData;

    ui->statusListWidget->clear();

    for (agentsIterator = agentsMap.begin(); agentsIterator != agentsMap.end(); ++agentsIterator){
        int id = agentsIterator.value()->id();
		bool isRoute = mManager->agent(id)->data("OFFLINE").toBool();

		if (isRoute) {
			QString statusText = QString("[Group:%1\t ID:%2]\t Off path").arg(id).arg(id);

            ui->statusListWidget->addItem(statusText);
            ui->statusListWidget->scrollToBottom();
			mPrevStatusText[id] = mManager->agent(id)->data("STATUSTEXT").toString();
            warningData[id] = "Off path";
		}
    }
    ui->departureControl->showWarning(warningData);
}

void MainWidget::updateNotifier()
{
    QMap<int, IVehicle*> agentsMap = mManager->agents();
    bool ready = true;

    QMap<int, IVehicle*>::const_iterator agentsIterator;
    for (agentsIterator = agentsMap.begin(); agentsIterator != agentsMap.end(); ++agentsIterator){
//    foreach (IVehicle* agent, agentsMap) {
        int id = agentsIterator.value()->id();

        if ( mManager->agent(id)->data("RTK_READY") != "YES" ) {
            ready = false;
        }
    }

}

void MainWidget::runScenario()
{
    QString scenario_name = mManager->property("emdscen", "name");
    ui->actionMode->setChecked(false);
}

void MainWidget::stopScenario()
{
    // mScenario->stop();
}

void MainWidget::loadDatabase()
{
    mManager->getAgent();

    initManager();
    if(mManager->numOfAgent() != 0){
        procInitTreeWidget();
    }else{
        QMessageBox msgBox;
        msgBox.setText("Database is empty.");
        msgBox.setStandardButtons(QMessageBox::Ok);
        msgBox.exec();
    }
}

void MainWidget::ResetDatabase()
{
    dbManager* mdbManger = new dbManager();
    QSqlDatabase db = mdbManger->db;

    QSqlQuery query(db);
    QString sql = QString("TRUNCATE drone");
    query.exec(sql);

    QMessageBox msgBox;
    msgBox.setText("Success! Please turn the program off and on.");
    msgBox.setStandardButtons(QMessageBox::Ok);
    msgBox.exec();
}


void MainWidget::loadConfigFile()
{
    if(mManager->numOfAgent() != 0){
        QMessageBox msgBox;
        msgBox.setText("There is already an imported UAV.");
        msgBox.setStandardButtons(QMessageBox::Ok);
        msgBox.exec();
    }else{
        QString fileName = QFileDialog::getOpenFileName(
                this,
                tr("Open Agent Configuration File"),
                QString(CONFIG_FILE_PATH),
                tr("Conf Files (*.conf)"));

        if ( !fileName.isEmpty() ) {
            qDebug() << mManager->numOfAgent();
            initManager();

            mManager->loadAgentFile(fileName);

            // wait for initializing manager thread
            // TODO: reduce sleep and check init Manager is finished.
            QMap<int, IVehicle*> agentsMap = mManager->agents();
            QMap<int, QString> agentsTimeMap = mManager->agentsTime();
            QMap<int, int> agentsGroupMap = mManager->agentsGroup();
            QMap<int, int> agentsVehicleMap = mManager->agentsVehicle();



            QMap<int, IVehicle*>::const_iterator agentsIterator;
            QMap<int, QString>::const_iterator agentsTimeIterator;
            QMap<int, int>::const_iterator agentsGroupIterator;
            QMap<int, int>::const_iterator agentsVehicleIterator;
            bool isAllAgentsReady = true;
            

            do{
                CSleeper::msleep(500);
                isAllAgentsReady = true;
                for (agentsIterator = agentsMap.begin(); agentsIterator != agentsMap.end(); ++agentsIterator){
                    if(agentsIterator.value()->isInitialized == false) {
                        isAllAgentsReady = false;
                        break;
                    }
                }
                qDebug() << "isAllAgentsReadey : " << isAllAgentsReady;
            } while(!isAllAgentsReady);

            procInitTreeWidget();
        }

        _base_latlng.setLatitude(mManager->property("base", "latitude").toDouble());
        _base_latlng.setLongitude(mManager->property("base", "longitude").toDouble());
        _base_latlng.setAltitude(mManager->property("base", "altitude").toDouble());
        qDebug() << "set base! for SITL. " << _base_latlng;
    }
}

void MainWidget::checkFlight()
{
    const QMap<int, IVehicle*> agentsMap = mManager->agents();

    QMap<int, IVehicle*>::const_iterator agentsIterator;
    for (agentsIterator = agentsMap.begin(); agentsIterator != agentsMap.end(); ++agentsIterator){
        agentsIterator.value()->cmd("RESET_PARAM");
        agentsIterator.value()->cmd("CHECK_PARAM");
	}
}

void MainWidget::updateMap()
{
    const QMap<int, IVehicle*> agentsMap = mManager->agents();
    QMap<int, IVehicle*>::const_iterator agentsIterator;
    for (agentsIterator = agentsMap.begin(); agentsIterator != agentsMap.end(); ++agentsIterator){
        IVehicle* agent = agentsIterator.value();
        agent->cmd("MISSION_PLAN");
    }
}

void MainWidget::runParamDialog()
{    
    mParamDialog->updateNode();

	mParamDialog->show();
}

void MainWidget::runCalibration()
{
	mCalibDialog->initDialog();

    mCalibDialog->show();
}

void MainWidget::runMonitoringDialog()
{
    mMonitorDialog->startTimer();
    mMonitorDialog->show();
}

void MainWidget::onAlarm(bool aCheckable)
{    
    if ( aCheckable == true ) {
        mReadyAlarm = aCheckable;
    }
    else {
        // mAlarm.stop();
    }
}

void MainWidget::onControl()
{
    mControlDialog->show();
}

void MainWidget::onScenarioMode(bool aMode)
{
    QString cmdMode = "";
    if ( aMode == true ) {
        cmdMode = "OFF_EMBEDDED_SCENARIO";
    }
    else {
        cmdMode = "ON_EMBEDDED_SCENARIO";
    }

    const QMap<int, IVehicle*> agentsMap = mManager->agents();
    QMap<int, IVehicle*>::const_iterator agentsIterator;
    for (agentsIterator = agentsMap.begin(); agentsIterator != agentsMap.end(); ++agentsIterator){
//    foreach (IVehicle* agent, agent_list) {
        agentsIterator.value()->cmd(cmdMode.toLatin1().data());
    }

}




void MainWidget::updateUI()
{	    
    updateVehicleData();
    updateStatusText();
    rclcpp::spin_some(_ros2node);
    updateDronesInMap();
    updateDeparture();
}

bool MainWidget::event(QEvent *event)
{    
    if ( event->type() == QEvent::KeyRelease ){
        QKeyEvent *ke = static_cast<QKeyEvent *>(event);
        this->keyEvent(ke);
    }

    return QWidget::event(event);
}

QGeoCoordinate MainWidget::getNewPositionDiff(QGeoCoordinate oldPosition, double x, double y, double z)
{
    oldPosition = oldPosition.atDistanceAndAzimuth(x, 0, z);
    oldPosition = oldPosition.atDistanceAndAzimuth(y, 90);
    return oldPosition;
}

void MainWidget::on_actionsendSC_triggered()
{
    const QMap<int, IVehicle*> agentsMap = mManager->agents();
    if (agentsMap.size() < 1) {
        QMessageBox msgBox;
        msgBox.setText("Open conf file first!");
        msgBox.setStandardButtons(QMessageBox::Ok);
        msgBox.exec();
    } else {
        // mSendSCDialog->show();
    }
}

QVector3D MainWidget::LLH2NED(QGeoCoordinate pos)
{
    // Calc x,y,z of pos with refPos
    double NED_X = refPos.distanceTo(QGeoCoordinate(pos.latitude(), refPos.longitude(), refPos.altitude()));
    if (pos.latitude() < refPos.latitude())
        NED_X = -NED_X;
    double NED_Y = refPos.distanceTo(QGeoCoordinate(refPos.latitude(), pos.longitude(), refPos.altitude()));
    if (pos.longitude() < refPos.longitude())
        NED_Y = -NED_Y;
    double NED_Z = -(pos.altitude() - refPos.altitude());
    return QVector3D(NED_X, NED_Y, NED_Z);
}

QGeoCoordinate MainWidget::NED2LLH(QVector3D pos)
{
    // Calc lat, lon, alt of pos with refPos
    QGeoCoordinate LLHPosition = QGeoCoordinate(refPos.latitude(), refPos.longitude(), refPos.altitude());
    LLHPosition = LLHPosition.atDistanceAndAzimuth(pos.x(), 0, -pos.z());
    LLHPosition = LLHPosition.atDistanceAndAzimuth(pos.y(), 90);
    return LLHPosition;
}

QGeoCoordinate MainWidget::ENU2LLH(QVector3D pos)
{
    // Calc lat, lon, alt of pos with refPos
    QGeoCoordinate LLHPosition = QGeoCoordinate(refPos.latitude(), refPos.longitude(), refPos.altitude());
    LLHPosition = LLHPosition.atDistanceAndAzimuth(pos.y(), 0, pos.z());
    LLHPosition = LLHPosition.atDistanceAndAzimuth(pos.x(), 90);
    return LLHPosition;
}

void MainWidget::keyEvent(QKeyEvent *event)
{
	
	// find first agent
	int node = -1;
        const QMap<int, IVehicle*> agentsMap = mManager->agents();
        QMap<int, IVehicle*>::const_iterator agentsIterator;
        for (agentsIterator = agentsMap.begin(); agentsIterator != agentsMap.end(); ++agentsIterator){
            node = agentsIterator.value()->id();
            break;
	}
	if ( node < 0 ) return;

    QGeoCoordinate curPosition;

    switch(event->key()) {
    case Qt::Key_Q:
	{
        const QMap<int, IVehicle*> agentsMap = mManager->agents();
        QMap<int, IVehicle*>::const_iterator agentsIterator;
        for (agentsIterator = agentsMap.begin(); agentsIterator != agentsMap.end(); ++agentsIterator){
            IVehicle* agent = agentsIterator.value();
            agent->cmd("ARM", refPos.altitude());
            qDebug() << "ARM";
        }

	}
        break;

    case Qt::Key_W:
	{
        const QMap<int, IVehicle*> agentsMap = mManager->agents();
        QMap<int, IVehicle*>::const_iterator agentsIterator;
        for (agentsIterator = agentsMap.begin(); agentsIterator != agentsMap.end(); ++agentsIterator){
            IVehicle* agent = agentsIterator.value();
            agent->cmd("DISARM");
            qDebug() << "DISARM";
        }
	}
        break;

    case Qt::Key_N:
	{
        const QMap<int, IVehicle*> agentsMap = mManager->agents();
        QMap<int, IVehicle*>::const_iterator agentsIterator;
        for (agentsIterator = agentsMap.begin(); agentsIterator != agentsMap.end(); ++agentsIterator){
            IVehicle* agent = agentsIterator.value();
            qDebug() << "REBOOT";
            agent->cmd("REBOOT");
        }
	}
        break;

    case Qt::Key_O:
    {
        const QMap<int, IVehicle*> agentsMap = mManager->agents();
        QMap<int, IVehicle*>::const_iterator agentsIterator;
        for (agentsIterator = agentsMap.begin(); agentsIterator != agentsMap.end(); ++agentsIterator){
            IVehicle* agent = agentsIterator.value();
            agent->cmd("OFFBOARD");
            qDebug() << "OFFBOARD";
        }
    }
        break;

    case Qt::Key_M:
    {
        const QMap<int, IVehicle*> agentsMap = mManager->agents();
        QMap<int, IVehicle*>::const_iterator agentsIterator;
        for (agentsIterator = agentsMap.begin(); agentsIterator != agentsMap.end(); ++agentsIterator){
            IVehicle* agent = agentsIterator.value();
            agent->cmd("MANUAL");
            qDebug() << "MANUAL";
        }
    }
        break;

    // case Qt::Key_A:
    // {
    //     const QMap<int, IVehicle*> agentsMap = mManager->agents();
    //     QMap<int, IVehicle*>::const_iterator agentsIterator;
    //     for (agentsIterator = agentsMap.begin(); agentsIterator != agentsMap.end(); ++agentsIterator){
    //         IVehicle* agent = agentsIterator.value();
    //         qDebug() << "TAKEOFF......";
    //         agent->cmd("TAKEOFF", 3, HEADING);
    //     }
    // }
    //     break;

    // case Qt::Key_S:
    // {
    //     const QMap<int, IVehicle*> agentsMap = mManager->agents();
    //     QMap<int, IVehicle*>::const_iterator agentsIterator;
    //     for (agentsIterator = agentsMap.begin(); agentsIterator != agentsMap.end(); ++agentsIterator){
    //         IVehicle* agent = agentsIterator.value();
    //         qDebug() << "LANDING......";
	// 		agent->cmd("LANDING", HEADING);
    //     }
    // }
    //    break; 

    // case Qt::Key_1:
	// {
    //     QVector3D target_pos = QVector3D(0, 0, 0);
    //     const QMap<int, IVehicle*> agentsMap = mManager->agents();
    //     QMap<int, IVehicle*>::const_iterator agentsIterator;
    //     for (agentsIterator = agentsMap.begin(); agentsIterator != agentsMap.end(); ++agentsIterator){
    //         IVehicle* agent = agentsIterator.value();
    //         agent->cmd("MOVE_NED", target_pos, HEADING);
    //     }
	// }
    //     break;

    // case Qt::Key_F:
	// {
    //    HEADING = mManager->agent(3)->data("HEADING").value<qreal>();
    //    qDebug() << "HAEDING FIX...";
    //    qDebug() << HEADING;
	// }
    //     break;
    
    default:
        break;
    }; 

    QWidget::keyPressEvent(event);
}
void MainWidget::on_sysList_itemClicked(QListWidgetItem *item)
{
    QStringList list1 = item->text().split('\t');
    int id   = list1[0].replace("ID : ","").toInt();
    selectVehicleId = id;
    ui->mapView->selectVehicle(selectVehicleId);
}   


void MainWidget::updateDeparture()
{
    QStringList strItemList;
    strItemList << "POS_"
                << "RTK"
                << "FIXED_CHECK"
                << "MODE"
                << "Battery";
    int numItem = strItemList.size();

    const QMap<int, IVehicle*> agentsMap = mManager->agents();
    QMap<int, IVehicle*>::const_iterator agentsIterator;
    

    for (agentsIterator = agentsMap.begin(); agentsIterator != agentsMap.end(); ++agentsIterator){
        QString departureData{""};
        QString sysid = agentsIterator.value()->data("SYSID").toString();
        departureData.append(sysid);
        departureData.append("\t");

        for (int i = 0; i < numItem ; i++ ) {
            QString value;

            if ( agentsIterator.value() == NULL )  {
                qDebug("Error: agent == NULL");
                continue;
            }

            value = QString("%1").arg((agentsIterator.value()->data(strItemList[i])).toString());

            departureData.append(value);
            departureData.append("\t");
        }
        ui->departureControl->updateData(departureData);
        ui->departureControl->adjustColumnSizes(); // 새로운 메소드 호출
    }
}
void MainWidget::on_camera_type_toggled(bool checked)
{
    if(checked){
        camera_type = 0;
    }
}


void MainWidget::on_camera_type2_toggled(bool checked)
{
    if(checked){
        camera_type = 1;
    }
}

