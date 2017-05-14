#include "drivers_uartrc_user.h"
#include "drivers_uartrc_low.h"
#include "drivers_led_user.h"
#include "peripheral_laser.h"
#include "usart.h"
#include "FreeRTOS.h"
#include "semphr.h"
#include "cmsis_os.h"
#include "rtos_semaphore.h"
#include "drivers_ramp.h"
#include "peripheral_tim.h"
#include <stdlib.h>
#include <math.h>
#include "utilities_debug.h"
#include  "tim.h"
#include "drivers_uartrc_low.h"
#include "drivers_uartrc_user.h"
#include "peripheral_define.h"
#include "drivers_uartupper_low.h"
#include "drivers_uartupper_user.h"
#include "stm32f4xx_hal_uart.h"
NaiveIOPoolDefine(rcUartIOPool, {0});

void rcInit(){
	//遥控器DMA接收开启(一次接收18个字节)
	if(HAL_UART_Receive_DMA(&RC_UART, IOPool_pGetWriteData(rcUartIOPool)->ch, 18) != HAL_OK){
			Error_Handler();
	} 
//	__HAL_UART_ENABLE_IT(&RC_UART, UART_FLAG_IDLE);
	RemoteTaskInit();
}

void rcUartRxCpltCallback(){
	static portBASE_TYPE xHigherPriorityTaskWoken;
	 xHigherPriorityTaskWoken = pdFALSE; 
//	static HAL_UART_StateTypeDef uart_state;
//	fw_printfln("flag: %x",__HAL_UART_GET_FLAG(&RC_UART,UART_FLAG_RXNE));
//	while(__HAL_UART_GET_IT_SOURCE(&RC_UART, UART_FLAG_IDLE) == 0){
//		fw_Warning();
//	}
//	__HAL_UART_GET_IT_SOURCE(&RC_UART, UART_FLAG_IDLE);
//	fw_printfln("flag1: %d",__HAL_UART_GET_IT_SOURCE(&RC_UART, UART_FLAG_IDLE));
//	__HAL_UART_CLEAR_PEFLAG(&RC_UART);
//	fw_printfln("flag12: %d",__HAL_UART_GET_IT_SOURCE(&RC_UART, UART_FLAG_IDLE));
//	fw_printfln("flag: %x",__HAL_UART_GET_FLAG(&RC_UART,UART_FLAG_IDLE));
//	 while(__HAL_UART_GET_FLAG(&RC_UART,UART_FLAG_IDLE) == 0)
//   {}
//	__HAL_UART_CLEAR_FLAG(&RC_UART, UART_FLAG_IDLE);
//	fw_printfln("flag2: %x",__HAL_UART_GET_FLAG(&RC_UART,UART_FLAG_IDLE));
//	fw_printfln("(thiscount_rc - lastcount_rc):  %d", (thiscount_rc - lastcount_rc));
   xSemaphoreGiveFromISR(xSemaphore_rcuart, &xHigherPriorityTaskWoken);
	if( xHigherPriorityTaskWoken == pdTRUE ){
   portEND_SWITCHING_ISR(xHigherPriorityTaskWoken);
	 }
 }



RC_Ctl_t RC_CtrlData;   //remote control data
ChassisSpeed_Ref_t ChassisSpeedRef; //µ×ÅÌµç»úÄ¿±êËÙ¶È
Gimbal_Ref_t GimbalRef; //ÔÆÌ¨Ä¿±ê
FrictionWheelState_e friction_wheel_state = FRICTION_WHEEL_OFF; //Ä¦²ÁÂÖ×´Ì¬

volatile Shoot_State_e shootState = NOSHOOTING; //²¦ÅÌµç»ú×´Ì¬
InputMode_e inputmode = REMOTE_INPUT;   //ÊäÈëÄ£Ê½Éè¶¨

RampGen_t frictionRamp = RAMP_GEN_DAFAULT;  //Ä¦²ÁÂÖÐ±ÆÂ
RampGen_t LRSpeedRamp = RAMP_GEN_DAFAULT;   //mouse×óÓÒÒÆ¶¯Ð±ÆÂ
RampGen_t FBSpeedRamp = RAMP_GEN_DAFAULT;   //mouseÇ°ºóÒÆ¶¯Ð±ÆÂ

