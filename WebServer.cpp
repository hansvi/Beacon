#include <string.h>
#include <Arduino.h>
#include <Ethernet.h>
#include <SD.h>
#include "Config.h"
#include "Sensors.h"
#include <avr/pgmspace.h>

static byte mac[] = {MAC_ADDRESS};
static IPAddress ip(IP_ADDRESS);
static EthernetServer server(80);
static char HTTP_req[HTTP_REQ_BUF_SZ];    // buffered HTTP request stored as null terminated string
static int HTTP_req_index;                // index into HTTP_req buffer
static boolean HTTP_can_use_gzip;         // Send the gzip-compressed version if browser supports it
static boolean HTTP_is_post_request;      // Config updates are sent with HTTP POST requests
static char HTTP_req_filename[HTTP_REQ_FILENAME_SZ];  // The filename of the URL


/*
 static files are stored on the SD card under the /www/ directory.
 gzip compressed versions are stored in the /wwz/ directory
 /index.htm  - landing page, provides links to each beacon and their running state
 /<N>/index.htm - shows the running state and texts of beacon <N>
 /<N>/setdef.htm?txt=<msg>  - set a new default text
 /<N>/seth00.htm?txt=<msg>  - set a text to be sent at the hour
 /<N>/seth15.htm?txt=<msg>  - set a text to be sent at 15 minutes past the hour
 /<N>/seth30.htm?txt=<msg>  - set a text to be sent at half past the hour
 /<N>/seth45.htm?txt=<msg>  - set a text to be sent at 15 minutes before the hour
 /sensors.txt  - JSON formatted 
*/

static void urldecode2(char *dst, const char *src);
static const char *getMimeType(const char *filename);
static bool httpRespond(EthernetClient &client);
static void httpParseHeaderLine();
static bool send404NotFound(EthernetClient &client, const char* filename);


void WebServerInit()
{
  Ethernet.begin(mac, ip);  // initialize Ethernet device
  server.begin();           // start to listen for clients
}

// For now, just copy the code I used before. If there is a connection, it will be handled in one tick.
// Might need to make a state machine for improved performance (other jobs will be starved while handling a request).
void WebServerTick()
{
  EthernetClient client = server.available();  // try to get client

  if (client)
  {
    Serial.println("Client connected");

    HTTP_can_use_gzip = false;
    HTTP_is_post_request = false;
    HTTP_req_filename[0] = 0;
    
    // got client?
    while (client.connected())
    {
      if(client.available())
      {
        char c = client.read(); // read 1 byte (character) from client
        if(c == '\r')
        {
          continue; // skip \r, next character please. (go to the next iteration of the while loop) 
        }
        // Leave 1 character for the terminating zero
        if (HTTP_req_index < (HTTP_REQ_BUF_SZ-1))
        {
            HTTP_req[HTTP_req_index] = c;          // save HTTP request character
            HTTP_req_index++;
        }
        if(c == '\n')
        {
          HTTP_req[HTTP_req_index]=0; // zero-terminate

          // End of line detected. was it an empty line?
          if(HTTP_req_index==1)  // contains 1 character, i.e. \n
          {
            bool keepalive = httpRespond(client);
            HTTP_req_index = 0;  // reset position to receive next line
            HTTP_can_use_gzip = false;
            HTTP_is_post_request = false;
            HTTP_req_filename[0] = 0;
            if(!keepalive)
              break;
//            break;
          }
          else
          {
            httpParseHeaderLine();
          }

          HTTP_req_index = 0;  // reset position to receive next line
        }
      }
    }
    // No longer conneccted.
    delay(1);      // give the web browser time to receive the data
    client.stop(); // close the connection 
  }
}


/* urldecode2
  URL encoded string to character string
  Source: https://gist.github.com/jmsaavedra/7964251
*/
static void urldecode2(char *dst, const char *src)
{
  char a, b;
  while (*src) {
    if ((*src == '%') &&
      ((a = src[1]) && (b = src[2])) &&
      (isxdigit(a) && isxdigit(b))) {
      if (a >= 'a')
        a -= 'a'-'A';
      if (a >= 'A')
        a -= ('A' - 10);
      else
        a -= '0';
      if (b >= 'a')
        b -= 'a'-'A';
      if (b >= 'A')
        b -= ('A' - 10);
      else
        b -= '0';
      *dst++ = 16*a+b;
      src+=3;
    } 
    else {
      *dst++ = *src++;
    }
  }
  *dst++ = '\0';
}

