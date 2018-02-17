#include "board.h"
#include "pin_mux.h"
#include "clock_config.h"
#include "fsl_debug_console.h"

#include "rtos.h"

void dummy_task1(void)
{
	uint8_t counter = 0;
	for (;;)
	{
		PRINTF("IN TASK 1: %i +++++++++++++++\r\n", counter);
		counter++;
		rtos_delay(2000);
	}
}

void dummy_task2(void)
{
	uint8_t counter = 0;
	for (;;)
	{
		PRINTF("IN TASK 2: %i ***************\r\n", counter);
		counter++;
		rtos_delay(1000);
	}
}

void dummy_task3(void)
{
	uint8_t counter = 0;
	for (;;)
	{
		PRINTF("IN TASK 3: %i ---------------\r\n", counter);
		counter++;
		rtos_delay(4000);
	}
}

int main(void)
{
	BOARD_InitPins();
	BOARD_BootClockRUN();
	BOARD_InitDebugConsole();

	rtos_create_task(dummy_task1, 1, kAutoStart);
	rtos_create_task(dummy_task2, 2, kAutoStart);
	rtos_create_task(dummy_task3, 1, kAutoStart);
	rtos_start_scheduler();

	for (;;)
	{
		__asm("NOP");
	}
}
