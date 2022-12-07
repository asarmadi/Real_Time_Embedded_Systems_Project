#include "mainwindow.h"
#include "ui_mainwindow.h"

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{

/*

    */
    // setup a timer that repeatedly calls MainWindow::realtimeDataSlot:
    counter = 0;
    threshold = 0;
    pre_pressure = 0;
    nMilliseconds = 0;
    t1 = 0;
    t2 = 0;
    serial = new QSerialPort(this);
    timer = new QTimer(this);
    hr_time = new QTime();
    hr_time->start();
    timer->setInterval(100);
    connect(timer, SIGNAL(timeout()),this, SLOT(realtimeDataSlot()));

    //QObject::connect(serial,SIGNAL(readyRead()),this,SLOT(realtimeDataSlot()));
    ui->setupUi(this);
    Q_FOREACH(QSerialPortInfo port, QSerialPortInfo::availablePorts()) {
            ui->comboBox->addItem(port.portName());
        }
    ui->customPlot->replot();


    //QSharedPointer<QCPAxisTickerTime> timeTicker(new QCPAxisTickerTime);
    //timeTicker->setTimeFormat("%h:%m:%s");
    ui->customPlot->xAxis->setRange(0, 3500);
    ui->customPlot->axisRect()->setupFullAxesBox();
    ui->customPlot->yAxis->setRange(0, 200);
    ui->customPlot->xAxis->setLabel("time step");
    ui->customPlot->yAxis->setLabel("pressures");

    // make left and bottom axes transfer their ranges to right and top axes:
    QObject::connect(ui->customPlot->xAxis, SIGNAL(rangeChanged(QCPRange)), ui->customPlot->xAxis2, SLOT(setRange(QCPRange)));
    QObject::connect(ui->customPlot->yAxis, SIGNAL(rangeChanged(QCPRange)), ui->customPlot->yAxis2, SLOT(setRange(QCPRange)));


    ui->customPlot_2->xAxis->setLabel("pressure (mmHg)");
    ui->customPlot_2->yAxis->setLabel("Difference");


    ui->dp_lcd->setSegmentStyle(QLCDNumber::Flat);
    ui->sp_lcd->setSegmentStyle(QLCDNumber::Flat);
    ui->hr_lcd->setSegmentStyle(QLCDNumber::Flat);
}

MainWindow::~MainWindow()
{
    delete ui;
}

void MainWindow::plot_vertical_line(double x, double y1, double y2)
{
    QCPItemLine *infLine = new QCPItemLine(ui->customPlot_2);
    infLine->start->setCoords(x, y1);
    infLine->end->setCoords(x, y2);
    QPen pen;
    pen.setStyle(Qt::DashLine);
    pen.setColor(Qt::red);
    infLine->setPen(pen);

    ui->customPlot_2->replot();
}


void MainWindow::on_pushButton_clicked()
{
    ui->customPlot->addGraph(); // blue line
    ui->customPlot->graph(0)->setPen(QPen(QColor(40, 110, 255)));
    ui->customPlot_2->addGraph();

    serial->setPortName(ui->comboBox->currentText());
    if(!serial->open(QIODevice::ReadOnly))
        qDebug() << serial->errorString();

    if(!serial->setBaudRate(QSerialPort::Baud9600))
        qDebug() << serial->errorString();
    if(!serial->setDataBits(QSerialPort::Data8))
        qDebug() << serial->errorString();
    if(!serial->setParity(QSerialPort::NoParity))
        qDebug() << serial->errorString();
    if(!serial->setFlowControl(QSerialPort::NoFlowControl))
        qDebug() << serial->errorString();
    if(!serial->setStopBits(QSerialPort::OneStop))
        qDebug() << serial->errorString();
    serial->setDataTerminalReady(true);
    serial->setRequestToSend(true);

    timer->start();

}

