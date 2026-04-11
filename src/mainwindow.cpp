#include "mainwindow.h"
#include "ui_mainwindow.h"
#include <QDebug>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QJsonValue>
#include <QMessageBox>
#include <QMetaObject>
#include <QProcess>
#include <QSignalBlocker>
#include <QPushButton>
#include <QClipboard>
#include <QTime>
#include <QGuiApplication>
#include <QScreen>
#include <QTimer>
#include <QFont>
#include <QVBoxLayout>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QSizePolicy>
#include <QGroupBox>
#include <QSplitter>
#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QStyle>
#include <limits>

namespace {
constexpr double kStatsWindowSec = 60.0;
constexpr double kHistoryWindowSec = 600.0;

QStringList supportedMetricKeys()
{
    return QStringList() << "pitch"
                         << "roll"
                         << "x"
                         << "y"
                         << "z"
                         << "cpu_temp"
                         << "cpu_load";
}

QString normalizeBrokerUri(const QString &broker)
{
    const QString trimmed = broker.trimmed();
    if (trimmed.startsWith("mqtt://", Qt::CaseInsensitive)) {
        return QString("tcp://") + trimmed.mid(7);
    }
    return trimmed;
}

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

QString metricUnit(const QString &metric)
{
    if (metric == "pitch" || metric == "roll") {
        return "deg";
    }
    if (metric == "x" || metric == "y" || metric == "z") {
        return "mg";
    }
    if (metric == "cpu_temp") {
        return "C";
    }
    if (metric == "cpu_load") {
        return "%";
    }
    return "";
}

QString findCustomIconPath()
{
    const QString appDir = QCoreApplication::applicationDirPath();
    const QStringList candidates = {
        QDir(appDir).filePath("app_icon.ico"),
        QDir(appDir).filePath("app_icon.png"),
        QDir(appDir).filePath("assets/app_icon.ico"),
        QDir(appDir).filePath("assets/app_icon.png"),
        QDir(appDir).filePath("../../assets/app_icon.ico"),
        QDir(appDir).filePath("../../assets/app_icon.png")
    };
    for (const QString &path : candidates) {
        if (QFileInfo::exists(path)) {
            return path;
        }
    }
    return QString();
}

bool parseJsonPayload(const QString &text, QByteArray *outCompactJson, QString *error)
{
    QJsonParseError parseError;
    const QByteArray raw = text.trimmed().toUtf8();
    if (raw.isEmpty()) {
        if (error) {
            *error = "发布内容为空，请输入 JSON。";
        }
        return false;
    }

    const QJsonDocument doc = QJsonDocument::fromJson(raw, &parseError);
    if (parseError.error != QJsonParseError::NoError || doc.isNull()) {
        if (error) {
            *error = QString("JSON 解析失败：%1").arg(parseError.errorString());
        }
        return false;
    }

    if (outCompactJson) {
        *outCompactJson = doc.toJson(QJsonDocument::Compact);
    }
    return true;
}

bool hasMosquittoService()
{
    QProcess proc;
    proc.start("sc", QStringList() << "query" << "mosquitto");
    if (!proc.waitForFinished(1200)) {
        proc.kill();
        return false;
    }
    return proc.exitStatus() == QProcess::NormalExit && proc.exitCode() == 0;
}

bool isMosquittoServiceRunning()
{
    QProcess proc;
    proc.start("sc", QStringList() << "query" << "mosquitto");
    if (!proc.waitForFinished(1500)) {
        proc.kill();
        return false;
    }
    if (proc.exitStatus() != QProcess::NormalExit || proc.exitCode() != 0) {
        return false;
    }

    const QString output = QString::fromLocal8Bit(proc.readAllStandardOutput()).toUpper();
    return output.contains("RUNNING");
}

bool hasMosquittoInstalled()
{
    if (hasMosquittoService()) {
        return true;
    }

    if (QFileInfo::exists("C:/Program Files/mosquitto/mosquitto.exe")) {
        return true;
    }

    QProcess whereProc;
    whereProc.start("where", QStringList() << "mosquitto");
    if (!whereProc.waitForFinished(1200)) {
        whereProc.kill();
        return false;
    }

    return whereProc.exitStatus() == QProcess::NormalExit && whereProc.exitCode() == 0
           && !QString::fromLocal8Bit(whereProc.readAllStandardOutput()).trimmed().isEmpty();
}

bool isLikelyLocalBrokerAddress(const QString &broker)
{
    const QString lower = normalizeBrokerUri(broker).trimmed().toLower();
    return lower.contains("127.0.0.1") || lower.contains("localhost") || lower.contains("::1");
}

void runElevatedPowerShell(const QString &command)
{
    QString escaped = command;
    escaped.replace("`", "``");
    escaped.replace("\"", "`\"");

    const QString launcher =
        QString("Start-Process PowerShell -Verb RunAs -ArgumentList '-NoProfile -ExecutionPolicy Bypass -Command \"%1\"'")
            .arg(escaped);

    QProcess::startDetached("powershell", QStringList()
                                          << "-NoProfile"
                                          << "-ExecutionPolicy"
                                          << "Bypass"
                                          << "-Command"
                                          << launcher);
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

    setMinimumSize(1160, 760);
    resize(1280, 800);
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
        "QPushButton#publishButton {"
        "  background: #0ea5a4;"
        "}"
        "QPushButton#publishButton:hover {"
        "  background: #0f766e;"
        "}"
        "QPushButton#connectButton[connectedState=\"false\"] {"
        "  background: #2563eb;"
        "}"
        "QPushButton#connectButton[connectedState=\"false\"]:hover {"
        "  background: #1d4ed8;"
        "}"
        "QPushButton#connectButton[connectedState=\"true\"] {"
        "  background: #16a34a;"
        "}"
        "QPushButton#connectButton[connectedState=\"true\"]:hover {"
        "  background: #15803d;"
        "}"
        "QPlainTextEdit {"
        "  background: #ffffff;"
        "  border: 1px solid #d0d5dd;"
        "  border-radius: 10px;"
        "  padding: 8px;"
        "  color: #111827;"
        "}"
        "QGroupBox {"
        "  border: 1px solid #d0d5dd;"
        "  border-radius: 12px;"
        "  margin-top: 10px;"
        "  background: #f8fafc;"
        "}"
        "QGroupBox::title {"
        "  subcontrol-origin: margin;"
        "  left: 12px;"
        "  padding: 0 6px;"
        "  color: #334155;"
        "  font-weight: 700;"
        "}"
    );

