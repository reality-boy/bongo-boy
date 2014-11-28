#ifndef BONGOPIEZODRUM_H
#define BONGOPIEZODRUM_H

//------------------
// piezo drum class

/*
*/

class bongoPiezoDrum
{
public:
  bongoPiezoDrum(int pin)
    : kSensorPin(pin)
    , kSensorLast(0)
    , kSensorMax(0)
    , kSensorTimeout(0)
    , kSensorPeakTimeout(0)
    , kSensorInProgress(false)
  {}
  
  int process()
  {
    //kSensorHit = 0;
  
    unsigned long time = millis();
    int kSensor = analogRead(kSensorPin);
    bool kSensorDetected = kSensor != 0 && abs(kSensorLast - kSensor) > kSensorThreshold;
    kSensorLast = kSensor;
  
    if(time < kSensorTimeout)
    {
      if(time < kSensorPeakTimeout && kSensorMax < kSensor)
        kSensorMax = kSensor;
      else if(kSensorInProgress)
      {
        kSensorInProgress = false;
        kSensorHit = kSensorMax;
      }
    }
    else if(kSensorDetected)
    {
      kSensorTimeout = time + kSensorDebounceTime;
      kSensorPeakTimeout = time + kSensorPeakTime;
      kSensorMax = kSensor;
      kSensorInProgress = true;
    }

    return kSensorHit;
  }

  // returns 1-1023 if a hit was detected,
  // otherwise returs 0
  int getHit(bool clear)
  { 
    int hit = kSensorHit; 
    if(clear)
      kSensorHit = 0; 
    return hit; 
  }
  
 private:
 
  static const int kSensorPeakTime = 10;
  static const int kSensorDebounceTime = 100;
  static const int kSensorThreshold = 20;

  int kSensorPin; // the piezo

  int kSensorLast;
  int kSensorMax;
  int kSensorHit;
  unsigned long kSensorTimeout;
  unsigned long kSensorPeakTimeout;
  bool kSensorInProgress;
};

#endif //BONGOPIEZODRUM_H
