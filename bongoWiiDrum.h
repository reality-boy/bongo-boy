#ifndef BONGOWIIDRUM_H
#define BONGOWIIDRUM_H

//--------------
// Wii Drums communication


/*
  WiiMote connector looking into
  end of Wii controler
  ________
  | 1 3 5 |
  | 2 4 6 |
  |__---__|

  * 1 (Black) - VCC (3.3v)
  * 2 (Red) - SLC
  * 3 (White) - NC
  * 4 (Silver) - NC
  * 5 (Green) - SDA
  * 6 (Blue) - GND

  //---------
 
  Arduino to Wii connections

  3.3V -> WII pin 1
  Gnd -> WII pin 6
  A4 -> WII pin 5
  A5 -> WII pin 2
*/

// index into our array of buttons
typedef enum wichButton {
  BRED = 0,
  BBLUE,
  BGREEN,
  BYELLOW,
  BORANGE,
  BPEDAL,
  BMINUS,
  BPLUS,
  
  // generated from SX & SY
  BUP,
  BDOWN,
  BLEFT,
  BRIGHT,
  
  B_COUNT, // how many buttons in total are there
} wichButton;

typedef enum wichPad {
  RED = 0,
  BLUE,
  GREEN,
  YELLOW,
  ORANGE,
  PEDAL,
  
  YELLOW_SWITCH,

  EXT_DRM_1,
  EXT_DRM_2,
  EXT_DRM_3,

  P_COUNT, // how many buttons in total are there
} wichPad;

class bongoWiiDrum
{
public:
  bongoWiiDrum() {}

  // data recieved from drum
  int sx, sy, softness;
  bool isHHP, haveVel;
  
private:
  // wich pad is velocity dat for
  typedef enum wichPad_internal {
    PRED = 0x19,
    PBLUE = 0x0F,
    PGREEN = 0x12,
    PYELLOW = 0x11,
    PORANGE = 0x0E,
    PPEDAL = 0x1B,
  
    PI_COUNT, // how many pads
  } wichPad_internal;

  wichPad wich;
  bool buttons[B_COUNT];
  bool lastButtons[B_COUNT];

  byte lastRawBytes[B_COUNT];
  byte rawBytes[B_COUNT];

public:
  // drums use i2c bus, but they are not much harder to talk to than MIDI
  void begin()
  {
    Wire.begin();

    // first stage of wii nunchuck initialization
    Wire.beginTransmission(0x52);
    Wire.write(0xF0);
    Wire.write(0x55);
    Wire.endTransmission();
  
    delay(1);  
  
    // second stage of wii nunchuck initialization
    Wire.beginTransmission(0x52);
    Wire.write(0xFB);
    Wire.write((uint8_t)0x00);
    Wire.endTransmission();

    // send zero to request a data packet form drums
    // Important, do this before you request data or 
    // the bytes may not alighn properly
    Wire.beginTransmission(0x52);
    Wire.write(0x00);
    Wire.endTransmission();
  
    // zero out our last button hit array
    memset(lastButtons, 0, sizeof(lastButtons));

    // zero out our last data array
    memset(lastRawBytes, 0, sizeof(lastRawBytes));
    memset(rawBytes, 0, sizeof(rawBytes));
  }

  // Data comes in in a 6 byte block.  This does not match up with 
  // the usual format for Wii nunchuck remotes.  We get one packet every
  // time any pad is hit, with the velocity data filled in.  We may also get
  // additional packets indicating the pad is active, but with no velocity
  // data. We can safely ignore those packets, unless you want to use them to
  // simulate MIDI key off.  Data is queued up, so if you don't read often enough
  // then you get latency!  I find that going much past a 10ms delay will cause
  // troubles.
  void readData()
  {
    // stash off our last data set for debugging purposes
    memcpy(lastRawBytes, rawBytes, sizeof(rawBytes));

    // read 6 bytes  
    int count = 0;
    Wire.requestFrom(0x52, 6);
    while(Wire.available() && count < 6)
    {
      rawBytes[count] = Wire.read();
      count++;
    }

    // request next packet
    Wire.beginTransmission(0x52);
    Wire.write((uint8_t)0x00);
    Wire.endTransmission();
  
    // decode our recieved data
    sx       = rawBytes[0] & 0x3F; // joystick x
    sy       = rawBytes[1] & 0x3F; // joystick y
    isHHP    = !((rawBytes[2] >> 7) & 0x01); // velocity data is actually hi-hat pedal position
    haveVel  = !((rawBytes[2] >> 6) & 0x01); // velocity data available
    wich     = internalToPad((wichPad_internal)((rawBytes[2] >> 1) & 0x1F)); // what pad is velocity data for
    softness = 7 - ((rawBytes[3] >> 5) & 0x07); // velocity data, from 0-7
  
    // stash off old button list so we can detect changes
    memcpy(lastButtons, buttons, sizeof(lastButtons));
  
    // decode all the buttons
    buttons[BMINUS]  = !((rawBytes[4] >> 4) & 0x01); // minus button
    buttons[BPLUS]   = !((rawBytes[4] >> 2) & 0x01); // plus button
    buttons[BORANGE] = !((rawBytes[5] >> 7) & 0x01); // orange pad is active
    buttons[BRED]    = !((rawBytes[5] >> 6) & 0x01); // red pad is active
    buttons[BYELLOW] = !((rawBytes[5] >> 5) & 0x01); // yellow pad is active
    buttons[BGREEN]  = !((rawBytes[5] >> 4) & 0x01); // green pad is active
    buttons[BBLUE]   = !((rawBytes[5] >> 3) & 0x01); // blue pad is active
    buttons[BPEDAL]  = !((rawBytes[5] >> 2) & 0x01); // base pedal is active
    
    buttons[BUP]     = (sy > ((buttons[BUP])    ? 43 : 53)); // debounced up on joystick
    buttons[BDOWN]   = (sy < ((buttons[BDOWN])  ? 20 : 10)); // debounced down on joystick
    buttons[BLEFT]   = (sx < ((buttons[BLEFT])  ? 20 : 10)); // debounced left on joystick
    buttons[BRIGHT]  = (sx > ((buttons[BRIGHT]) ? 43 : 53)); // debounced right on joystick
  }
  
