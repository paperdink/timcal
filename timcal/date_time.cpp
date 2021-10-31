

#include <stdlib.h>
#include <time.h>
#include <sys/time.h>
#include "FS.h"
#include "SPIFFS.h"
#include "SD.h"
#include "SPI.h"
#include <sys/time.h>
#include "WiFi.h"
#include <ArduinoJson.h>
#include <HTTPClient.h>
#include "driver/adc.h"
#include <esp_wifi.h>
#include <esp_bt.h>

#include "config.h"
#include "date_time.h"

RTC_DATA_ATTR struct time_struct now; // keep track of time
String time_zone_base = "UTC";

// Time APIs
int8_t get_date_dtls(String time_zone) {
  String time_zone_string = time_zone_base+time_zone;
  DEBUG.printf("TZ: %s\n", time_zone_string.c_str());
  setenv("TZ", time_zone_string.c_str(), 1);
  
  struct tm timeinfo;
  if(!getLocalTime(&timeinfo)){
    DEBUG.println("Failed to obtain time");
    return -1;
  }
  time_t epoch = mktime(&timeinfo);

  sscanf(ctime(&epoch), "%s %s %hhd %hhd:%hhd:%hhd %d", now.wday, now.month, &now.mday, &now.mil_hour, &now.min, &now.sec, &now.year);
  
  now.month_num = timeinfo.tm_mon + 1;
  // gives offset of first day of the month with respect to Monday
  //https://www.tondering.dk/claus/cal/chrweek.php#calcdow
  // 0=Sunday, 1=Monday ... 6=Saturday
  uint8_t a = (14 - now.month_num) / 12;
  uint16_t y = now.year - a;
  uint16_t m = now.month_num + (12 * a) - 2;
  now.day_offset = (now.mday + y + (y/4) - (y/100) + (y/400) + ((31*m)/12))% 7;
  now.day_offset = (now.day_offset + START_DAY_OFFSET)%7;

  // convert to 12 hour
  if (now.mil_hour > 12) {
    now.hour = now.mil_hour - 12;
  } else {
    now.hour = now.mil_hour;
  }

  DEBUG.printf("Time is %d %d:%d:%d on %s on the %d/%d/%d . It is the month of %s. day_offset: %d\n",now.mil_hour,now.hour,now.min,now.sec,now.wday,now.mday,now.month_num,now.year, now.month, now.day_offset);
  return 0;
}

int8_t set_time() {

  struct tm t;
  t.tm_year = 2020 - 1900;
  t.tm_mon = 2 - 1;         // Month, 1 - jan to 12 - dec
  t.tm_mday = 3;          // Day of the month
  t.tm_hour = 9;
  t.tm_min = 57;
  t.tm_sec = 0;
  t.tm_isdst = -1;        // Is DST on? 1 = yes, 0 = no, -1 = unknown

  time_t epoch;
  epoch = mktime(&t);

  struct timeval now;
  now.tv_sec = epoch;
  now.tv_usec = 0;

  struct timezone tz = {-330, 0};
  return settimeofday(&now, &tz);

}