/* Returns mime type based on filename extension
*/
static const char * getMimeType(const char *filename)
{
  int fn_len = strlen(filename);
  Serial.print("Filename extension: '");
  Serial.print(filename+fn_len-3);
  Serial.println("'");
  if(strncasecmp(filename+fn_len-3, "png", 3)==0)
  {
    return "image/png";
  }
  if(strncasecmp(filename+fn_len-3, "css", 3)==0)
  {
    return "text/css";
  }
  if(strncasecmp(filename+fn_len-2, "js", 2)==0)
  {
    return "text/javascript";
  }
  if(strncasecmp(filename+fn_len-3, "ico", 3)==0)
  {
    return "image/x-icon";
  }
  return "text/html";
}

// Parses one line from the http header 
static void httpParseHeaderLine()
{
  if(strncasecmp(HTTP_req, "GET ", 4)==0)
  {
    // HTTP_req_index is equal to the string length. We need to remove the trailing " HTTP/1.1" though (9 characters)
    if(HTTP_req_index >= 14)  // 4 from "GET ", 10 from " HTTP/1.1" + newline
    {
      HTTP_req_index -= 10;
      HTTP_req[HTTP_req_index] = 0; // Replaces the ' ' with a terminating zero.
      sprintf(HTTP_req_filename, "%s", HTTP_req+4);
    }
    else
    {
      // Weird, GET malformed
      HTTP_req_filename[0] = 0;
    }
  }
  else if(strncasecmp(HTTP_req, "POST ", 5) == 0)
  {
    if(HTTP_req_index >= 15)  // 5 from "POST ", 10 from " HTTP/1.1" + newline
    {
      HTTP_req_index -= 10;
      HTTP_req[HTTP_req_index] = 0; // Replaces the ' ' with a terminating zero.
      sprintf(HTTP_req_filename, "%s", HTTP_req+5);
    }
    else
    {
      // Weird, POST malformed
      HTTP_req_filename[0] = 0;
    }
    HTTP_is_post_request = true;
  }
  else if(strncasecmp(HTTP_req, "Accept-Encoding: ", 17) == 0)
  {
    if(strstr(HTTP_req+17, "gzip"))
    {
      HTTP_can_use_gzip = true;
    }
  }
}

static void sendDynamicHeader(char *frame_buf, EthernetClient &client, const char* mimetype)
{
  // FIXME: Theoretical risk of buffer overflow
  sprintf_P(frame_buf, PSTR("HTTP/1.1 200 OK\r\nContent-Type: %s\r\n\r\n"), mimetype);
  client.write(frame_buf, strlen(frame_buf));
}

/* Header for static files, stored on the filesystem */
static void sendStaticHeader(char *frame_buf, EthernetClient &client, const char* mimetype, unsigned long contentsize, bool gzipped)
{
  // FIXME: Theoretical risk of buffer overflow
  sprintf_P(frame_buf, PSTR("HTTP/1.1 200 OK\r\nContent-Type: %s\r\nContent-Length: %lu\r\nContent-Encoding: gzip\r\n%s\r\n"), mimetype, contentsize,(gzipped? "Content-Encoding: gzip\r\n" : ""));
  client.write(frame_buf, strlen(frame_buf));
}

static void sendSDFile(EthernetClient &client, const char *filename, bool try_gzipped)
{
  char frame_buf[200];
  byte bytes_read;
  File web_file;
  if(try_gzipped)
  {
    sprintf_P(frame_buf, PSTR("/wwwgz/%s"), filename);
    web_file = SD.open(frame_buf, FILE_READ);
  }
  if(!web_file)
  {
    try_gzipped = false;
    sprintf_P(frame_buf, PSTR("/www/%s"), filename);
    web_file = SD.open(frame_buf, FILE_READ);
  }
  if(web_file)
  {
    sendStaticHeader(frame_buf, client, getMimeType(filename), web_file.size(), try_gzipped);
    web_file.setTimeout(0);
    while(web_file.available())
    {
      bytes_read = web_file.readBytes(frame_buf, sizeof(frame_buf));
      client.write(frame_buf, bytes_read);
    }
    web_file.close();
  }
  else
  {
    send404NotFound(client, filename);
    // File not found
  }
}

