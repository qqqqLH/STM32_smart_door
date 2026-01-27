#ifndef APP_H
#define APP_H

#include "main.h"
#include "OLED.h"
#include "KEY.h"
#include "FINGER.h"
#include "JQ8900.h"
#include "FLASH.h"
#include "RC522.h"
#include "servo.h"
#include "SHAKE.h"
#include "BT.h"
#include "tim.h"
#include "usart.h"
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "gpio.h"
#include <string.h>
#include <stdio.h>

#define APP_FLASH_CFG_ADDR  0x0800FC00u
#define APP_CFG_MAGIC       0x4C4F434Eu
#define APP_PWD_LEN         6u
#define APP_MAX_CARDS       50u
#define APP_FINGER_DB_SIZE  50u
#define APP_UNLOCK_MS       1000u
#define APP_SERVO_PULSE_MS  500u
#define APP_OLED_FONT       14u
#define APP_VOLUME_LEVELS   5u

/* 4x4 keypad
 * 1      2      3      U
 * 4      5      6      D
 * 7      8      9      A
 * C      0      R      OK
 */
#define KEY_U        4u
#define KEY_D        8u
#define KEY_A        12u
#define KEY_C        13u
#define KEY_ZERO     14u
#define KEY_R        15u
#define KEY_OK       16u

typedef enum
{
  APP_EVT_KEY = 0,
  APP_EVT_CARD,
  APP_EVT_SHAKE,
  APP_EVT_BT,
  APP_EVT_FINGER
} AppEvtType;

typedef struct
{
  AppEvtType type;
  uint32_t data;
} AppEvent;

typedef enum
{
  FINGER_CMD_IDENTIFY = 0,
  FINGER_CMD_ENROLL,
  FINGER_CMD_DELETE,
  FINGER_CMD_READ_INDEX
} FingerCmdType;

typedef struct
{
  FingerCmdType cmd;
  uint16_t param;
} FingerCmd;

#define APP_FINGER_RESULT_IDENTIFY 1u
#define APP_FINGER_RESULT_ENROLL   2u
#define APP_FINGER_RESULT_PROMPT   3u
#define APP_FINGER_RESULT_DELETE   4u
#define APP_FINGER_RESULT_INDEX    5u

typedef enum
{
  APP_MODE_NORMAL = 0,
  APP_MODE_ADMIN_PWD,
  APP_MODE_ADMIN_MENU,
  APP_MODE_ADMIN_CARD_MENU,
  APP_MODE_ADMIN_CARD_ADD,
  APP_MODE_ADMIN_FINGER_MENU,
  APP_MODE_ADMIN_FINGER_WAIT,
  APP_MODE_PWD_SET,
  APP_MODE_ADMIN_VOLUME
} AppMode;

static QueueHandle_t s_evt_q;
static QueueHandle_t s_finger_q;

static FINGER_Handle s_finger;
static JQ8900_Handle s_voice;

static uint8_t s_password[APP_PWD_LEN];
static uint32_t s_card_uids[APP_MAX_CARDS];
static uint64_t s_card_valid;

static uint8_t s_pwd_buf[APP_PWD_LEN];
static uint8_t s_pwd_len;
static AppMode s_mode;
static uint8_t s_menu_sel;
static uint16_t s_card_sel_id;
static uint16_t s_finger_sel_id;
static uint8_t s_fail_count;
static TickType_t s_lock_until;
static uint8_t s_lock_last_sec;
static uint64_t s_finger_index;
static uint8_t s_finger_index_valid;
static uint8_t s_volume_level;

static char s_lines[4][17];

static void App_ShowLines(const char *line1, const char *line2, const char *line3, const char *line4);
static void App_ShowKeypad(const char *line4);
static void App_ShowPwdEntry(uint8_t len);
static void App_ShowAdminPwd(uint8_t len);
static void App_ShowPwdSet(uint8_t len);
static void App_ShowAdminMenu(void);
static void App_ShowCardMenu(void);
static void App_ShowFingerMenu(void);
static void App_ShowVolumeMenu(void);

void App_CreateTasks(void);

#endif
