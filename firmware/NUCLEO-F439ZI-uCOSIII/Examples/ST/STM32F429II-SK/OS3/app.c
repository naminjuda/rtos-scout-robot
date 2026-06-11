/*
*********************************************************************************************************
*                                            APPLICATION CORE
*
*                       STM32 NUCLEO-F439ZI + uC/OS-III RTOS application skeleton
*********************************************************************************************************
*/

#include  <includes.h>
#include  "app_core.h"
#include  "app_cfg.h"
#include  "app_io.h"
#include  "stm32f4xx_rcc.h"
#include  "stm32f4xx_gpio.h"
#include  "stm32f4xx.h"

/*
*********************************************************************************************************
*                                         FUNCTION PROTOTYPES
*********************************************************************************************************
*/

static  void  AppTaskStart       (void  *p_arg);
static  void  AppTaskCreate      (void);
static  void  AppObjCreate       (void);

static  void  EmergencyTask      (void  *p_arg);
static  void  CommandTask        (void  *p_arg);
static  void  SensorTask         (void  *p_arg);
static  void  ControlTask        (void  *p_arg);
static  void  OutputLogTask      (void  *p_arg);

static  void  Setup_Gpio         (void);
static  void  App_HardwareStopAll(void);

static  void  Sensor_PostObstacleEvent(void);
static  void  Joystick_PostManualCommand(void);

/*
*********************************************************************************************************
*                                       LOCAL GLOBAL VARIABLES
*********************************************************************************************************
*/

static  OS_TCB   AppTaskStartTCB;
static  CPU_STK  AppTaskStartStk[APP_CFG_TASK_START_STK_SIZE];

static  OS_TCB   EmergencyTaskTCB;
static  OS_TCB   CommandTaskTCB;
static  OS_TCB   SensorTaskTCB;
static  OS_TCB   ControlTaskTCB;
static  OS_TCB   OutputLogTaskTCB;

static  CPU_STK  EmergencyTaskStk[APP_CFG_TASK_STK_SIZE];
static  CPU_STK  CommandTaskStk[APP_CFG_TASK_STK_SIZE];
static  CPU_STK  SensorTaskStk[APP_CFG_TASK_STK_SIZE];
static  CPU_STK  ControlTaskStk[APP_CFG_TASK_STK_SIZE];
static  CPU_STK  OutputLogTaskStk[APP_CFG_TASK_STK_SIZE];

typedef  struct  app_task_cfg {
    CPU_CHAR    *Name;
    OS_TASK_PTR  Task;
    OS_PRIO      Prio;
    CPU_STK     *Stk;
    OS_TCB      *TCB;
} APP_TASK_CFG;

static  const  APP_TASK_CFG  AppTaskTbl[] = {
    { (CPU_CHAR *)"EmergencyTask", EmergencyTask, APP_CFG_TASK_EMERGENCY_PRIO, &EmergencyTaskStk[0], &EmergencyTaskTCB },
    { (CPU_CHAR *)"ControlTask",   ControlTask,   APP_CFG_TASK_CONTROL_PRIO,   &ControlTaskStk[0],   &ControlTaskTCB   },
    { (CPU_CHAR *)"CommandTask",   CommandTask,   APP_CFG_TASK_COMMAND_PRIO,   &CommandTaskStk[0],   &CommandTaskTCB   },
    { (CPU_CHAR *)"SensorTask",    SensorTask,    APP_CFG_TASK_SENSOR_PRIO,    &SensorTaskStk[0],    &SensorTaskTCB    },
    { (CPU_CHAR *)"OutputLogTask", OutputLogTask, APP_CFG_TASK_OUTPUT_PRIO,    &OutputLogTaskStk[0], &OutputLogTaskTCB }
};

#define  APP_TASK_TBL_SIZE  (sizeof(AppTaskTbl) / sizeof(AppTaskTbl[0]))

static  volatile  int       g_last_distance_cm = -1;
static  volatile  int       g_last_ir_obstacle = 0;
static  volatile  uint16_t  g_last_joy_x = 0u;
static  volatile  uint16_t  g_last_joy_y = 0u;

/*
*********************************************************************************************************
*                                                main()
*********************************************************************************************************
*/

