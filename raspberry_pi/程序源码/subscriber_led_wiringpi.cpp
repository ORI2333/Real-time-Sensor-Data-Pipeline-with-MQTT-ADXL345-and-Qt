#include <iostream>
#include <thread>
#include <chrono>
#include <cstring>
#include <unistd.h>
#include <signal.h>
#include <atomic>
#include "MQTTClient.h"

// 先取消 HIGH 宏，再包含 ADXL345.h
#ifdef HIGH
#undef HIGH
#endif
#include "ADXL345.h"

#include <wiringPi.h>   // 该头文件会重新定义 HIGH 宏

using namespace een1071;

#define ADDRESS     "mqtt://192.168.127.152:1884"
#define CLIENTID    "Subscriber_LED"
#define TOPIC       "sensor/adxl345/PC"
#define USERNAME    "fangkuai"
#define PASSWORD    "2120033"

#define LED_LOW     0   // WiringPi 0 对应 BCM 17
#define LED_MED     2   // WiringPi 2 对应 BCM 27
#define LED_HIGH    3   // WiringPi 3 对应 BCM 22

std::atomic<bool> running(true);
MQTTClient mqtt_client_handle;

void stop_handler(int) {
    running = false;
    MQTTClient_disconnect(mqtt_client_handle, 0);
}

void set_led(int level) {
    if (level == -1) {
        digitalWrite(LED_LOW, LOW);
        digitalWrite(LED_MED, LOW);
        digitalWrite(LED_HIGH, LOW);
        return;
    }
    digitalWrite(LED_LOW,  (level == 0) ? HIGH : LOW);
    digitalWrite(LED_MED,  (level == 1) ? HIGH : LOW);
    digitalWrite(LED_HIGH, (level == 2) ? HIGH : LOW);
}

int message_arrived(void *context, char *topicName, int topicLen, MQTTClient_message *message) {
    char *payload = (char*)message->payload;
    printf("\n[MQTT] 收到消息: %s\n", payload);
    fflush(stdout);
    return 1;
}

int main() {
    if (wiringPiSetup() == -1) {
        std::cerr << "WiringPi 初始化失败，请以 root 权限运行" << std::endl;
        return -1;
    }
    pinMode(LED_LOW, OUTPUT);
    pinMode(LED_MED, OUTPUT);
    pinMode(LED_HIGH, OUTPUT);
    set_led(-1);

    signal(SIGINT, stop_handler);

    ADXL345 sensor(1, 0x53);
    if (sensor.readSensorState() != 0) {
        std::cerr << "ADXL345 初始化失败，请检查 I2C 连接" << std::endl;
        return -1;
    }
    sensor.setRange(ADXL345::PLUSMINUS_16_G);
    // 使用数值 1 代替枚举 HIGH，避免宏冲突
    sensor.setResolution(static_cast<ADXL345::RESOLUTION>(1));

    MQTTClient_create(&mqtt_client_handle, ADDRESS, CLIENTID, MQTTCLIENT_PERSISTENCE_NONE, NULL);
    MQTTClient_setCallbacks(mqtt_client_handle, NULL, NULL, message_arrived, NULL);

    MQTTClient_connectOptions conn_opts = MQTTClient_connectOptions_initializer;
    conn_opts.keepAliveInterval = 20;
    conn_opts.cleansession = 0;
    conn_opts.username = USERNAME;
    conn_opts.password = PASSWORD;
    conn_opts.connectTimeout = 5;

    int rc = MQTTClient_connect(mqtt_client_handle, &conn_opts);
    if (rc != MQTTCLIENT_SUCCESS) {
        printf("MQTT 连接失败，错误码 %d\n", rc);
        return -1;
    }
    printf("MQTT 已连接，订阅 %s\n", TOPIC);
    MQTTClient_subscribe(mqtt_client_handle, TOPIC, 1);

    int last_level = -1;
    printf("主循环开始，实时读取传感器控制 LED (使用 WiringPi)\n");
    while (running) {
        if (sensor.readSensorState() == 0) {
            float pitch = sensor.getPitch();
            int level = (pitch < 30) ? 0 : (pitch < 60) ? 1 : 2;
            if (level != last_level) {
                set_led(level);
                last_level = level;
                // 调试时可打开打印
                 printf("Pitch: %.2f, Level: %d\n", pitch, level);
                // fflush(stdout);
            }
        } else {
            std::cerr << "传感器读取失败" << std::endl;
        }
        for (int i = 0; i < 5 && running; ++i) usleep(10000);
    }

    set_led(-1);
    MQTTClient_unsubscribe(mqtt_client_handle, TOPIC);
    MQTTClient_disconnect(mqtt_client_handle, 1000);
    MQTTClient_destroy(&mqtt_client_handle);
    printf("程序正常退出\n");
    return 0;
}
