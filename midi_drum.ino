
#include <Arduino.h>
#include <Wire.h>

/*
 Guitar Hero Wourld Tour Wii drumset to MIDI converter.
 This project translates between the Wii Nunchuck connector on the Guitar Hero
 Drumset to MIDI sutable to output to a piano or drum machine so you can use
 your drums as an input.
 
 Basically we read from the Wii using the i2c protocol and write to the MIDI
 bus using a simple serial protocol.  The only parts needed are a suitable cable
 for connecting to the Wii nunchuck connector (or you can stuff wires directly 
 into the cable) and a MIDI cable cut into two and a single 330 ohm resistor.  
 Wire it up as indicated below.
 
 Each drum pad returns a velocity value from 0 to 7 and midi recieves a note and
 velocity ranging from 0-127, so we only need a simple translation layer to go
 from one to the other.
 
 I also included code to allow you to adjust the sensitivity of the drum pads. 
  - Unplug the MIDI cable from any device
  - Enable the code by setting mode = 1 in Loop()
  - Edit the initSensitivity() function to the desired sensitivity levels (0-63)
  - Program the arduino
  - Now the arduino will blast out the sensitivity code once a second, simply
    plug in the midi cable to the drum machine, wait a fiew seconds and unplug the
    cable.  The sensitivity should now be set.
 Chances are you can drive the sensitivity quite a bit higher, just increase it
 untill you start to get false triggers and then back off a bit.  A creative person
 coult turn this into an interactive routine that blinks a light when you hit a pad
 and allows you to use the buttons on the drum kit to adjust the sensitivity.
 
 Finally there is a little debugging code that lets you dump data straight to the
 serial port.  Unfortunently we are using the serial port to control our MIDI
 device so it is impossible to run both at the same time.  I suspect that you could
 get away with using a software based serial driver to move the MIDI TX line to
 a different pin.
 
 Some ideas for expansion:
 
 - Add in a metronome, prefferably one that you can bang out a rythme on the 
   pads to adjus the tempo
 
 - Add in a drumming monitor using an array of LED lights that can indicate 
   how closely your drumming matches the metronome
 
 - Add in a rythm trainer that blinks/plays a pattern, prompts you to match it,
   then scores your result
   
 - Create a simple sampler that can record a sequence of drum hits and play them
   back in a loop.  That way you can lay down a base track and then jam on top
   of it.
   
 - Hook up a LCD and keyboard, this allows us to customize the drum samples,
   Interactively adjust the sensitivity (safely), and much more
 
 - Figure out the hi-hat pedal so we can be more expressive with our playing.
 
 - Use the joystick to add in 4 more 'expressive' sounds like the cow bell.
 
 - Use the B+ and B- keys to switch between banks of drum samples
 
 - Double up the samples by making hard hits play a different sample. This
   could work especially well with the cymbols since we are short one pad.
 
 //---------
 
  MIDI connector looking into piano
  
    -----
  / 4 2 5 \
 | 1     3 |
  \   ,   /
    -----
 * 2 - Gnd
 * 4 - 5V
 * 5 - Dats
 
 //---------

 Arduino to MIDI connections
 
 Pin 1 (tx) -> MIDI pin 5
 Gnd -> MIDI pin 2
 5V -> 330 Ohm Resistor -> MIDI pin 4

 //---------
 
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

 //---------
 
*/

//---------------
// MIDI device communication

// MIDI drum notes that may be interesting to play
enum midiDrumNotes {
  
  // from my Yamaha PSR E323 keyboard
  // starts at 13, if you are interested
  SNARE_ROLL = 29,  // playes a roll untill key up
  CASTANET = 30,
  SNARE_H_SOFT = 31,
  STICKS = 32,
  BASE_DRUM_SOFT = 33,
  OPEN_RIM_SHOT = 34,
  
