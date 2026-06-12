#include  "app_core.h"

#define  APP_EVENT_Q_SIZE  16u

static  OS_Q         AppEventQ;
static  OS_Q         AppFreeEventQ;
static  OS_SEM       AppEmergencySem;
static  AppEvent     AppEventPool[APP_EVENT_Q_SIZE];
static  SystemState  AppState;

static  SystemState  App_NextState(SystemState state, EventType event);

void  App_CoreInit (OS_ERR *p_err)
{
    CPU_INT08U  i;
    OS_ERR      local_err;

    if (p_err == (OS_ERR *)0) {
        p_err = &local_err;
    }

    AppState = STATE_IDLE;

    OSQCreate(&AppEventQ,
              (CPU_CHAR *)"App Event Queue",
              APP_EVENT_Q_SIZE,
              p_err);
    if (*p_err != OS_ERR_NONE) {
        return;
    }

    OSQCreate(&AppFreeEventQ,
              (CPU_CHAR *)"App Free Event Queue",
              APP_EVENT_Q_SIZE,
              p_err);
    if (*p_err != OS_ERR_NONE) {
        return;
    }

    for (i = 0u; i < APP_EVENT_Q_SIZE; i++) {
        OSQPost(&AppFreeEventQ,
                (void *)&AppEventPool[i],
                (OS_MSG_SIZE)sizeof(AppEvent),
                OS_OPT_POST_FIFO,
                p_err);
        if (*p_err != OS_ERR_NONE) {
            return;
        }
    }

    OSSemCreate(&AppEmergencySem,
                (CPU_CHAR *)"Emergency Semaphore",
                0u,
                p_err);
}

CPU_BOOLEAN  App_PostEvent (EventType type, AppEventSource source, CPU_INT32U data)
{
    AppEvent     *p_event;
    OS_MSG_SIZE   msg_size;
    OS_ERR        err;

    if (type >= EVT_TYPE_COUNT) {
        return (DEF_FAIL);
    }

    p_event = (AppEvent *)OSQPend(&AppFreeEventQ,
                                  0u,
                                  OS_OPT_PEND_BLOCKING,
                                  &msg_size,
                                  (CPU_TS *)0,
                                  &err);
    if ((err != OS_ERR_NONE) || (p_event == (AppEvent *)0)) {
        return (DEF_FAIL);
    }

    p_event->Type      = type;
    p_event->Source    = source;
    p_event->Data      = data;
    p_event->Timestamp = 0u;

    OSQPost(&AppEventQ,
            (void *)p_event,
            (OS_MSG_SIZE)sizeof(AppEvent),
            OS_OPT_POST_FIFO,
            &err);

    if (err != OS_ERR_NONE) {
        OSQPost(&AppFreeEventQ,
                (void *)p_event,
                (OS_MSG_SIZE)sizeof(AppEvent),
                OS_OPT_POST_FIFO,
                &err);
        return (DEF_FAIL);
    }

    return (DEF_OK);
}

CPU_INT16U  App_FlushPendingEvents (void)
{
    AppEvent     *p_pool_event;
    OS_MSG_SIZE   msg_size;
    OS_ERR        err;
    CPU_INT16U    flushed;

    flushed = 0u;

    while (DEF_TRUE) {
        p_pool_event = (AppEvent *)OSQPend(&AppEventQ,
                                           0u,
                                           OS_OPT_PEND_NON_BLOCKING,
                                           &msg_size,
                                           (CPU_TS *)0,
                                           &err);

        if ((err != OS_ERR_NONE) || (p_pool_event == (AppEvent *)0)) {
            break;
        }

        OSQPost(&AppFreeEventQ,
                (void *)p_pool_event,
                (OS_MSG_SIZE)sizeof(AppEvent),
                OS_OPT_POST_FIFO,
                &err);

        flushed++;
    }

    return flushed;
}

void  App_WaitEvent (AppEvent *p_event, OS_ERR *p_err)
{
    AppEvent     *p_pool_event;
    OS_MSG_SIZE   msg_size;
    OS_ERR        local_err;

    if (p_err == (OS_ERR *)0) {
        p_err = &local_err;
    }

    if (p_event == (AppEvent *)0) {
        *p_err = OS_ERR_OBJ_PTR_NULL;
        return;
    }

    p_pool_event = (AppEvent *)OSQPend(&AppEventQ,
                                       0u,
                                       OS_OPT_PEND_BLOCKING,
                                       &msg_size,
                                       (CPU_TS *)0,
                                       p_err);
    if ((*p_err != OS_ERR_NONE) || (p_pool_event == (AppEvent *)0)) {
        return;
    }

    *p_event = *p_pool_event;

    OSQPost(&AppFreeEventQ,
            (void *)p_pool_event,
            (OS_MSG_SIZE)sizeof(AppEvent),
            OS_OPT_POST_FIFO,
            p_err);
}

void  App_SetState (SystemState state)
{
    CPU_SR_ALLOC();

    if (state >= STATE_COUNT) {
        return;
    }

    CPU_CRITICAL_ENTER();
    AppState = state;
    CPU_CRITICAL_EXIT();
}