int  main (void)
{
    OS_ERR  err;

    RCC_DeInit();
    Setup_Gpio();

    BSP_IntDisAll();

    CPU_Init();
    Mem_Init();
    Math_Init();

    OSInit(&err);

    OSTaskCreate((OS_TCB       *)&AppTaskStartTCB,
                 (CPU_CHAR     *)"App Task Start",
                 (OS_TASK_PTR   )AppTaskStart,
                 (void         *)0u,
                 (OS_PRIO       )APP_CFG_TASK_START_PRIO,
                 (CPU_STK      *)&AppTaskStartStk[0u],
                 (CPU_STK_SIZE  )APP_CFG_TASK_START_STK_SIZE_LIMIT,
                 (CPU_STK_SIZE  )APP_CFG_TASK_START_STK_SIZE,
                 (OS_MSG_QTY    )0u,
                 (OS_TICK       )0u,
                 (void         *)0u,
                 (OS_OPT        )(OS_OPT_TASK_STK_CHK | OS_OPT_TASK_STK_CLR),
                 (OS_ERR       *)&err);

    OSStart(&err);

    (void)&err;
    return (0u);
}

/*
*********************************************************************************************************
*                                          STARTUP TASK
*********************************************************************************************************
*/

static  void  AppTaskStart (void *p_arg)
{
    OS_ERR  err;

    (void)p_arg;

    BSP_Init();
    BSP_Tick_Init();

#if OS_CFG_STAT_TASK_EN > 0u
    OSStatTaskCPUUsageInit(&err);
#endif

#ifdef CPU_CFG_INT_DIS_MEAS_EN
    CPU_IntDisMeasMaxCurReset();
#endif

    AppIO_Init();

    AppObjCreate();
    AppTaskCreate();

    while (DEF_TRUE) {
        OSTimeDlyHMSM(0u, 0u, 1u, 0u, OS_OPT_TIME_HMSM_STRICT, &err);
    }
}

/*
*********************************************************************************************************
*                                          APPLICATION TASKS
*********************************************************************************************************
*/

static  void  EmergencyTask (void *p_arg)
{
    OS_ERR  err;

    (void)p_arg;

    while (DEF_TRUE) {
        OSSemPend(App_GetEmergencySem(),
                  0u,
                  OS_OPT_PEND_BLOCKING,
                  (CPU_TS *)0,
                  &err);

        if (err == OS_ERR_NONE) {
            App_HardwareStopAll();
            (void)App_PostEvent(EVT_EMERGENCY_STOP, APP_EVENT_SRC_EMERGENCY, 0u);
        }
    }
}

static  void  ControlTask (void *p_arg)
{
    AppEvent  event;
    OS_ERR    err;

    (void)p_arg;

    while (DEF_TRUE) {
        App_WaitEvent(&event, &err);
        if (err == OS_ERR_NONE) {
            App_HandleEvent(&event);
        }
    }
}

static  void  CommandTask (void *p_arg)
{
    OS_ERR  err;

    (void)p_arg;

    while (DEF_TRUE) {
        /*
         * TODO(developer USART): Parse USART command bytes and call App_PostEvent().
         * Example: App_PostEvent(EVT_CMD_AUTO, APP_EVENT_SRC_COMMAND, 0u);
         */
        OSTimeDlyHMSM(0u, 0u, 0u, 50u, OS_OPT_TIME_HMSM_STRICT, &err);
    }
}

static  void  SensorTask (void *p_arg)
{
    OS_ERR  err;

    (void)p_arg;

    Log_Print("[TASK] SensorTask started\r\n");

    while (DEF_TRUE) {
        /*
         * TODO(developer sensor): Read distance/IR sensors and post sensor events.
         * Example: App_PostEvent(EVT_SENSOR_OBSTACLE_WARN, APP_EVENT_SRC_SENSOR, value);
         */
    	Sensor_PostObstacleEvent();
    	Joystick_PostManualCommand();
        OSTimeDlyHMSM(0u, 0u, 0u, 100u, OS_OPT_TIME_HMSM_STRICT, &err);
    }
}