void RemoteTaskInit()
{

	frictionRamp.SetScale(&frictionRamp, FRICTION_RAMP_TICK_COUNT);
	LRSpeedRamp.SetScale(&LRSpeedRamp, MOUSE_LR_RAMP_TICK_COUNT);
	FBSpeedRamp.SetScale(&FBSpeedRamp, MOUSR_FB_RAMP_TICK_COUNT);
	frictionRamp.ResetCounter(&frictionRamp);
	LRSpeedRamp.ResetCounter(&LRSpeedRamp);
	FBSpeedRamp.ResetCounter(&FBSpeedRamp);

	GimbalRef.pitch_angle_dynamic_ref = 0.0f;
	GimbalRef.yaw_angle_dynamic_ref = 0.0f;
	ChassisSpeedRef.forward_back_ref = 0.0f;
	ChassisSpeedRef.left_right_ref = 0.0f;
	ChassisSpeedRef.rotate_ref = 0.0f;

	SetFrictionState(FRICTION_WHEEL_OFF);
}
/*拨杆数据处理*/
void GetRemoteSwitchAction(RemoteSwitch_t *sw, uint8_t val)
{
	static uint32_t switch_cnt = 0;

	/* ×îÐÂ×´Ì¬Öµ */
	sw->switch_value_raw = val;
	sw->switch_value_buf[sw->buf_index] = sw->switch_value_raw;

	/* È¡×îÐÂÖµºÍÉÏÒ»´ÎÖµ */
	sw->switch_value1 = (sw->switch_value_buf[sw->buf_last_index] << 2)|
	(sw->switch_value_buf[sw->buf_index]);


	/* ×îÀÏµÄ×´Ì¬ÖµµÄË÷Òý */
	sw->buf_end_index = (sw->buf_index + 1)%REMOTE_SWITCH_VALUE_BUF_DEEP;

	/* ºÏ²¢Èý¸öÖµ */
	sw->switch_value2 = (sw->switch_value_buf[sw->buf_end_index]<<4)|sw->switch_value1;	

	/* ³¤°´ÅÐ¶Ï */
	if(sw->switch_value_buf[sw->buf_index] == sw->switch_value_buf[sw->buf_last_index])
	{
		switch_cnt++;	
	}
	else
	{
		switch_cnt = 0;
	}

	if(switch_cnt >= 40)
	{
		sw->switch_long_value = sw->switch_value_buf[sw->buf_index]; 	
	}

	//Ë÷ÒýÑ­»·
	sw->buf_last_index = sw->buf_index;
	sw->buf_index++;		
	if(sw->buf_index == REMOTE_SWITCH_VALUE_BUF_DEEP)
	{
		sw->buf_index = 0;	
	}			
}
//return the state of the remote 0:no action 1:action 
uint8_t IsRemoteBeingAction(void)
{
	return (abs(ChassisSpeedRef.forward_back_ref)>=10 || abs(ChassisSpeedRef.left_right_ref)>=10 || fabs(GimbalRef.yaw_speed_ref)>=10 || fabs(GimbalRef.pitch_speed_ref)>=10);
}
/*取得右上角拨杆数据*/
void SetInputMode(Remote *rc)
{
	if(rc->s2 == 1)
	{
		inputmode = REMOTE_INPUT;
	}
	else if(rc->s2 == 3)
	{
		inputmode = KEY_MOUSE_INPUT;
	}
	else if(rc->s2 == 2)
	{
		inputmode = STOP;
	}	
}

InputMode_e GetInputMode()
{
	return inputmode;
}

/*
input: RemoteSwitch_t *sw, include the switch info
*/

void RemoteShootControl(RemoteSwitch_t *sw, uint8_t val) 
{
	GetRemoteSwitchAction(sw, val);
	switch(friction_wheel_state)
	{
		case FRICTION_WHEEL_OFF:
		{
			if(sw->switch_value1 == REMOTE_SWITCH_CHANGE_1TO3)   //´Ó¹Ø±Õµ½start turning
			{
				SetShootState(NOSHOOTING);
				frictionRamp.ResetCounter(&frictionRamp);
				friction_wheel_state = FRICTION_WHEEL_START_TURNNING;	 
				LASER_ON(); 
			}				 		
		}break;
		case FRICTION_WHEEL_START_TURNNING:
		{
			if(sw->switch_value1 == REMOTE_SWITCH_CHANGE_3TO1)   //¸ÕÆô¶¯¾Í±»¹Ø±Õ
			{
				LASER_OFF();
				SetShootState(NOSHOOTING);
				SetFrictionWheelSpeed(1000);
				friction_wheel_state = FRICTION_WHEEL_OFF;
				frictionRamp.ResetCounter(&frictionRamp);
			}
			else
			{
				SetFrictionWheelSpeed(1000 + (FRICTION_WHEEL_MAX_DUTY-1000)*frictionRamp.Calc(&frictionRamp)); 
				if(frictionRamp.IsOverflow(&frictionRamp))
				{
					friction_wheel_state = FRICTION_WHEEL_ON; 	
				}
				
			}
		}break;
		case FRICTION_WHEEL_ON:
		{
			if(sw->switch_value1 == REMOTE_SWITCH_CHANGE_3TO1)   //¹Ø±ÕÄ¦²ÁÂÖ
			{
				LASER_OFF();
				friction_wheel_state = FRICTION_WHEEL_OFF;				  
				SetFrictionWheelSpeed(1000); 
				frictionRamp.ResetCounter(&frictionRamp);
				SetShootState(NOSHOOTING);
			}
			else if(sw->switch_value_raw == 2)
			{
				SetShootState(SHOOTING);
			}
			else
			{
				SetShootState(NOSHOOTING);
			}					 
		} break;				
	}
}
	 