    connected_ = false;
    currentLanguage_ = UiLanguage::English;
    deviceStateCode_ = 0;
    client = nullptr;
    currentTopic_ = DEFAULT_TOPIC;
    lwtTopic_ = DEFAULT_LWT_TOPIC;
    selectedSubQos_ = DEFAULT_QOS;
    selectedPubQos_ = DEFAULT_QOS;
    connBox_ = nullptr;
    subBox_ = nullptr;
    pubBox_ = nullptr;
    chartBox_ = nullptr;
    logBox_ = nullptr;
    languageLabel_ = nullptr;
    languageCombo_ = nullptr;
    subQosLabel_ = nullptr;
    topicCombo_ = nullptr;
    subQosCombo_ = nullptr;
    lwtTopicLabel_ = nullptr;
    lwtTopicCombo_ = nullptr;
    lwtQosLabel_ = nullptr;
    lwtQosCombo_ = nullptr;
    pubTopicLabel_ = nullptr;
    pubTopicCombo_ = nullptr;
    pubQosLabel_ = nullptr;
    pubQosCombo_ = nullptr;
    pubPayloadLabel_ = nullptr;
    pubPayloadEdit_ = nullptr;
    publishButton_ = nullptr;
    clearCacheButton_ = nullptr;
    passwordToggleButton_ = nullptr;
    deviceStateLabel_ = nullptr;

    ui->brokerLabel->setText("Broker");
    ui->clientIdLabel->setText("Client ID");
    ui->usernameLabel->setText("Username");
    ui->passwordLabel->setText("Password");
    ui->topicLabel->setText("Data Topic");
    ui->metricLabel->setText("Metric");
    ui->connectButton->setText("Connect");
    ui->disconnectButton->setText("Disconnect");
    ui->subscribeButton->setText("Subscribe");
    ui->statusShadowLabel->setText("Data and LWT topics are independently configurable.");
    ui->connectButton->setProperty("connectedState", false);

    ui->brokerEdit->setText(DEFAULT_ADDRESS);
    ui->clientIdEdit->setText(DEFAULT_CLIENTID);
    ui->usernameEdit->setText(DEFAULT_AUTHMETHOD);
    ui->passwordEdit->setText(DEFAULT_AUTHTOKEN);
    ui->passwordEdit->setEchoMode(QLineEdit::Password);
    ui->passwordEdit->setClearButtonEnabled(true);
    ui->brokerEdit->setPlaceholderText("例如：tcp://127.0.0.1:1883");
    ui->outputText->setPlaceholderText("运行日志会显示在这里...");

    ui->brokerEdit->setMinimumWidth(280);
    ui->clientIdEdit->setMinimumWidth(220);
    ui->usernameEdit->setMinimumWidth(180);
    ui->passwordEdit->setMinimumWidth(220);
    ui->metricCombo->setMinimumWidth(190);
    ui->metricCombo->setMinimumContentsLength(14);

