# RTOS Scout Robot

STM32 NUCLEO-F439ZI + uC/OS-III TrueSTUDIO project for a scout robot RTOS application.

## Project

- Firmware path: `firmware/NUCLEO-F439ZI-uCOSIII`
- TrueSTUDIO project: `firmware/NUCLEO-F439ZI-uCOSIII/Examples/ST/STM32F429II-SK/OS3/TrueSTUDIO`
- Application core files:
  - `Examples/ST/STM32F429II-SK/OS3/app.c`
  - `Examples/ST/STM32F429II-SK/OS3/app_core.c`
  - `Examples/ST/STM32F429II-SK/OS3/app_core.h`
  - `Examples/ST/STM32F429II-SK/OS3/app_cfg.h`

## Developer Interfaces

- Command/USART code should post command events with `App_PostEvent(type, APP_EVENT_SRC_COMMAND, data)`.
- Sensor code should post sensor events with `App_PostEvent(type, APP_EVENT_SRC_SENSOR, data)`.
- Emergency input code should wake `EmergencyTask` with `OSSemPost(App_GetEmergencySem(), OS_OPT_POST_1, &err)`.
- Output/log code should read the current state with `App_GetState()`.

Do not change the state machine directly from command, sensor, or output modules. Route behavior through the common event interface in `app_core.h`.
