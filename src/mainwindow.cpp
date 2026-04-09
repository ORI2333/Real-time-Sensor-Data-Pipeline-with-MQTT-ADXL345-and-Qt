#include "mainwindow.h"
#include "ui_mainwindow.h"
#include <QDebug>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QJsonValue>
#include <QMetaObject>
#include <QSignalBlocker>
#include <QTime>
#include <QGuiApplication>
#include <QScreen>
#include <QTimer>
#include <QFont>
#include <limits>

namespace {
bool readDoubleByPath(const QJsonObject &root, const QString &path, double *out)
{
    QJsonValue current(root);
    const QStringList parts = path.split('.');
    for (const QString &part : parts) {
        if (!current.isObject()) {
            return false;
        }
        current = current.toObject().value(part);
    }

    if (current.isDouble()) {
        *out = current.toDouble();
        return true;
    }
    if (current.isString()) {
        bool ok = false;
        const double value = current.toString().toDouble(&ok);
        if (ok) {
            *out = value;
            return true;
        }
    }
    return false;
}
}

/**
 * @brief Constructor for MainWindow.
 * Initializes the UI, sets up the real-time graph, and connects internal signals.
 */
MainWindow::MainWindow(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::MainWindow)
{
    ui->setupUi(this);

    setMinimumSize(1040, 760);
    setStyleSheet(
        "QMainWindow {"
        "  background: #f3f6fb;"
        "  color: #1f2937;"
        "  font-family: 'Microsoft YaHei UI';"
        "}"
        "QLabel {"
        "  color: #344054;"
        "  font-size: 13px;"
        "}"
        "QLineEdit, QComboBox {"
        "  background: #ffffff;"
        "  border: 1px solid #d0d5dd;"
        "  border-radius: 8px;"
        "  padding: 4px 8px;"
        "  min-height: 26px;"
        "}"
        "QLineEdit:focus, QComboBox:focus {"
        "  border: 1px solid #2e90fa;"
        "}"
        "QPushButton {"
        "  background: #2563eb;"
        "  color: #ffffff;"
        "  border: none;"
        "  border-radius: 8px;"
        "  padding: 6px 14px;"
        "  font-weight: 600;"
        "}"
        "QPushButton:hover {"
        "  background: #1d4ed8;"
        "}"
        "QPushButton#disconnectButton {"
        "  background: #475467;"
        "}"
        "QPushButton#subscribeButton {"
        "  background: #0f766e;"
        "}"
        "QPlainTextEdit {"
        "  background: #ffffff;"
        "  border: 1px solid #d0d5dd;"
        "  border-radius: 10px;"
        "  padding: 8px;"
        "  color: #111827;"
        "}"
    );

    connected_ = false;
    client = nullptr;
    currentTopic_ = DEFAULT_TOPIC;

    ui->brokerLabel->setText("Broker 地址");
    ui->clientIdLabel->setText("客户端 ID");
    ui->usernameLabel->setText("用户名");
    ui->passwordLabel->setText("密码");
    ui->topicLabel->setText("订阅主题");
    ui->metricLabel->setText("监测指标");
    ui->connectButton->setText("连接");
    ui->disconnectButton->setText("断开");
    ui->subscribeButton->setText("订阅主题");
    ui->statusShadowLabel->setText("接收消息需为 JSON 格式，请手动选择主题与指标。");

    ui->brokerEdit->setText(DEFAULT_ADDRESS);
    ui->clientIdEdit->setText(DEFAULT_CLIENTID);
    ui->usernameEdit->setText(DEFAULT_AUTHMETHOD);
    ui->passwordEdit->setText(DEFAULT_AUTHTOKEN);
    ui->passwordEdit->setEchoMode(QLineEdit::Password);
    ui->topicEdit->setText(DEFAULT_TOPIC);
    ui->brokerEdit->setPlaceholderText("例如：tcp://127.0.0.1:1883");
    ui->topicEdit->setPlaceholderText("例如：een1071/sensor/adxl345");
    ui->outputText->setPlaceholderText("运行日志会显示在这里...");

    setWindowTitle("MQTT ADXL345 实时监控面板");
    setupPlot();

    {
        QSignalBlocker blocker(ui->metricCombo);
        ui->metricCombo->clear();
        ui->metricCombo->addItem("俯仰角 (pitch)", "pitch");
        ui->metricCombo->addItem("横滚角 (roll)", "roll");
        ui->metricCombo->addItem("X 轴加速度 (x)", "x");
        ui->metricCombo->addItem("Y 轴加速度 (y)", "y");
        ui->metricCombo->addItem("Z 轴加速度 (z)", "z");
        ui->metricCombo->addItem("CPU 温度 (cpu_temp)", "cpu_temp");
        ui->metricCombo->addItem("CPU 负载 (cpu_load)", "cpu_load");
    }
    currentMetric_ = ui->metricCombo->currentData().toString();

    // Ensure the window is visible on the primary display and focused.
    if (QGuiApplication::primaryScreen()) {
        const QRect area = QGuiApplication::primaryScreen()->availableGeometry();
        const QSize s = size();
        move(area.center() - QPoint(s.width() / 2, s.height() / 2));
    }
    QTimer::singleShot(0, this, [this]() {
        showNormal();
        raise();
        activateWindow();
    });

    QObject::connect(this, SIGNAL(messageSignal(QString)), this, SLOT(on_MQTTmessage(QString)));
    appendLog("应用已启动。请配置连接参数并点击“连接”。");
}