    ui->brokerEdit->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    ui->clientIdEdit->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
    ui->usernameEdit->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
    ui->passwordEdit->setSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::Fixed);
    ui->metricCombo->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);

    topicCombo_ = new QComboBox(ui->centralWidget);
    topicCombo_->setEditable(true);
    topicCombo_->setInsertPolicy(QComboBox::NoInsert);
    topicCombo_->setMinimumWidth(420);
    topicCombo_->setMinimumContentsLength(22);
    topicCombo_->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    topicCombo_->addItem(DEFAULT_TOPIC);
    topicCombo_->addItem("mqttx_local_test");
    topicCombo_->addItem("een1071/gui/status");
    topicCombo_->setCurrentText(DEFAULT_TOPIC);
    topicCombo_->setToolTip("可下拉选择，也可手动输入任意主题");
    topicCombo_->setStyleSheet(
        "QComboBox {"
        "  background: #ffffff;"
        "  border: 1px solid #d0d5dd;"
        "  border-radius: 8px;"
        "  padding: 4px 8px;"
        "  min-height: 26px;"
        "}"
        "QComboBox:focus {"
        "  border: 1px solid #2e90fa;"
        "}");
    ui->topicEdit->setVisible(false);

    setWindowTitle("MQTT ADXL345 Real-time Monitor");
    const QString iconPath = findCustomIconPath();
    if (!iconPath.isEmpty()) {
        setWindowIcon(QIcon(iconPath));
    } else {
        setWindowIcon(style()->standardIcon(QStyle::SP_ComputerIcon));
    }

    subQosLabel_ = new QLabel("接收 QoS", ui->centralWidget);
    subQosCombo_ = new QComboBox(ui->centralWidget);
    subQosCombo_->addItem("0 - 至多一次", 0);
    subQosCombo_->addItem("1 - 至少一次", 1);
    subQosCombo_->addItem("2 - 只有一次", 2);
    subQosCombo_->setCurrentIndex(subQosCombo_->findData(DEFAULT_QOS));
    subQosCombo_->setMinimumWidth(150);
    subQosCombo_->setMinimumContentsLength(12);

    lwtTopicLabel_ = new QLabel("遗嘱主题", ui->centralWidget);
    lwtTopicCombo_ = new QComboBox(ui->centralWidget);
    lwtTopicCombo_->setEditable(true);
    lwtTopicCombo_->setInsertPolicy(QComboBox::NoInsert);
    lwtTopicCombo_->setMinimumWidth(260);
    lwtTopicCombo_->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    lwtTopicCombo_->setToolTip("独立设置遗嘱主题；为空时默认使用 sensor/adxl345/status");

    lwtQosLabel_ = new QLabel("遗嘱 QoS", ui->centralWidget);
    lwtQosCombo_ = new QComboBox(ui->centralWidget);
    lwtQosCombo_->addItem("0 - 至多一次", 0);
    lwtQosCombo_->addItem("1 - 至少一次", 1);
    lwtQosCombo_->addItem("2 - 只有一次", 2);
    lwtQosCombo_->setCurrentIndex(lwtQosCombo_->findData(DEFAULT_QOS));
    lwtQosCombo_->setMinimumWidth(120);

    deviceStateLabel_ = new QLabel("Device: Disconnected", ui->centralWidget);
    deviceStateLabel_->setStyleSheet("QLabel { color: #475467; font-weight: 700; }");

    languageLabel_ = new QLabel("Language", ui->centralWidget);
    languageCombo_ = new QComboBox(ui->centralWidget);
    languageCombo_->addItem("English", "en");
    languageCombo_->addItem("中文", "zh");
    languageCombo_->setMinimumWidth(120);
    languageCombo_->setCurrentIndex(0);

    pubTopicLabel_ = new QLabel("发布主题", ui->centralWidget);
    pubTopicCombo_ = new QComboBox(ui->centralWidget);
    pubTopicCombo_->setEditable(true);
    pubTopicCombo_->setInsertPolicy(QComboBox::NoInsert);
    pubTopicCombo_->setMinimumWidth(260);
    pubTopicCombo_->setMinimumContentsLength(16);
    pubTopicCombo_->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    pubTopicCombo_->addItem(DEFAULT_TOPIC);
    pubTopicCombo_->addItem("een1071/gui/status");
    pubTopicCombo_->addItem("mqttx_local_test");
    pubTopicCombo_->setCurrentText(DEFAULT_TOPIC);
    pubTopicCombo_->setToolTip("选择或手动输入发布主题");

    pubQosLabel_ = new QLabel("发送 QoS", ui->centralWidget);
    pubQosCombo_ = new QComboBox(ui->centralWidget);
    pubQosCombo_->addItem("0 - 至多一次", 0);
    pubQosCombo_->addItem("1 - 至少一次", 1);
    pubQosCombo_->addItem("2 - 只有一次", 2);
    pubQosCombo_->setCurrentIndex(pubQosCombo_->findData(DEFAULT_QOS));
    pubQosCombo_->setMinimumWidth(120);
    pubQosCombo_->setMinimumContentsLength(12);

    publishButton_ = new QPushButton("发布 JSON", ui->centralWidget);
    publishButton_->setObjectName("publishButton");
    publishButton_->setMinimumWidth(120);

    clearCacheButton_ = new QPushButton("清除数据缓存", ui->centralWidget);
    clearCacheButton_->setObjectName("clearCacheButton");
    clearCacheButton_->setMinimumWidth(128);
    clearCacheButton_->setToolTip("清空指标缓存、图形曲线与统计数据");

    pubPayloadLabel_ = new QLabel("发布内容(JSON)", ui->centralWidget);
    pubPayloadEdit_ = new QPlainTextEdit(ui->centralWidget);
    pubPayloadEdit_->setPlaceholderText("请输入 JSON，例如：{\"accel_x\":-129,\"accel_y\":-193,\"accel_z\":-76,\"cpu_load\":0,\"cpu_temp_c\":51.8,\"pitch\":-31.8,\"roll\":-52.1,\"timestamp\":\"2026-04-10T07:17:46Z\"}");
    pubPayloadEdit_->setMinimumHeight(100);
    pubPayloadEdit_->setMaximumHeight(128);
    pubPayloadEdit_->setPlainText("{\n  \"accel_x\": -129,\n  \"accel_y\": -193,\n  \"accel_z\": -76,\n  \"cpu_load\": 0,\n  \"cpu_temp_c\": 51.799999237060547,\n  \"pitch\": -31.877994537353516,\n  \"roll\": -52.196861267089844,\n  \"timestamp\": \"2026-04-10T07:17:46Z\"\n}");

    passwordToggleButton_ = new QToolButton(ui->centralWidget);
    passwordToggleButton_->setText("显示");
    passwordToggleButton_->setCheckable(true);
    passwordToggleButton_->setCursor(Qt::PointingHandCursor);
    passwordToggleButton_->setToolTip("切换密码明文/隐藏");
    passwordToggleButton_->setStyleSheet(
        "QToolButton {"
        "  color: #2563eb;"
        "  background: transparent;"
        "  border: none;"
        "  font-weight: 600;"
        "  padding: 0 4px;"
        "}"
        "QToolButton:hover {"
        "  color: #1d4ed8;"
        "}");

    connect(passwordToggleButton_, &QToolButton::toggled, this, [this](bool checked) {
        ui->passwordEdit->setEchoMode(checked ? QLineEdit::Normal : QLineEdit::Password);
        passwordToggleButton_->setText(checked ? trUi("隐藏", "Hide") : trUi("显示", "Show"));
    });
    connect(subQosCombo_, qOverload<int>(&QComboBox::currentIndexChanged), this, [this](int) {
        selectedSubQos_ = subQosCombo_->currentData().toInt();
    });
    connect(pubQosCombo_, qOverload<int>(&QComboBox::currentIndexChanged), this, [this](int) {
        selectedPubQos_ = pubQosCombo_->currentData().toInt();
    });
    connect(languageCombo_, qOverload<int>(&QComboBox::currentIndexChanged), this, [this](int) {
        currentLanguage_ = languageCombo_ && languageCombo_->currentData().toString() == "zh"
                               ? UiLanguage::Chinese
                               : UiLanguage::English;
        applyUiLanguage();
    });
    connect(publishButton_, &QPushButton::clicked, this, [this]() { publishJsonMessage(); });
    connect(clearCacheButton_, &QPushButton::clicked, this, [this]() { clearDataCache(); });

    setupResponsiveLayout();

    if (lwtTopicCombo_) {
        lwtTopicCombo_->addItem(lwtTopic_);
        lwtTopicCombo_->setCurrentText(lwtTopic_);
    }

    updateConnectionUi(false);

    populateMetricCombo();
    currentMetric_ = ui->metricCombo->currentData().toString();
    applyUiLanguage();
    setupPlot();

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

    QObject::connect(this, &MainWindow::messageSignal, this, &MainWindow::on_MQTTmessage);
    appendLog(trUi("应用已启动。请配置连接参数并点击“连接”。", "Application started. Configure connection settings and click Connect."));
    if (!iconPath.isEmpty()) {
        appendLog(trUi(QString("已加载自定义图标：%1").arg(iconPath),
                       QString("Custom icon loaded: %1").arg(iconPath)));
    } else {
        appendLog(trUi("未检测到自定义图标。可在 assets/app_icon.ico 或 assets/app_icon.png 放置图标后重启应用生效。",
                       "No custom icon found. Put app_icon.ico or app_icon.png in assets and restart the app."));
    }
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

QString MainWindow::trUi(const QString &zh, const QString &en) const
{
    return currentLanguage_ == UiLanguage::Chinese ? zh : en;
}

QString MainWindow::metricDisplayNameUi(const QString &metric) const
{
    if (metric == "pitch") {
        return trUi("俯仰角", "Pitch");
    }
    if (metric == "roll") {
        return trUi("横滚角", "Roll");
    }
    if (metric == "x") {
        return trUi("X 轴加速度", "Accel X");
    }
    if (metric == "y") {
        return trUi("Y 轴加速度", "Accel Y");
    }
    if (metric == "z") {
        return trUi("Z 轴加速度", "Accel Z");
    }
    if (metric == "cpu_temp") {
        return trUi("CPU 温度", "CPU Temp");
    }
    if (metric == "cpu_load") {
        return trUi("CPU 负载", "CPU Load");
    }
    return metric;
}

QString MainWindow::metricAxisLabelUi(const QString &metric) const
{
    const QString unit = metricUnit(metric);
    if (unit.isEmpty()) {
        return metricDisplayNameUi(metric);
    }
    return QString("%1 (%2)").arg(metricDisplayNameUi(metric), unit);
}

QString MainWindow::emptyStatsText() const
{
    return trUi(
        "最近 60 秒统计：平均值=N/A，最小值=N/A，最大值=N/A",
        "Last 60s stats: avg=N/A, min=N/A, max=N/A");
}

void MainWindow::refreshDeviceStateLabel()
{
    if (!deviceStateLabel_) {
        return;
    }

    if (deviceStateCode_ == 1) {
        deviceStateLabel_->setText(trUi("设备状态：在线", "Device: Online"));
        deviceStateLabel_->setStyleSheet("QLabel { color: #027a48; font-weight: 700; }");
        return;
    }
    if (deviceStateCode_ == 2) {
        deviceStateLabel_->setText(trUi("设备状态：离线", "Device: Offline"));
        deviceStateLabel_->setStyleSheet("QLabel { color: #b42318; font-weight: 700; }");
        return;
    }

    deviceStateLabel_->setText(trUi("设备状态：未连接", "Device: Disconnected"));
    deviceStateLabel_->setStyleSheet("QLabel { color: #475467; font-weight: 700; }");
}

