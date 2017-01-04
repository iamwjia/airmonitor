#include "et_config.h"
#include "et_types.h"
#include "et_rtos.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "et_client.h"
// et_int32 g_mutex = 0;
// et_task_handle_t et_task_create(et_task_t task, et_int8 *name, void *param, et_uint8 priority, et_uint32 stack_size)
// {
// 	xTaskHandle i_task_handle;
// 	if(pdPASS == xTaskCreate(task, name, stack_size, param, priority, &i_task_handle))
// 	{
// 		return i_task_handle;
// 	}
// 	else
// 	{
// 		ET_LOG_USER("Create task failed\n");
// 		return NULL;
// 	}
// }

// et_int32 et_task_destroy(et_task_handle_t task_handle)
// {
// 	vTaskDelete(task_handle);
// 	return ET_SUCCESS;
// }


et_mutex_handle_t et_mutex_create(void)
{
	// return xSemaphoreCreateMutex();
	et_mutex_handle_t mutex_handle = NULL;
	mutex_handle = et_malloc(4);
	if(mutex_handle)
		*(et_int32 *)mutex_handle = 0;
	return mutex_handle;
}

et_int32 et_mutex_destroy(et_mutex_handle_t mutex_handle)
{
	// vSemaphoreDelete(mutex_handle);
	*(et_int32 *)mutex_handle = 0;
	et_free(mutex_handle);
	return ET_SUCCESS;
}

et_int32 et_mutex_lock(et_mutex_handle_t mutex_handle)
{
	// if(pdPASS == xSemaphoreTake(mutex_handle, portMAX_DELAY))
		// return ET_SUCCESS;
	// return ET_FAILURE;
	while(*(et_int32 *)mutex_handle)
	{
		vTaskDelay(20/portTICK_RATE_MS);
	}
	*(et_int32 *)mutex_handle = 1;
	return ET_SUCCESS;
}

et_int32 et_mutex_unlock(et_mutex_handle_t mutex_handle)
{
	// if(pdPASS == xSemaphoreGive(mutex_handle))
		// return ET_SUCCESS;
	// return ET_FAILURE;
	// if(*(et_int32 *)mutex_handle)
	*(et_int32 *)mutex_handle = 0;
	return ET_SUCCESS;
}