void MainWindow::realtimeDataSlot()
{
    QString command = "";
    QByteArray data = serial->readAll();
    //if (data.contains("\n")){
    QString str = QString(data);
    if(!str.isEmpty()){
    QRegExp rx("[\r\n]");
    QStringList query = str.split(rx);
    double temp_pressure = pre_pressure;
    for(int i=0;i < query.count();i++){
        qDebug() << query[i];
        if(query[i].contains("DS: ") and query[i].contains(" DE")){
            query[i].replace("DS: ","");
            query[i].replace("DE","");
            qDebug()<< query[i];
            pressures.append(query[i].toDouble());
            ui->customPlot->graph(0)->addData(counter, query[i].toDouble());

            if (query[i].toDouble() >= 150)
            {
                threshold = counter;
                hr_time->restart();
                qDebug() << hr_time->elapsed();
            }
            if ((counter>threshold) and (threshold!=0) and (query[i].toDouble() <= 30) and (nMilliseconds==0))
            {
                nMilliseconds = hr_time->elapsed();
                qDebug() << counter << threshold << nMilliseconds;
            }

            counter += 1;
            temp_pressure = query[i].toDouble();
            t1 = hr_time->elapsed();

        }
        if(query[i].contains("CD: ") and query[i].contains(" CE")){
            query[i].replace("CD: ","");
            query[i].replace(" CE","");
            if (query[i]=="S")
                command = "Too Slow!";
            else if (query[i] == "F")
                command = "Too Fast!";
            else
                command = "Good pace!";
            //ui->textBrowser->clear();
            ui->textBrowser->setText(command);
        }
        if(query[i].contains("SP: ") and query[i].contains(" CE")){
            query[i].replace("SP: ","");
            query[i].replace(" CE","");
            ui->textBrowser->append("Systolic Pressure: "+query[i]+"\n");
        }
        if(query[i].contains("DP: ") and query[i].contains(" CE")){
            query[i].replace("DP: ","");
            query[i].replace(" CE","");
            ui->textBrowser->append("Diastolic Pressure: "+query[i]+"\n");
        }
        if(query[i].contains("HR: ") and query[i].contains(" CE")){
            query[i].replace("HR: ","");
            query[i].replace(" CE","");
            ui->textBrowser->append("Heart Rate: "+query[i]+"\n");
        }
    }
    ui->pump_rate_lcd->display((temp_pressure-pre_pressure)/(0.001*(t1-t2)));
    pre_pressure = temp_pressure;
    t2 = hr_time->elapsed();
    ui->customPlot->replot();
    }
}


void MainWindow::on_close_btn_clicked()
{
    serial->close();
    timer->stop();
}


void MainWindow::on_cal_btn_clicked()
{

    QVector<double> x,y;
    double heart_beat = 0;
    int num_sam = 2;
    QVector<double> x1;
    for (int i=threshold; i<pressures.count()-2; i++)
    {
        if(pressures[i]>=40){
            double sum = 0;
            for(int j=-1*(num_sam/2);j<(num_sam/2);j++){
                sum += pressures[i-j];

            }
            x.append(sum/num_sam);
            x1.append(i-threshold);
        }
        else{
            break;
        }
    }
    ui->customPlot->graph(0)->setData(x1,x);
    ui->customPlot->replot();
    for(int i =1;i<x.count()-1;i++){
        if((x[i] > x[i-1]) and (x[i] > x[i+1]))
            heart_beat += 1;
        y.append(x[i+1]-x[i]);
    }
    //ui->hr_lcd->display((int)(heart_beat/(0.001*nMilliseconds)));
    auto max = std::max_element(std::begin(y), std::end(y));
    double biggest = *max;
    for (int i =0; i<y.count();i++)
        y[i] /= biggest;
    auto positionmax = std::distance(std::begin(y),max);
    int posmax = positionmax;

    ui->customPlot_2->addGraph();
    ui->customPlot_2->graph(0)->setData(x, y);

    // set axes ranges, so we see all data:
    ui->customPlot_2->xAxis->setRange(*std::max_element(std::begin(x), std::end(x)), *std::min_element(std::begin(x), std::end(x)));
    ui->customPlot_2->yAxis->setRange(*std::min_element(std::begin(y), std::end(y)), *std::max_element(std::begin(y), std::end(y)));
    ui->customPlot_2->replot();

    plot_vertical_line(x[posmax], *std::min_element(std::begin(y), std::end(y)), biggest);

    QVector<double> x_peak;
    for(int i = posmax; i > 0; i--){
        if((y[i] > y[i-1]) and (y[i] > y[i+1]) and (y[i]>0.3))
            x_peak.append(i);
    }


    for(int i = 0;i < x_peak.count();i++){
        if(y[x_peak[i]]>=(0.5) and y[x_peak[i+1]]<0.5){
            double SP = x[x_peak[i]];
            double DP = (3*x[posmax]-x[x_peak[i]])/2;
            ui->sp_lcd->display(SP);
            plot_vertical_line(SP, *std::min_element(std::begin(y), std::end(y)), biggest);
            ui->dp_lcd->display(DP);
            plot_vertical_line(DP, *std::min_element(std::begin(y), std::end(y)), biggest);
            ui->hr_lcd->display((double)(40.47/(4.14-qLn((x[posmax]-DP)/(0.01*(SP-DP)))))); // This formula comes from this paper: Calculation of Mean Arterial Pressure During Exercise as a Function of Heart Rate
            return;
        }
    }


}


void MainWindow::on_clear_btn_clicked()
{
    counter = 0;
    threshold = 0;
    pre_pressure = 0;
    nMilliseconds = 0;
    t1 = 0;
    t2 = 0;
    if(ui->customPlot->graphCount() != 0){
        ui->customPlot->graph(0)->data()->clear();
        ui->customPlot->clearGraphs();
        ui->customPlot->replot();
    }
    if(ui->customPlot_2->graphCount() != 0){
        ui->customPlot_2->graph(0)->data()->clear();
        ui->customPlot_2->clearGraphs();
        ui->customPlot_2->replot();
    }
    ui->sp_lcd->display(0);
    ui->hr_lcd->display(0);
    ui->dp_lcd->display(0);
    counter = 0;

}

