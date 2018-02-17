/**
 * @file rtos.h
 * @author ITESO
 * @date Feb 2018
 * @brief rtos API
 *
 * This is the API definition of the rtos for the
 * embedded systems course at ITESO
 */

#ifndef SOURCE_RTOS_H_
#define SOURCE_RTOS_H_

#include "rtos_config.h"
#include "stdint.h"

/*! @brief Autostart state type */
typedef enum
{
	kAutoStart, kStartSuspended
} rtos_autostart_e;

/*! @brief Task handle type, used to identify a task */
typedef int8_t rtos_task_handle_t;

/*! @brief Tick type, used for time measurement */
typedef uint64_t rtos_tick_t;

/*!
 * @brief Starts the scheduler, from this point the RTOS takes control
 * on the processor
 *
 * @param none
 * @retval none
 */
void rtos_start_scheduler(void);

/*!
 * @brief Create task API function
 *
 * @param task_body pointer to the body of the task
 * @param priority number for the RMS algorithm
 * @param autostart either autostart or start suspended
 * @retval task_handle of the task created
 */
rtos_task_handle_t rtos_create_task(void (*task_body)(), uint8_t priority,
        rtos_autostart_e autostart);

/*!
 * @brief Suspends the task calling this function
 *
 * @param none
 * @retval none
 */
void rtos_suspend_task(void);

/*!
 * @brief Activates the task identified by the task handle
 *
 * @param task handle of the task to be activated
 * @retval none
 */
void rtos_activate_task(rtos_task_handle_t task);

/*!
 * @brief Returns the rtos global tick
 *
 * @param none
 * @retval clock value
 */
rtos_tick_t rtos_get_clock(void);

/*!
 * @brief Suspends the task calling this function by a certain
 * amount of time specified by the parameter ticks
 *
 * @param ticks amount of ticks for the delay
 * @retval none
 */
void rtos_delay(rtos_tick_t ticks);

#endif /* SOURCE_RTOS_H_ */