void MainWindow::populateMetricCombo()
{
    if (!ui || !ui->metricCombo) {
        return;
    }

    const QString selectedMetric = ui->metricCombo->currentData().toString().isEmpty()
                                       ? currentMetric_
                                       : ui->metricCombo->currentData().toString();

    QSignalBlocker blocker(ui->metricCombo);
    ui->metricCombo->clear();
    ui->metricCombo->addItem(trUi("俯仰角 (pitch)", "Pitch (pitch)"), "pitch");
    ui->metricCombo->addItem(trUi("横滚角 (roll)", "Roll (roll)"), "roll");
    ui->metricCombo->addItem(trUi("X 轴加速度 (x)", "Accel X (x)"), "x");
    ui->metricCombo->addItem(trUi("Y 轴加速度 (y)", "Accel Y (y)"), "y");
    ui->metricCombo->addItem(trUi("Z 轴加速度 (z)", "Accel Z (z)"), "z");
    ui->metricCombo->addItem(trUi("CPU 温度 (cpu_temp)", "CPU Temp (cpu_temp)"), "cpu_temp");
    ui->metricCombo->addItem(trUi("CPU 负载 (cpu_load)", "CPU Load (cpu_load)"), "cpu_load");

    int index = ui->metricCombo->findData(selectedMetric);
    if (index < 0) {
        index = 0;
    }
    ui->metricCombo->setCurrentIndex(index);
    currentMetric_ = ui->metricCombo->currentData().toString();
}

void MainWindow::applyUiLanguage()
{
    if (!ui) {
        return;
    }

    setWindowTitle(trUi("MQTT ADXL345 实时监控面板", "MQTT ADXL345 Real-time Monitor"));
    ui->brokerLabel->setText(trUi("Broker 地址", "Broker"));
    ui->clientIdLabel->setText(trUi("客户端 ID", "Client ID"));
    ui->usernameLabel->setText(trUi("用户名", "Username"));
    ui->passwordLabel->setText(trUi("密码", "Password"));
    ui->topicLabel->setText(trUi("订阅主题", "Data Topic"));
    ui->metricLabel->setText(trUi("监测指标", "Metric"));
    ui->connectButton->setText(trUi("连接", "Connect"));
    ui->disconnectButton->setText(trUi("断开", "Disconnect"));
    ui->subscribeButton->setText(trUi("订阅主题", "Subscribe"));
    ui->statusShadowLabel->setText(trUi(
        "数据主题与遗嘱主题支持独立配置；遗嘱默认订阅 sensor/adxl345/status。",
        "Data and LWT topics are independently configurable; default LWT topic is sensor/adxl345/status."));

    ui->brokerEdit->setPlaceholderText(trUi("例如：tcp://127.0.0.1:1883", "e.g. tcp://127.0.0.1:1883"));
    ui->outputText->setPlaceholderText(trUi("运行日志会显示在这里...", "Runtime logs will appear here..."));

    if (topicCombo_) {
        topicCombo_->setToolTip(trUi("可下拉选择，也可手动输入任意主题", "Select from list or input any topic manually"));
    }
    if (subQosLabel_) {
        subQosLabel_->setText(trUi("接收 QoS", "Sub QoS"));
    }
    if (subQosCombo_ && subQosCombo_->count() >= 3) {
        subQosCombo_->setItemText(0, trUi("0 - 至多一次", "0 - At most once"));
        subQosCombo_->setItemText(1, trUi("1 - 至少一次", "1 - At least once"));
        subQosCombo_->setItemText(2, trUi("2 - 只有一次", "2 - Exactly once"));
    }
    if (lwtTopicLabel_) {
        lwtTopicLabel_->setText(trUi("遗嘱主题", "LWT Topic"));
    }
    if (lwtTopicCombo_) {
        lwtTopicCombo_->setToolTip(trUi("独立设置遗嘱主题；为空时默认使用 sensor/adxl345/status", "Set LWT topic independently; fallback is sensor/adxl345/status"));
    }
    if (lwtQosLabel_) {
        lwtQosLabel_->setText(trUi("遗嘱 QoS", "LWT QoS"));
    }
    if (lwtQosCombo_ && lwtQosCombo_->count() >= 3) {
        lwtQosCombo_->setItemText(0, trUi("0 - 至多一次", "0 - At most once"));
        lwtQosCombo_->setItemText(1, trUi("1 - 至少一次", "1 - At least once"));
        lwtQosCombo_->setItemText(2, trUi("2 - 只有一次", "2 - Exactly once"));
    }
    if (pubTopicLabel_) {
        pubTopicLabel_->setText(trUi("发布主题", "Publish Topic"));
    }
    if (pubTopicCombo_) {
        pubTopicCombo_->setToolTip(trUi("选择或手动输入发布主题", "Select or input publish topic manually"));
    }
    if (pubQosLabel_) {
        pubQosLabel_->setText(trUi("发送 QoS", "Pub QoS"));
    }
    if (pubQosCombo_ && pubQosCombo_->count() >= 3) {
        pubQosCombo_->setItemText(0, trUi("0 - 至多一次", "0 - At most once"));
        pubQosCombo_->setItemText(1, trUi("1 - 至少一次", "1 - At least once"));
        pubQosCombo_->setItemText(2, trUi("2 - 只有一次", "2 - Exactly once"));
    }
    if (pubPayloadLabel_) {
        pubPayloadLabel_->setText(trUi("发布内容(JSON)", "Payload (JSON)"));
    }
    if (pubPayloadEdit_) {
        pubPayloadEdit_->setPlaceholderText(trUi(
            "请输入 JSON，例如：{\"accel_x\":-129,\"accel_y\":-193,\"accel_z\":-76,\"cpu_load\":0,\"cpu_temp_c\":51.8,\"pitch\":-31.8,\"roll\":-52.1,\"timestamp\":\"2026-04-10T07:17:46Z\"}",
            "Input JSON, e.g.: {\"accel_x\":-129,\"accel_y\":-193,\"accel_z\":-76,\"cpu_load\":0,\"cpu_temp_c\":51.8,\"pitch\":-31.8,\"roll\":-52.1,\"timestamp\":\"2026-04-10T07:17:46Z\"}"));
    }
    if (publishButton_) {
        publishButton_->setText(trUi("发布 JSON", "Publish JSON"));
    }
    if (clearCacheButton_) {
        clearCacheButton_->setText(trUi("清除数据缓存", "Clear Cache"));
        clearCacheButton_->setToolTip(trUi("清空指标缓存、图形曲线与统计数据", "Clear metric cache, chart data and stats"));
    }
    if (passwordToggleButton_) {
        passwordToggleButton_->setText(passwordToggleButton_->isChecked() ? trUi("隐藏", "Hide") : trUi("显示", "Show"));
        passwordToggleButton_->setToolTip(trUi("切换密码明文/隐藏", "Toggle password visibility"));
    }
    if (languageLabel_) {
        languageLabel_->setText(trUi("语言", "Language"));
    }

    if (connBox_) {
        connBox_->setTitle(trUi("连接参数", "Connection"));
    }
    if (subBox_) {
        subBox_->setTitle(trUi("订阅设置", "Subscription"));
    }
    if (pubBox_) {
        pubBox_->setTitle(trUi("发布(JSON)", "Publish (JSON)"));
    }
    if (chartBox_) {
        chartBox_->setTitle(trUi("实时图形", "Realtime Chart"));
    }
    if (logBox_) {
        logBox_->setTitle(trUi("运行日志", "Runtime Log"));
    }

    populateMetricCombo();
    refreshDeviceStateLabel();

    if (ui->customPlot && ui->customPlot->graphCount() > 0 && ui->customPlot->graph(0)) {
        ui->customPlot->xAxis->setLabel(trUi("时间 (HH:mm:ss)", "Time (HH:mm:ss)"));
        ui->customPlot->yAxis->setLabel(metricAxisLabelUi(currentMetric_));
        ui->customPlot->graph(0)->setName(metricAxisLabelUi(currentMetric_));
        ui->customPlot->replot();
    }

    updateRollingStats(currentMetric_);
}

