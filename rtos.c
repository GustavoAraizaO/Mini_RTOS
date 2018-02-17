/**
 * @file rtos.c
 * @author ITESO
 * @date Feb 2018
 * @brief Implementation of rtos API
 *
 * This is the implementation of the rtos module for the
 * embedded systems II course at ITESO
 */

#include "rtos.h"
#include "rtos_config.h"
#include "clock_config.h"

#ifdef RTOS_ENABLE_IS_ALIVE
#include "fsl_gpio.h"
#include "fsl_port.h"
#endif
/**********************************************************************************/
// Module defines
/**********************************************************************************/

#define FORCE_INLINE 	__attribute__((always_inline)) inline

#define STACK_FRAME_SIZE			8
#define STACK_PC_OFFSET				2
#define STACK_PSR_OFFSET			1
#define STACK_PSR_DEFAULT			0x01000000
#define INVALID_TASK				-1

/**********************************************************************************/
// IS ALIVE definitions
/**********************************************************************************/

#ifdef RTOS_ENABLE_IS_ALIVE
#define CAT_STRING(x,y)  		x##y
#define alive_GPIO(x)			CAT_STRING(GPIO,x)
#define alive_PORT(x)			CAT_STRING(PORT,x)
#define alive_CLOCK(x)			CAT_STRING(kCLOCK_Port,x)
static void
init_is_alive ( void );
static void
refresh_is_alive ( void );
#endif

/**********************************************************************************/
// Type definitions
/**********************************************************************************/

typedef enum
{
	S_READY = 0, S_RUNNING, S_WAITING, S_SUSPENDED
} task_state_e;
typedef enum
{
	kFromISR = 0, kFromNormalExec
} task_switch_type_e;

typedef struct
{
	uint8_t priority;
	task_state_e state;
	uint32_t *sp;	//pointer to the stack end obtained from the PendSV_handler
	void
	(*task_body) ( );
	rtos_tick_t local_tick;
	uint32_t reserved [ 10 ];//saving space for debugging, may be deleted later (it must remain empty, else, something is wrong)
	uint32_t stack [ RTOS_STACK_SIZE ];
} rtos_tcb_t;

/**********************************************************************************/
// Global (static) task list
/**********************************************************************************/

struct
{
	uint8_t nTasks;
	rtos_task_handle_t current_task;
	rtos_task_handle_t next_task;
	rtos_tcb_t tasks [ RTOS_MAX_NUMBER_OF_TASKS + 1 ];
	rtos_tick_t global_tick;
} task_list =
{ 0 };

/**********************************************************************************/
// Local methods prototypes
/**********************************************************************************/

static void
reload_systick ( void );
static void
dispatcher ( task_switch_type_e type );
static void
activate_waiting_tasks ( );
FORCE_INLINE static void
context_switch ( task_switch_type_e type );
static void
idle_task ( void );

/**********************************************************************************/
// API implementation
/**********************************************************************************/

void rtos_start_scheduler ( void )
{
#ifdef RTOS_ENABLE_IS_ALIVE
	init_is_alive ();
	task_list.global_tick = 0;
	task_list.current_task = INVALID_TASK;
	rtos_create_task ( idle_task, 0, kAutoStart );
#endif
	SysTick->CTRL = SysTick_CTRL_CLKSOURCE_Msk | SysTick_CTRL_TICKINT_Msk
			| SysTick_CTRL_ENABLE_Msk;
	reload_systick ();
	for ( ;; )
		;
}

rtos_task_handle_t rtos_create_task ( void (*task_body) ( ), uint8_t priority,
		rtos_autostart_e autostart )
{
	rtos_task_handle_t retval = INVALID_TASK;
	if (RTOS_MAX_NUMBER_OF_TASKS > task_list.nTasks)
	{
		task_list.tasks [ task_list.nTasks ].priority = priority;
		task_list.tasks [ task_list.nTasks ].local_tick = 0;
		task_list.tasks [ task_list.nTasks ].task_body = task_body;
		task_list.tasks [ task_list.nTasks ].sp =
				& ( task_list.tasks [ task_list.nTasks ].stack [ RTOS_STACK_SIZE
						- 1 - STACK_FRAME_SIZE ] );	//stack is used bottoms up
		task_list.tasks [ task_list.nTasks ].state =
				kStartSuspended == autostart ? S_SUSPENDED : S_READY;
		task_list.tasks [ task_list.nTasks ].stack [ RTOS_STACK_SIZE
				- STACK_PC_OFFSET ] = ( uint32_t ) task_body;
		task_list.tasks [ task_list.nTasks ].stack [ RTOS_STACK_SIZE
				- STACK_PSR_OFFSET ] =
		STACK_PSR_DEFAULT;
		retval = task_list.nTasks;
		task_list.nTasks++;

	}
	return retval;
}

rtos_tick_t rtos_get_clock ( void )
{
	return task_list.global_tick;
}

void rtos_delay ( rtos_tick_t ticks )
{
	task_list.tasks [ task_list.current_task ].state = S_WAITING;
	task_list.tasks [ task_list.current_task ].local_tick = ticks;
	dispatcher ( kFromNormalExec );
}

void rtos_suspend_task ( void )
{
	task_list.tasks [ task_list.current_task ].state = S_SUSPENDED;
	dispatcher ( kFromNormalExec );
}