SystemState  App_GetState (void)
{
    SystemState  state;
    CPU_SR_ALLOC();

    CPU_CRITICAL_ENTER();
    state = AppState;
    CPU_CRITICAL_EXIT();

    return (state);
}

SystemState  App_HandleEvent (const AppEvent *p_event)
{
    SystemState  current;
    SystemState  next;

    if (p_event == (const AppEvent *)0) {
        return (App_GetState());
    }

    current = App_GetState();
    next    = App_NextState(current, p_event->Type);

    if (next != current) {
        App_SetState(next);
    }

    return (next);
}

OS_SEM  *App_GetEmergencySem (void)
{
    return (&AppEmergencySem);
}

CPU_BOOLEAN  App_RunStateMachineSelfTest (void)
{
    typedef  struct  app_sm_test_case {
        SystemState  Start;
        EventType    Event;
        SystemState  Expected;
    } APP_SM_TEST_CASE;

    static const APP_SM_TEST_CASE  tests[] = {
        { STATE_IDLE,             EVT_CMD_AUTO,                 STATE_AUTO_MODE        },
        { STATE_IDLE,             EVT_CMD_MANUAL,               STATE_MANUAL_MODE      },
        { STATE_AUTO_MODE,        EVT_SENSOR_OBSTACLE_WARN,     STATE_OBSTACLE_WARNING },
        { STATE_AUTO_MODE,        EVT_SENSOR_OBSTACLE_CRITICAL, STATE_AUTO_STOP        },
        { STATE_OBSTACLE_WARNING, EVT_IR_OBSTACLE,              STATE_AUTO_STOP        },
        { STATE_OBSTACLE_WARNING, EVT_SENSOR_CLEAR,             STATE_AUTO_MODE        },
        { STATE_AUTO_STOP,        EVT_CMD_RESET,                STATE_IDLE             },
        { STATE_MANUAL_MODE,      EVT_EMERGENCY_STOP,           STATE_EMERGENCY_STOP   },
        { STATE_EMERGENCY_STOP,   EVT_CMD_FORWARD,              STATE_EMERGENCY_STOP   },
        { STATE_EMERGENCY_STOP,   EVT_CMD_RESET,                STATE_IDLE             }
    };

    CPU_INT08U  i;
    AppEvent    event;

    for (i = 0u; i < (sizeof(tests) / sizeof(tests[0])); i++) {
        App_SetState(tests[i].Start);
        event.Type      = tests[i].Event;
        event.Source    = APP_EVENT_SRC_TEST;
        event.Data      = 0u;
        event.Timestamp = 0u;

        if (App_HandleEvent(&event) != tests[i].Expected) {
            return (DEF_FAIL);
        }
    }

    App_SetState(STATE_IDLE);
    return (DEF_OK);
}

static  SystemState  App_NextState (SystemState state, EventType event)
{
    /*
     * Emergency stop has the highest priority.
     * Any state + EVT_EMERGENCY_STOP -> STATE_EMERGENCY_STOP
     */
    if (event == EVT_EMERGENCY_STOP) {
        return (STATE_EMERGENCY_STOP);
    }

    /*
     * While emergency stop is active, ignore every event except RESET.
     */
    if (state == STATE_EMERGENCY_STOP) {
        if (event == EVT_CMD_RESET) {
            return (STATE_IDLE);
        }
        return (STATE_EMERGENCY_STOP);
    }

    /*
     * Common command events.
     * These commands should work from most normal states.
     */
    if (event == EVT_CMD_RESET) {
        return (STATE_IDLE);
    }

    if (event == EVT_CMD_STOP) {
        return (STATE_IDLE);
    }

    if (event == EVT_CMD_AUTO) {
        return (STATE_AUTO_MODE);
    }

    if (event == EVT_CMD_MANUAL) {
        return (STATE_MANUAL_MODE);
    }

    switch (state) {
        case STATE_IDLE:
            /*
             * AUTO / MANUAL / RESET are already handled above.
             */
            break;

        case STATE_MANUAL_MODE:
            /*
             * LEFT / RIGHT / CENTER / FORWARD / STOP keep manual mode.
             * Actual servo/output action is handled by C part CommandTask/OutputLogTask.
             */
            break;

        case STATE_AUTO_MODE:
            if (event == EVT_SENSOR_OBSTACLE_WARN) {
                return (STATE_OBSTACLE_WARNING);
            }

            if ((event == EVT_SENSOR_OBSTACLE_CRITICAL) ||
                (event == EVT_IR_OBSTACLE)) {
                return (STATE_AUTO_STOP);
            }
            break;

        case STATE_OBSTACLE_WARNING:
            if (event == EVT_SENSOR_CLEAR) {
                return (STATE_AUTO_MODE);
            }

            if ((event == EVT_SENSOR_OBSTACLE_CRITICAL) ||
                (event == EVT_IR_OBSTACLE)) {
                return (STATE_AUTO_STOP);
            }
            break;

        case STATE_AUTO_STOP:
            /*
             * RESET / AUTO / MANUAL are already handled above.
             */
            break;

        case STATE_RECOVERY_WAIT:
            /*
             * RESET / AUTO / MANUAL are already handled above.
             */
            break;

        default:
            break;
    }

    return (state);
}