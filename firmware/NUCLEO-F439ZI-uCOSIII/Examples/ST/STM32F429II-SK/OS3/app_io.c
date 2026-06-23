/*
*********************************************************************************************************
*                                             APP IO
*
*                         Sensor / Joystick / USART log implementation
*
*   USART3 TX       : PD9
*   USART3 RX       : PD8
*
*   HC-SR04 TRIG    : PC0
*   HC-SR04 ECHO    : PC1
*   IR OUT          : PC2
*
*   Joystick VRx    : PA0, ADC1 Channel 0
*   Joystick VRy    : PA1, ADC1 Channel 1
*
* joystick sw: not in use
*********************************************************************************************************
*/

#include  <includes.h>

#include  "app_io.h"
#include  "app_core.h"
#include  "app_cfg.h"

#include  "stm32f4xx_rcc.h"
#include  "stm32f4xx_gpio.h"
#include  "stm32f4xx_usart.h"
#include  "stm32f4xx_adc.h"
#include  "stm32f4xx_tim.h"
#include  "stm32f4xx.h"

#define APP_CPU_CLOCK_HZ              180000000u

#define  APP_DEMCR_REG                 (*(volatile uint32_t *)0xE000EDFCUL)
#define  APP_DWT_CTRL_REG              (*(volatile uint32_t *)0xE0001000UL)
#define  APP_DWT_CYCCNT_REG            (*(volatile uint32_t *)0xE0001004UL)

#define  APP_DEMCR_TRCENA_BIT          (1UL << 24)
#define  APP_DWT_CTRL_CYCCNTENA_BIT    (1UL << 0)

/*
*********************************************************************************************************
*                                        LOCAL FUNCTION PROTOTYPES
*********************************************************************************************************
*/

static  void      Setup_Usart3        (void);
static  void      SensorGPIO_Init     (void);
static  void      ButtonGPIO_Init     (void);
static  void      JoystickADC_Init    (void);
static  void      OutputGPIO_Init     (void);
static  void      ServoPWM_Init       (void);

static  void      DWT_DelayInit       (void);
static  void      DelayUs             (uint32_t us);
static  uint32_t  GetMicros           (void);

static  uint16_t  ADC_ReadChannel     (uint8_t channel);

static  void      UsartPutChar        (char c);

static  void      Buzzer_On           (void);
static  void      Buzzer_Off          (void);

/*
*********************************************************************************************************
*                                             AppIO_Init()
*********************************************************************************************************
*/

void  AppIO_Init (void)
{
    Setup_Usart3();
    SensorGPIO_Init();
    ButtonGPIO_Init();
    JoystickADC_Init();
    DWT_DelayInit();
    OutputGPIO_Init();
    ServoPWM_Init();

    RGB_SetState(STATE_IDLE);
    Buzzer_SetState(STATE_IDLE);
    Servo_SetAngle(SERVO_CENTER_ANGLE);

    Log_Print("[INIT] AppIO init done\r\n");
}
/*
*********************************************************************************************************
*                                      USART3 SETUP / LOG
*
* USART3 TX : PD9
* USART3 RX : PD8
* Baudrate  : 115200
*********************************************************************************************************
*/

static  void  Setup_Usart3 (void)
{
    GPIO_InitTypeDef   gpio_init;
    USART_InitTypeDef  usart_init;

    RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOD, ENABLE);
    RCC_APB1PeriphClockCmd(RCC_APB1Periph_USART3, ENABLE);

    GPIO_PinAFConfig(GPIOD, GPIO_PinSource8, GPIO_AF_USART3);
    GPIO_PinAFConfig(GPIOD, GPIO_PinSource9, GPIO_AF_USART3);

    gpio_init.GPIO_Pin   = GPIO_Pin_8 | GPIO_Pin_9;
    gpio_init.GPIO_Mode  = GPIO_Mode_AF;
    gpio_init.GPIO_OType = GPIO_OType_PP;
    gpio_init.GPIO_PuPd  = GPIO_PuPd_UP;
    gpio_init.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(GPIOD, &gpio_init);

    usart_init.USART_BaudRate            = 115200;
    usart_init.USART_WordLength          = USART_WordLength_8b;
    usart_init.USART_StopBits            = USART_StopBits_1;
    usart_init.USART_Parity              = USART_Parity_No;
    usart_init.USART_HardwareFlowControl = USART_HardwareFlowControl_None;
    usart_init.USART_Mode                = USART_Mode_Rx | USART_Mode_Tx;

    USART_Init(USART3, &usart_init);
    USART_Cmd(USART3, ENABLE);
}

