#include "BeaconController.h"

// About WPM:
// There are multiple definitions about WPM speeds.
// In this scope a standard word length is defined as having 50 dot lengths.
// 12 WPM = 600 dots => 1 dot = 0.1s

static const byte morse_chars[] =
{
  0xF9, 0xE8, 0xEA, 0xF4, // A, B, C, D
  0xFC, 0xE2, 0xF6, 0xE0, // E, F, G, H
  0xF8, 0xE7, 0xF5, 0xE4, // I, J, K, L
  0xFB, 0xFA, 0xF7, 0xE6, // M, N, O, P
  0xED, 0xF2, 0xF0, 0xFD, // Q, R, S, T
  0xF1, 0xE1, 0xF3, 0xE9, // U, V, W, X
  0xEB, 0xEC                   // Y, Z
};

static const byte morse_digits[]=
{ 
  0xDF, 0xCF, 0xC7, 0xC3, // 0 1 2 3
  0xC1, 0xC0, 0xD0, 0xD8, // 4 5 6 7
  0xDC, 0xDE // 8 9
};

// The morse characters are coded as follows:
// Each letter is contained within 1 byte
// The letter starts with a number of 1's followed by a 0.
// The actual morse code data starts at the bit following the 0
// 0 means dot(.), 1 means dash(-)
// E.g.: R = .-.
//
// Filler      . - .
// 1 1 1 1 0   0 1 0
//
// Special codes start with a 0
// 0-3: set power mode
// 0x10 + i  delay with CARRIER ON
// 0x20 + i  delay with CARRIER OFF

#define POWER_MODE(i)        (i)
#define DELAY_CARRIER_ON(i)  (0x10 + (i))
#define DELAY_CARRIER_OFF(i) (0x20 + (i))
#define MORSE_SPACE           0x7E
#define MORSE_END             0x7F

#define MORSE_PARSE_ERROR_STRLEN 50
static char morseParseError[MORSE_PARSE_ERROR_STRLEN];


/* Encode a single character to a morse-encoded byte
*/
byte morseEncodeChar(char c)
{
  if((c>='0') && (c<='9'))
  {
    return morse_digits[c-'0'];
  }
  if((c>='a') && (c<='z'))
  {
    return morse_chars[c-'a'];
  }
  if((c>='A') && (c<='Z'))
  {
    return morse_chars[c-'A'];
  }
  switch(c)
  {
    case '=':  // -...-
      return B11010001;
    case '?':  // ..--..
      return B10001100;
    case '/':  // -..-.
      return B11010010;
    case '.':  // .-.-.-
      return B10010101;
    case ',':  // --..--
      return B10110011;
    case 0:
      return MORSE_END;
    case ' ':
      return MORSE_SPACE;
    default:
      return 0;
  }
}


/* Encode a string into a coded morse message
   the user is responsible for the codedMessage memory space.
   Special codes:
    $P0 - $P3: Inserts a "Set power mode" code into the output.
    $A00 - $A15: Inserts the current value of the analog channel into the message (at encoding time, not transmit time!)
    $T0 - T7: Inserts the current temperature into the message (at encoding time)
    $+1 - $+9: insert delay with CARRIER ON
    $-1 - $-9: insert delay with CARRIER OFF

   Parameters:
    codedMessage: output of the encoder
    str: the message to encode
    maxBytes: the number of bytes codedOutput can hold

   Returns:
    true: success
    false: there was an error parsing the string
*/