void MainWindow::appendLog(const QString &line)
{
    const QString ts = QDateTime::currentDateTime().toString("HH:mm:ss");
    ui->outputText->appendPlainText(QString("[%1] %2").arg(ts, line));
    ui->outputText->ensureCursorVisible();
}

void MainWindow::updateConnectionUi(bool connected)
{
    if (!ui || !ui->connectButton) {
        return;
    }

    ui->connectButton->setProperty("connectedState", connected);
    ui->connectButton->style()->unpolish(ui->connectButton);
    ui->connectButton->style()->polish(ui->connectButton);
    ui->connectButton->update();
}

void MainWindow::setupPlot()
{
    ui->customPlot->addGraph();
    ui->customPlot->graph(0)->setName(metricAxisLabelUi(currentMetric_));
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

    QSharedPointer<QCPAxisTickerDateTime> timeTicker(new QCPAxisTickerDateTime);
    timeTicker->setDateTimeFormat("HH:mm:ss");
    timeTicker->setDateTimeSpec(Qt::LocalTime);
    ui->customPlot->xAxis->setTicker(timeTicker);
    ui->customPlot->xAxis->setLabel(trUi("时间 (HH:mm:ss)", "Time (HH:mm:ss)"));
    ui->customPlot->yAxis->setLabel(metricAxisLabelUi(currentMetric_));
    ui->customPlot->xAxis->setRange(QDateTime::currentSecsSinceEpoch() - 60, QDateTime::currentSecsSinceEpoch());
    ui->customPlot->yAxis->setRange(-10, 10);
    ui->customPlot->replot();

    ui->statsLabel->setText(emptyStatsText());
}

void MainWindow::setupResponsiveLayout()
{
    QVBoxLayout *rootLayout = new QVBoxLayout(ui->centralWidget);
    rootLayout->setContentsMargins(20, 20, 20, 20);
    rootLayout->setSpacing(10);

    connBox_ = new QGroupBox("Connection", ui->centralWidget);
    subBox_ = new QGroupBox("Subscription", ui->centralWidget);
    pubBox_ = new QGroupBox("Publish (JSON)", ui->centralWidget);
    chartBox_ = new QGroupBox("Realtime Chart", ui->centralWidget);
    logBox_ = new QGroupBox("Runtime Log", ui->centralWidget);

    connBox_->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    subBox_->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    pubBox_->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    chartBox_->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    logBox_->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);

    QGridLayout *connLayout = new QGridLayout();
    connLayout->setContentsMargins(12, 10, 12, 12);
    connLayout->setHorizontalSpacing(10);
    connLayout->setVerticalSpacing(6);

    connLayout->addWidget(ui->brokerLabel, 0, 0);
    connLayout->addWidget(ui->brokerEdit, 0, 1);
    connLayout->addWidget(ui->clientIdLabel, 0, 2);
    connLayout->addWidget(ui->clientIdEdit, 0, 3);
    connLayout->addWidget(ui->usernameLabel, 1, 0);
    connLayout->addWidget(ui->usernameEdit, 1, 1);
    connLayout->addWidget(ui->passwordLabel, 1, 2);

    QHBoxLayout *passwordLayout = new QHBoxLayout();
    passwordLayout->setContentsMargins(0, 0, 0, 0);
    passwordLayout->setSpacing(6);
    passwordLayout->addWidget(ui->passwordEdit, 1);
    passwordLayout->addWidget(passwordToggleButton_);
    connLayout->addLayout(passwordLayout, 1, 3);

    connLayout->setColumnStretch(1, 5);
    connLayout->setColumnStretch(3, 5);
    connBox_->setLayout(connLayout);

    QGridLayout *actionLayout = new QGridLayout();
    actionLayout->setContentsMargins(12, 10, 12, 12);
    actionLayout->setHorizontalSpacing(10);
    actionLayout->setVerticalSpacing(6);
    actionLayout->addWidget(ui->topicLabel, 0, 0);
    actionLayout->addWidget(topicCombo_, 0, 1);
    actionLayout->addWidget(ui->subscribeButton, 0, 2);
    actionLayout->addWidget(ui->connectButton, 0, 3);
    actionLayout->addWidget(ui->disconnectButton, 0, 4);
    actionLayout->addWidget(languageLabel_, 0, 5);
    actionLayout->addWidget(languageCombo_, 0, 6);

    actionLayout->addWidget(ui->metricLabel, 1, 0);
    actionLayout->addWidget(ui->metricCombo, 1, 1);
    actionLayout->addWidget(subQosLabel_, 1, 2);
    actionLayout->addWidget(subQosCombo_, 1, 3);
    actionLayout->addWidget(deviceStateLabel_, 1, 4);
    actionLayout->addWidget(clearCacheButton_, 1, 5);

    actionLayout->addWidget(lwtTopicLabel_, 2, 0);
    actionLayout->addWidget(lwtTopicCombo_, 2, 1);
    actionLayout->addWidget(lwtQosLabel_, 2, 2);
    actionLayout->addWidget(lwtQosCombo_, 2, 3);

    actionLayout->setColumnStretch(1, 8);
    actionLayout->setColumnStretch(4, 2);
    actionLayout->setColumnStretch(5, 2);
    actionLayout->setColumnStretch(6, 2);
    subBox_->setLayout(actionLayout);

    QVBoxLayout *chartLayout = new QVBoxLayout();
    chartLayout->setContentsMargins(12, 10, 12, 12);
    chartLayout->setSpacing(8);
    chartLayout->addWidget(ui->customPlot, 1);
    chartLayout->addWidget(ui->statsLabel);
    chartBox_->setLayout(chartLayout);

    QGridLayout *publishLayout = new QGridLayout();
    publishLayout->setContentsMargins(12, 10, 12, 12);
    publishLayout->setHorizontalSpacing(10);
    publishLayout->setVerticalSpacing(8);
    publishLayout->addWidget(pubTopicLabel_, 0, 0);
    publishLayout->addWidget(pubTopicCombo_, 0, 1, 1, 4);
    publishLayout->addWidget(pubQosLabel_, 1, 0);
    publishLayout->addWidget(pubQosCombo_, 1, 1);
    publishLayout->addWidget(publishButton_, 1, 4);
    publishLayout->addWidget(pubPayloadLabel_, 2, 0);
    publishLayout->addWidget(pubPayloadEdit_, 2, 1, 1, 4);
    publishLayout->setColumnStretch(1, 6);
    publishLayout->setColumnStretch(2, 1);
    publishLayout->setColumnStretch(3, 1);
    pubBox_->setLayout(publishLayout);

    QVBoxLayout *logLayout = new QVBoxLayout();
    logLayout->setContentsMargins(12, 10, 12, 12);
    logLayout->setSpacing(8);
    logLayout->addWidget(ui->statusShadowLabel);
    logLayout->addWidget(ui->outputText, 1);
    logBox_->setLayout(logLayout);

    QVBoxLayout *leftPane = new QVBoxLayout();
    leftPane->setSpacing(10);
    leftPane->addWidget(pubBox_, 4);
    leftPane->addWidget(logBox_, 6);

    QVBoxLayout *rightPane = new QVBoxLayout();
    rightPane->setSpacing(10);
    rightPane->addWidget(chartBox_, 1);

    QWidget *leftContainer = new QWidget(ui->centralWidget);
    leftContainer->setLayout(leftPane);
    QWidget *rightContainer = new QWidget(ui->centralWidget);
    rightContainer->setLayout(rightPane);

    QSplitter *contentSplitter = new QSplitter(Qt::Horizontal, ui->centralWidget);
    contentSplitter->addWidget(leftContainer);
    contentSplitter->addWidget(rightContainer);
    contentSplitter->setChildrenCollapsible(false);
    contentSplitter->setStretchFactor(0, 5);
    contentSplitter->setStretchFactor(1, 6);

    rootLayout->addWidget(connBox_);
    rootLayout->addWidget(subBox_);
    rootLayout->addWidget(contentSplitter, 1);

    ui->outputText->setMinimumHeight(240);
    ui->customPlot->setMinimumWidth(520);
}

