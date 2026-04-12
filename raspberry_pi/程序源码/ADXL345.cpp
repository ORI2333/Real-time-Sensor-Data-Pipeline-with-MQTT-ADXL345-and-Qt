/*
 * ADXL345.cpp  Created on: 17 May 2014
 * Copyright (c) 2014 Derek Molloy (www.derekmolloy.ie)
 * Made available for the book "Exploring BeagleBone" 
 * See: www.exploringbeaglebone.com
 * Licensed under the EUPL V.1.1
 *
 * This Software is provided to You under the terms of the European 
 * Union Public License (the "EUPL") version 1.1 as published by the 
 * European Union. Any use of this Software, other than as authorized 
 * under this License is strictly prohibited (to the extent such use 
 * is covered by a right of the copyright holder of this Software).
 * 
 * This Software is provided under the License on an "AS IS" basis and 
 * without warranties of any kind concerning the Software, including 
 * without limitation merchantability, fitness for a particular purpose, 
 * absence of defects or errors, accuracy, and non-infringement of 
 * intellectual property rights other than copyright. This disclaimer 
 * of warranty is an essential part of the License and a condition for 
 * the grant of any rights to this Software.
 * 
 * For more details, see http://www.derekmolloy.ie/
 */

#include "ADXL345.h"
#include <iostream>
#include <unistd.h>
#include <math.h>
#include <stdio.h>

using namespace std;

