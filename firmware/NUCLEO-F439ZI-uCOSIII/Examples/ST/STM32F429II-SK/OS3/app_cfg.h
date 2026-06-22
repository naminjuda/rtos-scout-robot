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
#include "stm32f4xx_rcc.h"
#include "stm32f4xx_tim.h"

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

/* Distance thresholds */
#define  DIST_WARN_CM                  30
#define  DIST_CRITICAL_CM              15
#define  SENSOR_TASK_PERIOD_MS         100u
#define  ULTRA_TIMEOUT_US              30000u

/*
 * HC-SR04 ultrasonic sensor
 *
 * TRIG -> D2 = PF15
 * ECHO -> A5 = PF10
 */
#define  ULTRA_TRIG_GPIO_CLK           RCC_AHB1Periph_GPIOF
#define  ULTRA_TRIG_GPIO_PORT          GPIOF
#define  ULTRA_TRIG_PIN                GPIO_Pin_15

#define  ULTRA_ECHO_GPIO_CLK           RCC_AHB1Periph_GPIOF
#define  ULTRA_ECHO_GPIO_PORT          GPIOF
#define  ULTRA_ECHO_PIN                GPIO_Pin_10

/*
 * IR obstacle sensor
 *
 * OUT -> D7 = PF13
 */
#define  IR_GPIO_CLK                   RCC_AHB1Periph_GPIOF
#define  IR_PORT                       GPIOF
#define  IR_PIN                        GPIO_Pin_13
#define  IR_DETECTED_LEVEL             Bit_RESET

/*
 * Joystick ADC
 *
 * VRx -> A1 = PC0 = ADC Channel 10
 * VRy -> A2 = PC3 = ADC Channel 13
 */
#define  JOY_ADC_GPIO_CLK              RCC_AHB1Periph_GPIOC
#define  JOY_ADC_PORT                  GPIOC
#define  JOY_VRX_PIN                   GPIO_Pin_0
#define  JOY_VRY_PIN                   GPIO_Pin_3
#define  JOY_VRX_ADC_CHANNEL           ADC_Channel_10
#define  JOY_VRY_ADC_CHANNEL           ADC_Channel_13

#define  JOY_ADC_LOW_TH                1400u
#define  JOY_ADC_HIGH_TH               2600u
#define  JOY_STABLE_COUNT              2u


/*
*********************************************************************************************************
*                                             BUTTON CONFIG
*
* Pull-up input:
*   Not pressed = 1
*   Pressed     = 0
*********************************************************************************************************
*/

/* MODE button -> A0 = PA3 */
#define  BUTTON_MODE_GPIO_CLK          RCC_AHB1Periph_GPIOA
#define  BUTTON_MODE_GPIO_PORT         GPIOA
#define  BUTTON_MODE_GPIO_PIN          GPIO_Pin_3

/* EMERGENCY button -> A3 = PF3 */
#define  BUTTON_EMERGENCY_GPIO_CLK     RCC_AHB1Periph_GPIOF
#define  BUTTON_EMERGENCY_GPIO_PORT    GPIOF
#define  BUTTON_EMERGENCY_GPIO_PIN     GPIO_Pin_3

/* RESET button -> A4 = PF5 */
#define  BUTTON_RESET_GPIO_CLK         RCC_AHB1Periph_GPIOF
#define  BUTTON_RESET_GPIO_PORT        GPIOF
#define  BUTTON_RESET_GPIO_PIN         GPIO_Pin_5

#define  BUTTON_PRESSED_LEVEL          Bit_RESET


/*
*********************************************************************************************************
*                                      C PART OUTPUT CONFIG
*********************************************************************************************************
*/

#define  SERVO_LEFT_ANGLE              0
#define  SERVO_CENTER_ANGLE            65
#define  SERVO_RIGHT_ANGLE             135

/* Servo PWM: D12 = PA6 = TIM3 CH1 */
#define  SERVO_GPIO_PORT               GPIOA
#define  SERVO_GPIO_PIN                GPIO_Pin_6
#define  SERVO_GPIO_PIN_SOURCE         GPIO_PinSource6
#define  SERVO_GPIO_AF                 GPIO_AF_TIM3
#define  SERVO_TIM                     TIM3
#define  SERVO_TIM_RCC                 RCC_APB1Periph_TIM3
#define  SERVO_TIM_PERIOD              20000u
#define  SERVO_PULSE_MIN_US            500u
#define  SERVO_PULSE_MAX_US            2500u

/* Active buzzer: D6 = PE9 */
#define  BUZZER_GPIO_PORT              GPIOE
#define  BUZZER_GPIO_PIN               GPIO_Pin_9

/*
 * RGB LED module
 *
 * R -> D3 = PE13
 * G -> D4 = PF14
 * B -> D5 = PE11
 */
#define  RGB_RED_GPIO_PORT             GPIOE
#define  RGB_RED_GPIO_PIN              GPIO_Pin_13

#define  RGB_GREEN_GPIO_PORT           GPIOF
#define  RGB_GREEN_GPIO_PIN            GPIO_Pin_14

#define  RGB_BLUE_GPIO_PORT            GPIOE
#define  RGB_BLUE_GPIO_PIN             GPIO_Pin_11

#endif
