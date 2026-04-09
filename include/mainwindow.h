#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QDateTime>
#include <QQueue>
#include "MQTTClient.h"

// --- MQTT Configuration Constants ---
#define DEFAULT_ADDRESS     "tcp://127.0.0.1:1883"
#define DEFAULT_CLIENTID    "qt_gui_subscriber"
#define DEFAULT_AUTHMETHOD  "molloyd"
#define DEFAULT_AUTHTOKEN   "password"
#define DEFAULT_TOPIC       "een1071/sensor/adxl345"
#define DEFAULT_QOS         1
#define TIMEOUT             10000L

QT_BEGIN_NAMESPACE
namespace Ui {
class MainWindow; // Forward declaration of the UI class generated from mainwindow.ui
}
QT_END_NAMESPACE

/**
 * @class MainWindow
 * @brief The main interface class handling UI logic and MQTT communication.
 */
class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = 0);
    ~MainWindow();

private slots:
    void on_connectButton_clicked();
    void on_disconnectButton_clicked();
    void on_subscribeButton_clicked();
    void on_metricCombo_currentTextChanged(const QString &text);

    void on_MQTTmessage(QString message);

signals:
    void messageSignal(QString message);

private:
    Ui::MainWindow *ui;
    void appendLog(const QString &line);
    void setupPlot();
    void subscribeToTopic(const QString &topic);
    void pushDataPoint(const QDateTime &ts, double value);
    void updateRollingStats(double nowSec, double value);
    double extractMetricValue(const QString &metric, const QByteArray &jsonPayload, bool *ok) const;
    QDateTime extractTimestamp(const QByteArray &jsonPayload) const;

    MQTTClient client;
    volatile MQTTClient_deliveryToken deliveredtoken;
    bool connected_;
    QString currentTopic_;
    QString currentMetric_;
    QQueue<QPair<double, double>> rollingWindow_;

    friend void delivered(void *context, MQTTClient_deliveryToken dt);
    friend int msgarrvd(void *context, char *topicName, int topicLen, MQTTClient_message *message);
    friend void connlost(void *context, char *cause);
};

void delivered(void *context, MQTTClient_deliveryToken dt);
int  msgarrvd(void *context, char *topicName, int topicLen, MQTTClient_message *message);
void connlost(void *context, char *cause);

#endif // MAINWINDOW_H