void MainWindow::subscribeToTopic(const QString &topic)
{
    if (!connected_ || !client) {
        appendLog("订阅已跳过：当前未连接 MQTT。 ");
        return;
    }

    currentTopic_ = topic.trimmed();
    if (currentTopic_.isEmpty()) {
        appendLog("主题为空，请先输入订阅主题。 ");
        return;
    }

    lwtTopic_ = lwtTopicCombo_ ? lwtTopicCombo_->currentText().trimmed() : QString();
    if (lwtTopic_.isEmpty()) {
        lwtTopic_ = DEFAULT_LWT_TOPIC;
    }

    if (lwtTopicCombo_ && lwtTopicCombo_->findText(lwtTopic_) < 0) {
        lwtTopicCombo_->addItem(lwtTopic_);
    }
    QStringList targetTopics;
    targetTopics << currentTopic_ << lwtTopic_;
    targetTopics.removeDuplicates();

    for (const QString &oldTopic : subscribedTopics_) {
        if (targetTopics.contains(oldTopic)) {
            continue;
        }
        MQTTClient_unsubscribe(client, oldTopic.toUtf8().constData());
        appendLog(QString("取消订阅主题 '%1'").arg(oldTopic));
    }

    if (topicCombo_ && topicCombo_->findText(currentTopic_) < 0) {
        topicCombo_->addItem(currentTopic_);
    }
    if (topicCombo_) {
        topicCombo_->setCurrentText(currentTopic_);
    }
    if (pubTopicCombo_ && pubTopicCombo_->findText(currentTopic_) < 0) {
        pubTopicCombo_->addItem(currentTopic_);
    }

    const QHash<QString, int> qosByTopic = {
        {currentTopic_, selectedSubQos_},
        {lwtTopic_, lwtQosCombo_ ? lwtQosCombo_->currentData().toInt() : selectedSubQos_}
    };

    for (const QString &t : targetTopics) {
        if (subscribedTopics_.contains(t)) {
            MQTTClient_unsubscribe(client, t.toUtf8().constData());
        }
        const int qos = qosByTopic.value(t, selectedSubQos_);
        const int rc = MQTTClient_subscribe(client, t.toUtf8().constData(), qos);
        appendLog(QString("订阅主题 '%1' (接收QoS=%2)，返回码 rc=%3").arg(t).arg(qos).arg(rc));
    }

    subscribedTopics_ = targetTopics;
    appendLog(QString("已启用两类订阅：数据=%1，遗嘱=%2").arg(currentTopic_, lwtTopic_));
}

void MainWindow::publishJsonMessage()
{
    if (!connected_ || !client) {
        appendLog("发布已跳过：当前未连接 MQTT。 ");
        QMessageBox::information(this, "未连接", "请先连接 MQTT Broker，再发布 JSON。 ");
        return;
    }

    const QString topic = pubTopicCombo_ ? pubTopicCombo_->currentText().trimmed() : QString();
    if (topic.isEmpty()) {
        appendLog("发布失败：发布主题为空。 ");
        QMessageBox::warning(this, "发布失败", "发布主题为空，请先输入主题。 ");
        return;
    }

    QString error;
    QByteArray payload;
    const QString payloadText = pubPayloadEdit_ ? pubPayloadEdit_->toPlainText() : QString();
    if (!parseJsonPayload(payloadText, &payload, &error)) {
        appendLog(QString("发布失败：%1").arg(error));
        QMessageBox::warning(this, "发布失败", error);
        return;
    }

    MQTTClient_message pubmsg = MQTTClient_message_initializer;
    pubmsg.payload = payload.data();
    pubmsg.payloadlen = payload.size();
    pubmsg.qos = selectedPubQos_;
    pubmsg.retained = 0;

    MQTTClient_deliveryToken token = 0;
    const int rc = MQTTClient_publishMessage(client, topic.toUtf8().constData(), &pubmsg, &token);
    if (rc != MQTTCLIENT_SUCCESS) {
        appendLog(QString("发布失败：topic='%1'，发送QoS=%2，rc=%3").arg(topic).arg(selectedPubQos_).arg(rc));
        QMessageBox::warning(this, "发布失败", QString("MQTT 发布失败，rc=%1").arg(rc));
        return;
    }

    MQTTClient_waitForCompletion(client, token, TIMEOUT);
    if (pubTopicCombo_ && pubTopicCombo_->findText(topic) < 0) {
        pubTopicCombo_->addItem(topic);
    }
    if (topicCombo_ && topicCombo_->findText(topic) < 0) {
        topicCombo_->addItem(topic);
    }

    appendLog(QString("已发布 JSON：topic='%1'，发送QoS=%2，字节数=%3")
                  .arg(topic)
                  .arg(selectedPubQos_)
                  .arg(payload.size()));
}

