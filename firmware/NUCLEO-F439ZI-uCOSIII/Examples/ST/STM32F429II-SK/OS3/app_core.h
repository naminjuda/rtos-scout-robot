#ifndef  APP_CORE_H
#define  APP_CORE_H

#include  <includes.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef  enum  event_type {
    EVT_CMD_AUTO = 0,
    EVT_CMD_MODE_TOGGLE,
    EVT_CMD_MANUAL,
    EVT_CMD_FORWARD,
    EVT_CMD_LEFT,
    EVT_CMD_RIGHT,
    EVT_CMD_CENTER,
    EVT_CMD_STOP,
    EVT_CMD_STATUS,
    EVT_CMD_CANCEL,
    EVT_CMD_RESET,
    EVT_SENSOR_CLEAR,
    EVT_SENSOR_OBSTACLE_WARN,
    EVT_SENSOR_OBSTACLE_CRITICAL,
    EVT_IR_OBSTACLE,
    EVT_EMERGENCY_STOP,
    EVT_TYPE_COUNT
} EventType;

typedef  enum  app_event_source {
    APP_EVENT_SRC_COMMAND = 0,
    APP_EVENT_SRC_BUTTON,
    APP_EVENT_SRC_SENSOR,
    APP_EVENT_SRC_EMERGENCY,
    APP_EVENT_SRC_TEST
} AppEventSource;

typedef  struct  app_event {
    EventType       Type;
    AppEventSource  Source;
    CPU_INT32U      Data;
    CPU_TS          Timestamp;
} AppEvent;

typedef  enum  system_state {
    STATE_IDLE = 0,
    STATE_MANUAL_MODE,
    STATE_AUTO_MODE,
    STATE_OBSTACLE_WARNING,
    STATE_AUTO_STOP,
    STATE_EMERGENCY_STOP,
    STATE_RECOVERY_WAIT,
    STATE_COUNT
} SystemState;

void         App_CoreInit             (OS_ERR       *p_err);
CPU_BOOLEAN  App_PostEvent            (EventType     type,
                                       AppEventSource source,
                                       CPU_INT32U     data);
CPU_INT16U   App_FlushPendingEvents   (void);
void         App_WaitEvent            (AppEvent     *p_event,
                                       OS_ERR       *p_err);
void         App_SetState             (SystemState   state);
SystemState  App_GetState             (void);
SystemState  App_HandleEvent          (const AppEvent *p_event);
OS_SEM      *App_GetEmergencySem      (void);
CPU_BOOLEAN  App_RunStateMachineSelfTest(void);

#ifdef __cplusplus
}
#endif

#endif