static  void  OutputLogTask (void *p_arg)
{
    OS_ERR  err;

    (void)p_arg;

    while (DEF_TRUE) {
        /*
         * TODO(developer output): Periodically read App_GetState() and update LED/buzzer/USART log.
         */
        OSTimeDlyHMSM(0u, 0u, 0u, 200u, OS_OPT_TIME_HMSM_STRICT, &err);
    }
}

/*
*********************************************************************************************************
*                                          AppTaskCreate()
*********************************************************************************************************
*/

static  void  AppTaskCreate (void)
{
    OS_ERR    err;
    CPU_INT08U  i;

    for (i = 0u; i < APP_TASK_TBL_SIZE; i++) {
        OSTaskCreate((OS_TCB       *)AppTaskTbl[i].TCB,
                     (CPU_CHAR     *)AppTaskTbl[i].Name,
                     (OS_TASK_PTR   )AppTaskTbl[i].Task,
                     (void         *)0u,
                     (OS_PRIO       )AppTaskTbl[i].Prio,
                     (CPU_STK      *)AppTaskTbl[i].Stk,
                     (CPU_STK_SIZE  )APP_CFG_TASK_STK_SIZE_LIMIT,
                     (CPU_STK_SIZE  )APP_CFG_TASK_STK_SIZE,
                     (OS_MSG_QTY    )0u,
                     (OS_TICK       )0u,
                     (void         *)0u,
                     (OS_OPT        )(OS_OPT_TASK_STK_CHK | OS_OPT_TASK_STK_CLR),
                     (OS_ERR       *)&err);
    }
}

/*
*********************************************************************************************************
*                                          AppObjCreate()
*********************************************************************************************************
*/

static  void  AppObjCreate (void)
{
    OS_ERR  err;

    App_CoreInit(&err);
}

/*
*********************************************************************************************************
*                                      SENSOR EVENT POST
*********************************************************************************************************
*/

static  void  Sensor_PostObstacleEvent (void)
{
    int  distance;
    int  ir;

    distance = Ultrasonic_ReadDistanceCm();
    ir       = IR_IsObstacleDetected();

    g_last_distance_cm = distance;
    g_last_ir_obstacle = ir;

    Log_PrintNum("[SENSOR] distance=", distance, "cm, ");
    Log_PrintNum("ir=", ir, "\r\n");

    /*

     * 30cm -> EVT_SENSOR_CLEAR
     * 15~30cm -> EVT_SENSOR_OBSTACLE_WARN
     * 15cm -> EVT_SENSOR_OBSTACLE_CRITICAL
     */
    if (distance < 0) {
        Log_Print("[SENSOR] ultrasonic timeout\r\n");
    }
    else if (distance <= DIST_CRITICAL_CM) {
        Log_Print("[POST] EVT_SENSOR_OBSTACLE_CRITICAL\r\n");
        (void)App_PostEvent(EVT_SENSOR_OBSTACLE_CRITICAL, APP_EVENT_SRC_SENSOR, distance);
    }
    else if (distance < DIST_WARN_CM) {
        Log_Print("[POST] EVT_SENSOR_OBSTACLE_WARN\r\n");
        (void)App_PostEvent(EVT_SENSOR_OBSTACLE_WARN, APP_EVENT_SRC_SENSOR, distance);
    }
    else {
        Log_Print("[POST] EVT_SENSOR_CLEAR\r\n");
        (void)App_PostEvent(EVT_SENSOR_CLEAR, APP_EVENT_SRC_SENSOR, distance);
    }

    if (ir == 1) {
        Log_Print("[POST] EVT_IR_OBSTACLE\r\n");
        (void)App_PostEvent(EVT_IR_OBSTACLE, APP_EVENT_SRC_SENSOR, 0u);
    }
    Log_Print("\r\n");
}

/*
*********************************************************************************************************
*                                  JOYSTICK MANUAL COMMAND
*
* MANUAL_MODE -> joystick
*
* VRx:
*    -> LEFT
*    -> RIGHT
*
* VRy:
*    -> FORWARD
*
*   LEFT/RIGHT , center -> CENTER
*   FORWARD , center    -> STOP
*
*********************************************************************************************************
*/

typedef enum {
    JOY_DIR_CENTER = 0,
    JOY_DIR_LEFT,
    JOY_DIR_RIGHT,
    JOY_DIR_FORWARD
} JOY_DIR;