int  USART3_ReadCharNonBlocking (char *p_char)
{
    if (p_char == (char *)0) {
        return 0;
    }

    if (USART_GetFlagStatus(USART3, USART_FLAG_RXNE) == RESET) {
        return 0;
    }

    *p_char = (char)(USART_ReceiveData(USART3) & 0xFFu);
    return 1;
}

static  void  UsartPutChar (char c)
{
    while (USART_GetFlagStatus(USART3, USART_FLAG_TXE) == RESET) {
    }

    USART_SendData(USART3, (uint16_t)c);
}

void  Log_Print (const char *msg)
{
    while (*msg != '\0') {
        UsartPutChar(*msg++);
    }
}

void  Log_PrintNum (const char *prefix, int value, const char *suffix)
{
    char  buf[32];
    char  rev[16];
    int   i;
    int   n;
    int   is_negative;

    Log_Print(prefix);

    if (value == 0) {
        UsartPutChar('0');
        Log_Print(suffix);
        return;
    }

    is_negative = 0;

    if (value < 0) {
        is_negative = 1;
        value = -value;
    }

    i = 0;

    while ((value > 0) && (i < (int)sizeof(rev))) {
        rev[i++] = (char)('0' + (value % 10));
        value /= 10;
    }

    n = 0;

    if (is_negative) {
        buf[n++] = '-';
    }

    while (i > 0) {
        buf[n++] = rev[--i];
    }

    buf[n] = '\0';

    Log_Print(buf);
    Log_Print(suffix);
}

/*
*********************************************************************************************************
*                                      SENSOR GPIO INIT
*
* HC-SR04:
*   TRIG -> PC0
*   ECHO -> PC1
*
* IR sensor:
*   OUT  -> PC2
*********************************************************************************************************
*/

static  void  SensorGPIO_Init (void)
{
    GPIO_InitTypeDef  gpio_init;

    RCC_AHB1PeriphClockCmd(ULTRA_TRIG_GPIO_CLK |
                           ULTRA_ECHO_GPIO_CLK |
                           IR_GPIO_CLK,
                           ENABLE);

    /*
     * HC-SR04 TRIG: D2 = PF15 output
     */
    gpio_init.GPIO_Mode  = GPIO_Mode_OUT;
    gpio_init.GPIO_OType = GPIO_OType_PP;
    gpio_init.GPIO_Speed = GPIO_Speed_50MHz;
    gpio_init.GPIO_PuPd  = GPIO_PuPd_DOWN;
    gpio_init.GPIO_Pin   = ULTRA_TRIG_PIN;
    GPIO_Init(ULTRA_TRIG_GPIO_PORT, &gpio_init);

    GPIO_ResetBits(ULTRA_TRIG_GPIO_PORT, ULTRA_TRIG_PIN);

    /*
     * HC-SR04 ECHO: A5 = PF10 input
     */
    gpio_init.GPIO_Mode  = GPIO_Mode_IN;
    gpio_init.GPIO_OType = GPIO_OType_PP;
    gpio_init.GPIO_Speed = GPIO_Speed_50MHz;
    gpio_init.GPIO_PuPd  = GPIO_PuPd_DOWN;
    gpio_init.GPIO_Pin   = ULTRA_ECHO_PIN;
    GPIO_Init(ULTRA_ECHO_GPIO_PORT, &gpio_init);

    /*
     * IR OUT: D7 = PF13 input pull-up
     */
    gpio_init.GPIO_Mode  = GPIO_Mode_IN;
    gpio_init.GPIO_OType = GPIO_OType_PP;
    gpio_init.GPIO_Speed = GPIO_Speed_50MHz;
    gpio_init.GPIO_PuPd  = GPIO_PuPd_UP;
    gpio_init.GPIO_Pin   = IR_PIN;
    GPIO_Init(IR_PORT, &gpio_init);
}


