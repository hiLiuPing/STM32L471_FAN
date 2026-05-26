/* Private includes -----------------------------------------------------------*/
//includes
#include "user_TasksInit.h"
#include "main.h"
#include "key.h"
#include "FreeRTOS.h"   // 必须加
#include "queue.h"      // 必须加
// #include "beep.h"
// #include "BL24C02.h" // settings
 #include "multi_led.h"
  #include "rgb_led.h"
#include "log.h"
extern RGB_Object_t rgb;
/* Private typedef -----------------------------------------------------------*/

/* Private define ------------------------------------------------------------*/

/* Private variables ---------------------------------------------------------*/

/* Private function prototypes -----------------------------------------------*/


/**
  * @brief  Key press check task
  * @param  argument: Not used
  * @retval None
  */
//  按键和led合二为一了
void KeyTask(void *argument)
{
	key_event_t key_event;
	while(1)
	{
		if(Key_Scan(&key_event))
		{
			// 原生队列发送
			xQueueSend(Key_MessageQueue, &key_event, 1);

			log_printf("key: %d\n", key_event.id);
			
			// beep
			// if(Sys_Get_KeySoundEnable())
			// {
			// 	// beep_open();
			// 	vTaskDelay(50);  // 原生延时
			// 	// beep_close();
			// }
		}
		LED_Driver_Update();
		RGB_Update(&rgb, 10);
		vTaskDelay(10);  // 原生延时
	}
}