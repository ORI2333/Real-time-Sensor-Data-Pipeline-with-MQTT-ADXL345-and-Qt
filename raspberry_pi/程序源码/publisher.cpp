#include <iostream>
#include <chrono>
#include <thread>
#include <ctime>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <unistd.h>
#include <fstream>
#include <sstream>
#include <jsoncpp/json/json.h>
#include "MQTTClient.h"
#include "ADXL345.h"

using namespace een1071;

#define ADDRESS     "mqtt://192.168.127.152:1884"
#define CLIENTID    "ADXL345_Publisher"
#define TOPIC       "sensor/adxl345"
#define USERNAME    "fangkuai"
#define PASSWORD    "2120033"
#define TIMEOUT     10000L

// 遗嘱消息配置
#define WILL_TOPIC  "sensor/adxl345/status"
#define WILL_PAYLOAD "offline"
#define WILL_QOS    1
#define WILL_RETAIN 1

float get_cpu_temp() {
    std::ifstream temp_file("/sys/class/thermal/thermal_zone0/temp");
    float temp = 0;
    if (temp_file.is_open()) {
        temp_file >> temp;
        temp /= 1000.0;
        temp_file.close();
    }
    return temp;
}

float get_cpu_load() {
    std::ifstream load_file("/proc/loadavg");
    std::string line;
    if (load_file.is_open()) {
        std::getline(load_file, line);
        load_file.close();
        std::istringstream iss(line);
        float load1;
        iss >> load1;
        return load1;
    }
    return 0;
}

std::string get_timestamp() {
    auto now = std::chrono::system_clock::now();
    auto itt = std::chrono::system_clock::to_time_t(now);
    char buf[64];
    std::strftime(buf, sizeof(buf), "%Y-%m-%dT%H:%M:%SZ", std::gmtime(&itt));
    return std::string(buf);
}

std::string build_json(float pitch, float roll, short ax, short ay, short az,
                       float cpu_load, float cpu_temp, const std::string& ts) {
    Json::Value root;
    root["pitch"] = pitch;
    root["roll"] = roll;
    root["accel_x"] = ax;
    root["accel_y"] = ay;
    root["accel_z"] = az;
    root["cpu_load"] = cpu_load;
    root["cpu_temp_c"] = cpu_temp;
    root["timestamp"] = ts;
    return Json::FastWriter().write(root);
}

int main(int argc, char* argv[]) {
    // 检查命令行参数：指定 QoS 等级
    if (argc != 2) {
        std::cerr << "用法: " << argv[0] << " <QoS等级> (0, 1 或 2)" << std::endl;
        return -1;
    }
    int qos = std::atoi(argv[1]);
    if (qos < 0 || qos > 2) {
        std::cerr << "QoS 等级必须是 0, 1 或 2" << std::endl;
        return -1;
    }
    std::cout << "使用 QoS " << qos << " 发布消息" << std::endl;

    // 初始化 ADXL345
    ADXL345 sensor(1, 0x53);
    if (sensor.readSensorState() != 0) {
        std::cerr << "ADXL345 初始化失败" << std::endl;
        return -1;
    }
    sensor.setRange(ADXL345::PLUSMINUS_16_G);
    sensor.setResolution(ADXL345::HIGH);

    // MQTT 客户端
    MQTTClient client;
    MQTTClient_connectOptions conn_opts = MQTTClient_connectOptions_initializer;
    MQTTClient_willOptions will_opts = MQTTClient_willOptions_initializer;
    MQTTClient_message pubmsg = MQTTClient_message_initializer;
    MQTTClient_deliveryToken token;

    MQTTClient_create(&client, ADDRESS, CLIENTID, MQTTCLIENT_PERSISTENCE_NONE, NULL);

    // 设置遗嘱消息
    will_opts.topicName = WILL_TOPIC;
    will_opts.message = WILL_PAYLOAD;
    will_opts.retained = WILL_RETAIN;
    will_opts.qos = WILL_QOS;
    conn_opts.will = &will_opts;
    conn_opts.keepAliveInterval = 20;
    conn_opts.cleansession = 0;
    conn_opts.username = USERNAME;
    conn_opts.password = PASSWORD;

    int rc;
    if ((rc = MQTTClient_connect(client, &conn_opts)) != MQTTCLIENT_SUCCESS) {
        printf("连接失败，错误码：%d\n", rc);
        return -1;
    }
    printf("MQTT 连接成功，发布者已上线\n");

    // 发布保留的“在线”状态消息
    MQTTClient_message status_msg = MQTTClient_message_initializer;
    status_msg.payload = (void*)"online";
    status_msg.payloadlen = 6;
    status_msg.qos = WILL_QOS;
    status_msg.retained = 1;
    MQTTClient_publishMessage(client, WILL_TOPIC, &status_msg, &token);
    MQTTClient_waitForCompletion(client, token, TIMEOUT);

    // 主循环：每 2 秒读取传感器并发布
    int count = 0;
    while (true) {
        if (sensor.readSensorState() != 0) {
            std::cerr << "读取传感器失败" << std::endl;
            break;
        }

        float pitch = sensor.getPitch();
        float roll = sensor.getRoll();
        short ax = sensor.getAccelerationX();
        short ay = sensor.getAccelerationY();
        short az = sensor.getAccelerationZ();
        float cpu_load = get_cpu_load();
        float cpu_temp = get_cpu_temp();
        std::string ts = get_timestamp();

        std::string payload = build_json(pitch, roll, ax, ay, az, cpu_load, cpu_temp, ts);
        printf("[%s] 发布: %s\n", ts.c_str(), payload.c_str());

        // 使用命令行指定的 QoS
        pubmsg.payload = (void*)payload.c_str();
        pubmsg.payloadlen = payload.length();
        pubmsg.qos = qos;
        pubmsg.retained = 1;   // 保留消息演示
        MQTTClient_publishMessage(client, TOPIC, &pubmsg, &token);
        rc = MQTTClient_waitForCompletion(client, token, TIMEOUT);
        if (rc != MQTTCLIENT_SUCCESS) {
            printf("发布失败，QoS=%d, 错误码：%d\n", qos, rc);
        } else {
            printf("发布成功，QoS=%d\n", qos);
        }

        count++;
        std::this_thread::sleep_for(std::chrono::seconds(2));
    }

    // 正常断开前手动发送 offline 消息
    status_msg.payload = (void*)"offline";
    status_msg.payloadlen = 7;
    MQTTClient_publishMessage(client, WILL_TOPIC, &status_msg, &token);
    MQTTClient_waitForCompletion(client, token, TIMEOUT);

    MQTTClient_disconnect(client, 1000);
    MQTTClient_destroy(&client);
    return 0;
}
