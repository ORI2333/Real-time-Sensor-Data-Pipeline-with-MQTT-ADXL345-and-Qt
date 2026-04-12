/*
 * ADXL345.h  Created on: 17 May 2014
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

#ifndef ADXL345_H_
#define ADXL345_H_
#include"I2CDevice.h"

/// ADXL345 寄存器空间大小（0x01~0x1C 为保留区）
#define BUFFER_SIZE 0x40

namespace een1071 {

/**
 * @class ADXL345
 * @brief ADXL345 传感器封装类，继承自 I2CDevice。
 * 使用 protected 继承，避免直接暴露底层 I2C 公共接口。
 */
class ADXL345:protected I2CDevice{

public:

	/// 传感器量程枚举
	enum RANGE {
		PLUSMINUS_2_G 		= 0,//!< +-2g
		PLUSMINUS_4_G 		= 1,//!< +-4g
		PLUSMINUS_8_G 		= 2,//!< +-8g
		PLUSMINUS_16_G 		= 3 //!< +-16g
	};
	/// 传感器分辨率枚举（高分辨率通常配合 +-16g 使用）
	enum RESOLUTION {
		NORMAL = 0,//!< 普通 10 位分辨率
		HIGH = 1   //!< 高精度 13 位分辨率
	};

private:
	unsigned int I2CBus, I2CAddress;
	unsigned char *registers;
	ADXL345::RANGE range;
	ADXL345::RESOLUTION resolution;
	short accelerationX, accelerationY, accelerationZ; // 原始二补码加速度值
	float pitch, roll;                                 // 姿态角（度）
	short combineRegisters(unsigned char msb, unsigned char lsb);
	void calculatePitchAndRoll();
	virtual int updateRegisters();

public:
	ADXL345(unsigned int I2CBus, unsigned int I2CAddress=0x53);
	virtual int readSensorState();

	virtual void setRange(ADXL345::RANGE range);
	virtual ADXL345::RANGE getRange() { return this->range; }
	virtual void setResolution(ADXL345::RESOLUTION resolution);
	virtual ADXL345::RESOLUTION getResolution() { return this->resolution; }

	virtual short getAccelerationX() { return accelerationX; }
	virtual short getAccelerationY() { return accelerationY; }
	virtual short getAccelerationZ() { return accelerationZ; }
	virtual float getPitch() { return pitch; }
	virtual float getRoll() { return roll; }

	// 调试方法：在一行中持续输出 pitch/roll
	virtual void displayPitchAndRoll(int iterations = 600);
	virtual ~ADXL345();
};

} /* namespace een1071 */

#endif /* ADXL345_H_ */