static bool send404NotFound(EthernetClient &client, const char* filename)
{
  const char *pre  = "<html><header><title>404 File not found</title></header><body><h1>File not found</h1><p>Sorry the file ";
  const char *post  = " was not found on the SD card</<p></body></html>";
  int total = strlen(pre) + strlen(post) + strlen(filename);
  
/*    Serial.print("Could not open ");
    Serial.println(filename);
    Serial.print("Length:");
    Serial.println(strlen(frame_buf));
*/
    client.println(F("HTTP/1.1 404 Not found"));
    client.println(F("Content-Type: text/html"));
    client.print(F("Content-Length: "));
    client.println(total);
    client.println();

    client.print(pre);
    client.print(filename);
    client.print(post);
    return false;
}

static bool sendIndexHtm(EthernetClient &client)
{
  Serial.println("Sending index.htm");
  sendSDFile(client, "index.htm", HTTP_can_use_gzip);
  return true;
}

static bool sendAnalogHtm(EthernetClient &client)
{
  sendSDFile(client, "analog.htm", HTTP_can_use_gzip);
  return true;
}
static bool sendTemperatureHtm(EthernetClient &client)
{
  sendSDFile(client, "temp.htm", HTTP_can_use_gzip);
  return true;
}
static bool sendAnalogJSON(EthernetClient &client)
{
  char frame_buf[250];
  sendDynamicHeader(frame_buf, client, "application/json"); 
  strcpy(frame_buf, "{\"values\": [\"");
  char *ptr = &frame_buf[12];
  for(int i=0; i<NUM_ANALOG_CHANNELS; i++)
  {
    if(i>0)
    {
      *ptr++ = ',';
      *ptr++ = '"';
    }
    readAnalogSensor(ptr, i);
    while(*ptr) 
    {
      ptr++;
    }
    *ptr++ = '"';
  }
  *ptr++ = ']';
  *ptr++ = '}';
  *ptr = 0;
  
  client.print(frame_buf);
  return false;
}

static bool sendtemperatureJSON(EthernetClient &client)
{
  char frame_buf[250];
  // http://stackoverflow.com/questions/477816/what-is-the-correct-json-content-type
  sendDynamicHeader(frame_buf, client, "application/json"); 
  strcpy(frame_buf, "{\"values\": [\"");
  char *ptr = &frame_buf[12];
  for(int i=0; i<NUM_TEMPERATURE_CHANNELS; i++)
  {
    if(i>0)
    {
      *ptr++ = ',';
      *ptr++ = '"';
    }
    readAnalogSensor(ptr, i);
    while(*ptr) 
    {
      ptr++;
    }
    *ptr++ = '"';
    i++;
  }
  *ptr++ = ']';
  *ptr++ = '}';
  *ptr = 0;
  client.print(frame_buf);
  return false;
}


struct WebPage
{
  char* filename;
  bool (*handler)(EthernetClient &client);
};

WebPage rootpages[] =
{
  {"/index.htm", sendIndexHtm},
  {"/", sendIndexHtm},
  {"/analog.htm", sendAnalogHtm},
  {"/temperature.htm", sendTemperatureHtm},
  {"/analog.txt", sendAnalogJSON},
  {"/temperature.txt", sendtemperatureJSON},
  {NULL, NULL}
};

// Send back the requested page
// Return value:
// true: can keep connection open
// false: close connection
static bool httpRespond(EthernetClient &client)
{
  Serial.print("got request ");
  Serial.println(HTTP_req_filename);
  delay(1000);
  if(HTTP_is_post_request)
  {
    // Continue reading data in the post request.
    // TODO NYI
    return false;
  }
  else
  {
    // send web page
    WebPage *page;
    for(page = rootpages; page->filename; page++)
    {
      // can be optimized by sorting/binary search
      if(strcasecmp(page->filename, HTTP_req_filename)==0)
        break;
    }
    if(page->filename)
    {
      return page->handler(client);
    }
    else
    {
      Serial.println("404 not found");
      return send404NotFound(client, HTTP_req_filename);
    }
  }
}