  // beginning of GM standard midi sounds
  BASE_DRUM_HARD = 35,
  BASE_DRUM = 36,
  SIDE_STICK = 37,
  SNARE_M = 38,
  HAND_CLAP = 39,
  SNARE_H_HARD = 40,
  FLOOR_TOM_L = 41,
  HI_HAT_CLOSE = 42,
  FLOOR_TOM_H = 43,
  HI_HAT_PEDAL = 44,
  LOW_TOM = 45,
  HI_HAT_OPEN = 46,
  MID_TOM_L = 47,
  MID_TOM_H = 48,
  CRASH_CYMBAL_1 = 49,
  HIGH_TOM = 50,
  RIDE_CYMBAL_1 = 51,
  CHINESE_CYMBAL = 52,
  RIDE_BELL = 53,
  TAMBOURINE = 54,
  SPLASH_CYMBAL = 55,
  COWBELL = 56,
  CRASH_CYMBAL_2 = 57,
  VIBRA_SLAP = 58,
  RIDE_CYMBAL_2 = 59,
  // goes on to 81 if you are interested
  // 84 for my Yamaha keyboard
};

// MIDI commands always have there high byte set
// and are located in the upper 4 byte block
// the lower 4 bytes are reserved for the 16 MIDI channels
enum midiCommands {
  NOTE_OFF = 0x80,
  NOTE_ON = 0x90,
  CONTROL_CHANGE = 0xB0,
};

// MIDI devices support 16 communication channels so 16 devices 
// can talk to one device.  Channel 16 is for configuration of the 
// device and channel 10 is a special dedicated drum channel
enum midiChannels {
  CHAN_10 = 0x09, // drum channel
  CHAN_16 = 0x0F, // config channel?
};

// MIDI is simple, we just transmit out the serial port at 31250 KB
void initMIDI()
{
  // Setup serial port to talk over MIDI
  Serial.begin(31250);
}

// Talking is just as simple, toss out a command (command | channel) and 
// two data bytes and you are done.
void transmitMIDI(int cmd, int data_1, int data_2)
{
  Serial.write(cmd & 0xFF);
  Serial.write(data_1 & 0xFF);
  Serial.write(data_2 & 0xFF);
}

//--------------
// Wii Drums communication

// use B+ button to toggle snare sound on/off
bool isSnareOn = true;

// wich pad is velocity dat for
enum wichPad {
  RED = 0x19,
  BLUE = 0x0F,
  GREEN = 0x12,
  YELLOW = 0x11,
  ORANGE = 0x0E,
  PEDAL = 0x1B,
};

// index into our array of buttons
enum buttons {
  BRED = 0,
  BBLUE,
  BGREEN,
  BYELLOW,
  BORANGE,
  BPEDAL,
  BMINUS,
  BPLUS,
  
  B_COUNT, // how many buttons in total are there
};

// data recieved from drum
int sx, sy, wich, softness;
bool isHHP, haveVel;
bool buttons[B_COUNT];
bool lastButtons[B_COUNT];

// drums use i2c bus, but they are not much harder to talk to than MIDI
void drumBegin()
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
}

// Data comes in in a 6 byte block.  This does not match up with 
// the usual format for Wii nunchuck remotes.  We get one packet every
// time any pad is hit, with the velocity data filled in.  We may also get
// additional packets indicating the pad is active, but with no velocity
// data. We can safely ignore those packets, unless you want to use them to
// simulate MIDI key off.  Data is queued up, so if you don't read often enough
// then you get latency!  I find that going much past a 10ms delay will cause
// troubles.
void drumReadData()
{
  byte values[6];
  int count = 0;

  // read 6 bytes  
  count = 0;
  Wire.requestFrom(0x52, 6);
  while(Wire.available() && count < 6)
  {
    values[count] = Wire.read();
    count++;
  }

  // request next packet
  Wire.beginTransmission(0x52);
  Wire.write((uint8_t)0x00);
  Wire.endTransmission();
  
  // decode our recieved data
  sx       = values[0] & 0x3F; // joystick x
  sy       = values[1] & 0x3F; // joystick y
  isHHP    = !((values[2] >> 7) & 0x01); // velocity data is actually hi-hat pedal position
  haveVel  = !((values[2] >> 6) & 0x01); // velocity data available
  wich     = (values[2] >> 1) & 0x1F; // what pad is velocity data for
  softness = 7 - ((values[3] >> 5) & 0x07); // velocity data, from 0-7
  
  // stash off old button list so we can detect changes
  memcpy(lastButtons, buttons, sizeof(lastButtons));
  
  // decode all the buttons
  buttons[BMINUS]  = !((values[4] >> 4) & 0x01); // minus button
  buttons[BPLUS]   = !((values[4] >> 2) & 0x01); // plus button
  buttons[BORANGE] = !((values[5] >> 7) & 0x01); // orange pad is active
  buttons[BRED]    = !((values[5] >> 6) & 0x01); // red pad is active
  buttons[BYELLOW] = !((values[5] >> 5) & 0x01); // yellow pad is active
  buttons[BGREEN]  = !((values[5] >> 4) & 0x01); // green pad is active
  buttons[BBLUE]   = !((values[5] >> 3) & 0x01); // blue pad is active
  buttons[BPEDAL]  = !((values[5] >> 2) & 0x01); // base pedal is active
}