/*
*********************************************************************************************************
*                                      BUTTON GPIO INIT
*
* MODE button
* EMERGENCY button
* RESET button
*********************************************************************************************************
*/

static  void  ButtonGPIO_Init (void)
{
    GPIO_InitTypeDef  gpio_init;

    RCC_AHB1PeriphClockCmd(BUTTON_MODE_GPIO_CLK |
                           BUTTON_EMERGENCY_GPIO_CLK |
                           BUTTON_RESET_GPIO_CLK,
                           ENABLE);

    gpio_init.GPIO_Mode  = GPIO_Mode_IN;
    gpio_init.GPIO_OType = GPIO_OType_PP;
    gpio_init.GPIO_Speed = GPIO_Speed_50MHz;
    gpio_init.GPIO_PuPd  = GPIO_PuPd_UP;

    gpio_init.GPIO_Pin = BUTTON_MODE_GPIO_PIN;
    GPIO_Init(BUTTON_MODE_GPIO_PORT, &gpio_init);

    gpio_init.GPIO_Pin = BUTTON_EMERGENCY_GPIO_PIN;
    GPIO_Init(BUTTON_EMERGENCY_GPIO_PORT, &gpio_init);

    gpio_init.GPIO_Pin = BUTTON_RESET_GPIO_PIN;
    GPIO_Init(BUTTON_RESET_GPIO_PORT, &gpio_init);
}

/*
*********************************************************************************************************
*                                      JOYSTICK ADC INIT
*
* Joystick:
*   VRx -> PA0, ADC1 Channel 0
*   VRy -> PA1, ADC1 Channel 1
*
*********************************************************************************************************
*/

static  void  JoystickADC_Init (void)
{
    GPIO_InitTypeDef       gpio_init;
    ADC_CommonInitTypeDef  adc_common_init;
    ADC_InitTypeDef        adc_init;

    RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOC, ENABLE);
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_ADC1, ENABLE);

    gpio_init.GPIO_Mode  = GPIO_Mode_AN;
    gpio_init.GPIO_OType = GPIO_OType_PP;
    gpio_init.GPIO_Speed = GPIO_Speed_50MHz;
    gpio_init.GPIO_PuPd  = GPIO_PuPd_NOPULL;
    gpio_init.GPIO_Pin   = JOY_VRX_PIN | JOY_VRY_PIN;
    GPIO_Init(JOY_ADC_PORT, &gpio_init);

    ADC_CommonStructInit(&adc_common_init);
    adc_common_init.ADC_Mode             = ADC_Mode_Independent;
    adc_common_init.ADC_Prescaler        = ADC_Prescaler_Div4;
    adc_common_init.ADC_DMAAccessMode    = ADC_DMAAccessMode_Disabled;
    adc_common_init.ADC_TwoSamplingDelay = ADC_TwoSamplingDelay_5Cycles;
    ADC_CommonInit(&adc_common_init);

    ADC_StructInit(&adc_init);
    adc_init.ADC_Resolution           = ADC_Resolution_12b;
    adc_init.ADC_ScanConvMode         = DISABLE;
    adc_init.ADC_ContinuousConvMode   = DISABLE;
    adc_init.ADC_ExternalTrigConvEdge = ADC_ExternalTrigConvEdge_None;
    adc_init.ADC_DataAlign            = ADC_DataAlign_Right;
    adc_init.ADC_NbrOfConversion      = 1u;

    ADC_Init(ADC1, &adc_init);
    ADC_Cmd(ADC1, ENABLE);
}

static  uint16_t  ADC_ReadChannel (uint8_t channel)
{
    ADC_RegularChannelConfig(ADC1, channel, 1u, ADC_SampleTime_144Cycles);

    ADC_SoftwareStartConv(ADC1);

    while (ADC_GetFlagStatus(ADC1, ADC_FLAG_EOC) == RESET) {
    }

    return ADC_GetConversionValue(ADC1);
}

uint16_t  Joystick_ReadX (void)
{
    return ADC_ReadChannel(JOY_VRX_ADC_CHANNEL);
}

