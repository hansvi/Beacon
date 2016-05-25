#include <string.h>
#include <Arduino.h>
#include <Ethernet.h>
#include <SD.h>
#include "Config.h"
#include "Sensors.h"
#include "ControlPanel.h"
#include <avr/pgmspace.h>

static byte mac[] = {MAC_ADDRESS};
static IPAddress ip(IP_ADDRESS);
static EthernetServer server(80);
static char HTTP_req[HTTP_REQ_BUF_SZ];    // buffered HTTP request stored as null terminated string
static int HTTP_req_index;                // index into HTTP_req buffer
static boolean HTTP_can_use_gzip;         // Send the gzip-compressed version if browser supports it
static boolean HTTP_is_post_request;      // Config updates are sent with HTTP POST requests
static char HTTP_req_filename[HTTP_REQ_FILENAME_SZ];  // The filename of the URL
static int HTTP_req_content_length;

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

struct BeaconSettings
{
  bool enabled;
  char text[BEACON_MESSAGE_LENGTH];
  int textid;
};

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
    HTTP_req_index = 0;
    HTTP_can_use_gzip = false;
    HTTP_is_post_request = false;
    HTTP_req_filename[0] = 0;
    HTTP_req_content_length = 0;
    
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
            HTTP_req_content_length = 0;
            if(!keepalive)
              break;
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
    else if(*src=='+') // modified this, for parsing POST requests
    {
      *dst++= ' ';
      src++;
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
  Serial.print("> ");
  Serial.print(HTTP_req);
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
  else if(strncasecmp(HTTP_req, "Content-Length: ", 16) == 0)
  {
    HTTP_req_content_length = atoi(HTTP_req+16);
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
  sprintf_P(frame_buf, PSTR("HTTP/1.1 200 OK\r\nContent-Type: %s\r\nContent-Length: %lu\r\n%s\r\n"), mimetype, contentsize,(gzipped? "Content-Encoding: gzip\r\n" : ""));
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

/*!
 * Send a raw CSV log file
 *
 * \param client    connection to web browser
 * \param filename  log file to send
 *
 * \returns true to keep connectio open, false to close it.
 */
static bool sendSDLogFile(EthernetClient &client, const char *filename)
{
  char frame_buf[200];
  byte bytes_read;
  File web_file;
  web_file = SD.open(filename, FILE_READ);
  if(web_file)
  {
    sendStaticHeader(frame_buf, client, "text/csv", web_file.size(), false);
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
  return false;
}

/*
 * Given a directory like "/log/2016/05/", generate a html file containing the list of log files.
 *
 * \param client    connection to web browser
 * \param filename  directory to open
 *
 * \returns true to keep connectio open, false to close it.
 */
static bool sendLogDays(EthernetClient &client, const char *filename)
{
  const char *pre = "<html><head><title>Log entries</title></head><body><ul>";
  const char *post = "</ul></body></html>";
  char frame_buf[241];
  char *ptr = frame_buf;
  byte file_count=0;
  File directory, entry;

  directory = SD.open(filename);
  if(directory && directory.isDirectory())
  {
    sendDynamicHeader(frame_buf, client, "text/html");
    client.write(pre, strlen(pre));
    
    for(entry=directory.openNextFile(); entry; entry=directory.openNextFile())
    {
      // 24 chars per entry + 2 filenames (8.3) so 48 characters per entry max.
      // This implies we can concatenate up to 5 entries and send them as one frame
      sprintf(ptr, "<li><a href=\"%s%s\">%s</a></li>", entry.name(), (entry.isDirectory() ? "/" : ""), entry.name());
      file_count++;
      if(file_count < 5)
      {
        ptr += strlen(ptr);
      }
      else
      {
        client.write(frame_buf, strlen(frame_buf));
        file_count=0;
        ptr = frame_buf;
      }
      entry.close();
    }
    if(file_count) // send the remainder
    {
      client.write(frame_buf, strlen(frame_buf));
    }
    client.write(post, strlen(post));
  }
  else
  {
    send404NotFound(client, filename);
    // File not found
  }
  return false;
}

static bool sendLogMonths(EthernetClient &client, const char *filename)
{
  return sendLogDays(client, filename);
}

static bool sendLogYears(EthernetClient &client, const char *filename)
{
  return sendLogDays(client, filename);
}

static bool send404NotFound(EthernetClient &client, const char* filename)
{
  const char *pre  = "<html><header><title>404 File not found</title></header><body><h1>File not found</h1><p>Sorry the file ";
  const char *post  = " was not found on the SD card</<p></body></html>";
  int total = strlen(pre) + strlen(post) + strlen(filename);
  
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
  sendSDFile(client, "index.htm", HTTP_can_use_gzip);
  return false;
}

static bool sendAnalogHtm(EthernetClient &client)
{
  sendSDFile(client, "analog.htm", HTTP_can_use_gzip);
  return false;
}
static bool sendTemperatureHtm(EthernetClient &client)
{
  sendSDFile(client, "temp.htm", HTTP_can_use_gzip);
  return true;
}
static bool sendFavicon(EthernetClient &client)
{
  sendSDFile(client, "favicon.ico", HTTP_can_use_gzip);
  return true;
}

static bool sendAnalogJSON(EthernetClient &client)
{
  char frame_buf[250];
  sendDynamicHeader(frame_buf, client, "application/json"); 
  char *ptr = frame_buf;
  *ptr++ = '[';
  *ptr++ = '"';
  for(int i=0; i<NUM_ANALOG_CHANNELS; i++)
  {
    if(i>0)
    {
      *ptr++ = ',';
      *ptr++ = '"';
    }
    ptr += readAnalogSensor(ptr, i);
    *ptr++ = '"';
  }
  *ptr++ = ']';
  *ptr = 0;
  
  client.print(frame_buf);
  return false;
}

static bool sendTemperatureJSON(EthernetClient &client)
{
  char frame_buf[250];
  // http://stackoverflow.com/questions/477816/what-is-the-correct-json-content-type
  sendDynamicHeader(frame_buf, client, "application/json"); 
  char *ptr = frame_buf;
  *ptr++='[';
  *ptr++='"';
  
  for(int i=0; i<NUM_TEMPERATURE_CHANNELS; i++)
  {
    if(i>0)
    {
      *ptr++ = ',';
      *ptr++ = '"';
    }
    ptr += readTemperatureSensor(ptr, i);
    *ptr++ = '"';
  }
  *ptr++ = ']';
  *ptr = 0;
  client.print(frame_buf);
  return false;
}

bool sendRunningJSON(EthernetClient &client)
{
  char frame_buf[200];
  // http://stackoverflow.com/questions/477816/what-is-the-correct-json-content-type
  sendDynamicHeader(frame_buf, client, "application/json"); 
  char *ptr = frame_buf;
  *ptr++='[';
  for(int i=0; i<BEACON_COUNT; i++)
  {
    if(i>0)
    {
      *ptr++ = ',';
    }
    if(isBeaconRunning(i))
    {
      *ptr++ = '1';
    }
    else
    {
      *ptr++='0';
    }
  }
  *ptr++ = ']';
  *ptr = 0;
  client.print(frame_buf);
  return false;
}

// TODO Could add the beacon nr to all pages and get rid of code duplication
struct WebPage
{
  char* filename;
  bool (*handler)(EthernetClient &client);
};

struct BeaconPage
{
  char* filename;
  bool (*handler)(EthernetClient &client, int beacon_nr);
};

// Pages at the root of the website, ie http://a.b.c.d/file.ext
WebPage rootpages[] =
{
  {"/index.htm", sendIndexHtm},
  {"/", sendIndexHtm},
  {"/analog.htm", sendAnalogHtm},
  {"/temperature.htm", sendTemperatureHtm},
  {"/analog.txt", sendAnalogJSON},
  {"/temperature.txt", sendTemperatureJSON},
  {"/running.txt", sendRunningJSON},
  {"/favicon.ico", sendFavicon},
  {NULL, NULL}
};
// TODO: /favicon.ico

// WIP
// TODO: Make messages enable setting in controller
// TODO: urldecode & verify incoming data.
static void parsePostParam(char *text, int beacon_nr, BeaconSettings *settings)
{
  if(strncasecmp(text, "enabled=", 8) == 0)
  {
    settings->enabled = true;
    // enable sending this?
  }
  else if(strncasecmp(text, "msg=", 4) == 0)
  {
    if(strlen(text+4) < BEACON_MESSAGE_LENGTH)
    {
      urldecode2(settings->text, text+4);
      // new beacon text
    }
  }
  else if(strncasecmp(text, "textid=", 7) == 0)
  {
    if(strncasecmp(text+7, "def", 3)==0)
    {
      settings->textid = 4;
    }
    else if(strncasecmp(text+7, "H00", 3)==0)
    {
      settings->textid = 0;
    }
    else if(strncasecmp(text+7, "H15", 3)==0)
    {
      settings->textid = 1;
    }
    else if(strncasecmp(text+7, "H30", 3)==0)
    {
      settings->textid = 2;
    }
    else if(strncasecmp(text+7, "H45", 3)==0)
    {
      settings->textid = 3;
    }
  }
}

static void sendMessageForm(char *frame_buf, EthernetClient &client, const char *textid, const char *title, const char *content, bool enabled)
{
  sprintf_P(frame_buf, PSTR("<form action=\"index.htm\" method=\"POST\"><fieldset><legend>%s:</legend>\r\n"), title);
  client.write(frame_buf, strlen(frame_buf));
  sprintf_P(frame_buf, PSTR("<input type=\"checkbox\" name=\"enabled\" %s> Enabled<br/>"), (enabled ? " checked" : ""));
  client.write(frame_buf, strlen(frame_buf));
  sprintf_P(frame_buf, PSTR("<input type=\"text\" name=\"msg\" value=\"%s\">\r\n"), content);
  client.write(frame_buf, strlen(frame_buf));
  sprintf_P(frame_buf, PSTR("<input type=\"hidden\" name=\"textid\" value=\"%s\"><input type=\"submit\" value=\"Update\"></fieldset></form>\r\n"), textid);
  client.write(frame_buf, strlen(frame_buf));
}

static void sendBeaconSettingsPage(char *frame_buf, EthernetClient &client, int beacon_nr)
{
  char beacon_text[BEACON_MESSAGE_LENGTH];
  sendDynamicHeader(frame_buf, client, "text/html");
  const char *pre  = "<html><header><title>Beacon Settings</title></header><body><h1>";
  const char *post  = "<h1><a href=\"/index.htm\">Back to main page</a></h1></body></html>";
  client.write(pre, strlen(pre));
  sprintf(frame_buf, "<h1>beacon number %d <a href=\"index.htm\">(refresh page)</a></h1>", beacon_nr);
  client.write(frame_buf, strlen(frame_buf));
  
  getBeaconMessage(beacon_nr, BEACON_DEFMSG, beacon_text, BEACON_MESSAGE_LENGTH);
  sendMessageForm(frame_buf, client, "def", "Default text", beacon_text, isBeaconMessageEnabled(beacon_nr, BEACON_DEFMSG));

  getBeaconMessage(beacon_nr, BEACON_H00MSG, beacon_text, BEACON_MESSAGE_LENGTH);
  sendMessageForm(frame_buf, client, "H00", "Message at hh:00", beacon_text, isBeaconMessageEnabled(beacon_nr, BEACON_H00MSG));

  getBeaconMessage(beacon_nr, BEACON_H15MSG, beacon_text, BEACON_MESSAGE_LENGTH);
  sendMessageForm(frame_buf, client, "H15", "Message at hh:15", beacon_text, isBeaconMessageEnabled(beacon_nr, BEACON_H15MSG));
  
  getBeaconMessage(beacon_nr, BEACON_H30MSG, beacon_text, BEACON_MESSAGE_LENGTH);
  sendMessageForm(frame_buf, client, "H30", "Message at hh:30", beacon_text, isBeaconMessageEnabled(beacon_nr, BEACON_H00MSG));

  getBeaconMessage(beacon_nr, BEACON_H45MSG, beacon_text, BEACON_MESSAGE_LENGTH);
  sendMessageForm(frame_buf, client, "H45", "Message at hh:45", beacon_text, isBeaconMessageEnabled(beacon_nr, BEACON_H00MSG));

  client.write(post, strlen(post));
}

static bool processPostRequest(EthernetClient &client, int beacon_nr)
{
  int i=0;
  char frame_buf[250];
  struct BeaconSettings settings;
  settings.enabled = false;
  settings.text[0] = 0;
  settings.textid = -1;
  char new_msg[BEACON_MESSAGE_LENGTH];
  Serial.print("content-length ");
  Serial.print(HTTP_req_content_length);
  while(client.available() && HTTP_req_content_length-- && (i<249))
  {
    char c = client.read(); // read 1 byte (character) from client
    if(c=='&')
    {
      frame_buf[i]=0;
      parsePostParam(frame_buf, beacon_nr, &settings);
      i=0;
    }
    else
    {
      frame_buf[i++]=c;
    }
  }
  if(i)
  {
    frame_buf[i]=0;
    parsePostParam(frame_buf, beacon_nr, &settings);
  }
  
  if((settings.textid>=0) && (settings.textid<5))
  {
    setBeaconMessage(beacon_nr, settings.textid, settings.text);
    setBeaconMessageEnabled(beacon_nr, settings.textid, settings.enabled);
  }
  // The POST request has been parsed. Let's do a sanity check, update the configuration and send back the page.
  sendBeaconSettingsPage(frame_buf, client, beacon_nr);
  return false;
}

static bool sendBeaconIndexHtm(EthernetClient & client, int beacon_nr)
{
  sendSDFile(client, "beacon.htm", HTTP_can_use_gzip);
  return true;
}


// Beacon-specific pages, ie http://a.b.c.d/<beacon-nr>/file.ext
BeaconPage beaconpages[] =
{
  {"/index.htm", processPostRequest},
  {"/", processPostRequest},
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
  if(1)
  {
    if( (HTTP_req_filename[0]=='/') &&
        isdigit(HTTP_req_filename[1]) &&
        (HTTP_req_filename[2]=='/') )
    {
      BeaconPage *page;
      for(page=beaconpages; page->filename; page++)
      {
        if(strcasecmp(page->filename, HTTP_req_filename+2)==0)
        {
          break;
        }
      }
      if(page->handler)
      {
        return page->handler(client, HTTP_req_filename[1]-'0');
      }
      else
      {
        return send404NotFound(client, HTTP_req_filename);
      }
    }
    else if(strncasecmp(HTTP_req_filename, "/log/", 5) == 0)
    {
      // check year in URL
      if(isdigit(HTTP_req_filename[5]) &&
         isdigit(HTTP_req_filename[6]) &&
         isdigit(HTTP_req_filename[7]) &&
         isdigit(HTTP_req_filename[8]))
      {
        // Check month in URL
        if((HTTP_req_filename[9]=='/') &&
           isdigit(HTTP_req_filename[10]) &&
           isdigit(HTTP_req_filename[11]))
        {
          // Check day in URL
          if((HTTP_req_filename[12]=='/') &&
             isdigit(HTTP_req_filename[13]) &&
             isdigit(HTTP_req_filename[14]) &&
             (HTTP_req_filename[15]=='.') &&
             ((HTTP_req_filename[16]=='C') || (HTTP_req_filename[16]=='c')) &&
             ((HTTP_req_filename[17]=='S') || (HTTP_req_filename[17]=='s')) &&
             ((HTTP_req_filename[18]=='V') || (HTTP_req_filename[18]=='v')) &&
             (HTTP_req_filename[19]==0))
          {
            return sendSDLogFile(client, HTTP_req_filename);
          }
          else
          {
            // no day part found in URL
            if((HTTP_req_filename[13]==0) ||
               ((HTTP_req_filename[13]=='/') && (HTTP_req_filename[14]==0)) )
            {
              return sendLogDays(client, HTTP_req_filename);
            }
            else
            {
              return send404NotFound(client, HTTP_req_filename);
            }
          }
        }
        else
        {
          // No month part found in URL
          if((HTTP_req_filename[10]==0) ||
             ((HTTP_req_filename[10]=='/') && (HTTP_req_filename[11]==0)))
          {
            return sendLogMonths(client, HTTP_req_filename);
          }
          else
          {
            return send404NotFound(client, HTTP_req_filename);
          }
        }
      }
      else
      {
        // No year part found in URL
        if((HTTP_req_filename[5]==0) ||
           ((HTTP_req_filename[5]=='/') && (HTTP_req_filename[6]==0)))
        {
          return sendLogYears(client, HTTP_req_filename);
        }
        else
        {
          return send404NotFound(client, HTTP_req_filename);
        }
      }
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
      if(page->handler)
      {
        return page->handler(client);
      }
      else
      {
        return send404NotFound(client, HTTP_req_filename);
      }
    }
  }
}

