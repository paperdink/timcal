
#ifndef CUSTOM_PARSER_H
#define CUSTOM_PARSER_H

#include "JsonStreamingParser.h"
#include "JsonListener.h"
#include "Print.h"
#include "Stream.h"

/* Convert the Json data into an Arduino Stream */
class ArudinoStreamParser: public Stream, public JsonStreamingParser  {
  /* https://github.com/mrfaptastic/json-streaming-parser2 */
public:
    size_t write(const uint8_t *buffer, size_t size) override;
    size_t write(uint8_t data) override;

    int available() override;
    int read() override;
    int peek() override;
    void flush() override;
};

class TodoJsonListener: public JsonListener {

  public:
    virtual void whitespace(char c);
  
    virtual void startDocument();

    virtual void key(String key);

    virtual void value(String value);

    virtual void endArray();

    virtual void endObject();

    virtual void endDocument();

    virtual void startArray();

    virtual void startObject();
};

class WeatherJsonListener: public JsonListener {

  public:
    virtual void whitespace(char c);
  
    virtual void startDocument();

    virtual void key(String key);

    virtual void value(String value);

    virtual void endArray();

    virtual void endObject();

    virtual void endDocument();

    virtual void startArray();

    virtual void startObject();
};

#endif /* CUSTOM_PARSER_H */