// The meat of the program, we recieve drum hits, convert to MIDI notes,
// and blast it out.
void processMidiDrum()
{
  drumReadData();
  
  // we only recieve velocity data when a pad was hit
  if(haveVel)
  {
    int note = 0;
    int velocity = 0;
   
    // decode what note to play   
    switch(wich) {
      // pads
      case RED:
        if(isSnareOn)
          note = SNARE_H_HARD;
        else
          note = SNARE_M;
        break;
        
      case BLUE:   note = MID_TOM_L;   break;
      case GREEN:  note = FLOOR_TOM_L;  break;
    
      // cymbols
      case YELLOW: 
        if(buttons[BMINUS])
          note = HI_HAT_OPEN;
        else
          note = HI_HAT_CLOSE;
        break;

      case ORANGE: note = RIDE_CYMBAL_1; break;

      // one of two pedals...
      case PEDAL:  note = BASE_DRUM_HARD;  break;
    }
    
    // velocity is a value from 0-7
    // map it to 0-127 for export to MIDI
    velocity = 15 + (softness * 16 );
    // make sure we don't go over 127, the high byte is reserved for control codes
    if(velocity > 127)
      velocity = 127;

    // turn a note on, on chanel 10 (the drum channel)
    // in principle we should turn the note back off as well but my keyboard
    // ignores note off commands form drums and so I don't bother
    transmitMIDI(NOTE_ON | CHAN_10, note, velocity);
  }
  
  // map hi-hat to B- button for now
  // later map it to secondary pedal.
  if( !buttons[BMINUS] && lastButtons[BMINUS] )
  {
    // I tried to transmit the high hat close sound here, but
    // my keyboard can not play a hi-hat close sound at the same time
    // you play a hi-hat hit sound so it just muddled up the sound
    //transmitMIDI(NOTE_ON | CHAN_10, HI_HAT_PEDAL, 63);
  }
  
  // toggle snare on/off with B+ button
  // you could extend this to switch between two sets of sounds
  // for all pads
  if(buttons[BPLUS] && !lastButtons[BPLUS])
    isSnareOn = !isSnareOn;

  // don't run too fast or we overwhelm the MIDI port
  // but don't run too slow or the drums get laggy
  delay(10);
}

// Set sensitivity of pads through external MIDI in port on drum kit
// be careful not to blast this to a real MIDI device, I am not confident
// that it wont reprogram your device to do something bad.
//
// This allows you to adjust the sensitivity of the pads on your drum kit
// This adjustment happens in the hardware so it will also affect how the 
// drums work on Guitar Hero.  This replicates the stand alone sensitivity
// adjustment provided by GuitarHero tech support to customers who had
// sensitivity issues with there hardware.
//
// I also read about a popsicle stick hack, where you slip a popsicle (or
// any other hard item, like tape) behind the pizo-electric pickup in the pads
// so that the pizo device is more firmly pressed against the rubber pad.  This may
// help bring down the upper limit on velocity so you don't have to waill as hard
// to register the maximum force on the pads.  Note that I have not yet tested
// this idea out, and you run a real risk of breaking your kit if you try it.
void setDrumkitSensitivityOverMIDI(int /*wichPad*/ pad, int level)
{
  int padNum = 0;
  switch(pad) {
    case RED: padNum = 0x68; break;
    case BLUE: padNum = 0x66; break;
    case GREEN: padNum = 0x67; break;
    case YELLOW: padNum = 0x69; break;
    case ORANGE: padNum = 0x6A; break;
    case PEDAL: padNum = 0x64; break;
  }
  
  // limit range to 1-63 for sensitivity, 0 may work, but I did not test it
  if(level < 1)
    level = 1;
  if(level > 63)
    level = 63;
  
  // reverse the range when communicating with hardware
  level = 63 - level;
  
  if(padNum != 0)
    transmitMIDI(CONTROL_CHANGE | CHAN_16, padNum, level);
}

