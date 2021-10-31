
#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <SPIFFS.h>
#include <FS.h>
#include <SD.h>
#include <WiFiClientSecure.h>

#include "driver/adc.h"
#include "PCF8574.h"

#include "config.h"
#include "WiFiManager.h"
#include "date_time.h"
#include "custom_parser.h"

#define seekSet seek

static const uint16_t input_buffer_pixels = 20; // may affect performance
static const uint16_t max_palette_pixels = 256; // for depth <= 8
uint8_t input_buffer[3 * input_buffer_pixels]; // up to depth 24
uint8_t mono_palette_buffer[max_palette_pixels / 8]; // palette buffer for depth <= 8 b/w
uint8_t color_palette_buffer[max_palette_pixels / 8]; // palette buffer for depth <= 8 c/w

uint16_t read16(File& f)
{
  // BMP data is stored little-endian, same as Arduino.
  uint16_t result;
  ((uint8_t *)&result)[0] = f.read(); // LSB
  ((uint8_t *)&result)[1] = f.read(); // MSB
  return result;
}

uint32_t read32(File& f)
{
  // BMP data is stored little-endian, same as Arduino.
  uint32_t result;
  ((uint8_t *)&result)[0] = f.read(); // LSB
  ((uint8_t *)&result)[1] = f.read();
  ((uint8_t *)&result)[2] = f.read();
  ((uint8_t *)&result)[3] = f.read(); // MSB
  return result;
}

