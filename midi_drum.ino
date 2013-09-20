
/*

B+/B- Controls:
- volume
- enter/exit pad assignment mode
- adjust pad sample up/down
- enter/exit sensitivity adjust mode
- adjust sensitivity up/down
- start tap-n-set metronome


button alone adjust volume

both buttons held for 1 second enters tap-n-set metronome (with ding sound on keyboard)
 you then have 15 seconds to start taping out a rythm.
 once you have tapped at least two, and up to 4 beats we calculate the time and start playing the ding sound
 if more than 2x seconds have gone by without a tap we assume you are finished inputting the beat

both buttons held with the up stick enters the pad assignment mode
 tap any pad to select it as the 'active pad'
 we apply some debounce to lock the loudest pad in the last 1/16th of a second so double pads don't cause issues
 hitting button up/down will change the sound, playing it on the midi as it changes.  At ends we just play ding sound
 holding both buttons for 1 second leaves the assigment mode
 
both buttons and down stick enters sensitivity mode
 Same as assignment mode but adjust sensitivity over secondary midi port
 
*/

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
*/

#include <Arduino.h>
#include <Wire.h>
#include <SoftwareSerial.h>

#include "bongoMIDI.h"
#include "bongoWiiDrum.h"

bongoWiiDrum drum;
bongoMIDI MIDIOut(2, 3);
bongoMIDI DrumConfig(4, 5);

int midiVolume = 100;

int padSensitivity[P_COUNT] = {58, 58, 58, 58, 58, 58};

int padMIDIMap[P_COUNT] = {
  SNARE_ROLL, //SNARE_H_HARD, // RED
  MID_TOM_L, // BLUE
  FLOOR_TOM_L, // GREEN
  HI_HAT_CLOSE, // YELLOW
  RIDE_CYMBAL_1, // ORANGE
  BASE_DRUM_HARD, //PEDAL
};

int state = 0;
int lastPad = 0;

const unsigned long msPerMin = 60000L;
unsigned long beatPerMin = 100L;

unsigned long msPerBeat = 4 * msPerMin / beatPerMin;
unsigned long msPer4th = msPerMin / beatPerMin;
unsigned long msPer8th = msPerMin / (beatPerMin * 2);
unsigned long msPer16th = msPerMin / (beatPerMin * 4);

unsigned long startBeatTime = 0L;  // when we started to play
unsigned long nextBeat = 0L;       // when the next note will play

unsigned long tapStartTime = 0;
unsigned long tapEndTime = 0;
unsigned long tapBeatCount = 0;
bool tapFirstTime = true;

void setBeat(int beat)
{
  if(beat > 300)
    beat = 300;
  if(beat < 20)
    beat = 20;
    
  beatPerMin = beat;
  msPerBeat = 4 * msPerMin / beatPerMin;
  msPer4th = msPerMin / beatPerMin;
  msPer8th = msPerMin / (beatPerMin * 2);
  msPer16th = msPerMin / (beatPerMin * 4);
  
  startBeatTime = millis();
  nextBeat = startBeatTime + msPer8th;
  Serial.print(beatPerMin);
  Serial.print(" ");
  Serial.print(msPerBeat);
  Serial.print(" ");
  Serial.print(msPer4th);
  Serial.print(" ");
  Serial.print(msPer8th);
  Serial.print(" ");
  Serial.print(msPer16th);
  Serial.print(" ");
  Serial.print(startBeatTime);
  Serial.println(" ");
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
    case RED:    padNum = 0x68; break;
    case BLUE:   padNum = 0x66; break;
    case GREEN:  padNum = 0x67; break;
    case YELLOW: padNum = 0x69; break;
    case ORANGE: padNum = 0x6A; break;
    case PEDAL:  padNum = 0x64; break;
  }
  
  // limit range to 1-63 for sensitivity, 0 may work, but I did not test it
  if(level < 1)
    level = 1;
  if(level > 63)
    level = 63;
  
  // reverse the range when communicating with hardware
  level = 63 - level;
  
  if(padNum != 0)
    DrumConfig.transmitMIDI(CONTROL_CHANGE | CHAN_16, padNum, level);
    
  // eat up junk data returned by drums after change in sensitivity
  for(int i=0; i<33; i++)
  {
    drum.readData();
    delay(10);
  }
}

void saveDrumkitSensitivityOverMIDI()
{
  // save data permanently to eeprom
  // this call is optional, if you just want to test the sensitivity without
  // making it a permanent change
  DrumConfig.transmitMIDI(CONTROL_CHANGE | CHAN_16, 0x65, 0x03);
  DrumConfig.transmitMIDI(CONTROL_CHANGE | CHAN_16, 0x77, 0x77);
  
  // eat up junk data returned by drums after change in sensitivity
  for(int i=0; i<33; i++)
  {
    drum.readData();
    delay(10);
  }
}

void playPad(int pad, int softness)
{
  // velocity is a value from 0-7
  // map it to 0-127 for export to MIDI
  int velocity = 15 + (softness * 16 );

  // make sure we don't go over 127, the high byte is reserved for control codes
  if(velocity > 127)
    velocity = 127;

  int index = drum.padToButton(pad);
  
  int chan = CHAN_10;

  // turn a note on, on chanel 10 (the drum channel)
  MIDIOut.transmitMIDI(NOTE_ON | CHAN_10, padMIDIMap[index], velocity);
  delay(5);
  MIDIOut.transmitMIDI(NOTE_OFF | CHAN_10, padMIDIMap[index], 127); // 127 indicates fastest decay
}

int increment(int val, int min, int max, int inc)
{
  val += inc;
  if(val > max)
    val = max;
  if(val < min)
    val = min;
  return val;
}