MainWindow::~MainWindow()
{
    if (connected_ && client) {
        MQTTClient_disconnect(client, TIMEOUT);
    }
    if (client) {
        MQTTClient_destroy(&client);
        client = nullptr;
    }
    delete ui;
}

void MainWindow::appendLog(const QString &line)
{
    const QString ts = QDateTime::currentDateTime().toString("HH:mm:ss");
    ui->outputText->appendPlainText(QString("[%1] %2").arg(ts, line));
    ui->outputText->ensureCursorVisible();
}

void MainWindow::setupPlot()
{
    ui->customPlot->addGraph();
    ui->customPlot->graph(0)->setName("指标曲线");
    ui->customPlot->graph(0)->setPen(QPen(QColor(37, 99, 235), 2));
    ui->customPlot->graph(0)->setScatterStyle(QCPScatterStyle(QCPScatterStyle::ssCircle, QColor(14, 116, 144), QColor(255, 255, 255), 6));
    ui->customPlot->legend->setVisible(false);
    ui->customPlot->setBackground(QColor(255, 255, 255));

    ui->customPlot->xAxis->setBasePen(QPen(QColor(113, 128, 150)));
    ui->customPlot->yAxis->setBasePen(QPen(QColor(113, 128, 150)));
    ui->customPlot->xAxis->setTickPen(QPen(QColor(113, 128, 150)));
    ui->customPlot->yAxis->setTickPen(QPen(QColor(113, 128, 150)));
    ui->customPlot->xAxis->setSubTickPen(QPen(QColor(148, 163, 184)));
    ui->customPlot->yAxis->setSubTickPen(QPen(QColor(148, 163, 184)));
    ui->customPlot->xAxis->setTickLabelColor(QColor(71, 85, 105));
    ui->customPlot->yAxis->setTickLabelColor(QColor(71, 85, 105));
    ui->customPlot->xAxis->grid()->setPen(QPen(QColor(226, 232, 240), 1, Qt::DashLine));
    ui->customPlot->yAxis->grid()->setPen(QPen(QColor(226, 232, 240), 1, Qt::DashLine));
    ui->customPlot->xAxis->grid()->setSubGridVisible(false);
    ui->customPlot->yAxis->grid()->setSubGridVisible(false);

    QSharedPointer<QCPAxisTickerTime> timeTicker(new QCPAxisTickerTime);
    timeTicker->setTimeFormat("%h:%m:%s");
    ui->customPlot->xAxis->setTicker(timeTicker);
    ui->customPlot->xAxis->setLabel("时间");
    ui->customPlot->yAxis->setLabel("数值");
    ui->customPlot->xAxis->setRange(QDateTime::currentSecsSinceEpoch() - 60, QDateTime::currentSecsSinceEpoch());
    ui->customPlot->yAxis->setRange(-10, 10);
    ui->customPlot->replot();

    ui->statsLabel->setText("最近 60 秒统计：平均值=N/A，最小值=N/A，最大值=N/A");
}

void MainWindow::subscribeToTopic(const QString &topic)
{
    if (!connected_ || !client) {
        appendLog("订阅已跳过：当前未连接 MQTT。 ");
        return;
    }

    if (!currentTopic_.isEmpty()) {
        MQTTClient_unsubscribe(client, currentTopic_.toUtf8().constData());
    }

    currentTopic_ = topic.trimmed();
    if (currentTopic_.isEmpty()) {
        appendLog("主题为空，请先输入订阅主题。 ");
        return;
    }

    const int rc = MQTTClient_subscribe(client, currentTopic_.toUtf8().constData(), DEFAULT_QOS);
    appendLog(QString("订阅主题 '%1'，返回码 rc=%2").arg(currentTopic_).arg(rc));
}

void MainWindow::pushDataPoint(const QDateTime &ts, double value)
{
    const double x = ts.toSecsSinceEpoch();
    ui->customPlot->graph(0)->setName(currentMetric_);
    ui->customPlot->graph(0)->addData(x, value);

    ui->customPlot->xAxis->setRange(x - 120.0, x + 2.0);
    ui->customPlot->graph(0)->rescaleValueAxis(true, true);
    ui->customPlot->replot();

    updateRollingStats(x, value);
}