void drawBitmapFrom_SD_ToBuffer(GxEPD_Class* display, fs::FS &fs, const char *filename, int16_t x, int16_t y, bool with_color)
{
  File file;
  bool valid = false; // valid format to be handled
  bool flip = true; // bitmap is stored bottom-to-top
  uint32_t startTime = millis();
  if ((x >= display->width()) || (y >= display->height())) return;
  DEBUG.println();
  DEBUG.print("Loading image '");
  DEBUG.print(filename);
  DEBUG.println('\'');

  file = fs.open(String("/") + filename, FILE_READ);
  if (!file)
  {
    DEBUG.print("File not found");
    return;
  }

  // Parse BMP header
  if (read16(file) == 0x4D42) // BMP signature
  {
    uint32_t fileSize = read32(file);
    uint32_t creatorBytes = read32(file);
    uint32_t imageOffset = read32(file); // Start of image data
    uint32_t headerSize = read32(file);
    uint32_t width  = read32(file);
    uint32_t height = read32(file);
    uint16_t planes = read16(file);
    uint16_t depth = read16(file); // bits per pixel
    uint32_t format = read32(file);
    if ((planes == 1) && ((format == 0) || (format == 3))) // uncompressed is handled, 565 also
    {
      DEBUG.print("File size: "); DEBUG.println(fileSize);
      DEBUG.print("Image Offset: "); DEBUG.println(imageOffset);
      DEBUG.print("Header size: "); DEBUG.println(headerSize);
      DEBUG.print("Bit Depth: "); DEBUG.println(depth);
      DEBUG.print("Image size: ");
      DEBUG.print(width);
      DEBUG.print('x');
      DEBUG.println(height);
      // BMP rows are padded (if needed) to 4-byte boundary
      uint32_t rowSize = (width * depth / 8 + 3) & ~3;
      if (depth < 8) rowSize = ((width * depth + 8 - depth) / 8 + 3) & ~3;
      if (height < 0)
      {
        height = -height;
        flip = false;
      }
      uint16_t w = width;
      uint16_t h = height;
      if ((x + w - 1) >= display->width())  w = display->width()  - x;
      if ((y + h - 1) >= display->height()) h = display->height() - y;
      valid = true;
      uint8_t bitmask = 0xFF;
      uint8_t bitshift = 8 - depth;
      uint16_t red, green, blue;
      bool whitish, colored;
      if (depth == 1) with_color = false;
      if (depth <= 8)
      {
        if (depth < 8) bitmask >>= depth;
        file.seekSet(54); //palette is always @ 54
        for (uint16_t pn = 0; pn < (1 << depth); pn++)
        {
          blue  = file.read();
          green = file.read();
          red   = file.read();
          file.read();
          whitish = with_color ? ((red > 0x80) && (green > 0x80) && (blue > 0x80)) : ((red + green + blue) > 3 * 0x80); // whitish
          colored = (red > 0xF0) || ((green > 0xF0) && (blue > 0xF0)); // reddish or yellowish?
          if (0 == pn % 8) mono_palette_buffer[pn / 8] = 0;
          mono_palette_buffer[pn / 8] |= whitish << pn % 8;
          if (0 == pn % 8) color_palette_buffer[pn / 8] = 0;
          color_palette_buffer[pn / 8] |= colored << pn % 8;
        }
      }
      //display->fillScreen(GxEPD_WHITE);
      uint32_t rowPosition = flip ? imageOffset + (height - h) * rowSize : imageOffset;
      for (uint16_t row = 0; row < h; row++, rowPosition += rowSize) // for each line
      {
        uint32_t in_remain = rowSize;
        uint32_t in_idx = 0;
        uint32_t in_bytes = 0;
        uint8_t in_byte = 0; // for depth <= 8
        uint8_t in_bits = 0; // for depth <= 8
        uint16_t color = GxEPD_BLACK;
        file.seekSet(rowPosition);
        for (uint16_t col = 0; col < w; col++) // for each pixel
        {
          // Time to read more pixel data?
          if (in_idx >= in_bytes) // ok, exact match for 24bit also (size IS multiple of 3)
          {
            in_bytes = file.read(input_buffer, in_remain > sizeof(input_buffer) ? sizeof(input_buffer) : in_remain);
            in_remain -= in_bytes;
            in_idx = 0;
          }
          switch (depth)
          {
            case 24:
              blue = input_buffer[in_idx++];
              green = input_buffer[in_idx++];
              red = input_buffer[in_idx++];
              whitish = with_color ? ((red > 0x80) && (green > 0x80) && (blue > 0x80)) : ((red + green + blue) > 3 * 0x80); // whitish
              colored = (red > 0xF0) || ((green > 0xF0) && (blue > 0xF0)); // reddish or yellowish?
              break;
            case 16:
              {
                uint8_t lsb = input_buffer[in_idx++];
                uint8_t msb = input_buffer[in_idx++];
                if (format == 0) // 555
                {
                  blue  = (lsb & 0x1F) << 3;
                  green = ((msb & 0x03) << 6) | ((lsb & 0xE0) >> 2);
                  red   = (msb & 0x7C) << 1;
                }
                else // 565
                {
                  blue  = (lsb & 0x1F) << 3;
                  green = ((msb & 0x07) << 5) | ((lsb & 0xE0) >> 3);
                  red   = (msb & 0xF8);
                }
                whitish = with_color ? ((red > 0x80) && (green > 0x80) && (blue > 0x80)) : ((red + green + blue) > 3 * 0x80); // whitish
                colored = (red > 0xF0) || ((green > 0xF0) && (blue > 0xF0)); // reddish or yellowish?
              }
              break;
            case 1:
            case 4:
            case 8:
              {
                if (0 == in_bits)
                {
                  in_byte = input_buffer[in_idx++];
                  in_bits = 8;
                }
                uint16_t pn = (in_byte >> bitshift) & bitmask;
                whitish = mono_palette_buffer[pn / 8] & (0x1 << pn % 8);
                colored = color_palette_buffer[pn / 8] & (0x1 << pn % 8);
                in_byte <<= depth;
                in_bits -= depth;
              }
              break;
          }
          if (whitish)
          {
            color = GxEPD_WHITE;
          }
          else if (colored && with_color)
          {
            color = GxEPD_RED;
          }
          else
          {
            color = GxEPD_BLACK;
          }
          uint16_t yrow = y + (flip ? h - row - 1 : row);
          display->drawPixel(x + col, yrow, color);
        } // end pixel
      } // end line
      DEBUG.print("loaded in "); DEBUG.print(millis() - startTime); DEBUG.println(" ms");
    }
  }
  file.close();
  if (!valid)
  {
    DEBUG.println("bitmap format not handled.");
  }
}