void MainWindow::pushDataPoint(const QString &metric, const QDateTime &ts, double value)
{
    const double x = ts.toSecsSinceEpoch();
    QQueue<QPair<double, double>> &history = metricHistory_[metric];
    history.enqueue(qMakePair(x, value));
    while (!history.isEmpty() && history.head().first < x - kHistoryWindowSec) {
        history.dequeue();
    }

    if (metric != currentMetric_) {
        return;
    }

    ui->customPlot->graph(0)->setName(metricAxisLabelUi(currentMetric_));
    ui->customPlot->graph(0)->addData(x, value);

    ui->customPlot->xAxis->setRange(x - 120.0, x + 2.0);
    ui->customPlot->graph(0)->rescaleValueAxis(true, true);
    ui->customPlot->replot();

    updateRollingStats(currentMetric_);
}

void MainWindow::updateRollingStats(const QString &metric)
{
    const QQueue<QPair<double, double>> history = metricHistory_.value(metric);
    if (history.isEmpty()) {
        ui->statsLabel->setText(emptyStatsText());
        return;
    }

    const double nowSec = history.last().first;
    double sum = 0.0;
    int count = 0;
    double minVal = std::numeric_limits<double>::max();
    double maxVal = std::numeric_limits<double>::lowest();

    for (const auto &p : history) {
        if (p.first < nowSec - kStatsWindowSec) {
            continue;
        }
        sum += p.second;
        ++count;
        if (p.second < minVal) {
            minVal = p.second;
        }
        if (p.second > maxVal) {
            maxVal = p.second;
        }
    }

    if (count == 0) {
        ui->statsLabel->setText(emptyStatsText());
        return;
    }

    const double avg = sum / static_cast<double>(count);
    const QString unit = metricUnit(metric);
    const QString avgText = unit.isEmpty() ? QString::number(avg, 'f', 3) : QString("%1 %2").arg(avg, 0, 'f', 3).arg(unit);
    const QString minText = unit.isEmpty() ? QString::number(minVal, 'f', 3) : QString("%1 %2").arg(minVal, 0, 'f', 3).arg(unit);
    const QString maxText = unit.isEmpty() ? QString::number(maxVal, 'f', 3) : QString("%1 %2").arg(maxVal, 0, 'f', 3).arg(unit);
    ui->statsLabel->setText(
        trUi("最近 60 秒统计：平均值=%1，最小值=%2，最大值=%3",
             "Last 60s stats: avg=%1, min=%2, max=%3")
            .arg(avgText)
            .arg(minText)
            .arg(maxText));
}

void MainWindow::refreshPlotForCurrentMetric()
{
    if (!ui->customPlot || ui->customPlot->graphCount() == 0 || !ui->customPlot->graph(0)) {
        return;
    }

    ui->customPlot->graph(0)->data()->clear();
    ui->customPlot->graph(0)->setName(metricAxisLabelUi(currentMetric_));

    const QQueue<QPair<double, double>> history = metricHistory_.value(currentMetric_);
    if (history.isEmpty()) {
        ui->customPlot->xAxis->setRange(QDateTime::currentSecsSinceEpoch() - 60, QDateTime::currentSecsSinceEpoch());
        ui->customPlot->yAxis->setRange(-10, 10);
        ui->customPlot->replot();
        updateRollingStats(currentMetric_);
        return;
    }

    for (const auto &p : history) {
        ui->customPlot->graph(0)->addData(p.first, p.second);
    }

    const double nowSec = history.last().first;
    ui->customPlot->xAxis->setRange(nowSec - 120.0, nowSec + 2.0);
    ui->customPlot->graph(0)->rescaleValueAxis(true, true);
    ui->customPlot->replot();
    updateRollingStats(currentMetric_);
}