void MainWindow::updateRollingStats(double nowSec, double value)
{
    rollingWindow_.enqueue(qMakePair(nowSec, value));
    while (!rollingWindow_.isEmpty() && rollingWindow_.head().first < nowSec - 60.0) {
        rollingWindow_.dequeue();
    }

    if (rollingWindow_.isEmpty()) {
        ui->statsLabel->setText("最近 60 秒统计：平均值=N/A，最小值=N/A，最大值=N/A");
        return;
    }

    double sum = 0.0;
    double minVal = std::numeric_limits<double>::max();
    double maxVal = std::numeric_limits<double>::lowest();
    for (const auto &p : rollingWindow_) {
        sum += p.second;
        if (p.second < minVal) {
            minVal = p.second;
        }
        if (p.second > maxVal) {
            maxVal = p.second;
        }
    }
    const double avg = sum / static_cast<double>(rollingWindow_.size());
    ui->statsLabel->setText(
        QString("最近 60 秒统计：平均值=%1，最小值=%2，最大值=%3")
            .arg(avg, 0, 'f', 3)
            .arg(minVal, 0, 'f', 3)
            .arg(maxVal, 0, 'f', 3));
}

double MainWindow::extractMetricValue(const QString &metric, const QByteArray &jsonPayload, bool *ok) const
{
    *ok = false;
    QJsonParseError parseError;
    const QJsonDocument doc = QJsonDocument::fromJson(jsonPayload, &parseError);
    if (parseError.error != QJsonParseError::NoError || !doc.isObject()) {
        return 0.0;
    }
    const QJsonObject obj = doc.object();

    QStringList paths;
    if (metric == "pitch") {
        paths << "pitch" << "adxl345.pitch" << "adxl.pitch" << "sensor.pitch";
    } else if (metric == "roll") {
        paths << "roll" << "adxl345.roll" << "adxl.roll" << "sensor.roll";
    } else if (metric == "x") {
        paths << "x" << "adxl345.x" << "adxl.x" << "sensor.x";
    } else if (metric == "y") {
        paths << "y" << "adxl345.y" << "adxl.y" << "sensor.y";
    } else if (metric == "z") {
        paths << "z" << "adxl345.z" << "adxl.z" << "sensor.z";
    } else if (metric == "cpu_temp") {
        paths << "cpu_temp" << "cpu.temp" << "system.cpu_temp";
    } else if (metric == "cpu_load") {
        paths << "cpu_load" << "cpu.load" << "system.cpu_load";
    }

    for (const QString &path : paths) {
        double value = 0.0;
        if (readDoubleByPath(obj, path, &value)) {
            *ok = true;
            return value;
        }
    }
    return 0.0;
}

QDateTime MainWindow::extractTimestamp(const QByteArray &jsonPayload) const
{
    QJsonParseError parseError;
    const QJsonDocument doc = QJsonDocument::fromJson(jsonPayload, &parseError);
    if (parseError.error != QJsonParseError::NoError || !doc.isObject()) {
        return QDateTime::currentDateTimeUtc();
    }

    const QJsonObject obj = doc.object();
    const QStringList keys = {"timestamp", "ts", "time", "datetime"};
    for (const QString &key : keys) {
        if (!obj.contains(key)) {
            continue;
        }

        const QJsonValue v = obj.value(key);
        if (v.isDouble()) {
            const qint64 n = static_cast<qint64>(v.toDouble());
            if (n > 1000000000000LL) {
                return QDateTime::fromMSecsSinceEpoch(n, Qt::UTC);
            }
            return QDateTime::fromSecsSinceEpoch(n, Qt::UTC);
        }

        if (v.isString()) {
            const QString s = v.toString();
            QDateTime dt = QDateTime::fromString(s, Qt::ISODateWithMs);
            if (!dt.isValid()) {
                dt = QDateTime::fromString(s, Qt::ISODate);
            }
            if (dt.isValid()) {
                return dt.toUTC();
            }
        }
    }
    return QDateTime::currentDateTimeUtc();
}

