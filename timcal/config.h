
#ifndef CONFIG_H
#define CONFIG_H

#include <Adafruit_GFX.h>
#include <gfxfont.h>
#include <GxEPD.h>
#include <GxGDEW042T2/GxGDEW042T2.h>      // 4.2" b/w
#include <GxIO/GxIO_SPI/GxIO_SPI.h>
#include <GxIO/GxIO.h>

#include "Gobold_Thin25pt7b.h"
#include "Gobold_Thin9pt7b.h"
#include "Gobold_Thin7pt7b.h"

#define DEBUG Serial

// I2C pins
#define SDA 16
#define SCL 17

// SPI pins

// SD card pins
#define SD_CS 21
#define SD_EN 5

// E-paper pins
#define EPD_CS 22
#define EPD_DC 15
#define EPD_BUSY 34
#define EPD_EN 12
#define EPD_RES 13

// PCF8574 pins
#define PCF_INT 35
#define SD_CD P4 // input
#define EXT_GPIO1 P5
#define EXT_GPIO2 P6
#define EXT_GPIO3 P7
#define PCF_I2C_ADDR 0x20

// LiPo
#define CHARGING_PIN 36
#define BATT_EN 25
#define BATTERY_VOLTAGE 34

// Fonts
#define LARGE_FONT &Gobold_Thin25pt7b
#define MED_FONT &Gobold_Thin9pt7b
#define SMALL_FONT &Gobold_Thin7pt7b

#define FORECAST_HOURS 0 // Show forecasted weather this many hours from update time. 0 for current weather

// Update interval
// Note: 0 should not be used!!
#define UPDATE_HOUR_INTERVAL 6  // update after every UPDATE_HOUR_INTERVAL hours, 0 should not be used
#define UPDATE_MIN_INTERVAL 60   // and at UPDATE_MIN mins, 0 should not be used
// for example to update afer every 6 hours, UPDATE_HOUR_INTERVAL = 6 and UPDATE_MIN = 60
// to update every 15 mins, UPDATE_HOUR_INTERVAL = 1 and UPDATE_MIN = 15

// Calendar
// Offset to change start day. 0=>Sun, 1=>Sat, 2=>Fri ... 6=>Mon
#define START_DAY_OFFSET 6
#define CAL_STRING "Mon   Tue   Wed   Thu   Fri   Sat   Sun"

// todo list definitions
// memory allocated for getting json output from todoist
#define MAX_TASKS 8
#define MAX_TODO_STR_LENGTH 19
extern RTC_DATA_ATTR char tasks[MAX_TASKS][MAX_TODO_STR_LENGTH+1];
extern RTC_DATA_ATTR uint8_t task_count;
extern uint16_t resp_pointer;
// memory to get the http response
extern bool request_finished;

// weather definitions
extern char weather_string[10];
extern RTC_DATA_ATTR char weather_icon[15];

// To keep track of config done
extern RTC_DATA_ATTR uint8_t config_done;
extern RTC_DATA_ATTR uint8_t first_boot;
extern uint8_t wifi_update;

extern String todoist_token_base;
extern String openweathermap_link_base;
extern RTC_DATA_ATTR char city_string[30];
extern RTC_DATA_ATTR char country_string[30];
extern RTC_DATA_ATTR char todoist_token_string[42];
extern RTC_DATA_ATTR char openweather_appkey_string[34];
extern RTC_DATA_ATTR char time_zone_string[7];

extern char buf[2048];
int8_t save_config(const char* config);
int8_t load_config();
#endif /* CONFIG_H */
