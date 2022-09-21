/*
  Alarm Monitoring Sensor (powered by mySensors on a Arduino Nano)
  By: Jean-Francois Auger
  Contact: nechry@gmail.com
  License: GNU LGPL 2.1+
  https://github.com/nechry/AlarmMonitoringSensor/

  Purpose:

  This is a monitoring system for home alarm control panel, base on mySensors
  I use Light Sensors to monitor alarm leds:
    operation mode,
	trigger,
	bell is activated,
	maintenance.
  The LEDs have tree states, Off, blink and On

  INPUT:
  Pin A0, A1, A2 and A3 for led sensor.
  OUTPUT:
  Pins 8 and 7 to report the current operation mode.
  Pin 3 report Trigger status
  Pin 4 report Alarm status
  Pin 5 report Maintenace statue

  NRF24L01+:
  See http://www.mysensors.org/build/connect_radio for more details
  9	  CE
  10	CSN/CS
  13	SCK
  11	MOSI
  12	MISO
  2	  IRQ
  Require lib mysensors version 2.3.2
*/

// Enable debug prints to serial monitor
//#define MY_DEBUG

// Enable and select radio type attached
#define MY_RADIO_RF24
//#define MY_RADIO_NRF5_ESB
//#define MY_RADIO_RFM69
//#define MY_RADIO_RFM95

// Enable repeater functionality for this node
#define MY_REPEATER_FEATURE

#include <MySensors.h>

#define Operation_Child_Id 0
#define Trigger_Child_Id 1
#define Bell_Child_Id 2
#define Maintenance_Child_Id 3

// input sensors pins
#define Operation_Sensor_Pin 0
#define Trigger_Sensor_Pin 1
#define Bell_Sensor_Pin 2
#define Maintenance_Sensor_Pin 3

//output leds to repesente operation
#define OperationFull_Led_Pin 8
#define OperationPartial_Led_Pin 7
#define Trigger_Led_Pin 3
#define Bell_Led_Pin 4
#define Maintenance_Led_Pin 5

unsigned long SLEEP_TIME = 5; // Sleep time between reads (in milliseconds)

enum alarm_status {
  Unknown,
  Desactivated,
  Blinking,
  Activated
};

MyMessage messageOperation(Operation_Child_Id, V_LIGHT_LEVEL);
MyMessage messageTrigger(Trigger_Child_Id, V_LIGHT_LEVEL);
MyMessage messageRaise(Bell_Child_Id, V_LIGHT_LEVEL);
MyMessage messageMaintenance(Maintenance_Child_Id, V_LIGHT_LEVEL);

int lastLedOperation = Unknown;
int lastLedTrigger = Unknown;
int lastLedBell = Unknown;
int lastLedMaintenance = Unknown;

uint8_t levelA0Threshold = 70;
uint8_t levelA1Threshold = 70;
uint8_t levelA2Threshold = 70;
uint8_t levelA3Threshold = 70;

// define, how many sensors
#define SENSORS 4
#define LEDS 5

// add each led sensors
int analogInPins[SENSORS] = { Operation_Sensor_Pin, Trigger_Sensor_Pin, Bell_Sensor_Pin, Maintenance_Sensor_Pin };
// add each led outputs
int ledPins[LEDS] = { OperationFull_Led_Pin, OperationPartial_Led_Pin, Trigger_Led_Pin, Bell_Led_Pin, Maintenance_Led_Pin };
// add each level Thresholds
uint8_t levelThresholds[SENSORS] = { levelA0Threshold, levelA1Threshold, levelA2Threshold, levelA3Threshold };

// the current state of each sensor (off, blink, on)
int states[SENSORS];
int pending_states[SENSORS];
// how many blinks have been counted on each sensor
int blinks[SENSORS];

// Serial port speed
#define SERIAL_SPEED 115200

// Reporting interval on serial port, in milliseconds
#define REPORT_INTERVAL 5000

unsigned long loops = 0;
unsigned long next_print = millis() + REPORT_INTERVAL;
bool lastLedOperation_skip = true;

void presentation()
{
  // Send the sketch version information to the gateway and Controller
  sendSketchInfo("Alarm Monitoring Sensor", "2.0", true);

  // Register all sensors to gateway (they will be created as child devices)
  present(Operation_Child_Id, S_LIGHT_LEVEL);
  sleep(250);
  present(Trigger_Child_Id, S_LIGHT_LEVEL);
  sleep(250);
  present(Bell_Child_Id, S_LIGHT_LEVEL);
  sleep(250);
  present(Maintenance_Child_Id, S_LIGHT_LEVEL);
  sleep(250);
}