// The meat of the program, we recieve drum hits, convert to MIDI notes,
// and blast it out.
void processMidiDrum()
{
  // we only recieve velocity data when a pad was hit
  if(drum.haveVel)
    playPad(drum.wich, drum.softness);
  
  // save off last pad hit for later use
  if(drum.buttonDown(BDOWN) && drum.haveVel)
  {
    state = 1;
    lastPad = drum.wich;
    
    // set the sensitivity, just in case it has not been set yet...
    int index = drum.padToButton(lastPad);
    setDrumkitSensitivityOverMIDI(lastPad, padSensitivity[index]);

    Serial.print("State 1: Set pad sensitivity ");
    Serial.print("Pad ");
    Serial.print(drum.padToString(lastPad));
    Serial.print(" Sensitivity ");
    Serial.println(padSensitivity[index]);
  }
  else if(drum.buttonDown(BUP) && drum.haveVel)
  {
    state = 2;
    lastPad = drum.wich;
    
    int index = drum.padToButton(lastPad);

    Serial.print("State 2: Set MIDI note ");
    Serial.print("Pad ");
    Serial.print(drum.padToString(lastPad));
    Serial.print(" MIDI Note ");
    Serial.println(padMIDIMap[index]);
  }
  else if(drum.buttonDown(BLEFT) && drum.haveVel)
  {
    if(tapFirstTime)
    {
      tapFirstTime = false;
      tapStartTime = millis();
      tapEndTime = tapStartTime;
      tapBeatCount = 0;
      state = 3;
      lastPad = drum.wich;
      nextBeat = 0xFFFFFFFFL;

      Serial.print("State 3: Play rythm ");
      Serial.print("Pad ");
      Serial.println(drum.padToString(lastPad));
    }
    else
    {
      tapEndTime = millis();
      tapBeatCount++;
    }
  }
  else if(state == 3 && !drum.buttonDown(BLEFT) && !tapFirstTime)
  {
    tapFirstTime = true;

    if(tapBeatCount > 0)
    {
      unsigned long tapTime = (tapEndTime - tapStartTime) / tapBeatCount;
      unsigned long beat = msPerMin / tapTime;
      setBeat(beat);
      
      Serial.print("bpm ");
      Serial.println(beat);
    }
  }

  // release state
  if(drum.buttonDown(BPLUS) && drum.buttonDown(BMINUS))
  {
    if(state != 0)
    {
      if(state == 1)
      {
        saveDrumkitSensitivityOverMIDI();
        Serial.println("saving sensitivity permanently");
      }
    
      state = 0;
      Serial.println("State 0: Normal ");
    }
  }

  if(drum.buttonPressed(BPLUS) || drum.buttonPressed(BMINUS))
  {
    if(state == 0)
    {
      if(drum.buttonPressed(BPLUS))
        midiVolume = increment(midiVolume, 1, 127, 8);
      else
        midiVolume = increment(midiVolume, 1, 127, -8);
  
      // Set the channel volume
      MIDIOut.transmitMIDI(CONTROL_CHANGE | CHAN_10, CHANNEL_VOLUME, midiVolume);
      //delay(1);
      playPad(RED, 1);

      Serial.print("Volume ");
      Serial.println(midiVolume);
    }
    else if(state == 1)
    {
      int index = drum.padToButton(lastPad);
  
      if(drum.buttonPressed(BPLUS))
        padSensitivity[index] = increment(padSensitivity[index], 1, 62, 1);
      else
        padSensitivity[index] = increment(padSensitivity[index], 1, 62, -1);
    
      setDrumkitSensitivityOverMIDI(lastPad, padSensitivity[index]);
  
      Serial.print("Pad ");
      Serial.print(drum.padToString(lastPad));
      Serial.print(" Sensitivity ");
      Serial.println(padSensitivity[index]);
    }
    else if(state == 2)
    {
      int index = drum.padToButton(lastPad);
  
      if(drum.buttonPressed(BPLUS))
        padMIDIMap[index] = increment(padMIDIMap[index], 0, 127, 1);
      else
        padMIDIMap[index] = increment(padMIDIMap[index], 0, 127, -1);

      playPad(lastPad, 1);
      
      Serial.print("Pad ");
      Serial.print(drum.padToString(lastPad));
      Serial.print(" MIDI Note ");
      Serial.println(padMIDIMap[index]);
    }
  }
  
  if(state == 3)
  {
    unsigned long currMillis = millis();
    if(currMillis >= nextBeat)
    {
      unsigned long beatCount = (currMillis - startBeatTime) / msPer8th;
      unsigned long currBeat = beatCount % 8;
        
      playPad(YELLOW, 1);
      
      /*
      if(currBeat == 0 || currBeat == 4)
        playPad(PEDAL, 1);
        
      if(currBeat == 2 || currBeat == 6)
        playPad(RED, 1);
      
      Serial.print("Play beat ");
      Serial.print(" ");
      Serial.print(currBeat);
      Serial.print(" ");
      Serial.print(beatCount);
      Serial.print(" ");
      Serial.print(currMillis);
      Serial.print(" ");
      Serial.print(currMillis - nextBeat);
      Serial.println(" ");
      */
      
      nextBeat = startBeatTime + ((beatCount + 1) * msPer8th);
    }
  }

  // don't run too fast or we overwhelm the MIDI port
  // but don't run too slow or the drums get laggy
  delay(5);
}

void setup()
{
  // init serial port for debugging
  Serial.begin(57600);

  // setup MIDI output
  MIDIOut.begin();
  
  // setup MIDI communication with drum pads
  DrumConfig.begin();

  // setup drum controller reader
  drum.begin();
}

void loop()
{
  drum.readData();

  // normally we are a drum...
  processMidiDrum();
  
  // dump debug info to serial
  drum.dumpToSerial();
}