// Display APIs
// display config page
void display_config_gui(GxEPD_Class* display){
  int16_t  x, y;
  uint16_t w, h;
  uint8_t prev_height;

  uint16_t config_base_x = (display->width()/2);
  uint16_t config_base_y = (display->height()/2);
  display->fillScreen(GxEPD_WHITE);
  display->setRotation(0);
  
  display->setFont(LARGE_FONT);
  display->setTextSize(1);
  display->setTextColor(GxEPD_BLACK);

  display->getTextBounds(F("Hi!"), 0, 0, &x, &y, &w, &h);
  display->setCursor(config_base_x-(w/2),config_base_y-(h/2));
  display->println(F("Hi!"));
  prev_height = h;

  display->setFont(MED_FONT);
  display->getTextBounds(F("Let's set up your paperd.ink"), 0, 0, &x, &y, &w, &h);
  display->setCursor(config_base_x-(w/2), config_base_y-(h/2)+prev_height);
  display->println(F("Let's set up your paperd.ink"));
  prev_height += (display->height()/2)-(h/2);
  
  display->getTextBounds(F("Press reset and connect to"), 0, 0, &x, &y, &w, &h);
  display->setCursor(config_base_x-(w/2), prev_height+h+10);
  display->print(F("Press reset and connect to"));
  prev_height += h;
  
  display->getTextBounds(F("paperd.ink_0000000000"), 0, 0, &x, &y, &w, &h);
  display->setCursor(config_base_x-(w/2), prev_height+h+10);
  display->print(F("paperd.ink_"));
  display->println(String(ESP_getChipId()));

  display->update();
  delay(2000);
  display->updateWindow(0, 0, GxEPD_WIDTH, GxEPD_HEIGHT, false);
}


// Fetch tasks
RTC_DATA_ATTR char tasks[MAX_TASKS][MAX_TODO_STR_LENGTH+1];
uint8_t task_count;
String todoist_token_base = "Bearer ";
const char* todoist_url = "https://api.todoist.com/rest/v1/tasks";
TodoJsonListener todo_listener;

int8_t fetch_todo(){
  //TODO: Handle failures
  int httpCode;
  HTTPClient https;
  WiFiClientSecure *client = new WiFiClientSecure;

  // Example of how to make a HTTP request with more control
  DEBUG.println("Starting connection to server...");
  
  https.begin(*client, todoist_url);
  String todoist_token = todoist_token_base + todoist_token_string;
  https.addHeader("Authorization", todoist_token.c_str(), 0, 0);
  httpCode = https.GET();
  
  if(httpCode == HTTP_CODE_OK) {
    DEBUG.printf("[HTTP] GET SUCCESS\n");
    ArudinoStreamParser parser;
    parser.setListener(&todo_listener);
    https.writeToStream(&parser);
  } else {
    DEBUG.printf("[HTTP] GET... failed, error: %s\n", https.errorToString(httpCode).c_str());
  }
  
  https.end();
  
  return 0;
}

// display to-do list
void display_tasks(GxEPD_Class* display){
  uint8_t todo_items_num = MAX_TASKS;    
  int16_t  x1, y1;
  uint16_t w, h;

  display->setFont(MED_FONT);
  display->setTextColor(GxEPD_WHITE);
  int16_t td_list_head_base_y = 220;
  int16_t td_list_base_x = 115;
  display->setTextSize(1);

  display->setCursor(td_list_base_x, td_list_head_base_y);
  display->println("To-Do List");
  display->getTextBounds("To-Do List", td_list_base_x, td_list_head_base_y, &x1, &y1, &w, &h);
  uint16_t td_list_base_y = td_list_head_base_y + 5;
  display->setFont(SMALL_FONT);
  display->getTextBounds("item MAX_TODO_LENGTH", td_list_base_x, td_list_head_base_y, &x1, &y1, &w, &h); // dummy item to get font height and MAX_TODO_LENGTH width

  uint16_t y = td_list_base_y;
  uint16_t x = td_list_base_x;
  uint8_t row = 1;
  for(uint8_t i = 0; i<task_count; i++){
    if(tasks[i] != NULL){
      y = td_list_base_y +(row * h) + (row * 3);
      if(y > display->height()){
        row = 1;
        y = td_list_base_y + (row * h) + (row * 3);
        x += w;
      }
      display->setCursor(x,y);
      display->println(tasks[i]);
      row++;
    }
  }
}

