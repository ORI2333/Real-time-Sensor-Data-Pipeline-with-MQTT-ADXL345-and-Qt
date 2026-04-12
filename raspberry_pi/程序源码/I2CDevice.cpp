/*
 * I2C.cpp  Created on: 17 May 2014
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

#include"I2CDevice.h"
#include<iostream>
#include<sstream>
#include<fcntl.h>
#include<stdio.h>
#include<iomanip>
#include<unistd.h>
#include<sys/ioctl.h>
#include<linux/i2c.h>
#include<linux/i2c-dev.h>
using namespace std;

#define HEX(x) setw(2) << setfill('0') << hex << (int)(x)

namespace een1071 {

/**
 * I2CDevice 构造函数。
 * @param bus I2C 总线号（通常 0 或 1）
 * @param device 设备地址
 */
I2CDevice::I2CDevice(unsigned int bus, unsigned int device) {
	this->file=-1;
	this->bus = bus;
	this->device = device;
	this->open();
}

/**
 * 打开 I2C 设备连接。
 * @return 0 成功，1 失败
 */
int I2CDevice::open(){
   string name;
   if(this->bus==0) name = BBB_I2C_0;
   else name = BBB_I2C_1;

   if((this->file=::open(name.c_str(), O_RDWR)) < 0){
      perror("I2C: failed to open the bus\n");
	  return 1;
   }
   if(ioctl(this->file, I2C_SLAVE, this->device) < 0){
      perror("I2C: Failed to connect to the device\n");
	  return 1;
   }
   return 0;
}

/**
 * 向寄存器写入一个字节。
 * @param registerAddress 寄存器地址
 * @param value 要写入的值
 * @return 0 成功，1 失败
 */

int I2CDevice::writeRegister(unsigned int registerAddress, unsigned char value){
   unsigned char buffer[2];
   buffer[0] = registerAddress;
   buffer[1] = value;
   if(::write(this->file, buffer, 2)!=2){
      perror("I2C: Failed write to the device\n");
      return 1;
   }
   return 0;
}

/**
 * 向 I2C 设备写入一个字节（常用于设置读起始地址）。
 * @param value 写入值
 * @return 0 成功，1 失败
 */
int I2CDevice::write(unsigned char value){
   unsigned char buffer[1];
   buffer[0]=value;
   if (::write(this->file, buffer, 1)!=1){
      perror("I2C: Failed to write to the device\n");
      return 1;
   }
   return 0;
}

/**
 * 读取单个寄存器。
 * @param registerAddress 寄存器地址
 * @return 读取到的字节值
 */
unsigned char I2CDevice::readRegister(unsigned int registerAddress){
   this->write(registerAddress);
   unsigned char buffer[1];
   if(::read(this->file, buffer, 1)!=1){
      perror("I2C: Failed to read in the value.\n");
      return 1;
   }
   return buffer[0];
}

/**
 * 连续读取多个寄存器。
 * @param number 读取字节数
 * @param fromAddress 起始寄存器地址（默认 0x00）
 * @return 指向读取缓冲区首地址的指针
 */
unsigned char* I2CDevice::readRegisters(unsigned int number, unsigned int fromAddress){
	this->write(fromAddress);
	unsigned char* data = new unsigned char[number];
    if(::read(this->file, data, number)!=(int)number){
       perror("IC2: Failed to read in the full buffer.\n");
	   return NULL;
    }
	return data;
}

/**
 * 以十六进制打印寄存器内容，便于调试。
 * @param number 打印长度，默认 0xff
 */

void I2CDevice::debugDumpRegisters(unsigned int number){
	cout << "Dumping Registers for Debug Purposes:" << endl;
	unsigned char *registers = this->readRegisters(number);
	for(int i=0; i<(int)number; i++){
		cout << HEX(*(registers+i)) << " ";
		if (i%16==15) cout << endl;
	}
	cout << dec;
}

/**
 * 关闭设备文件并将句柄置为 -1。
 */
void I2CDevice::close(){
	::close(this->file);
	this->file = -1;
}

/**
 * 析构时自动关闭设备文件（若仍处于打开状态）。
 */
I2CDevice::~I2CDevice() {
	if(file!=-1) this->close();
}

} /* namespace een1071 */
