#include "config.h"
#include "custom_parser.h"
#include "JsonListener.h"

enum STATE {NOT_FOUND, FOUND_TASK, FOUND_TIME, FOUND_WEATHER, FOUND_MAIN};
static STATE key_found; 

extern uint8_t task_count;

extern char weather_string[10];
static uint8_t weather_count;

void TodoJsonListener::whitespace(char c) {
}

void TodoJsonListener::startDocument() {
  task_count = 0;
}

void TodoJsonListener::key(String key) {
  if(key.equals("content") && task_count < MAX_TASKS){
    key_found = FOUND_TASK;
  }
}

void TodoJsonListener::value(String value) {
  if(key_found){
    // TODO: Figure out a way to directly print to display
    // Limit to MAX_TODO_STR_LENGTH to display properly on screen
    strncpy((char*)tasks[task_count], (char*)value.c_str(), MAX_TODO_STR_LENGTH);
    tasks[task_count][MAX_TODO_STR_LENGTH+1] = '\0';
    task_count++;
    key_found = NOT_FOUND;
  }
}

void TodoJsonListener::endArray() {
}

void TodoJsonListener::endObject() {
}

void TodoJsonListener::endDocument() {
}

void TodoJsonListener::startArray() {
}

void TodoJsonListener::startObject() {
}

void WeatherJsonListener::whitespace(char c) {
}

void WeatherJsonListener::startDocument() {
  weather_count = 0;
  key_found = NOT_FOUND;
}

void WeatherJsonListener::key(String key) {
    switch(key_found){
      case NOT_FOUND:
        if(key.equals("dt")){
          if(weather_count == (FORECAST_HOURS/3)){
            key_found = FOUND_TIME;
          }
          weather_count++;
        }
        break;
      case FOUND_TIME:
        if(key.equals("weather")){
          key_found = FOUND_WEATHER;
        }
        break;
      case FOUND_WEATHER:
        if(key.equals("main")){
          key_found = FOUND_MAIN;
        }
        break;  
    }
}

void WeatherJsonListener::value(String value) {
  if(key_found == FOUND_MAIN){
    DEBUG.printf("Got weather: %s\n", value);
    strncpy(weather_string, (char*)value.c_str(), 10);
    key_found = NOT_FOUND;
  }
}

void WeatherJsonListener::endArray() {
}

void WeatherJsonListener::endObject() {
}

void WeatherJsonListener::endDocument() {
}

void WeatherJsonListener::startArray() {
}

void WeatherJsonListener::startObject() {
}

size_t ArudinoStreamParser::write(const uint8_t *data, size_t size) {
    
  uint8_t char_val = 0x00;
  
  if(size && data) {
    for (size_t idx=0; idx<size; idx++)
    {
      char_val = *(data + idx);
      parse(char_val);
    }
    return size;    
  }
  
    return 0;
}

size_t ArudinoStreamParser::write(uint8_t data) {
  
  parse(data);
  
  return 1;
}

int ArudinoStreamParser::available() {
    return 1;
}

int ArudinoStreamParser::read() {
    return -1;
}

int ArudinoStreamParser::peek() {
    return -1;
}

void ArudinoStreamParser::flush() {
}