// fetch weather
RTC_DATA_ATTR char weather_icon[15] = "Error.bmp";
char weather_string[10];
WeatherJsonListener weather_listener;
String openweathermap_link_base = "http://api.openweathermap.org/data/2.5/forecast?q={p},{c}&appid={i}";

const char* fetch_weather(){
  HTTPClient http;

  openweathermap_link_base.replace("{p}", city_string);
  openweathermap_link_base.replace("{c}", country_string);
  openweathermap_link_base.replace("{i}", openweather_appkey_string);
  
  DEBUG.printf("Openweathermap link: %s \n",openweathermap_link_base.c_str());
  http.begin(openweathermap_link_base.c_str());
  int httpCode = http.GET();
  
  if(httpCode == HTTP_CODE_OK) {
    DEBUG.printf("[HTTP] GET SUCCESS\n");
    ArudinoStreamParser parser;
    parser.setListener(&weather_listener);
    //String payload = http.getString();
    //DEBUG.println(payload);
    http.writeToStream(&parser);
  }else {
    DEBUG.printf("[HTTP] GET... failed, error: %s\n", http.errorToString(httpCode).c_str());
    http.end(); //Free the resources
    return "Error.bmp";
  }
  http.end(); //Free the resources

  DEBUG.printf("Weather is %s\n", weather_string);
  
  if(!strcmp(weather_string,"Drizzle") || !strcmp(weather_string,"Rain") || !strcmp(weather_string,"Thunderstorm")){
    DEBUG.println("Displaying Drizzle.bmp");
    return "Drizzle.bmp";
  }else if(!strcmp(weather_string,"Clouds")){
    DEBUG.println("Displaying Clouds.bmp");
    return "Clouds.bmp";
  }else if(!strcmp(weather_string,"Clear")){
    DEBUG.println("Displaying Clear.bmp");
    return "Clear.bmp";
  }else{
    return "Error.bmp";
  }
}

void display_weather(GxEPD_Class* display, const char* icon){
  int16_t weather_base_y = 135;
  int16_t weather_base_x = 25;
  if(wifi_update || first_boot == 1){
    //update every 6 hours
    display->fillRect(weather_base_x - 5, weather_base_y - 5, 55 + 5, weather_base_y + 55 + 5, GxEPD_WHITE);
    if(icon != NULL){
      strcpy(weather_icon,icon);
    }
  }
  drawBitmapFrom_SD_ToBuffer(display, SPIFFS, weather_icon, weather_base_x, weather_base_y, 0);
}

void display_time(GxEPD_Class* display){
  int16_t  x1, y1;
  uint16_t w, h;
  
  //display time
  display->setFont(LARGE_FONT);
  display->setTextSize(1);
  display->setTextColor(GxEPD_BLACK);
  int16_t time_base_y = 60;
  int16_t time_base_x = 25;
  display->getTextBounds("03", time_base_x, time_base_y, &x1, &y1, &w, &h); // 03 is arbitrary text to get the height and width
  display->fillRect(time_base_x - 10, time_base_y - h - 10, w + 15, time_base_y + h + 10, GxEPD_WHITE);

  display->setCursor(time_base_x, time_base_y);
  if (now.hour < 10) {
    display->print("0");
    display->print(now.hour);
  } else {
    display->println(now.hour);
  }

  display->setCursor(time_base_x, time_base_y + h + 10);
  if (now.min < 10) {
    display->print("0");
    display->print(now.min);
  } else {
    display->println(now.min);
  }
}