void MainWindow::clearDataCache()
{
    metricHistory_.clear();

    if (ui && ui->customPlot && ui->customPlot->graphCount() > 0 && ui->customPlot->graph(0)) {
        ui->customPlot->graph(0)->data()->clear();
        ui->customPlot->xAxis->setRange(QDateTime::currentSecsSinceEpoch() - 60, QDateTime::currentSecsSinceEpoch());
        ui->customPlot->yAxis->setRange(-10, 10);
        ui->customPlot->replot();
    }

    if (ui && ui->statsLabel) {
        ui->statsLabel->setText(emptyStatsText());
    }

    appendLog("已清除数据缓存：指标历史、曲线与统计已重置。 ");
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
        paths << "accel_x" << "x" << "adxl345.accel_x" << "adxl345.x" << "adxl.accel_x" << "adxl.x" << "sensor.accel_x" << "sensor.x";
    } else if (metric == "y") {
        paths << "accel_y" << "y" << "adxl345.accel_y" << "adxl345.y" << "adxl.accel_y" << "adxl.y" << "sensor.accel_y" << "sensor.y";
    } else if (metric == "z") {
        paths << "accel_z" << "z" << "adxl345.accel_z" << "adxl345.z" << "adxl.accel_z" << "adxl.z" << "sensor.accel_z" << "sensor.z";
    } else if (metric == "cpu_temp") {
        paths << "cpu_temp_c" << "cpu_temp" << "cpu.temp" << "system.cpu_temp" << "system.cpu_temp_c";
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
    const QString brokerForClient = normalizeBrokerUri(broker);
    const QString clientId = ui->clientIdEdit->text().trimmed().isEmpty()
                                 ? QString("qt_gui_%1").arg(QDateTime::currentSecsSinceEpoch())
                                 : ui->clientIdEdit->text().trimmed();
    const QString username = ui->usernameEdit->text();
    const QString password = ui->passwordEdit->text();

    if (broker.isEmpty()) {
        appendLog("Broker 地址为空，请先填写。 ");
        return;
    }

        const bool localTarget = isLikelyLocalBrokerAddress(brokerForClient);

        if (!hasMosquittoInstalled()) {
         appendLog(localTarget
                 ? trUi("未检测到 Mosquitto 安装，且当前是本机 Broker 地址。建议先安装/启动 Mosquitto。",
                     "Mosquitto is not detected and the broker address is local. Install/start Mosquitto first.")
                 : trUi("未检测到本机 Mosquitto，但当前是远程 Broker 地址。可忽略并继续连接。",
                     "Local Mosquitto is not detected, but the broker address is remote. You can continue."));

        QMessageBox box(this);
        box.setIcon(QMessageBox::Information);
        box.setWindowTitle(trUi("未检测到 Mosquitto", "Mosquitto Not Detected"));
         box.setText(localTarget
                   ? trUi("当前 Broker 地址是本机。\n未检测到 Mosquitto，建议先安装并启动服务后再连接。",
                       "Current broker address is local.\nMosquitto is not detected. Install and start it before connecting.")
                   : trUi("当前 Broker 地址是远程。\n本机未安装 Mosquitto不影响远程连接，可直接继续。",
                       "Current broker address is remote.\nMissing local Mosquitto does not block remote connection."));
         box.setInformativeText(trUi(
             "如需本机调试：可点击“安装 Mosquitto”或“复制安装命令”。",
             "For local debugging, click 'Install Mosquitto' or 'Copy Install Command'."));

        QPushButton *installBtn = box.addButton(trUi("安装 Mosquitto", "Install Mosquitto"), QMessageBox::ActionRole);
        QPushButton *copyCmdBtn = box.addButton(trUi("复制安装命令", "Copy Install Command"), QMessageBox::ActionRole);
        QPushButton *continueBtn = box.addButton(trUi("继续连接", "Continue"), QMessageBox::AcceptRole);
        Q_UNUSED(continueBtn);
        box.exec();

        if (box.clickedButton() == installBtn) {
            runElevatedPowerShell("winget install --id EclipseMosquitto.Mosquitto -e --accept-source-agreements --accept-package-agreements; Start-Service mosquitto");
            appendLog(trUi("已发起管理员命令：安装并启动 Mosquitto。", "Triggered elevated command to install/start Mosquitto."));
            return;
        } else if (box.clickedButton() == copyCmdBtn) {
            const QString cmds =
                "winget install --id EclipseMosquitto.Mosquitto -e --accept-source-agreements --accept-package-agreements\n"
                "Start-Service mosquitto\n"
                "Get-NetTCPConnection -LocalPort 1883 -State Listen";
            if (QGuiApplication::clipboard()) {
                QGuiApplication::clipboard()->setText(cmds);
                appendLog(trUi("已复制 Mosquitto 安装/诊断命令到剪贴板。", "Copied Mosquitto install/diagnostic commands to clipboard."));
            }
            return;
        } else {
            appendLog(localTarget
                          ? trUi("用户选择继续连接（本机 Broker 可能仍不可用）。", "User chose to continue (local broker may still be unavailable).")
                          : trUi("用户选择继续连接远程 Broker。", "User chose to continue to remote broker."));
        }
    }

    if (localTarget && hasMosquittoService() && !isMosquittoServiceRunning()) {
        appendLog(trUi("检测到 Mosquitto 已安装但服务未运行。", "Mosquitto is installed but service is not running."));

        QMessageBox box(this);
        box.setIcon(QMessageBox::Warning);
        box.setWindowTitle(trUi("Mosquitto 服务未启动", "Mosquitto Service Not Running"));
        box.setText(trUi("当前是本机 Broker 地址，且 Mosquitto 服务未运行。", "Current broker is local and Mosquitto service is not running."));
        box.setInformativeText(trUi("先启动服务可避免连接时界面卡住。", "Start the service first to avoid UI stall during connect."));

        QPushButton *startBtn = box.addButton(trUi("启动服务", "Start Service"), QMessageBox::ActionRole);
        QPushButton *continueBtn = box.addButton(trUi("仍然连接", "Continue Anyway"), QMessageBox::AcceptRole);
        QPushButton *cancelBtn = box.addButton(trUi("取消", "Cancel"), QMessageBox::RejectRole);
        Q_UNUSED(continueBtn);
        box.exec();

        if (box.clickedButton() == startBtn) {
            runElevatedPowerShell("Start-Service mosquitto");
            appendLog(trUi("已发起管理员命令：Start-Service mosquitto。请启动后重试连接。", "Triggered elevated command: Start-Service mosquitto. Retry after service starts."));
            return;
        }
        if (box.clickedButton() == cancelBtn) {
            appendLog(trUi("已取消连接。", "Connection canceled."));
            return;
        }
    }

    const QByteArray brokerUtf8 = brokerForClient.toUtf8();
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
    opts.connectTimeout = 3;
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
    updateConnectionUi(true);
    appendLog(QString("已连接到 %1，客户端 ID：%2").arg(broker, clientId));
    subscribeToTopic(topicCombo_ ? topicCombo_->currentText() : QString());
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

    const QString topic = topicName ? QString::fromUtf8(topicName) : QString();
    const QString payload = QString::fromUtf8((const char*)message->payload, message->payloadlen);
    emit self->messageSignal(topic, payload);

    MQTTClient_freeMessage(&message);
    MQTTClient_free(topicName);
    return 1;
}

void MainWindow::on_MQTTmessage(QString topic, QString payload){
    const QString normalizedTopic = topic.trimmed();

    if (!lwtTopic_.isEmpty() && normalizedTopic == lwtTopic_) {
        const QString lower = payload.trimmed().toLower();
        const bool offline = lower.contains("offline") || lower.contains("disconnect") || lower.contains("lost");
        deviceStateCode_ = offline ? 2 : 1;
        refreshDeviceStateLabel();
        appendLog(QString("遗嘱主题消息：%1").arg(payload));
        return;
    }

    if (!currentTopic_.isEmpty() && normalizedTopic != currentTopic_) {
        appendLog(QString("收到其他主题消息 '%1'：%2").arg(normalizedTopic, payload));
        return;
    }

    const QByteArray bytes = payload.toUtf8();
    const QDateTime ts = extractTimestamp(bytes);

    QHash<QString, double> parsedValues;
    for (const QString &metric : supportedMetricKeys()) {
        bool ok = false;
        const double value = extractMetricValue(metric, bytes, &ok);
        if (!ok) {
            continue;
        }
        parsedValues.insert(metric, value);
        pushDataPoint(metric, ts, value);
    }

    if (parsedValues.isEmpty()) {
        appendLog(QString("收到 JSON，但未识别到可用指标：%1").arg(payload));
        return;
    }

    if (!parsedValues.contains(currentMetric_)) {
        appendLog(QString("收到 JSON，已在后台记录其他指标；当前指标 '%1' 本条无数据。 ").arg(currentMetric_));
        return;
    }

    const double value = parsedValues.value(currentMetric_);
    const QString unit = metricUnit(currentMetric_);
    const QString valueText = unit.isEmpty() ? QString::number(value, 'f', 3) : QString("%1 %2").arg(value, 0, 'f', 3).arg(unit);
    appendLog(QString("指标=%1，数值=%2，主题=%3")
                  .arg(metricDisplayNameUi(currentMetric_))
                  .arg(valueText)
                  .arg(normalizedTopic));
}

void connlost(void *context, char *cause) {
    MainWindow *self = static_cast<MainWindow *>(context);
    if (!self) {
        return;
    }

    const QString causeText = cause ? QString::fromUtf8(cause) : QString("unknown");
    QMetaObject::invokeMethod(self, [self, causeText]() {
        self->connected_ = false;
        self->updateConnectionUi(false);
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
    subscribedTopics_.clear();
    updateConnectionUi(false);
    deviceStateCode_ = 0;
    refreshDeviceStateLabel();
    appendLog("已断开连接。 ");
}

void MainWindow::on_subscribeButton_clicked()
{
    subscribeToTopic(topicCombo_ ? topicCombo_->currentText() : QString());
}

void MainWindow::on_metricCombo_currentTextChanged(const QString &text)
{
    Q_UNUSED(text);
    currentMetric_ = ui->metricCombo->currentData().toString();
    if (ui->customPlot) {
        ui->customPlot->yAxis->setLabel(metricAxisLabelUi(currentMetric_));
    }
    refreshPlotForCurrentMetric();
    appendLog(QString("已切换指标为 '%1'，已加载后台缓存数据。 ").arg(currentMetric_));
}
