#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QDateTime>
#include <QQueue>
#include <QHash>
#include <QStringList>
#include <QLabel>
#include <QComboBox>
#include <QGroupBox>
#include <QToolButton>
#include <QPushButton>
#include <QPlainTextEdit>
#include "MQTTClient.h"

// --- MQTT Configuration Constants ---
#define DEFAULT_ADDRESS     "mqtt://192.168.127.152:1884"
#define DEFAULT_CLIENTID    "qt"
#define DEFAULT_AUTHMETHOD  "fangkuai"
#define DEFAULT_AUTHTOKEN   "2120033"
#define DEFAULT_TOPIC       "sensor/adxl345"
#define DEFAULT_LWT_TOPIC   "sensor/adxl345/status"
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

    void on_MQTTmessage(QString topic, QString message);

signals:
    void messageSignal(QString topic, QString message);

private:
    enum class UiLanguage {
        Chinese,
        English
    };

    Ui::MainWindow *ui;
    QString trUi(const QString &zh, const QString &en) const;
    void applyUiLanguage();
    void populateMetricCombo();
    void refreshDeviceStateLabel();
    QString metricDisplayNameUi(const QString &metric) const;
    QString metricAxisLabelUi(const QString &metric) const;
    QString emptyStatsText() const;
    void appendLog(const QString &line);
    void updateConnectionUi(bool connected);
    void setupResponsiveLayout();
    void setupPlot();
    void subscribeToTopic(const QString &topic);
    void publishJsonMessage();
    void pushDataPoint(const QString &metric, const QDateTime &ts, double value);
    void updateRollingStats(const QString &metric);
    void refreshPlotForCurrentMetric();
    void clearDataCache();
    double extractMetricValue(const QString &metric, const QByteArray &jsonPayload, bool *ok) const;
    QDateTime extractTimestamp(const QByteArray &jsonPayload) const;

    MQTTClient client;
    volatile MQTTClient_deliveryToken deliveredtoken;
    bool connected_;
    UiLanguage currentLanguage_;
    int deviceStateCode_;
    QString currentTopic_;
    QString lwtTopic_;
    QStringList subscribedTopics_;
    QString currentMetric_;
    int selectedSubQos_;
    int selectedPubQos_;
    QHash<QString, QQueue<QPair<double, double>>> metricHistory_;
    QGroupBox *connBox_;
    QGroupBox *subBox_;
    QGroupBox *pubBox_;
    QGroupBox *chartBox_;
    QGroupBox *logBox_;
    QLabel *languageLabel_;
    QComboBox *languageCombo_;
    QLabel *subQosLabel_;
    QComboBox *topicCombo_;
    QComboBox *subQosCombo_;
    QLabel *lwtTopicLabel_;
    QComboBox *lwtTopicCombo_;
    QLabel *lwtQosLabel_;
    QComboBox *lwtQosCombo_;
    QLabel *pubTopicLabel_;
    QComboBox *pubTopicCombo_;
    QLabel *pubQosLabel_;
    QComboBox *pubQosCombo_;
    QLabel *pubPayloadLabel_;
    QPlainTextEdit *pubPayloadEdit_;
    QPushButton *publishButton_;
    QPushButton *clearCacheButton_;
    QToolButton *passwordToggleButton_;
    QLabel *deviceStateLabel_;

    friend void delivered(void *context, MQTTClient_deliveryToken dt);
    friend int msgarrvd(void *context, char *topicName, int topicLen, MQTTClient_message *message);
    friend void connlost(void *context, char *cause);
};

void delivered(void *context, MQTTClient_deliveryToken dt);
int  msgarrvd(void *context, char *topicName, int topicLen, MQTTClient_message *message);
void connlost(void *context, char *cause);

#endif // MAINWINDOW_H
