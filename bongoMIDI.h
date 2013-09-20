#ifndef BONGOMIDI_H
#define BONGOMIDI_H

//---------------
// MIDI device communication
// see http://www.midi.org/techspecs/midimessages.php for more info

/*
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
*/

// MIDI drum notes that may be interesting to play
enum midiDrumNotes {
  
  // from my Yamaha PSR E323 keyboard
  // starts at 13, if you are interested
  BRUSH_SWIRL = 26,    // key off
  BRUSH_TAP_SWIRL = 28, // key off
  SNARE_ROLL = 29,     // playes a roll untill key off
  
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
  
  SAMBA_WHISTLE_H = 71, // key off
  SAMBA_WHISTLE_L = 72, // key off
  GUIRO_LONG = 74,      // key off
  
  // goes on to 81 if you are interested
  // 84 for my Yamaha keyboard
};

// MIDI commands always have there high byte set
// and are located in the upper 4 byte block
// the lower 4 bytes are reserved for the 16 MIDI channels
enum midiCommands {
  NOTE_OFF         = 0x80, // Stop playing a note, not worth much with drums
  NOTE_ON          = 0x90, // Play a note
  POLY_AFTERTOUCH  = 0xA0, // Modify note velocity ?
  CONTROL_CHANGE   = 0xB0,
  PROGRAM_CHANGE   = 0xC0,
  CHANNEL_PRESSURE = 0xD0, // Modify all note velocity (sustain?)
  PITCH_WHEEL      = 0xE0, //Modify all note pitch
  SYSTEM_EXCLUSIVE = 0xF0  // SysEx, or custom controll command
};
//Equation for setting midi volume in a linear fashion: 40 log(Volume/127)

enum controlChangeCommands {
  BANK_SELECT = 0x00,
  MODULATION_LEVER = 0x01,
  FOOT_CONTROLLER = 0x04,
  CHANNEL_VOLUME = 0x07,
  EXPRESSION_CONTROLLER = 0x11,
  DAMPER_PEDAL = 0x40,
  ALL_CONTROLLERS_OFF = 0x79,
};

// MIDI devices support 16 communication channels so 16 devices 
// can talk to one device.  Channel 16 is for configuration of the 
// device and channel 10 is a special dedicated drum channel
enum midiChannels {
  CHAN_1  = 0x00,
  CHAN_10 = 0x09, // drum channel
  CHAN_11 = 0x10,
  CHAN_16 = 0x0F, // config channel?
};

//-----------------

class bongoMIDI 
{
public:

  SoftwareSerial sSerial;
  
  bongoMIDI(int rx, int tx) : sSerial(rx, tx) {}

  // MIDI is simple, we just transmit out the serial port at 31250 KB
  void begin()
  {
    // Setup serial port to talk over MIDI
    // use software based serial port
    // not that usefull for recieving, but we dont use that anyway
    sSerial.begin(31250);
    
    // reset the midi channel to defaults
    delay(10);
    transmitMIDI(CONTROL_CHANGE | CHAN_10, ALL_CONTROLLERS_OFF, 0);
    
    // select the default drum kit
    delay(10);
    transmitMIDI_2(PROGRAM_CHANGE | CHAN_10, 109);
  }

  // Talking is just as simple, toss out a command (command | channel) and 
  // two data bytes and you are done.
  void transmitMIDI(int cmd, int data_1, int data_2)
  {
    sSerial.write(cmd    & 0xFF);
    sSerial.write(data_1 & 0xFF);
    sSerial.write(data_2 & 0xFF);
  }
  void transmitMIDI_2(int cmd, int data)
  {
    sSerial.write(cmd    & 0xFF);
    sSerial.write(data   & 0xFF);
  }
};



#endif // BONGOMIDI_H
