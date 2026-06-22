#include  <includes.h>

#include  "button.h"
#include  "app_io.h"
#include  "app_core.h"

#define  BUTTON_TASK_PRIO       15u
#define  BUTTON_TASK_STK_SIZE   256u
#define  BUTTON_POLL_MS         50u

static  OS_TCB   ButtonTaskTCB;
static  CPU_STK  ButtonTaskStk[BUTTON_TASK_STK_SIZE];

static  void     ButtonTask(void* p_arg);


void  ButtonTaskCreate(OS_ERR* p_err)
{
    OSTaskCreate(&ButtonTaskTCB,
        (CPU_CHAR*)"Button Task",
        ButtonTask,
        (void*)0,
        BUTTON_TASK_PRIO,
        &ButtonTaskStk[0],
        BUTTON_TASK_STK_SIZE / 10u,
        BUTTON_TASK_STK_SIZE,
        0u,
        0u,
        (void*)0,
        OS_OPT_TASK_STK_CHK | OS_OPT_TASK_STK_CLR,
        p_err);
}


static  void  ButtonTask(void* p_arg)
{
    OS_ERR  err;

    int     mode_now;
    int     emergency_now;
    int     reset_now;

    int     mode_prev;
    int     emergency_prev;
    int     reset_prev;

    (void)p_arg;

    mode_prev = 0;
    emergency_prev = 0;
    reset_prev = 0;

    while (DEF_TRUE) {
        mode_now = Button_ModeIsPressed();
        emergency_now = Button_EmergencyIsPressed();
        reset_now = Button_ResetIsPressed();

        /*
         * Rising edge detection
         */
        if ((mode_now == 1) && (mode_prev == 0)) {
            App_PostEvent(EVT_CMD_MODE_TOGGLE,
                APP_EVENT_SRC_BUTTON,
                0u);
        }

        if ((emergency_now == 1) && (emergency_prev == 0)) {
            App_PostEvent(EVT_EMERGENCY_STOP,
                APP_EVENT_SRC_BUTTON,
                0u);
        }

        if ((reset_now == 1) && (reset_prev == 0)) {
            App_PostEvent(EVT_CMD_RESET,
                APP_EVENT_SRC_BUTTON,
                0u);
        }

        mode_prev = mode_now;
        emergency_prev = emergency_now;
        reset_prev = reset_now;

        OSTimeDlyHMSM(0u,
            0u,
            0u,
            BUTTON_POLL_MS,
            OS_OPT_TIME_HMSM_STRICT,
            &err);
    }
}
