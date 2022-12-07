#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QDebug>
#include <QtSerialPort/QSerialPort>
#include <QtSerialPort/QSerialPortInfo>

QT_BEGIN_NAMESPACE
namespace Ui { class MainWindow; }
QT_END_NAMESPACE

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();
    QSerialPort *serial;
    void plot_vertical_line(double x, double y1, double y2);

private slots:
    void on_pushButton_clicked();
    void realtimeDataSlot();

    void on_close_btn_clicked();

    void on_cal_btn_clicked();

    void on_clear_btn_clicked();

private:
    Ui::MainWindow *ui;
    QTimer *timer;
    QTime *hr_time;
    int counter;
    int nMilliseconds;
    QVector<double> pressures;
    int threshold;
    double pre_pressure;
    double t1 = 0;
    double t2 = 0;
};
#endif // MAINWINDOW_H