void display_calender(GxEPD_Class* display){
  // display calender
  int16_t  x1, y1;
  uint16_t w, h;
  display->setFont(MED_FONT);
  display->setTextSize(1);
  display->setTextColor(GxEPD_BLACK);
  int16_t calender_base_y = 40;
  int16_t calender_base_x = 120;
  display->setCursor(calender_base_x, calender_base_y);
  display->println("Mon   Tue   Wed   Thu   Fri   Sat   Sun");
  display->getTextBounds("Mon   Tue   Wed   Thu   Fri   Sat   Sun", calender_base_x, calender_base_y, &x1, &y1, &w, &h);
  uint8_t num_offset, print_valid = 0;
  uint8_t day = 1;
  for (uint8_t j = 0; j <= 5; j++) {
    for (uint8_t i = 1; i <= 7 && day <= 31; i++) {
      // you can hack around with this value to align your text properly based on what font, font size etc you are using
      num_offset = 21; // 21 is what works for me for the first 2 columns
      if (i >= 3 && i <= 7) {
        num_offset = 17;    // then i need to reduce to 17
      }
      if (j == 0 && i == now.day_offset) {
        // start from the offset in the month, ie which day does 1st of the month lie on
        print_valid = 1;
      }
      if (print_valid) {
        display->setCursor(calender_base_x + (i * (w / 7)) - num_offset, calender_base_y + ((j + 1)*h) + ((j + 1) * 7));
        if (day == now.mday) {
          char str[3];
          sprintf(str, "%d", day);
          int16_t  x2, y2;
          uint16_t w2, h2;
          display->getTextBounds(str, calender_base_x + (i * (w / 7)) - num_offset, calender_base_y + ((j + 1)*h) + ((j + 1) * 7), &x2, &y2, &w2, &h2);
          display->fillRect(x2 - 4, y2 - 4, w2 + 8, h2 + 8, GxEPD_BLACK);
          display->setTextColor(GxEPD_WHITE);
        } else {
          display->setTextColor(GxEPD_BLACK);
        }
        // once the offset is reached, start incrementing
        display->println(day);
        day += 1;
      }
    }
  }

  // display day
  display->setTextColor(GxEPD_WHITE);
  display->setFont(LARGE_FONT);
  display->setTextSize(1);
  display->setCursor(33, 250);
  display->println(now.mday);

  // display month
  display->setFont(MED_FONT);
  display->setTextSize(2);
  display->setCursor(30, 290);
  display->println(now.month);
}

// display battery status
void display_battery(GxEPD_Class* display, float batt_voltage, uint8_t not_charging){
  int16_t batt_base_y = 3;
  int16_t batt_base_x = 324;
    
  DEBUG.printf("Battery charging: %d\n",not_charging);
  if (not_charging) {
      //not charging
      drawBitmapFrom_SD_ToBuffer(display, SPIFFS, "Battery.bmp", batt_base_x, batt_base_y, 0);
  }else{
      //charging
      drawBitmapFrom_SD_ToBuffer(display, SPIFFS, "Charging.bmp", batt_base_x, batt_base_y, 0);
  }
  
  DEBUG.printf("Battery voltage: %fV\n",batt_voltage);
  display->setCursor(batt_base_x+28, batt_base_y+7);
  display->setFont(SMALL_FONT);
  display->setTextSize(1);
  display->setTextColor(GxEPD_BLACK);
  display->print(batt_voltage);
  display->print("V");
  display->fillRect(batt_base_x+3, batt_base_y+3, 13*(batt_voltage/4.2), 8, GxEPD_BLACK);

}

void display_wifi(GxEPD_Class* display, uint8_t status){
  int16_t batt_base_y = 1;
  int16_t batt_base_x = 295;
  if(status == 1){
    drawBitmapFrom_SD_ToBuffer(display, SPIFFS, "Wifi.bmp", batt_base_x, batt_base_y, 0);
  }else{
    drawBitmapFrom_SD_ToBuffer(display, SPIFFS, "Wifi_off.bmp", batt_base_x, batt_base_y, 0);
  }
}

void display_background(GxEPD_Class* display){
  // fill background
  display->fillRect(0, (display->height() / 3) * 2, display->width(), (display->height() / 3), GxEPD_BLACK);
  display->fillRect((display->width() / 4), 0, 5, (display->height() / 3) * 2, GxEPD_BLACK);
  display->fillRect((display->width() / 4), (display->height() / 3) * 2, 5, (display->height() / 3), GxEPD_WHITE);
}

void display_update(GxEPD_Class* display){
   if (!(now.min % 5) || first_boot == 1) {
    // Full update once every 5 mins and on the first boot
    display->update();
    delay(2000);
    display->updateWindow(0, 0, GxEPD_WIDTH, GxEPD_HEIGHT, false);
  }else{
    display->updateWindow(0, 0, GxEPD_WIDTH, GxEPD_HEIGHT, false);
    //display->updateWindow(time_base_x - 10, time_base_y - h - 10, w + 15, time_base_y + h + 10, false);
  }
}
