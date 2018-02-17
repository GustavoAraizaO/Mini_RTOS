/**
 * @file rtos_config.h
 * @author ITESO
 * @date Feb 2018
 * @brief rtos API
 *
 * This is the configuration file for the rtos
 * of the emebedded systems course at ITESO
 */


#ifndef SOURCE_RTOS_CONFIG_H_
#define SOURCE_RTOS_CONFIG_H_

/*! @brief Tick period */
#define RTOS_TIC_PERIOD_IN_US 		(1000)

/*! @brief Stack size for each task */
#define RTOS_STACK_SIZE				(100)

/*! @brief Max number of tasks for runtime */
#define RTOS_MAX_NUMBER_OF_TASKS	(10)

/*! @brief Is alive configuration */
#define RTOS_ENABLE_IS_ALIVE
#ifdef RTOS_ENABLE_IS_ALIVE
/*! @brief Is alive signal port */
#define RTOS_IS_ALIVE_PORT			E
/*! @brief Is alive signal pin */
#define RTOS_IS_ALIVE_PIN			26
/*! @brief Is alive signal period */
#define RTOS_IS_ALIVE_PERIOD_IN_US  (1000000)
#endif

#endif /* SOURCE_RTOS_CONFIG_H_ */