void rtos_activate_task ( rtos_task_handle_t task )
{
	task_list.tasks [ task ].state = S_READY;
	dispatcher ( kFromNormalExec );
}

/**********************************************************************************/
// Local methods implementation
/**********************************************************************************/

static void reload_systick ( void )
{
	SysTick->LOAD = USEC_TO_COUNT( RTOS_TIC_PERIOD_IN_US,
			CLOCK_GetCoreSysClkFreq () );
	SysTick->VAL = 0;
}

//Dispatcher is the scheuler's main function, as it assigns the tasks order to execute.
static void dispatcher ( task_switch_type_e type )
{
	rtos_task_handle_t next_task = INVALID_TASK;
	int8_t highest = -1;
	for ( uint8_t index = 0; index < task_list.nTasks; index++ )
	{
		if (highest < task_list.tasks [ index ].priority
				&& ( task_list.tasks [ index ].state == S_READY
						|| task_list.tasks [ index ].state == S_RUNNING ))
		{
			next_task = index;
			highest = task_list.tasks [ index ].priority;
		}
	}
	if (next_task != task_list.current_task)
	{
		task_list.next_task = next_task;
		context_switch ( type );
	}
}

//Context switch allows the microkernel to reload the previous state through the stack pointer handling and
//the PendSV interrupt to reload the stack pointer value to the one we want.
FORCE_INLINE static void context_switch ( task_switch_type_e type )
{
	register uint32_t* sp asm("sp");
	static uint8_t first_run = 1;
	if (!first_run)
	{
		if (kFromNormalExec == type)
		{
			task_list.tasks [ task_list.current_task ].sp = sp - 9;
		}
		else
		{
			task_list.tasks [ task_list.current_task ].sp = sp + 9;
		}
	}
	first_run = 0;
	task_list.current_task = task_list.next_task;
	task_list.tasks [ task_list.current_task ].state = S_RUNNING;
	SCB->ICSR |= SCB_ICSR_PENDSVSET_Msk;
}

//This function allows the microkernel to wake up instructions basedon the global tick count from the OS
static void activate_waiting_tasks ( )
{
	for ( uint8_t task_to_check = 0; task_to_check <= task_list.nTasks;
			task_to_check++ )
	{
		if (task_list.tasks [ task_to_check ].state == S_WAITING)
		{
			task_list.tasks [ task_to_check ].local_tick--;
			if (!task_list.tasks [ task_to_check ].local_tick)
			{
				task_list.tasks [ task_to_check ].state = S_READY;
			}
		}
	}
}

/**********************************************************************************/
// IDLE TASK
/**********************************************************************************/

static void idle_task ( void )
{
	for ( ;; )
	{

	}
}

/**********************************************************************************/
// ISR implementation
/**********************************************************************************/
//SysTick handles the interrupts that the OS defines for itself to check if the tasks have completed
//their times based on their local tick count, and the global tick.
void SysTick_Handler ( void )
{
#ifdef RTOS_ENABLE_IS_ALIVE
	refresh_is_alive ();
#endif
	task_list.global_tick++;
	activate_waiting_tasks ();
	dispatcher ( kFromISR );
	reload_systick ();
}

//Lowest priority software-enabled interrupt which allows us to reload the proper context based on a trick
//over the stack pointer to bring back the function that calls the context switch and the dispatcher.
void PendSV_Handler ( void )
{
	register uint32_t *r0 asm("r0");
	SCB->ICSR |= SCB_ICSR_PENDSVCLR_Msk;
	r0 = task_list.tasks [ task_list.current_task ].sp;
	asm("mov r7, r0");
}

/**********************************************************************************/
// IS ALIVE SIGNAL IMPLEMENTATION
/**********************************************************************************/

#ifdef RTOS_ENABLE_IS_ALIVE
static void init_is_alive ( void )
{
	gpio_pin_config_t gpio_config =
	{ kGPIO_DigitalOutput, 1, };

	port_pin_config_t port_config =
	{ kPORT_PullDisable, kPORT_FastSlewRate, kPORT_PassiveFilterDisable,
			kPORT_OpenDrainDisable, kPORT_LowDriveStrength, kPORT_MuxAsGpio,
			kPORT_UnlockRegister, };
	CLOCK_EnableClock ( alive_CLOCK( RTOS_IS_ALIVE_PORT ) );
	PORT_SetPinConfig ( alive_PORT( RTOS_IS_ALIVE_PORT ), RTOS_IS_ALIVE_PIN,
			&port_config );
	GPIO_PinInit ( alive_GPIO( RTOS_IS_ALIVE_PORT ), RTOS_IS_ALIVE_PIN,
			&gpio_config );
}

static void refresh_is_alive ( void )
{
	static uint8_t state = 0;
	static uint32_t count = 0;
	SysTick->LOAD = USEC_TO_COUNT( RTOS_TIC_PERIOD_IN_US,
			CLOCK_GetCoreSysClkFreq () );
	SysTick->VAL = 0;
	if (RTOS_IS_ALIVE_PERIOD_IN_US / RTOS_TIC_PERIOD_IN_US - 1 == count)
	{
		GPIO_WritePinOutput ( alive_GPIO( RTOS_IS_ALIVE_PORT ),
				RTOS_IS_ALIVE_PIN, state );
		state = state == 0 ? 1 : 0;
		count = 0;
	}
	else
	{
		count++;
	}
}
#endif