void MouseShootControl(Mouse *mouse)
{
	static int16_t closeDelayCount = 0;   //ÓÒ¼ü¹Ø±ÕÄ¦²ÁÂÖ3sÑÓÊ±¼ÆÊý
	switch(friction_wheel_state)
	{
		case FRICTION_WHEEL_OFF:
		{
			if(mouse->last_press_r == 0 && mouse->press_r == 1)   //´Ó¹Ø±Õµ½start turning
			{
				SetShootState(NOSHOOTING);
				frictionRamp.ResetCounter(&frictionRamp);
				friction_wheel_state = FRICTION_WHEEL_START_TURNNING;	 
				LASER_ON(); 
				closeDelayCount = 0;
			}				 		
		}break;
		case FRICTION_WHEEL_START_TURNNING:
		{
			if(mouse->press_r == 1)
			{
				closeDelayCount++;
			}
			else
			{
				closeDelayCount = 0;
			}
			if(closeDelayCount>50)   //¹Ø±ÕÄ¦²ÁÂÖ
			{
				LASER_OFF();
				friction_wheel_state = FRICTION_WHEEL_OFF;				  
				SetFrictionWheelSpeed(1000); 
				frictionRamp.ResetCounter(&frictionRamp);
				SetShootState(NOSHOOTING);
			}
			else
			{
				//Ä¦²ÁÂÖ¼ÓËÙ				
				SetFrictionWheelSpeed(1000 + (FRICTION_WHEEL_MAX_DUTY-1000)*frictionRamp.Calc(&frictionRamp)); 
				if(frictionRamp.IsOverflow(&frictionRamp))
				{
					friction_wheel_state = FRICTION_WHEEL_ON; 	
				}
				
			}
		}break;
		case FRICTION_WHEEL_ON:
		{
			if(mouse->press_r == 1)
			{
				closeDelayCount++;
			}
			else
			{
				closeDelayCount = 0;
			}
			
			if(closeDelayCount>50)   //¹Ø±ÕÄ¦²ÁÂÖ
			{
				LASER_OFF();
				friction_wheel_state = FRICTION_WHEEL_OFF;				  
				SetFrictionWheelSpeed(1000); 
				frictionRamp.ResetCounter(&frictionRamp);
				SetShootState(NOSHOOTING);
			}			
			else if(mouse->press_l== 1)  //°´ÏÂ×ó¼ü£¬Éä»÷
			{
				SetShootState(SHOOTING);				
			}
			else
			{
				SetShootState(NOSHOOTING);				
			}					 
		} break;				
	}	
	mouse->last_press_r = mouse->press_r;
}






Shoot_State_e GetShootState()
{
	return shootState;
}

void SetShootState(Shoot_State_e v)
{
	shootState = v;
}

FrictionWheelState_e GetFrictionState()
{
	return friction_wheel_state;
}

void SetFrictionState(FrictionWheelState_e v)
{
	friction_wheel_state = v;
}
void SetFrictionWheelSpeed(uint16_t x)
{
	__HAL_TIM_SET_COMPARE(&FRICTION_TIM, TIM_CHANNEL_1, x);
	__HAL_TIM_SET_COMPARE(&FRICTION_TIM, TIM_CHANNEL_2, x);
}
Shoot_Mode_e shootMode = MANUL;

Shoot_Mode_e GetShootMode()
{
	return shootMode;
}
void SetShootMode(Shoot_Mode_e v)
{
	shootMode = v;
}

Emergency_Flag emergency_Flag = NORMAL;

Emergency_Flag GetEmergencyFlag()
{
	return emergency_Flag;
}

void SetEmergencyFlag(Emergency_Flag v)
{
	emergency_Flag = v;
}

Move_Speed_e movespeed = NORMAL_s;

Move_Speed_e GetMoveSpeed()
{
	return movespeed;
}

void SetMoveSpeed(Move_Speed_e v)
{
	movespeed = v;
}

Slab_Mode_e slabmode = CLOSE;

Slab_Mode_e GetSlabState()
{
	return slabmode;
}

void SetSlabState(Slab_Mode_e v)
{
	slabmode = v;
}