uint16_t  Joystick_ReadY (void)
{
    return ADC_ReadChannel(JOY_VRY_ADC_CHANNEL);
}

/*
*********************************************************************************************************
*                                      MICROSECOND DELAY
*********************************************************************************************************
*/

static  void  DWT_DelayInit (void)
{
    APP_DEMCR_REG      |= APP_DEMCR_TRCENA_BIT;
    APP_DWT_CYCCNT_REG  = 0u;
    APP_DWT_CTRL_REG   |= APP_DWT_CTRL_CYCCNTENA_BIT;
}

static  void  DelayUs (uint32_t us)
{
    uint32_t  start;
    uint32_t  ticks;

    start = APP_DWT_CYCCNT_REG;
    ticks = (APP_CPU_CLOCK_HZ / 1000000u) * us;
    while ((APP_DWT_CYCCNT_REG - start) < ticks) {
    }
}

static  uint32_t  GetMicros (void)
{
    return APP_DWT_CYCCNT_REG / (APP_CPU_CLOCK_HZ / 1000000u);
}

/*
*********************************************************************************************************
*                                      ULTRASONIC READ
*
* HC-SR04:
*   distance(cm) = echo_width_us / 58
*
* return:
*   normal: >0
*   timeout : -1
*********************************************************************************************************
*/

int  Ultrasonic_ReadDistanceCm (void)
{
    uint32_t  start_time;
    uint32_t  echo_start;
    uint32_t  echo_end;
    uint32_t  echo_width;

    GPIO_ResetBits(ULTRA_TRIG_GPIO_PORT, ULTRA_TRIG_PIN);
    DelayUs(2u);

    GPIO_SetBits(ULTRA_TRIG_GPIO_PORT, ULTRA_TRIG_PIN);
    DelayUs(10u);

    GPIO_ResetBits(ULTRA_TRIG_GPIO_PORT, ULTRA_TRIG_PIN);

    start_time = GetMicros();

    while (GPIO_ReadInputDataBit(ULTRA_ECHO_GPIO_PORT, ULTRA_ECHO_PIN) == Bit_RESET) {
        if ((GetMicros() - start_time) > ULTRA_TIMEOUT_US) {
            return -2;
        }
    }

    echo_start = GetMicros();

    while (GPIO_ReadInputDataBit(ULTRA_ECHO_GPIO_PORT, ULTRA_ECHO_PIN) == Bit_SET) {
        if ((GetMicros() - echo_start) > ULTRA_TIMEOUT_US) {
            return -3;
        }
    }

    echo_end   = GetMicros();
    echo_width = echo_end - echo_start;

    return (int)(echo_width / 58u);
}

/*
*********************************************************************************************************
*                                      IR READ
*********************************************************************************************************
*/

int  IR_IsObstacleDetected (void)
{
    BitAction  level;

    level = GPIO_ReadInputDataBit(IR_PORT, IR_PIN);

    if (level == IR_DETECTED_LEVEL) {
        return 1;
    }

    return 0;
}

/*
*********************************************************************************************************
*                                      BUTTON READ
*********************************************************************************************************
*/

int  Button_ModeIsPressed (void)
{
    if (GPIO_ReadInputDataBit(BUTTON_MODE_GPIO_PORT,
                              BUTTON_MODE_GPIO_PIN) == BUTTON_PRESSED_LEVEL) {
        return 1;
    }

    return 0;
}


int  Button_EmergencyIsPressed (void)
{
    if (GPIO_ReadInputDataBit(BUTTON_EMERGENCY_GPIO_PORT,
                              BUTTON_EMERGENCY_GPIO_PIN) == BUTTON_PRESSED_LEVEL) {
        return 1;
    }

    return 0;
}


int  Button_ResetIsPressed (void)
{
    if (GPIO_ReadInputDataBit(BUTTON_RESET_GPIO_PORT,
                              BUTTON_RESET_GPIO_PIN) == BUTTON_PRESSED_LEVEL) {
        return 1;
    }

    return 0;
}

/*
*********************************************************************************************************
*                                      OUTPUT CONTROL
*********************************************************************************************************
*/