void MainWindow::on_connectButton_clicked()
{
    MQTTClient_connectOptions opts = MQTTClient_connectOptions_initializer;
    MQTTClient_willOptions willOpts = MQTTClient_willOptions_initializer;

    if (connected_) {
        appendLog("已处于连接状态，无需重复连接。 ");
        return;
    }

    const QString broker = ui->brokerEdit->text().trimmed();
    const QString clientId = ui->clientIdEdit->text().trimmed().isEmpty()
                                 ? QString("qt_gui_%1").arg(QDateTime::currentSecsSinceEpoch())
                                 : ui->clientIdEdit->text().trimmed();
    const QString username = ui->usernameEdit->text();
    const QString password = ui->passwordEdit->text();

    if (broker.isEmpty()) {
        appendLog("Broker 地址为空，请先填写。 ");
        return;
    }

    const QByteArray brokerUtf8 = broker.toUtf8();
    const QByteArray clientIdUtf8 = clientId.toUtf8();
    const QByteArray userUtf8 = username.toUtf8();
    const QByteArray passUtf8 = password.toUtf8();

    if (client) {
        MQTTClient_destroy(&client);
        client = nullptr;
    }

    const int createRc = MQTTClient_create(&client, brokerUtf8.constData(), clientIdUtf8.constData(), MQTTCLIENT_PERSISTENCE_DEFAULT, nullptr);
    if (createRc != MQTTCLIENT_SUCCESS) {
        appendLog(QString("MQTTClient_create 失败：rc=%1").arg(createRc));
        return;
    }

    opts.keepAliveInterval = 20;
    opts.cleansession = 0;
    opts.username = userUtf8.constData();
    opts.password = passUtf8.constData();

    willOpts.topicName = "een1071/gui/status";
    willOpts.message = "qt_gui_disconnected_unexpectedly";
    willOpts.qos = 1;
    willOpts.retained = 0;
    opts.will = &willOpts;

    if (MQTTClient_setCallbacks(client, this, connlost, msgarrvd, delivered) != MQTTCLIENT_SUCCESS) {
        appendLog("设置 MQTT 回调失败。 ");
        return;
    }

    const int rc = MQTTClient_connect(client, &opts);
    if (rc != MQTTCLIENT_SUCCESS) {
        appendLog(QString("连接失败：rc=%1").arg(rc));
        return;
    }

    connected_ = true;
    appendLog(QString("已连接到 %1，客户端 ID：%2").arg(broker, clientId));
    subscribeToTopic(ui->topicEdit->text());
}

void delivered(void *context, MQTTClient_deliveryToken dt) {
    MainWindow *self = static_cast<MainWindow *>(context);
    if (!self) {
        return;
    }
    qDebug() << "Message delivery confirmed";
    self->deliveredtoken = dt;
}

int msgarrvd(void *context, char *topicName, int topicLen, MQTTClient_message *message) {
    MainWindow *self = static_cast<MainWindow *>(context);
    (void)topicLen;

    if (!self) {
        MQTTClient_freeMessage(&message);
        MQTTClient_free(topicName);
        return 1;
    }

    QString payload = QString::fromUtf8((const char*)message->payload, message->payloadlen);
    emit self->messageSignal(payload);

    MQTTClient_freeMessage(&message);
    MQTTClient_free(topicName);
    return 1;
}

void MainWindow::on_MQTTmessage(QString payload){
    const QByteArray bytes = payload.toUtf8();
    bool ok = false;
    const double value = extractMetricValue(currentMetric_, bytes, &ok);
    if (!ok) {
        appendLog(QString("收到 JSON，但未找到指标 '%1'：%2").arg(currentMetric_, payload));
        return;
    }

    const QDateTime ts = extractTimestamp(bytes);
    pushDataPoint(ts, value);
    appendLog(QString("指标=%1，数值=%2，主题=%3").arg(currentMetric_).arg(value, 0, 'f', 3).arg(currentTopic_));
}

void connlost(void *context, char *cause) {
    MainWindow *self = static_cast<MainWindow *>(context);
    if (!self) {
        return;
    }

    const QString causeText = cause ? QString::fromUtf8(cause) : QString("unknown");
    QMetaObject::invokeMethod(self, [self, causeText]() {
        self->connected_ = false;
        self->appendLog(QString("连接已断开：%1").arg(causeText));
    }, Qt::QueuedConnection);
}

void MainWindow::on_disconnectButton_clicked()
{
    if (!client || !connected_) {
        appendLog("断开连接已跳过：当前未连接。 ");
        return;
    }

    appendLog("正在断开与 Broker 的连接...");
    MQTTClient_disconnect(client, TIMEOUT);
    MQTTClient_destroy(&client);
    client = nullptr;
    connected_ = false;
    appendLog("已断开连接。 ");
}

void MainWindow::on_subscribeButton_clicked()
{
    subscribeToTopic(ui->topicEdit->text());
}

void MainWindow::on_metricCombo_currentTextChanged(const QString &text)
{
    Q_UNUSED(text);
    currentMetric_ = ui->metricCombo->currentData().toString();
    rollingWindow_.clear();
    if (ui->customPlot && ui->customPlot->graphCount() > 0 && ui->customPlot->graph(0)) {
        ui->customPlot->graph(0)->data()->clear();
    }
    ui->statsLabel->setText("最近 60 秒统计：平均值=N/A，最小值=N/A，最大值=N/A");
    if (ui->customPlot) {
        ui->customPlot->replot();
    }
    appendLog(QString("已切换指标为 '%1'，图表已重置。 ").arg(currentMetric_));
}
