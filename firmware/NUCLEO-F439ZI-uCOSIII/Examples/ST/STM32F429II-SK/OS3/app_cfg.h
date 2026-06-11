/*
*********************************************************************************************************
*                                              uC/OS-II
*                                        The Real-Time Kernel
*
*                             (c) Copyright 2012; Micrium, Inc.; Weston, FL
*
*               All rights reserved.  Protected by international copyright laws.
*
*               uC/OS-II is provided in source form for FREE evaluation, for educational
*               use or peaceful research.  If you plan on using uC/OS-II in a commercial
*               product you need to contact Micrium to properly license its use in your
*               product.  We provide ALL the source code for your convenience and to
*               help you experience uC/OS-II.  The fact that the source code is provided
*               does NOT mean that you can use it without paying a licensing fee.
*
*               Knowledge of the source code may NOT be used to develop a similar product.
*
*               Please help us continue to provide the Embedded community with the finest
*               software available.  Your honesty is greatly appreciated.
*********************************************************************************************************
*/

/*
*********************************************************************************************************
*
*                                       APPLICATION CONFIGURATION
*
*                                       IAR Development Kits
*                                              on the
*
*                                    STM32F429II-SK KICKSTART KIT
*
* Filename      : app_cfg.h
* Version       : V1.00
* Programmer(s) : FT
*********************************************************************************************************
*/

#ifndef  APP_CFG_MODULE_PRESENT
#define  APP_CFG_MODULE_PRESENT

#include "stm32f4xx_gpio.h"
#include "stm32f4xx_adc.h"

/*
*********************************************************************************************************
*                                       ADDITIONAL uC/MODULE ENABLES
*********************************************************************************************************
*/


/*
*********************************************************************************************************
*                                            TASK PRIORITIES
*********************************************************************************************************
*/

#define  APP_CFG_TASK_START_PRIO                           2u
#define  APP_CFG_TASK_EMERGENCY_PRIO                       3u
#define  APP_CFG_TASK_CONTROL_PRIO                         4u
#define  APP_CFG_TASK_COMMAND_PRIO                         5u
#define  APP_CFG_TASK_SENSOR_PRIO                          6u
#define  APP_CFG_TASK_OUTPUT_PRIO                          7u

/*
*********************************************************************************************************
*                                            TASK STACK SIZES
*********************************************************************************************************
*/

#define  APP_CFG_TASK_START_STK_SIZE                     128u
#define  APP_CFG_TASK_STK_SIZE                           256u


/*
*********************************************************************************************************
*                                            TASK STACK SIZES LIMIT
*********************************************************************************************************
*/

#define  APP_CFG_TASK_START_STK_SIZE_PCT_FULL             90u
#define  APP_CFG_TASK_START_STK_SIZE_LIMIT       (APP_CFG_TASK_START_STK_SIZE     * (100u - APP_CFG_TASK_START_STK_SIZE_PCT_FULL))    / 100u
#define  APP_CFG_TASK_STK_SIZE_LIMIT             (APP_CFG_TASK_STK_SIZE           * (100u - APP_CFG_TASK_START_STK_SIZE_PCT_FULL))    / 100u


/*
*********************************************************************************************************
*                                       TRACE / DEBUG CONFIGURATION
*********************************************************************************************************
*/

#ifndef  TRACE_LEVEL_OFF
#define  TRACE_LEVEL_OFF                0
#endif

#ifndef  TRACE_LEVEL_INFO
#define  TRACE_LEVEL_INFO               1
#endif

#ifndef  TRACE_LEVEL_DBG
#define  TRACE_LEVEL_DBG                2
#endif

#define  APP_CFG_TRACE_LEVEL             TRACE_LEVEL_OFF
#define  APP_CFG_TRACE                   printf

#define  BSP_CFG_TRACE_LEVEL             TRACE_LEVEL_OFF
#define  BSP_CFG_TRACE                   printf

#define  APP_TRACE_INFO(x)               ((APP_CFG_TRACE_LEVEL >= TRACE_LEVEL_INFO)  ? (void)(APP_CFG_TRACE x) : (void)0)
#define  APP_TRACE_DBG(x)                ((APP_CFG_TRACE_LEVEL >= TRACE_LEVEL_DBG)   ? (void)(APP_CFG_TRACE x) : (void)0)

#define  BSP_TRACE_INFO(x)               ((BSP_CFG_TRACE_LEVEL  >= TRACE_LEVEL_INFO) ? (void)(BSP_CFG_TRACE x) : (void)0)
#define  BSP_TRACE_DBG(x)                ((BSP_CFG_TRACE_LEVEL  >= TRACE_LEVEL_DBG)  ? (void)(BSP_CFG_TRACE x) : (void)0)


/*
*********************************************************************************************************
*                                      SENSOR / JOYSTICK CONFIG
*********************************************************************************************************
*/

// 거리 기준값
#define  DIST_WARN_CM                  30
#define  DIST_CRITICAL_CM              15
#define  SENSOR_TASK_PERIOD_MS         100u
#define  ULTRA_TIMEOUT_US              30000u

// HC-SR04 초음파 센서
#define  ULTRA_PORT                    GPIOF
#define  ULTRA_TRIG_PIN                GPIO_Pin_15
#define  ULTRA_ECHO_PIN                GPIO_Pin_14

// IR 장애물 센서
#define  IR_PORT                       GPIOF
#define  IR_PIN                        GPIO_Pin_13
#define  IR_DETECTED_LEVEL             Bit_RESET

// 조이스틱
#define  JOY_ADC_PORT                  GPIOC
#define  JOY_VRX_PIN                   GPIO_Pin_0
#define  JOY_VRY_PIN                   GPIO_Pin_3
#define  JOY_VRX_ADC_CHANNEL           ADC_Channel_10
#define  JOY_VRY_ADC_CHANNEL           ADC_Channel_13

/*
 * 12-bit ADC 범위: 0 ~ 4095
 * 중앙값은 보통 약 2048 근처.
 */
#define  JOY_ADC_LOW_TH                1400u
#define  JOY_ADC_HIGH_TH               2600u
#define  JOY_STABLE_COUNT              2u

#endif