  // just pressed
  bool buttonPressed(wichButton b)
  {
    if(b >= 0 && b < B_COUNT)
      return buttons[b] && !lastButtons[b];

    return false;
  }
  
  // just released
  bool buttonReleased(wichButton b)
  {
    if(b >= 0 && b < B_COUNT)
      return !buttons[b] && lastButtons[b];

    return false;
  }
  
  // held down
  bool buttonDown(wichButton b)
  {
    if(b >= 0 && b < B_COUNT)
      return buttons[b];

    return false;
  }

  wichPad getPad()
  {
    return wich;
  }
  
  const char * buttonToString(wichButton button) 
  {
    switch(button) 
    {
      case BRED:    return "RED";    break;
      case BBLUE:   return "BLUE";   break;
      case BGREEN:  return "GREEN";  break;
      case BYELLOW: return "YELLOW"; break;
      case BORANGE: return "ORANGE"; break;
      case BPEDAL:  return "PEDAL";  break;
      case BMINUS:  return "MINUS";  break;
      case BPLUS:   return "PLUS";   break;
      case BUP:     return "UP";     break;
      case BDOWN:   return "DOWN";   break;
      case BLEFT:   return "LEFT";   break;
      case BRIGHT:  return "RIGHT";  break;
    }
    
    return "Unknown";
  }

  const char * padToString(wichPad pad) 
  {
    switch(pad) 
    {
      case RED:    return "RED";    break;
      case BLUE:   return "BLUE";   break;
      case GREEN:  return "GREEN";  break;
      case YELLOW: return "YELLOW"; break;
      case ORANGE: return "ORANGE"; break;
      case PEDAL:  return "PEDAL";  break;
      // not wii drum pads...
      case YELLOW_SWITCH: return "YELLOW_SWITCH"; break;
      case EXT_DRM_1: return "EXT_DRM_1"; break;
      case EXT_DRM_2: return "EXT_DRM_2"; break;
      case EXT_DRM_3: return "EXT_DRM_3"; break;
    }
    
    return "Unknown";
  }

  // just a little debug routine to dump the drum data to the serial port
  // don't call this when using the MIDI out
  void dumpToSerial()
  {
    // if something changed
    if(0 != memcmp(lastRawBytes, rawBytes, sizeof(lastRawBytes)))
    {
      /*
      if(rawBytes[2] != 0xFF || rawBytes[3] != 0xFF || rawBytes[4] != 0xFF || rawBytes[5] != 0xFF)
      {
        Serial.print(rawBytes[0], HEX);
        Serial.print(' ');
        Serial.print(rawBytes[1], HEX);
        Serial.print(' ');
        Serial.print(rawBytes[2], HEX);
        Serial.print(' ');
        Serial.print(rawBytes[3], HEX);
        Serial.print(' ');
        Serial.print(rawBytes[4], HEX);
        Serial.print(' ');
        Serial.print(rawBytes[5], HEX);
        Serial.println(' ');
      }
      */
      
      if(haveVel)
      {
        Serial.print("hit ");
        Serial.print(padToString(wich));
        Serial.print(' ');
        Serial.print(softness);
        Serial.println(' ');
      }
    }
  }

private:
  wichPad internalToPad(bongoWiiDrum::wichPad_internal pad_internal)
  {
    switch(pad_internal) {
    case PRED:    return RED;    break;
    case PBLUE:   return BLUE;   break;
    case PGREEN:  return GREEN;  break;
    case PYELLOW: return YELLOW; break;
    case PORANGE: return ORANGE; break;
    case PPEDAL:  return PEDAL;  break;
    }
    
    return RED;
  }
};

#endif // BONGOWIIDRUM_H