boolean morseEncodeMessage(byte *codedMessage, const char *str, int maxBytes)
{
  morseParseError[0] = 0;
  int j = 0;
  for(int i = 0; str[i]; i++)
  {
    if(str[i]=='$')
    {
      i++;
      if(str[i]=='P')
      {
        i++;
        if( (str[i]>='0') && (str[i]<='3') )
        {
          if(j>=maxBytes)
          {
            sprintf(morseParseError, "output buffer too small (max. %d bytes)", maxBytes);
            codedMessage[0] = MORSE_END;
            return false;
          }
          codedMessage[j++] = POWER_MODE(str[i]-'0');
        }
        else
        {
          sprintf(morseParseError, "Error parsing power mode at position %d (expecting 0-3)", i);
          codedMessage[0] = MORSE_END;
          return false;
        }
      }
      else if(str[i]=='A')
      {
        int channel=0;
        i++;
        if(str[i]=='1')
        {
          channel = 10;
        }
        else if(str[i]!='0')
        {
          sprintf(morseParseError, "Error parsing analog channel number at position %d (expecting 00-15)", i);
          codedMessage[0] = MORSE_END;
          return false;
        }
        i++;
        if((str[i]>='0') && (str[i]<='9'))
        {
          channel += str[i]-'0';
        }
        else
        {
          sprintf(morseParseError, "Error parsing analog channel number at position %d (expecting 00-15)", i);
          codedMessage[0] = MORSE_END;
          return false;
        }
      }
      else if(str[i]=='T')
      {
        int channel=0;
        i++;
        if((str[i]>='0') && (str[i]<='7'))
        {
          channel = str[i]-'0';
        }
        else
        {
          sprintf(morseParseError, "Error parsing temperature sensor at position %d (expecting 0-7)", i);
          codedMessage[0] = MORSE_END;
          return false;
        }
      }
      else if(str[i]=='+')
      {
        i++;
        if((str[i]>='1') && (str[i]<='9'))
        {
          if(j>=maxBytes)
          {
            sprintf(morseParseError, "output buffer too small (max. %d bytes)", maxBytes);
            codedMessage[0] = MORSE_END;
            return false;
          }
          codedMessage[j++] = DELAY_CARRIER_ON(str[i]-'0');
        }
        else
        {
          sprintf(morseParseError, "Error parsing delay at position %d (expecting 1-9)", i);
          codedMessage[0] = MORSE_END;
          return false;
        }
      }
      else if(str[i]=='-')
      {
        i++;
        if((str[i]>='1') && (str[i]<='9'))
        {
          if(j>=maxBytes)
          {
            sprintf(morseParseError, "output buffer too small (max. %d bytes)", maxBytes);
            codedMessage[0] = MORSE_END;
            return false;
          }
          codedMessage[j++] = DELAY_CARRIER_OFF(str[i]-'0');
        }
        else
        {
          sprintf(morseParseError, "Error parsing delay at position %d (expecting 1-9)", i);
          codedMessage[0] = MORSE_END;
          return false;
        }
      }
      else
      {
        sprintf(morseParseError, "Error parsing special code at position %d (expecting P, A, T, + or -)", i);
        codedMessage[0] = MORSE_END;
        return false;
      }
    }
    else
    {
      // Not a special $-code, interpret as a morse character
      char c = morseEncodeChar(str[i]);
      if(c == 0)
      {
        sprintf(morseParseError, "Can not encode character '%c' at position %d", str[i], i);
        codedMessage[0] = MORSE_END;
        return false;
      }
      else
      {
          if(j>=maxBytes)
          {
            sprintf(morseParseError, "output buffer too small (max. %d bytes)", maxBytes);
            codedMessage[0] = MORSE_END;
            return false;
          }
        codedMessage[j++]=c;
      }
    }
  }
  codedMessage[j] = MORSE_END;
  return true;
}

/* Returns a human-readable string detailing the last error while parsing a morse string (if any)
*/
const char* morseGetError()
{
  return morseParseError;
}

void debugPrintMorse(const char *msg)
{
  for(int i = 0; msg[i]!=MORSE_END; i++)
  {
    Serial.println((byte)(msg[i]), BIN);
  }
  Serial.println("---END OF MESSAGE---");
}

/********************************************************************************************************\
|***                                                                                                  ***|
|***                                         Beacon Class                                             ***|
|***                                                                                                  ***|
\********************************************************************************************************/


