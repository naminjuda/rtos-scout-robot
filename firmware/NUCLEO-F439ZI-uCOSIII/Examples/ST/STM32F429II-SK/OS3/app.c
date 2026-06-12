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

static  void       Command_HandleLine        (char *line);
static  EventType  Command_Parse             (const char *line);
static  void       Command_PrintStatus       (void);
static  void       Command_ApplyManualOutput (EventType type);
static  void       Command_NormalizeLine     (char *line);
static  const char *State_ToString           (SystemState state);
static  const char *Event_ToString           (EventType type);
static  void       Output_ApplyState         (SystemState state);

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
static  CPU_INT08U  g_forward_display_ticks = 0u;

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
    OS_ERR      err;
    char        ch;
    static char line[32];
    static int  len = 0;

    (void)p_arg;

    Log_Print("[TASK] CommandTask started\r\n");
    Log_Print("[HELP] AUTO MANUAL FORWARD LEFT RIGHT CENTER STOP STATUS CANCEL RESET\r\n");

    while (DEF_TRUE) {
        while (USART3_ReadCharNonBlocking(&ch) != 0) {
            if ((ch == '\r') || (ch == '\n')) {
                if (len > 0) {
                    line[len] = '\0';
                    Command_HandleLine(line);
                    len = 0;
                }
            } else if ((ch == 8) || (ch == 127)) {
                if (len > 0) {
                    len--;
                }
            } else {
                if (len < ((int)sizeof(line) - 1)) {
                    line[len++] = ch;
                } else {
                    len = 0;
                    Log_Print("[CMD] input too long. cleared\r\n");
                }
            }
        }

        OSTimeDlyHMSM(0u, 0u, 0u, 20u, OS_OPT_TIME_HMSM_STRICT, &err);
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
    OS_ERR       err;
    SystemState  state;
    SystemState  last_state;

    (void)p_arg;

    last_state = STATE_COUNT;
    Log_Print("[TASK] OutputLogTask started\r\n");

    while (DEF_TRUE) {
        state = App_GetState();

        Output_ApplyState(state);

        if (state != last_state) {
            Log_Print("[STATE] ");
            Log_Print(State_ToString(last_state));
            Log_Print(" -> ");
            Log_Print(State_ToString(state));
            Log_Print("\r\n");
            last_state = state;
        }

        OSTimeDlyHMSM(0u, 0u, 0u, 100u, OS_OPT_TIME_HMSM_STRICT, &err);
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
*                                      COMMAND / OUTPUT HELPERS
*********************************************************************************************************
*/

static  void  Command_NormalizeLine (char *line)
{
    int  r;
    int  w;
    char c;

    r = 0;
    while ((line[r] == ' ') || (line[r] == '\t')) {
        r++;
    }

    w = 0;
    while (line[r] != '\0') {
        c = line[r++];
        if ((c == ' ') || (c == '\t')) {
            continue;
        }
        line[w++] = (char)toupper((int)c);
    }

    line[w] = '\0';
}

static  EventType  Command_Parse (const char *line)
{
    if (strcmp(line, "AUTO") == 0) {
        return EVT_CMD_AUTO;
    }

    if (strcmp(line, "MANUAL") == 0) {
        return EVT_CMD_MANUAL;
    }

    if (strcmp(line, "FORWARD") == 0) {
        return EVT_CMD_FORWARD;
    }

    if (strcmp(line, "LEFT") == 0) {
        return EVT_CMD_LEFT;
    }

    if (strcmp(line, "RIGHT") == 0) {
        return EVT_CMD_RIGHT;
    }

    if (strcmp(line, "CENTER") == 0) {
        return EVT_CMD_CENTER;
    }

    if (strcmp(line, "STOP") == 0) {
        return EVT_CMD_STOP;
    }

    if (strcmp(line, "STATUS") == 0) {
        return EVT_CMD_STATUS;
    }

    if (strcmp(line, "CANCEL") == 0) {
        return EVT_CMD_CANCEL;
    }

    if (strcmp(line, "RESET") == 0) {
        return EVT_CMD_RESET;
    }

    return EVT_TYPE_COUNT;
}

static  void  Command_HandleLine (char *line)
{
    EventType type;

    Command_NormalizeLine(line);

    if (line[0] == '\0') {
        return;
    }

    /*
    * Temporary C-part output test commands.
    * These commands are useful before the real sensor/button/buzzer modules are connected.
    */
    if (strcmp(line, "TESTWARN") == 0) {
        Log_Print("[TEST] Force AUTO_MODE -> OBSTACLE_WARNING\r\n");
        (void)App_PostEvent(EVT_CMD_AUTO, APP_EVENT_SRC_COMMAND, 0u);
        (void)App_PostEvent(EVT_SENSOR_OBSTACLE_WARN, APP_EVENT_SRC_SENSOR, 20u);
        return;
    }

    if (strcmp(line, "TESTCRIT") == 0) {
        Log_Print("[TEST] Force AUTO_MODE -> AUTO_STOP\r\n");
        (void)App_PostEvent(EVT_CMD_AUTO, APP_EVENT_SRC_COMMAND, 0u);
        (void)App_PostEvent(EVT_SENSOR_OBSTACLE_CRITICAL, APP_EVENT_SRC_SENSOR, 8u);
        return;
    }

    if (strcmp(line, "TESTEMG") == 0) {
        Log_Print("[TEST] Force EMERGENCY_STOP\r\n");
        (void)App_PostEvent(EVT_EMERGENCY_STOP, APP_EVENT_SRC_EMERGENCY, 0u);
        return;
    }

    if (strcmp(line, "TESTRECOV") == 0) {
        Log_Print("[TEST] Force RECOVERY_WAIT output\r\n");
        App_SetState(STATE_RECOVERY_WAIT);
        return;
    }

    type = Command_Parse(line);

    if (type == EVT_TYPE_COUNT) {
        Log_Print("[CMD] unknown: ");
        Log_Print(line);
        Log_Print("\r\n");
        Log_Print("[HELP] AUTO MANUAL FORWARD LEFT RIGHT CENTER STOP STATUS CANCEL RESET\r\n");
        return;
    }

    Log_Print("[CMD] ");
    Log_Print(line);
    Log_Print(" received\r\n");

    if (type == EVT_CMD_STATUS) {
        Command_PrintStatus();
    }

    if (type == EVT_CMD_CANCEL) {
        CPU_INT16U flushed;

        flushed = App_FlushPendingEvents();
        g_forward_display_ticks = 0u;

        Servo_SetAngle(SERVO_CENTER_ANGLE);
        Buzzer_SetState(STATE_IDLE);

        Log_PrintNum("[CMD] CANCEL flushed ", flushed, " pending event(s)\r\n");

        if (App_PostEvent(EVT_CMD_CANCEL, APP_EVENT_SRC_COMMAND, 0u) == DEF_OK) {
            Log_Print("[POST] EVT_CMD_CANCEL\r\n");
        } else {
            Log_Print("[POST] EVT_CMD_CANCEL failed\r\n");
        }

        return;
    }

    if (App_PostEvent(type, APP_EVENT_SRC_COMMAND, 0u) == DEF_OK) {
        Log_Print("[POST] ");
        Log_Print(Event_ToString(type));
        Log_Print("\r\n");

        Command_ApplyManualOutput(type);
    } else {
        Log_Print("[POST] failed\r\n");
    }
}

static  void  Command_ApplyManualOutput (EventType type)
{
    SystemState state;

    state = App_GetState();

    if ((state == STATE_EMERGENCY_STOP) && (type != EVT_CMD_RESET)) {
        Log_Print("[ACT] ignored because state is EMERGENCY_STOP\r\n");
        return;
    }

    switch (type) {
        case EVT_CMD_LEFT:
            Servo_SetAngle(SERVO_LEFT_ANGLE);
            Log_Print("[ACT] Servo angle = LEFT\r\n");
            break;

        case EVT_CMD_RIGHT:
            Servo_SetAngle(SERVO_RIGHT_ANGLE);
            Log_Print("[ACT] Servo angle = RIGHT\r\n");
            break;

        case EVT_CMD_CENTER:
            Servo_SetAngle(SERVO_CENTER_ANGLE);
            Log_Print("[ACT] Servo angle = CENTER\r\n");
            break;

        case EVT_CMD_FORWARD:
            Servo_SetAngle(SERVO_CENTER_ANGLE);
            g_forward_display_ticks = 25u;
            Log_Print("[ACT] Forward display ON: servo center, cyan blink\r\n");
            break;

        case EVT_CMD_STOP:
            g_forward_display_ticks = 0u;
            Servo_SetAngle(SERVO_CENTER_ANGLE);
            Buzzer_SetState(STATE_IDLE);
            Log_Print("[ACT] Stop output / servo center / forward display off\r\n");
            break;

        case EVT_CMD_RESET:
            g_forward_display_ticks = 0u;
            Servo_SetAngle(SERVO_CENTER_ANGLE);
            Buzzer_SetState(STATE_IDLE);
            Log_Print("[ACT] Reset output / servo center / buzzer off\r\n");
            break;

        default:
            break;
    }
}

static  void  Command_PrintStatus (void)
{
    Log_Print("[STATUS] state=");
    Log_Print(State_ToString(App_GetState()));
    Log_Print(", distance=");
    Log_PrintNum("", g_last_distance_cm, "cm, ");
    Log_PrintNum("ir=", g_last_ir_obstacle, ", ");
    Log_PrintNum("joy_x=", g_last_joy_x, ", ");
    Log_PrintNum("joy_y=", g_last_joy_y, "\r\n");
}

static  void  Output_ApplyState (SystemState state)
{
    if ((state == STATE_MANUAL_MODE) && (g_forward_display_ticks > 0u)) {
        RGB_SetForwardState();
        g_forward_display_ticks--;
    } else {
        RGB_SetState(state);
    }

    Buzzer_SetState(state);

    if ((state == STATE_IDLE) ||
        (state == STATE_AUTO_MODE) ||
        (state == STATE_OBSTACLE_WARNING) ||
        (state == STATE_AUTO_STOP) ||
        (state == STATE_EMERGENCY_STOP) ||
        (state == STATE_RECOVERY_WAIT)) {
        Servo_SetAngle(SERVO_CENTER_ANGLE);
    }
}

static  const char *State_ToString (SystemState state)
{
    switch (state) {
        case STATE_IDLE:
            return "IDLE";

        case STATE_MANUAL_MODE:
            return "MANUAL_MODE";

        case STATE_AUTO_MODE:
            return "AUTO_MODE";

        case STATE_OBSTACLE_WARNING:
            return "OBSTACLE_WARNING";

        case STATE_AUTO_STOP:
            return "AUTO_STOP";

        case STATE_EMERGENCY_STOP:
            return "EMERGENCY_STOP";

        case STATE_RECOVERY_WAIT:
            return "RECOVERY_WAIT";

        default:
            return "UNKNOWN";
    }
}

static  const char *Event_ToString (EventType type)
{
    switch (type) {
        case EVT_CMD_AUTO:
            return "EVT_CMD_AUTO";

        case EVT_CMD_MANUAL:
            return "EVT_CMD_MANUAL";

        case EVT_CMD_FORWARD:
            return "EVT_CMD_FORWARD";

        case EVT_CMD_LEFT:
            return "EVT_CMD_LEFT";

        case EVT_CMD_RIGHT:
            return "EVT_CMD_RIGHT";

        case EVT_CMD_CENTER:
            return "EVT_CMD_CENTER";

        case EVT_CMD_STOP:
            return "EVT_CMD_STOP";

        case EVT_CMD_STATUS:
            return "EVT_CMD_STATUS";

        case EVT_CMD_CANCEL:
            return "EVT_CMD_CANCEL";

        case EVT_CMD_RESET:
            return "EVT_CMD_RESET";

        case EVT_SENSOR_CLEAR:
            return "EVT_SENSOR_CLEAR";

        case EVT_SENSOR_OBSTACLE_WARN:
            return "EVT_SENSOR_OBSTACLE_WARN";

        case EVT_SENSOR_OBSTACLE_CRITICAL:
            return "EVT_SENSOR_OBSTACLE_CRITICAL";

        case EVT_IR_OBSTACLE:
            return "EVT_IR_OBSTACLE";

        case EVT_EMERGENCY_STOP:
            return "EVT_EMERGENCY_STOP";

        default:
            return "EVT_UNKNOWN";
    }
}

/*
*********************************************************************************************************
*                                      SENSOR EVENT POST
*********************************************************************************************************
*/
static  void  Sensor_PostObstacleEvent (void)
{
    int             distance;
    int             ir;
    static int      last_zone = -1;
    static int      last_ir = -1;
    static CPU_INT16U timeout_count = 0u;

    distance = Ultrasonic_ReadDistanceCm();
    ir       = IR_IsObstacleDetected();

    g_last_distance_cm = distance;
    g_last_ir_obstacle = ir;

    /*
     * If HC-SR04 is not connected, timeout happens repeatedly.
     * Do not spam USART logs.
     */
    if (distance < 0) {
        timeout_count++;

        if (timeout_count >= 50u) {
            Log_Print("[SENSOR] ultrasonic timeout. HC-SR04 may be disconnected.\r\n");
            timeout_count = 0u;
        }

        return;
    }

    timeout_count = 0u;

    /*
     * zone:
     * 0 = clear
     * 1 = warning
     * 2 = critical
     */
    if (distance <= DIST_CRITICAL_CM) {
        if (last_zone != 2) {
            Log_PrintNum("[SENSOR] distance=", distance, "cm, ");
            Log_PrintNum("ir=", ir, "\r\n");
            Log_Print("[POST] EVT_SENSOR_OBSTACLE_CRITICAL\r\n");
        }

        (void)App_PostEvent(EVT_SENSOR_OBSTACLE_CRITICAL, APP_EVENT_SRC_SENSOR, distance);
        last_zone = 2;
    }
    else if (distance < DIST_WARN_CM) {
        if (last_zone != 1) {
            Log_PrintNum("[SENSOR] distance=", distance, "cm, ");
            Log_PrintNum("ir=", ir, "\r\n");
            Log_Print("[POST] EVT_SENSOR_OBSTACLE_WARN\r\n");
        }

        (void)App_PostEvent(EVT_SENSOR_OBSTACLE_WARN, APP_EVENT_SRC_SENSOR, distance);
        last_zone = 1;
    }
    else {
        if (last_zone != 0) {
            Log_PrintNum("[SENSOR] distance=", distance, "cm, ");
            Log_PrintNum("ir=", ir, "\r\n");
            Log_Print("[POST] EVT_SENSOR_CLEAR\r\n");
        }

        (void)App_PostEvent(EVT_SENSOR_CLEAR, APP_EVENT_SRC_SENSOR, distance);
        last_zone = 0;
    }

    if (ir != last_ir) {
        Log_PrintNum("[SENSOR] ir=", ir, "\r\n");
        last_ir = ir;
    }

    if (ir == 1) {
        if (last_zone != 2) {
            Log_Print("[POST] EVT_IR_OBSTACLE\r\n");
        }

        (void)App_PostEvent(EVT_IR_OBSTACLE, APP_EVENT_SRC_SENSOR, 0u);
        last_zone = 2;
    }
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
