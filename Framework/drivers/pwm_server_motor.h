#ifndef _pwm_server_motor_
#define _pwm_server_motor_
#include "stm32f4xx_hal.h"
#include "main.h" 
#include "tim.h"

//motorIndex: �������أ���Դ�߳��ҷ��ã�����һ�ŵ�PWM������,������������Ϊ0~7������һ��PWM�����ڣ�������������Ϊ8~15
void pwm_server_motor_init(uint8_t motorIndex);
void pwm_server_motor_deinit(uint8_t motorIndex);
//angle:0~180 in degrees
void pwm_server_motor_set_angle(uint8_t motorIndex,float angle);

#endif