namespace een1071 {

// ADXL345 常用寄存器定义（参考数据手册）
#define DEVID          0x00   // 设备 ID
#define THRESH_TAP     0x1D   // 轻敲阈值
#define OFSX           0x1E   // X 轴偏移
#define OFSY           0x1F   // Y 轴偏移
#define OFSZ           0x20   // Z 轴偏移
#define DUR            0x21   // 轻敲持续时间
#define LATENT         0x22   // 轻敲延迟
#define WINDOW         0x23   // 轻敲窗口
#define THRESH_ACT     0x24   // 活动阈值
#define THRESH_INACT   0x25   // 静止阈值
#define TIME_INACT     0x26   // 静止时间
#define ACT_INACT_CTL  0x27   // 活动/静止轴使能
#define THRESH_FF      0x28   // 自由落体阈值
#define TIME_FF        0x29   // 自由落体时间
#define TAP_AXES       0x2A   // 轻敲轴配置
#define ACT_TAP_STATUS 0x2B   // 轻敲状态来源
#define BW_RATE        0x2C   // 带宽与速率配置
#define POWER_CTL      0x2D   // 电源控制
#define INT_ENABLE     0x2E   // 中断使能
#define INT_MAP        0x2F   // 中断映射
#define INT_SOURCE     0x30   // 中断来源
#define DATA_FORMAT    0x31   // 数据格式配置
#define DATAX0         0x32   // X 轴低字节
#define DATAX1         0x33   // X 轴高字节
#define DATAY0         0x34   // Y 轴低字节
#define DATAY1         0x35   // Y 轴高字节
#define DATAZ0         0x36   // Z 轴低字节
#define DATAZ1         0x37   // Z 轴高字节
#define FIFO_CTL       0x38   // FIFO 控制
#define FIFO_STATUS    0x39   // FIFO 状态

/**
 * 将两个 8 位寄存器拼成一个 16 位有符号值。
 * @param msb 高字节
 * @param lsb 低字节
 */
short ADXL345::combineRegisters(unsigned char msb, unsigned char lsb){
	 // 高字节左移后与低字节合并
   return ((short)msb<<8)|(short)lsb;
}

/**
 * 根据加速度值计算 pitch/roll（单位：度）。
 */
void ADXL345::calculatePitchAndRoll(){
	float gravity_range;
	switch(ADXL345::range){
		case ADXL345::PLUSMINUS_16_G: gravity_range=32.0f; break;
		case ADXL345::PLUSMINUS_8_G: gravity_range=16.0f; break;
		case ADXL345::PLUSMINUS_4_G: gravity_range=8.0f; break;
		default: gravity_range=4.0f; break;
	}
    float resolution = 1024.0f;
	if (this->resolution==ADXL345::HIGH) resolution = 8192.0f; // 13 位分辨率
    float factor = gravity_range/resolution;

    float accXg = this->accelerationX * factor;
    float accYg = this->accelerationY * factor;
    float accZg = this->accelerationZ * factor;
	float accXSquared = accXg * accXg ;
	float accYSquared = accYg * accYg ;
	float accZSquared = accZg * accZg ;
	this->pitch = 180 * atan(accXg/sqrt(accYSquared + accZSquared))/M_PI;
	this->roll = 180 * atan(accYg/sqrt(accXSquared + accZSquared))/M_PI;
}

/**
 * 更新数据格式寄存器。
 * @return 0 表示更新成功
 */
int ADXL345::updateRegisters(){
	 // 组装 DATA_FORMAT 位字段
	 char data_format = 0x00;  // 默认 +-2g，普通分辨率
	 // FULL_RES 位在 bit3
   data_format = data_format|((this->resolution)<<3);
	 data_format = data_format|this->range; // 量程位在 bit0/bit1
   return this->writeRegister(DATA_FORMAT, data_format);
}

/**
 * 构造函数：初始化总线、地址和默认工作模式。
 * @param I2CBus I2C 总线号（通常 0 或 1）
 * @param I2CAddress 设备地址（默认 0x53）
 */
ADXL345::ADXL345(unsigned int I2CBus, unsigned int I2CAddress):
	I2CDevice(I2CBus, I2CAddress){   // 初始化列表中调用父类构造
	this->I2CAddress = I2CAddress;
	this->I2CBus = I2CBus;
	this->accelerationX = 0;
	this->accelerationY = 0;
	this->accelerationZ = 0;
	this->pitch = 0.0f;
	this->roll = 0.0f;
	this->registers = NULL;
	this->range = ADXL345::PLUSMINUS_16_G;
	this->resolution = ADXL345::HIGH;
	this->writeRegister(POWER_CTL, 0x08);
	this->updateRegisters();
}

/**
 * 读取传感器当前状态，并刷新加速度与姿态角。
 * @return 0 表示读取成功，-1 表示设备校验失败
 */
int ADXL345::readSensorState(){
	this->registers = this->readRegisters(BUFFER_SIZE, 0x00);
	if(*this->registers!=0xe5){
		perror("ADXL345: Failure Condition - Sensor ID not Verified");
		return -1;
	}
	this->accelerationX = this->combineRegisters(*(registers+DATAX1), *(registers+DATAX0));
	this->accelerationY = this->combineRegisters(*(registers+DATAY1), *(registers+DATAY0));
	this->accelerationZ = this->combineRegisters(*(registers+DATAZ1), *(registers+DATAZ0));
	this->resolution = (ADXL345::RESOLUTION) (((*(registers+DATA_FORMAT))&0x08)>>3);
	this->range = (ADXL345::RANGE) ((*(registers+DATA_FORMAT))&0x03);
	this->calculatePitchAndRoll();
	return 0;
}

/**
 * 设置量程。
 * @param range 目标量程
 */
void ADXL345::setRange(ADXL345::RANGE range) {
	this->range = range;
	updateRegisters();
}

/**
 * 设置分辨率。
 * @param resolution HIGH 或 NORMAL
 */
void ADXL345::setResolution(ADXL345::RESOLUTION resolution) {
	this->resolution = resolution;
	updateRegisters();
}

/**
 * 调试输出：循环打印 pitch/roll。
 * @param iterations 循环次数（每次约 0.1s）
 */
void ADXL345::displayPitchAndRoll(int iterations){
	int count = 0;
	while(count < iterations){
	      cout << "Pitch:"<< this->getPitch() << " Roll:" << this->getRoll() << "     \r"<<flush;
	      usleep(100000);
	      this->readSensorState();
	      count++;
	}
}

ADXL345::~ADXL345() {}

} /* namespace een1071 */