void before() {
  /* initialize each led sensor */
  for (int index = 0; index < SENSORS; index++) {
    //restore level Threshold
    uint8_t levelThreshold = loadState(index);
    //ensure level is ok
    if (levelThreshold > 99) {
      levelThreshold = 99;
    }
    if (levelThreshold < 0) {
      levelThreshold = 0;
    }
    levelThresholds[index] = levelThreshold;
    Serial.print("Set level threshold to: ");
    Serial.println(levelThreshold);
    //init states
    states[index] = HIGH;
    pending_states[index] = HIGH;
    blinks[index] = 0;
  }
  //setup led status as output for mode
  for (int index = 0; index < LEDS; index++) {
    pinMode(ledPins[index], OUTPUT);
    digitalWrite(ledPins[index], HIGH);
    delay(100);
    //desactivate leds
    digitalWrite(ledPins[index], LOW);
    delay(100);
  }
  digitalWrite(ledPins[1], HIGH);
  //do a small blink effect as startup completed
  digitalWrite(ledPins[0], LOW);
  delay(250);
  digitalWrite(ledPins[0], HIGH);
  delay(250);
  digitalWrite(ledPins[0], LOW);
  delay(250);
  digitalWrite(ledPins[0], HIGH);
}

void setup() {
}

void loop() {
  // go through each LED sensor
  for (int index = 0; index < SENSORS; index++) {
    // read current value
    int16_t lightLevel = (1023 - analogRead(analogInPins[index])) / 10.23;
    //Serial.println(lightLevel); // start marker
    int ledState = lightLevel < levelThresholds[index] ? LOW : HIGH ;
    // check if state is changed:
    if (ledState != states[index]) {
      states[index] = ledState;
      // no light to light shows as HIGH to LOW change on the pin,
      // as the photodiode starts to conduct
      if (ledState == LOW) {
        blinks[index] += 1;
      }
    }
  }

  // Every 10000 rounds (quite often), check current uptime
  // and if REPORT_INTERVAL has passed, print out current values.
  // If this would take too long time (slow serial speed) or happen
  // very often, we could miss some blinks while doing this.
  loops++;
  if (loops == 10000) {
    loops = 0;
    if (millis() >= next_print) {
      next_print += REPORT_INTERVAL;
      Serial.println("+"); // start marker
      for (int index = 0; index < SENSORS; index++) {
        //print a report of led current bink detection
        Serial.print(index, DEC);
        Serial.print(" ");
        Serial.print(blinks[index], DEC);
        Serial.print(" ");
        //led interpreter
        int currentLedStatus = Unknown;
        if (blinks[index] > 1) {
          currentLedStatus = Blinking;
          Serial.println("Partial");
        }
        else if (states[index] == LOW) {
          currentLedStatus = Activated;
          Serial.println("Activated");
        }
        else {
          currentLedStatus = Desactivated;
          Serial.println("Desactivated");
        }
        //report to gateway for each sensor if changes
        switch (index) {
          case 0 :
            {
              //check last alarm mode
              if (currentLedStatus != lastLedOperation) {
                if (lastLedOperation_skip) {
                  lastLedOperation_skip = false;
                }
                else {
                  lastLedOperation_skip = true;
                  //reset led mode
                  digitalWrite(OperationFull_Led_Pin, HIGH);
                  digitalWrite(OperationPartial_Led_Pin, HIGH);
                  //activate leds
                  switch (currentLedStatus) {
                    case Blinking:
                      digitalWrite(OperationPartial_Led_Pin, LOW);
                      break;
                    case Activated:
                      digitalWrite(OperationFull_Led_Pin, LOW);
                      break;
                    default:
                      Serial.println("Unknown");
                      break;
                  }
                  //send a report to the gateway
                  send(messageOperation.set(currentLedStatus));
                  //save led operation
                  lastLedOperation = currentLedStatus;
                }
              }
              break;
            }
          case 1:
            {
              //check last alarm trigger
              if (lastLedTrigger != currentLedStatus) {
                //send report to gateway
                send(messageTrigger.set(currentLedStatus));
                //save led trigger
                lastLedTrigger = currentLedStatus;
                digitalWrite(Trigger_Led_Pin, currentLedStatus != Desactivated ? HIGH : LOW);
              }
              break;
            }
          case 2:
            {
              //check last alarm raise
              if (lastLedBell != currentLedStatus) {
                //send report to gateway
                send(messageRaise.set(currentLedStatus));
                //save led bell state
                lastLedBell = currentLedStatus;
                digitalWrite(Bell_Led_Pin, currentLedStatus != Desactivated ? HIGH : LOW);
              }
              break;
            }
          case 3:
            {
              if (lastLedMaintenance != currentLedStatus) {
                //send report to gateway
                send(messageMaintenance.set(currentLedStatus));
                //save led maintenance
                lastLedMaintenance = currentLedStatus;
                digitalWrite(Maintenance_Led_Pin, currentLedStatus != Desactivated ? HIGH : LOW);
              }
              break;
            }
        }
        //reset the blink counter
        blinks[index] = 0;
      }
      Serial.println("-"); // end marker
    }
  }
}



void receive(const MyMessage &message) {
  // We only expect one type of message from controller. But we better check anyway.
  if (message.getType() == V_VAR1) {
    // Change level threshold
    uint8_t levelThreshold = message.getInt();
    uint8_t sensor = message.getSensor();
    // Store state in eeprom
    saveState(sensor, levelThreshold);
    levelThresholds[sensor] = levelThreshold;
    // Write some debug info
    Serial.print("Change level threshold for: ");
    Serial.print(sensor);
    Serial.print(" : ");
    Serial.println(levelThreshold);
  }
}