static  void  RGB_AllOff (void)
{
    GPIO_ResetBits(RGB_RED_GPIO_PORT, RGB_RED_GPIO_PIN);
    GPIO_ResetBits(RGB_GREEN_GPIO_PORT, RGB_GREEN_GPIO_PIN);
    GPIO_ResetBits(RGB_BLUE_GPIO_PORT, RGB_BLUE_GPIO_PIN);
}

static  void  RGB_RedOn (void)
{
    GPIO_SetBits(RGB_RED_GPIO_PORT, RGB_RED_GPIO_PIN);
}

static  void  RGB_GreenOn (void)
{
    GPIO_SetBits(RGB_GREEN_GPIO_PORT, RGB_GREEN_GPIO_PIN);
}

static  void  RGB_BlueOn (void)
{
    GPIO_SetBits(RGB_BLUE_GPIO_PORT, RGB_BLUE_GPIO_PIN);
}

static  void  Buzzer_On (void)
{
    /*
     * Active-low buzzer module:
     * LOW = ON
     */
    GPIO_ResetBits(BUZZER_GPIO_PORT, BUZZER_GPIO_PIN);
}

static  void  Buzzer_Off (void)
{
    /*
     * Active-low buzzer module:
     * HIGH = OFF
     */
    GPIO_SetBits(BUZZER_GPIO_PORT, BUZZER_GPIO_PIN);
}

static  void  OutputGPIO_Init (void)
{
    GPIO_InitTypeDef  gpio_init;

    /*
     * RGB:
     * R -> D3 = PE13
     * G -> D4 = PF14
     * B -> D5 = PE11
     *
     * Buzzer:
     * PE9
     */
    RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOE, ENABLE);
    RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOF, ENABLE);

    gpio_init.GPIO_Mode  = GPIO_Mode_OUT;
    gpio_init.GPIO_OType = GPIO_OType_PP;
    gpio_init.GPIO_Speed = GPIO_Speed_50MHz;
    gpio_init.GPIO_PuPd  = GPIO_PuPd_NOPULL;

    /* GPIOE: Red, Blue, Buzzer */
    gpio_init.GPIO_Pin = RGB_RED_GPIO_PIN |
                         RGB_BLUE_GPIO_PIN |
                         BUZZER_GPIO_PIN;
    GPIO_Init(GPIOE, &gpio_init);

    /* GPIOF: Green */
    gpio_init.GPIO_Pin = RGB_GREEN_GPIO_PIN;
    GPIO_Init(GPIOF, &gpio_init);

    GPIO_ResetBits(RGB_RED_GPIO_PORT, RGB_RED_GPIO_PIN);
    GPIO_ResetBits(RGB_GREEN_GPIO_PORT, RGB_GREEN_GPIO_PIN);
    GPIO_ResetBits(RGB_BLUE_GPIO_PORT, RGB_BLUE_GPIO_PIN);

    Buzzer_Off();
}

static  void  ServoPWM_Init (void)
{
    GPIO_InitTypeDef        gpio_init;
    TIM_TimeBaseInitTypeDef tim_init;
    TIM_OCInitTypeDef       oc_init;

    RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOA, ENABLE);
    RCC_APB1PeriphClockCmd(SERVO_TIM_RCC, ENABLE);

    GPIO_PinAFConfig(SERVO_GPIO_PORT, SERVO_GPIO_PIN_SOURCE, SERVO_GPIO_AF);

    gpio_init.GPIO_Pin   = SERVO_GPIO_PIN;
    gpio_init.GPIO_Mode  = GPIO_Mode_AF;
    gpio_init.GPIO_OType = GPIO_OType_PP;
    gpio_init.GPIO_PuPd  = GPIO_PuPd_NOPULL;
    gpio_init.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(SERVO_GPIO_PORT, &gpio_init);

    /*
     * APB1 timer clock is treated as about 90MHz here.
     * Prescaler 90-1 makes 1MHz timer tick, so 1 count = 1us.
     * Period 20000-1 makes 20ms PWM period = 50Hz servo PWM.
     */
    TIM_TimeBaseStructInit(&tim_init);
    tim_init.TIM_Prescaler     = 90u - 1u;
    tim_init.TIM_CounterMode   = TIM_CounterMode_Up;
    tim_init.TIM_Period        = SERVO_TIM_PERIOD - 1u;
    tim_init.TIM_ClockDivision = TIM_CKD_DIV1;
    TIM_TimeBaseInit(SERVO_TIM, &tim_init);

    TIM_OCStructInit(&oc_init);
    oc_init.TIM_OCMode      = TIM_OCMode_PWM1;
    oc_init.TIM_OutputState = TIM_OutputState_Enable;
    oc_init.TIM_Pulse       = 1500u;
    oc_init.TIM_OCPolarity  = TIM_OCPolarity_High;
    TIM_OC1Init(SERVO_TIM, &oc_init);
    TIM_OC1PreloadConfig(SERVO_TIM, TIM_OCPreload_Enable);
    TIM_ARRPreloadConfig(SERVO_TIM, ENABLE);
    TIM_Cmd(SERVO_TIM, ENABLE);
}

