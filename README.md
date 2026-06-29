# RTOS 기반 UGV 정찰 로봇 제어 시스템

NUCLEO-F439ZI 보드와 uC/OS-III를 사용해서 만든 RTOS 기반 로봇 제어 프로젝트입니다.
USART 명령, 버튼, 조이스틱, 초음파 센서, IR 센서를 이용해 수동/자동 모드, 장애물 감지, 정지 기능 등을 구현했습니다.

## 프로젝트 개요

위험 지역에서 사용하는 소형 정찰 로봇을 가정하고 만든 프로젝트입니다.
USART 명령이나 조이스틱으로 수동 제어가 가능하고, 자동 모드에서는 센서를 이용해 장애물을 감지합니다.
문제가 발생하면 자동으로 정지 상태로 전환됩니다.

## 개발 환경

| 항목            | 내용                 |
| ------------- | ------------------ |
| Board         | NUCLEO-F439ZI      |
| RTOS          | uC/OS-III          |
| IDE           | Atollic TrueSTUDIO |
| Language      | C                  |
| Communication | USART3             |

## 사용 부품

| 부품          | 역할       |
| ----------- | -------- |
| RGB LED     | 상태 표시    |
| Servo Motor | 방향 표시    |
| Buzzer      | 경고음      |
| HC-SR04     | 거리 측정    |
| IR Sensor   | 근거리 감지   |
| Button      | 모드/정지/리셋 |
| Joystick    | 수동 제어    |
| USART       | 명령 입력    |

## 주요 기능

### USART 명령

```text
STATUS
MANUAL
AUTO
LEFT
RIGHT
CENTER
FORWARD
STOP
RESET
CANCEL
```

* MANUAL / AUTO 모드 전환
* LEFT / RIGHT / CENTER 방향 제어
* FORWARD 상태 표시 (LED)
* STOP / RESET / CANCEL 처리

### 수동 모드

조이스틱이나 명령으로 방향 제어

| 입력      | 동작     |
| ------- | ------ |
| LEFT    | 왼쪽     |
| RIGHT   | 오른쪽    |
| CENTER  | 중앙     |
| FORWARD | LED 점멸 |

### 자동 모드

센서로 장애물 감지

| 조건      | 상태        | 출력       |
| ------- | --------- | -------- |
| 없음      | AUTO_MODE | 초록 LED   |
| 15~30cm | WARNING   | 빨강/초록 점멸 |
| 15cm 이하 | STOP      | 빨강 점멸    |
| IR 감지   | STOP      | 빨강 점멸    |

### 긴급 정지

버튼 입력 시 즉시 정지

* LED: 빨강 점멸
* Servo: 중앙
* Buzzer: 빠른 beep
* RESET으로 복구

## RTOS 구조

Task 분리해서 구현

| Task          | 역할    |
| ------------- | ----- |
| CommandTask   | 명령 처리 |
| SensorTask    | 센서 처리 |
| ButtonTask    | 버튼 처리 |
| EmergencyTask | 긴급 정지 |
| ControlTask   | 상태 관리 |
| OutputLogTask | 출력    |

```text
Input
→ Queue / Semaphore
→ Control / Emergency
→ State 변경
→ LED / Servo / Buzzer
```

## 시스템 상태

| 상태        | 의미    |
| --------- | ----- |
| IDLE      | 대기    |
| MANUAL    | 수동    |
| AUTO      | 자동    |
| WARNING   | 경고    |
| STOP      | 정지    |
| EMERGENCY | 긴급 정지 |