// A simple helper function to loop through all pads and set there sensitivity
// Adjust this as needed
void initSensitivity()
{
  // I believe the default sensitivity from the factory is around 32
  // but I am not positive
  
  // I find that making the snare more sensitive helps with drum rolls
  setDrumkitSensitivityOverMIDI(RED, 60);
  setDrumkitSensitivityOverMIDI(BLUE, 55);
  setDrumkitSensitivityOverMIDI(GREEN, 55);
  
  // the cymbols are poorly isolated so reduce there sensitivity (relatively)
  setDrumkitSensitivityOverMIDI(YELLOW, 50);
  setDrumkitSensitivityOverMIDI(ORANGE, 50);
  
  // my pedal worked best with high sensitivity, but then
  // it started to play by itself, so back down it goes
  setDrumkitSensitivityOverMIDI(PEDAL, 50);

  // save data permanently to eeprom
  // this call is optional, if you just want to test the sensitivity without
  // making it a permanent change
  transmitMIDI(CONTROL_CHANGE | CHAN_16, 0x65, 0x03);
  transmitMIDI(CONTROL_CHANGE | CHAN_16, 0x77, 0x77);

  // give it a long break between tries
  delay(1000);
}

// just a little debug routine to dump the drum data to the serial port
// don't call this when using the MIDI out
void drumDebugRead()
{
  static byte oldValues[6] = {0};
  byte values[6];
  int count = 0;

  // read 6 bytes  
  count = 0;
  Wire.requestFrom(0x52, 6);
  while(Wire.available() && count < 6)
  {
    values[count] = Wire.read();
    count++;
  }

  // request next packet
  Wire.beginTransmission(0x52);
  Wire.write((uint8_t)0x00);
  Wire.endTransmission();
  
  if(0 != memcmp(oldValues, values, sizeof(oldValues)) &&
    (values[2] != 0xFF || values[3] != 0xFF || values[4] != 0xFF || values[5] != 0xFF))
  {
    //Serial.print(values[0], HEX);
    //Serial.print(' ');
    //Serial.print(values[1], HEX);
    //Serial.print(' ');
    Serial.print(values[2], BIN);
    Serial.print(' ');
    Serial.print(values[3], BIN);
    Serial.print(' ');
    Serial.print(values[4], BIN);
    Serial.print(' ');
    Serial.print(values[5], BIN);
    Serial.print(' ');
    
    bool haveVel = !((values[2] >> 6) & 0x01); // velocity data available
    int wich     =   (values[2] >> 1) & 0x1F; // what pad is velocity data for
    int softness = 7 - ((values[3] >> 5) & 0x07); // velocity data, from 0-7
    
    if(haveVel)
    {
      Serial.print("vel ");
      Serial.print(wich, HEX);
      Serial.print(' ');
      Serial.print(softness);
      Serial.print(' ');
    }

    Serial.println("");
  }
  memcpy(oldValues, values, sizeof(oldValues));
  
  delay(10);
}

void setup()
{
  // setup MIDI
  initMIDI();
  
  // setup drum controller
  drumBegin();
  
}

void loop()
{
  // change this to pick our opperating mode
  int mode = 0;
  
  switch(mode) {
  case 0:
    // normally we are a drum...
    processMidiDrum();
    break;
    
  case 1:
    // but if you enable this function and
    // hook the MIDI cable directly into the Guitar hero drum kit
    // then we can adjust the pad sensitivity
    // I'm not sure how safe this is to blast to a standard midi
    // device, so I made it a compile option...
    initSensitivity();
    break;
  
  case 2:
    // dump debug data to serial port
    static bool needsInit = true;
    if(needsInit)
    {
      Serial.begin(57600);
      needsInit = false;
    }

    drumDebugRead();
    break;
  }
}
