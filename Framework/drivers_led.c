#include "drivers_led_user.h"
#include "drivers_led_low.h"

#include "cmsis_os.h"
#include "gpio.h"

#include "tasks_testtasks.h"

#define G_GPIO GPIOC
#define G_GPIO_PIN GPIO_PIN_1
#define R_GPIO GPIOC
#define R_GPIO_PIN GPIO_PIN_2

#define ledGOn() HAL_GPIO_WritePin(G_GPIO, G_GPIO_PIN, GPIO_PIN_RESET)
#define ledGOff() HAL_GPIO_WritePin(G_GPIO, G_GPIO_PIN, GPIO_PIN_SET)
#define ledROn() HAL_GPIO_WritePin(R_GPIO, R_GPIO_PIN, GPIO_PIN_RESET)
#define ledROff() HAL_GPIO_WritePin(R_GPIO, R_GPIO_PIN, GPIO_PIN_SET)

LedStatus_t ledGStatus = blink, ledRStatus = blink;

void ledGTask(void const * argument){
	while(1){
		ledGTasktaskcount++;
		if(ledGStatus == on){
			ledGOn();
		}else if(ledGStatus == off){
			ledGOff();
		}else if(ledGStatus == blink){
			ledGOn();
			osDelay(999);
			ledGOff();
			osDelay(234);
		}
	}
}

void ledRTask(void const * argument){
	while(1){
		ledRTasktaskcount++;
		if(ledRStatus == on){
			ledROn();
		}else if(ledRStatus == off){
			ledROff();
		}else if(ledRStatus == blink){
			ledROn();
			osDelay(88);
			ledROff();
			osDelay(88);
		}
	}
}