static  void  Joystick_PostManualCommand(void)
{
    uint16_t     x;
    uint16_t     y;
    JOY_DIR      now_dir;
    SystemState  state;

    static  JOY_DIR     last_dir = JOY_DIR_CENTER;
    static  JOY_DIR     candidate_dir = JOY_DIR_CENTER;
    static  CPU_INT08U  stable_count = 0u;

    x = Joystick_ReadX();
    y = Joystick_ReadY();

    g_last_joy_x = x;
    g_last_joy_y = y;

/*
    state = App_GetState();

    if (state != STATE_MANUAL_MODE) {
        last_dir = JOY_DIR_CENTER;
        candidate_dir = JOY_DIR_CENTER;
        stable_count = 0u;
        return;
    }
*/

    if (x < JOY_ADC_LOW_TH) {
        now_dir = JOY_DIR_LEFT;
    }
    else if (x > JOY_ADC_HIGH_TH) {
        now_dir = JOY_DIR_RIGHT;
    }
    else if (y > JOY_ADC_HIGH_TH) {
        now_dir = JOY_DIR_FORWARD;
    }
    else {
        now_dir = JOY_DIR_CENTER;
    }

    if (now_dir == candidate_dir) {
        if (stable_count < JOY_STABLE_COUNT) {
            stable_count++;
        }
    }
    else {
        candidate_dir = now_dir;
        stable_count = 0u;
        return;
    }

    if (stable_count < JOY_STABLE_COUNT) {
        return;
    }

    if (now_dir == last_dir) {
        return;
    }

    if (now_dir == JOY_DIR_LEFT) {
        (void)App_PostEvent(EVT_CMD_LEFT,
            APP_EVENT_SRC_COMMAND,
            0u);

        Log_Print("[JOY] LEFT -> EVT_CMD_LEFT\r\n");
    }
    else if (now_dir == JOY_DIR_RIGHT) {
        (void)App_PostEvent(EVT_CMD_RIGHT,
            APP_EVENT_SRC_COMMAND,
            0u);

        Log_Print("[JOY] RIGHT -> EVT_CMD_RIGHT\r\n");
    }
    else if (now_dir == JOY_DIR_FORWARD) {
        (void)App_PostEvent(EVT_CMD_FORWARD,
            APP_EVENT_SRC_COMMAND,
            0u);

        Log_Print("[JOY] FORWARD -> EVT_CMD_FORWARD\r\n");
    }
    else {
        if ((last_dir == JOY_DIR_LEFT) || (last_dir == JOY_DIR_RIGHT)) {
            (void)App_PostEvent(EVT_CMD_CENTER,
                APP_EVENT_SRC_COMMAND,
                0u);

            Log_Print("[JOY] CENTER -> EVT_CMD_CENTER\r\n");
        }
        else if (last_dir == JOY_DIR_FORWARD) {
            (void)App_PostEvent(EVT_CMD_STOP,
                APP_EVENT_SRC_COMMAND,
                0u);

            Log_Print("[JOY] RELEASE FORWARD -> EVT_CMD_STOP\r\n");
        }
    }

    last_dir = now_dir;
}

/*
*********************************************************************************************************
*                                          Board skeletons
*********************************************************************************************************
*/

static  void  App_HardwareStopAll (void)
{
    /*
     * TODO(developer output/motor): Stop motors/servo immediately, then enable buzzer/LED indication.
     */
    BSP_LED_On(1u);
    BSP_LED_On(2u);
    BSP_LED_On(3u);
}

static  void  Setup_Gpio (void)
{
    GPIO_InitTypeDef  led_init;

    led_init.GPIO_Mode  = GPIO_Mode_OUT;
    led_init.GPIO_OType = GPIO_OType_PP;
    led_init.GPIO_Speed = GPIO_Speed_2MHz;
    led_init.GPIO_PuPd  = GPIO_PuPd_NOPULL;
    led_init.GPIO_Pin   = GPIO_Pin_0 | GPIO_Pin_7 | GPIO_Pin_14;

    RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOB, ENABLE);
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_SYSCFG, ENABLE);

    GPIO_Init(GPIOB, &led_init);
}
