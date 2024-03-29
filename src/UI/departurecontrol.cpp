#include "departurecontrol.h"
#include "ui_departurecontrol.h"

#include "manager.h"
#include "rosdata.h"
#include <QDebug>

DepartureControl::DepartureControl(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::DepartureControl)
{
    ui->setupUi(this);
    ui->tableWidget->horizontalHeader()->setStretchLastSection(true);
    ui->tableWidget->setEditTriggers(QAbstractItemView::NoEditTriggers);
}

void DepartureControl::initData(QString data)
{
    QStringList list = data.split("//");
    for(int i=0; i<list.size() - 1; i++){
        QStringList itemList = list[i].split("\t");
        
        ui->tableWidget->insertRow(ui->tableWidget->rowCount() );
        ui->tableWidget->setItem(i,0,new QTableWidgetItem(itemList[1]));
    }
}

void DepartureControl::updateData(QString data)
{
    int rowCount = ui->tableWidget->rowCount();
    QStringList list = data.split("\t");
    for(int i=0; i<rowCount; i++){
        if(ui->tableWidget->item(i,0)->text() == list[0]){
            ui->tableWidget->setItem(i,1,new QTableWidgetItem(list[1]));
            ui->tableWidget->setItem(i,2,new QTableWidgetItem(list[2]));
            ui->tableWidget->setItem(i,3,new QTableWidgetItem(list[3]));
            ui->tableWidget->setItem(i,4,new QTableWidgetItem(list[4]));
            ui->tableWidget->setItem(i,5,new QTableWidgetItem(list[5]));
        }
        
    }
}

void DepartureControl::adjustColumnSizes()
{
    // 열의 좌우 크기를 콘텐츠에 맞게 조절
    for (int i = 0; i < ui->tableWidget->columnCount(); ++i) {
        ui->tableWidget->horizontalHeader()->setSectionResizeMode(i, QHeaderView::ResizeToContents);
    }
}

void DepartureControl::showWarning(const QMap<int, QString> warning)
{
    warningsec++;
    int rowCount = ui->tableWidget->rowCount();
    for(int i=0; i<rowCount; i++){
        int id = ui->tableWidget->item(i,0)->text().toInt();
        if(warning.contains(id)){
            ui->tableWidget->setItem(i,6,new QTableWidgetItem(warning[id]));
            if(warningsec >= 15){
                ui->tableWidget->item(i,0)->setBackground(QColor(255,79,40,100));
            }else{
                ui->tableWidget->item(i,0)->setBackground(Qt::transparent);
            }
        }
    }
    if(warningsec == 30){
        warningsec = 0;
    }
}

DepartureControl::~DepartureControl()
{
    delete ui;
}