void Beacon::keyOn()
{
  digitalWrite(normalPin, 1);
  digitalWrite(invPin, 0);
}
void Beacon::keyOff()
{
  digitalWrite(normalPin, 0);
  digitalWrite(invPin, 1);
//debug    noTone(invPin);
}
void Beacon::carrierOff()
{
  digitalWrite(normalPin, 0);
  digitalWrite(invPin, 0);
}

void Beacon::powerMode(byte mode)
{
  digitalWrite(modePin0, mode & 1);
  digitalWrite(modePin1, mode & 2);
}

Beacon::Beacon(int normalPin, int invPin, int modePin0, int modePin1)
{
  begin(normalPin, invPin, modePin0, modePin1);
}
Beacon::Beacon()
{
  end();
}
void Beacon::begin(int normalPin, int invPin, int modePin0, int modePin1)
{
  this->normalPin = normalPin;
  this->invPin = invPin;
  this->modePin0 = modePin0;
  this->modePin1 = modePin1;
  done = true;
  nextMsg = 0;
  pinMode(normalPin, OUTPUT);
  pinMode(invPin, OUTPUT);
  pinMode(modePin0, OUTPUT);
  pinMode(modePin1, OUTPUT);
  carrierOff();
  powerMode(0);
}

void Beacon::end()
{
  normalPin = invPin = modePin0 = modePin1 = -1;
  done = true;
  nextMsg = 0;
}

void Beacon::tick()
{
  if(done)
  {
    if(nextMsg)
    {
      msg = nextMsg;
      nextMsg = 0;
      msgPos = 0;
      bitPos = 7;
      done = false;
    }
  }
  else
  {
    if(bitPos==7)
    {
      // Interpret next byte
      code = msg[msgPos];
      if((code & 0x80) == 0)
      {
        if(code == MORSE_END)
        {
          carrierOff();
          done = true;
          bitPos=0;
        }
        else if(code == MORSE_SPACE)
        {
          keyOff();
          offPause=5;
          bitPos=0;
        }
        else if(code<4)
        {
          powerMode(code);
          msgPos++;
        }
        else if(code & 0x10)
        {
          keyOn();
          cmdPause = 16 * (code & 0xEF);
          bitPos=0;
        }
        else if(code & 0x20)
        {
          keyOff();
          cmdPause = 16 * (code & 0xDF);
          bitPos=0;
        }
        else
        {
          // unknown code?????? Set to done
          done = true;
        }
      }
      else
      {
        // Remove filler
        while(code & 0x80)
        {
          code <<= 1;
          bitPos--;
        }
        code <<= 1;
        bitPos--;
        
        // Start transmitting first sign
        keyOn();
        if(code & 0x80)
        {
          onPause = 3;
        }
        else
        {
          onPause = 1;
        }
      }
    }
    else
    {
      // It wasn't the begin of a byte
      if(cmdPause)
      {
        cmdPause--;
        if(cmdPause==0)
        {
          msgPos++;
          bitPos=7;
        }
      }
      else if(onPause)
      {
        onPause--;
        if(onPause==0)
        {
          keyOff();
          if(bitPos==0)
          {
            offPause=3;
          }
          else
          {
            offPause=1;
          }
        }
      }
      else if(offPause)
      {
        offPause--;
        if(offPause==0)
        {
          if(bitPos)
          {
            code <<= 1;
            bitPos--;
            keyOn();
            if(code & 0x80)
            {
              onPause=3;
            }
            else
            {
              onPause = 1;
            }
          }
          else
          {
            // Proceed to next byte
            msgPos++;
            bitPos = 7;
          }
        }
      }
    }
  }
}
boolean Beacon::isDone()
{
  return done && (nextMsg==0);
}
void Beacon::setNextMessage(byte *codedMessage)
{
  nextMsg = codedMessage;
}