void  Servo_SetAngle (int angle)
{
    uint32_t pulse;

    if (angle < 0) {
        angle = 0;
    }

    if (angle > 180) {
        angle = 180;
    }

    pulse = SERVO_PULSE_MIN_US +
            (((uint32_t)angle * (SERVO_PULSE_MAX_US - SERVO_PULSE_MIN_US)) / 180u);

    TIM_SetCompare1(SERVO_TIM, pulse);
}

void  RGB_SetState (SystemState state)
{
    static CPU_INT08U tick = 0u;

    tick++;

    RGB_AllOff();

    switch (state) {
        case STATE_IDLE:
            RGB_BlueOn();
            break;

        case STATE_MANUAL_MODE:
            RGB_GreenOn();
            break;

        case STATE_AUTO_MODE:
            /*
             * Green slow blink.
             */
            if ((tick % 6u) < 3u) {
                RGB_GreenOn();
            }
            break;

        case STATE_OBSTACLE_WARNING:
            /*
            * Yellow/orange blink.
            * Green LED is usually brighter than red, so green is turned on less often.
            */
            if ((tick % 6u) < 3u) {
                RGB_RedOn();

                if ((tick % 6u) == 0u) {
                    RGB_GreenOn();
                }
            }
            break;
            
        case STATE_AUTO_STOP:
            /*
             * Red blink.
             */
            if ((tick % 4u) < 2u) {
                RGB_RedOn();
            }
            break;

        case STATE_EMERGENCY_STOP:
            /*
             * Red fast blink.
             */
            if ((tick % 2u) == 0u) {
                RGB_RedOn();
            }
            break;

        case STATE_RECOVERY_WAIT:
            /*
             * Blue/purple blink.
             */
            if ((tick % 4u) < 2u) {
                RGB_BlueOn();
                RGB_RedOn();
            }
            break;

        default:
            break;
    }
}

void  RGB_SetForwardState (void)
{
    static CPU_INT08U tick = 0u;

    tick++;

    RGB_AllOff();

    /*
     * Forward display: cyan blink = Green + Blue.
     * It means "moving forward" without a real DC motor.
     */
    if ((tick % 2u) == 0u) {
        RGB_GreenOn();
        RGB_BlueOn();
    }
}

void  Buzzer_SetState (SystemState state)
{
    static CPU_INT08U tick = 0u;

    tick++;

    switch (state) {
        case STATE_OBSTACLE_WARNING:
            /*
             * Warning: very short beep.
             */
            if ((tick % 15u) == 0u) {
                Buzzer_On();
            } else {
                Buzzer_Off();
            }
            break;

        case STATE_AUTO_STOP:
            /*
             * Auto stop: short intermittent beep.
             */
            if ((tick % 10u) == 0u) {
                Buzzer_On();
            } else {
                Buzzer_Off();
            }
            break;

        case STATE_EMERGENCY_STOP:
            /*
             * Emergency stop: intermittent beep, not continuous.
             */
            if ((tick % 5u) == 0u) {
                Buzzer_On();
            } else {
                Buzzer_Off();
            }
            break;

        case STATE_RECOVERY_WAIT:
            Buzzer_Off();
            break;

        default:
            /*
             * IDLE, MANUAL, AUTO, RESET state.
             */
            Buzzer_Off();
            break;
    }
}
