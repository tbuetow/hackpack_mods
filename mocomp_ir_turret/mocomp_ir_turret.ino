/*

************************************************************************************
* MIT License
*
* Copyright (c) 2023 Crunchlabs LLC (IRTurret Control Code)
* Copyright (c) 2020-2022 Armin Joachimsmeyer (IRremote Library)

* Permission is hereby granted, free of charge, to any person obtaining a copy
* of this software and associated documentation files (the "Software"), to deal
* in the Software without restriction, including without limitation the rights
* to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
* copies of the Software, and to permit persons to whom the Software is furnished
* to do so, subject to the following conditions:
*
* The above copyright notice and this permission notice shall be included in all
* copies or substantial portions of the Software.
*
* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED,
* INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A
* PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT
* HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF
* CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE
* OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
*
************************************************************************************
*/

//////////////////////////////////////////////////
              //  LIBRARIES  //
//////////////////////////////////////////////////
#include <Arduino.h>
#include <Servo.h>
#include "PinDefinitionsAndMore.h" // Define macros for input and output pin etc.
#include <IRremote.hpp>
#include <Wire.h>
#include <Adafruit_LIS3DH.h>
#include <Adafruit_Sensor.h>

#define DEBUG false

#define LIS3DH_I2C_ADDR 0x18

#define DECODE_NEC  //defines the type of IR transmission to decode based on the remote. See IRremote library for examples on how to decode other types of remote

/*
** if you want to add other remotes (as long as they're on the same protocol above):
** press the desired button and look for a hex code similar to those below (ex: 0x11)
** then add a new line to #define newCmdName 0x11,
** and add a case to the switch statement like case newCmdName: 
** this will let you add new functions to buttons on other remotes!
** the best remotes to try are cheap LED remotes, some TV remotes, and some garage door openers
*/

//defines the specific command code for each button on the remote
#define left 0x8
#define right 0x5A
#define up 0x52
#define down 0x18
#define ok 0x1C
#define cmd1 0x45
#define cmd2 0x46
#define cmd3 0x47
#define cmd4 0x44
#define cmd5 0x40
#define cmd6 0x43
#define cmd7 0x7
#define cmd8 0x15
#define cmd9 0x9
#define cmd0 0x19
#define star 0x16
#define hashtag 0xD

//////////////////////////////////////////////////
          //  PINS AND PARAMETERS  //
//////////////////////////////////////////////////
//this is where we store global variables!
Servo yawServo; //names the servo responsible for YAW rotation, 360 spin around the base
Servo pitchServo; //names the servo responsible for PITCH rotation, up and down tilt
Servo rollServo; //names the servo responsible for ROLL rotation, spins the barrel to fire darts
Adafruit_LIS3DH imu = Adafruit_LIS3DH();

int yawServoVal; //initialize variables to store the current value of each servo
int pitchServoVal = 100;
int rollServoVal;

int pitchTarget = 100; // This is the absolute ECEF pitch target. When initialized on a flat surface, it should be the same as pitchServoVal

int pitchMoveSpeed = 8; //this variable is the angle added to the pitch servo to control how quickly the PITCH servo moves - try values between 3 and 10
int yawMoveSpeed = 90; //this variable is the speed controller for the continuous movement of the YAW servo motor. It is added or subtracted from the yawStopSpeed, so 0 would mean full speed rotation in one direction, and 180 means full rotation in the other. Try values between 10 and 90;
int yawStopSpeed = 90; //value to stop the yaw motor - keep this at 90
int rollMoveSpeed = 90; //this variable is the speed controller for the continuous movement of the ROLL servo motor. It is added or subtracted from the rollStopSpeed, so 0 would mean full speed rotation in one direction, and 180 means full rotation in the other. Keep this at 90 for best performance / highest torque from the roll motor when firing.
int rollStopSpeed = 90; //value to stop the roll motor - keep this at 90

int yawPrecision = 150; // this variable represents the time in milliseconds that the YAW motor will remain at it's set movement speed. Try values between 50 and 500 to start (500 milliseconds = 1/2 second)
int rollPrecision = 158; // this variable represents the time in milliseconds that the ROLL motor with remain at it's set movement speed. If this ROLL motor is spinning more or less than 1/6th of a rotation when firing a single dart (one call of the fire(); command) you can try adjusting this value down or up slightly, but it should remain around the stock value (160ish) for best results.

int pitchMax = 160; // this sets the maximum angle of the pitch servo to prevent it from crashing, it should remain below 180, and be greater than the pitchMin
int pitchMin = 20; // this sets the minimum angle of the pitch servo to prevent it from crashing, it should remain above 0, and be less than the pitchMax

//PID tuning
float pitchPID_K = 0.2;
float pitchPID_I = 0.0005;


long pitchErrorIntegral = 0; // this is the error accumulator for the I term. It's global so that it can be reset when the target position changes

//////////////////////////////////////////////////
              //  S E T U P  //
//////////////////////////////////////////////////
void setup() {
    Serial.begin(115200); // initializes the Serial communication between the computer and the microcontroller

    yawServo.attach(10); //attach YAW servo to pin 10
    pitchServo.attach(11); //attach PITCH servo to pin 11
    rollServo.attach(12); //attach ROLL servo to pin 12

    // Just to know which program is running on my microcontroller
    Serial.println(F("START " __FILE__ " from " __DATE__ "\r\nUsing library version " VERSION_IRREMOTE));

    // Start the receiver and if not 3. parameter specified, take LED_BUILTIN pin from the internal boards definition as default feedback LED
    IrReceiver.begin(9, ENABLE_LED_FEEDBACK);

    Serial.print(F("Ready to receive IR signals of protocols: "));
    printActiveIRProtocols(&Serial);
    Serial.println(F("at pin " STR(9)));


    homeServos(); //set servo motors to home position

    //Init IMU
    if (! imu.begin(LIS3DH_I2C_ADDR)) {
      Serial.println("Couldnt start IMU");
      while (1) yield();
    }
    imu.setRange(LIS3DH_RANGE_4_G);   // 2, 4, 8 or 16 G!
    imu.setPerformanceMode(LIS3DH_MODE_HIGH_RESOLUTION);
    imu.setDataRate(LIS3DH_DATARATE_400_HZ);
    Serial.println("LIS3DH found!");

    if (DEBUG) {
      Serial.print("Range = "); Serial.print(2 << imu.getRange());
      Serial.println("G");
      Serial.print("Performance mode set to: ");
      switch (imu.getPerformanceMode()) {
        case LIS3DH_MODE_NORMAL: Serial.println("Normal 10bit"); break;
        case LIS3DH_MODE_LOW_POWER: Serial.println("Low Power 8bit"); break;
        case LIS3DH_MODE_HIGH_RESOLUTION: Serial.println("High Resolution 12bit"); break;
      }
      Serial.print("Data rate set to: ");
      switch (imu.getDataRate()) {
        case LIS3DH_DATARATE_1_HZ: Serial.println("1 Hz"); break;
        case LIS3DH_DATARATE_10_HZ: Serial.println("10 Hz"); break;
        case LIS3DH_DATARATE_25_HZ: Serial.println("25 Hz"); break;
        case LIS3DH_DATARATE_50_HZ: Serial.println("50 Hz"); break;
        case LIS3DH_DATARATE_100_HZ: Serial.println("100 Hz"); break;
        case LIS3DH_DATARATE_200_HZ: Serial.println("200 Hz"); break;
        case LIS3DH_DATARATE_400_HZ: Serial.println("400 Hz"); break;

        case LIS3DH_DATARATE_POWERDOWN: Serial.println("Powered Down"); break;
        case LIS3DH_DATARATE_LOWPOWER_5KHZ: Serial.println("5 Khz Low Power"); break;
        case LIS3DH_DATARATE_LOWPOWER_1K6HZ: Serial.println("1.6 Khz Low Power"); break;
      }
    }

}

////////////////////////////////////////////////
              //  L O O P  //
////////////////////////////////////////////////

void loop() {

    /*
    * Check if received data is available and if yes, try to decode it.
    */
    if (IrReceiver.decode()) {

        /*
        * Print a short summary of received data
        */
        IrReceiver.printIRResultShort(&Serial);
        IrReceiver.printIRSendUsage(&Serial);
        // if (IrReceiver.decodedIRData.protocol == UNKNOWN) { //command garbled or not recognized
        //     Serial.println(F("Received noise or an unknown (or not yet enabled) protocol - if you wish to add this command, define it at the top of the file with the hex code printed below (ex: 0x8)"));
        //     // We have an unknown protocol here, print more info
        //     IrReceiver.printIRResultRawFormatted(&Serial, true);
        // }
        // Serial.println();

        /*
        * !!!Important!!! Enable receiving of the next value,
        * since receiving has stopped after the end of the current received data packet.
        */
        IrReceiver.resume(); // Enable receiving of the next value


        /*
        * Finally, check the received data and perform actions according to the received command
        */

        switch(IrReceiver.decodedIRData.command){ //this is where the commands are handled

            case up://pitch up
              upMove(1);
              break;
            
            case down://pitch down
              downMove(1);
              break;

            case left://fast counterclockwise rotation
              leftMove(1);
              break;
            
            case right://fast clockwise rotation
              rightMove(1);
              break;
            
            case ok: //firing routine 
              fire();
              //Serial.println("FIRE");
              break;
              
            case star:
              fireAll();
              delay(50);
              break;

        }
    }
    movePitchPID();
    delay(5);
}


void shakeHeadYes(int moves = 3) {
      Serial.println("YES");
    int startAngle = pitchServoVal; // Current position of the pitch servo
    int lastAngle = pitchServoVal;
    int nodAngle = startAngle + 20; // Angle for nodding motion

    for (int i = 0; i < moves; i++) { // Repeat nodding motion three times
        // Nod up
        for (int angle = startAngle; angle <= nodAngle; angle++) {
            pitchServo.write(angle);
            delay(7); // Adjust delay for smoother motion
        }
        delay(50); // Pause at nodding position
        // Nod down
        for (int angle = nodAngle; angle >= startAngle; angle--) {
            pitchServo.write(angle);
            delay(7); // Adjust delay for smoother motion
        }
        delay(50); // Pause at starting position
    }
}

void shakeHeadNo(int moves = 3) {
    Serial.println("NO");
    int startAngle = pitchServoVal; // Current position of the pitch servo
    int lastAngle = pitchServoVal;
    int nodAngle = startAngle + 60; // Angle for nodding motion

    for (int i = 0; i < moves; i++) { // Repeat nodding motion three times
        // rotate right, stop, then rotate left, stop
        yawServo.write(140);
        delay(190); // Adjust delay for smoother motion
        yawServo.write(yawStopSpeed);
        delay(50);
        yawServo.write(40);
        delay(190); // Adjust delay for smoother motion
        yawServo.write(yawStopSpeed);
        delay(50); // Pause at starting position
    }
}

void leftMove(int moves){
    for (int i = 0; i < moves; i++){
        yawServo.write(yawStopSpeed + yawMoveSpeed); // adding the servo speed = 180 (full counterclockwise rotation speed)
        delay(yawPrecision); // stay rotating for a certain number of milliseconds
        yawServo.write(yawStopSpeed); // stop rotating
        delay(5); //delay for smoothness
        Serial.println("LEFT");
  }

}

void rightMove(int moves){ // function to move right
  for (int i = 0; i < moves; i++){
      yawServo.write(yawStopSpeed - yawMoveSpeed); //subtracting the servo speed = 0 (full clockwise rotation speed)
      delay(yawPrecision);
      yawServo.write(yawStopSpeed);
      delay(5);
      Serial.println("RIGHT");
  }
}

void upMove(int moves){
  for (int i = 0; i < moves; i++){
      if(pitchServoVal > pitchMin){//make sure the servo is within rotation limits (greater than 10 degrees by default)
        pitchServoVal = pitchServoVal - pitchMoveSpeed; //decrement the current angle and update
        updateTargetPitch();
        pitchServo.write(pitchServoVal);
        delay(50);
        Serial.println("UP");
      }
  }
}

void downMove (int moves){
  for (int i = 0; i < moves; i++){
        if(pitchServoVal < pitchMax){ //make sure the servo is within rotation limits (less than 175 degrees by default)
        pitchServoVal = pitchServoVal + pitchMoveSpeed;//increment the current angle and update
        updateTargetPitch();
        pitchServo.write(pitchServoVal);
        delay(50);
        Serial.println("DOWN");
      }
  }
}

/**
 * fire does xyz
 */
void fire() { //function for firing a single dart
    rollServo.write(rollStopSpeed + rollMoveSpeed);//start rotating the servo
    delay(rollPrecision);//time for approximately 60 degrees of rotation
    rollServo.write(rollStopSpeed);//stop rotating the servo
    delay(5); //delay for smoothness
    Serial.println("FIRING");
}

void fireAll() { //function to fire all 6 darts at once
    rollServo.write(rollStopSpeed + rollMoveSpeed);//start rotating the servo
    delay(rollPrecision * 6); //time for 360 degrees of rotation
    rollServo.write(rollStopSpeed);//stop rotating the servo
    delay(5); // delay for smoothness
    Serial.println("FIRING ALL");
}

void homeServos(){
    yawServo.write(yawStopSpeed); //setup YAW servo to be STOPPED (90)
    delay(20);
    rollServo.write(rollStopSpeed); //setup ROLL servo to be STOPPED (90)
    delay(100);
    pitchServo.write(100); //set PITCH servo to 100 degree position
    delay(100);
    pitchServoVal = 100; // store the pitch servo value
    updateTargetPitch(); //update for the PID controller too.
    Serial.println("HOMING");
}

int getImuPitch(){
  static sensors_event_t event;
  imu.getEvent(&event);
  float accel_y = event.acceleration.y; // positive Y is up
  float accel_z = event.acceleration.z; // positive Z is out from top of board
  int pitch_angle = (atan(accel_z / accel_y) + PI/2) * 180 / PI; //rotate so that forward is pi/2 rad (90 deg) and straight down is 0 rad (0 deg)
  return pitch_angle;
}

void updateTargetPitch(){
  pitchTarget = pitchServoVal;
  pitchErrorIntegral = 0;
}

int getEstimatedPitch(){
  int estPitch = getImuPitch();
  return estPitch;
}

int computePitchPID(){ // This is a position controller
  const int Imax = 5; //contrain I correction to this many degrees
  int dT = calcTimeDiff();

  //Get error term
  int pitchEstimated = getEstimatedPitch();
  int pitchError = pitchTarget - pitchEstimated;
  
  //Calculate P term
  int pitchPCorrection = pitchError * pitchPID_K;
  
  //Calculate I term
  pitchErrorIntegral += pitchError * dT; //dT is in milliseconds. Less floats to take up RAM and slow math, but makes I coeff 1/1000 of expected.
  int pitchICorrection = constrain(pitchErrorIntegral * pitchPID_I, -Imax, Imax);
  pitchErrorIntegral *= .5; //leaky I term to prevent windup
  
  //Accumulate corrections
  int pitchOutput = pitchServoVal + pitchPCorrection + pitchICorrection;

  if (DEBUG){
    Serial.print(" dT: "); Serial.print(dT);
    Serial.print("\tTarget: "); Serial.print(pitchTarget);
    Serial.print("\tEstimated: "); Serial.print(pitchEstimated);
    Serial.print("\tError: "); Serial.print(pitchError);
    Serial.print("\tP term: "); Serial.print(pitchPCorrection);
    Serial.print("\tI int: "); Serial.print(pitchErrorIntegral);
    Serial.print("\tI term: "); Serial.print(pitchICorrection);
    Serial.print("\tOutput: "); Serial.print(pitchOutput);
    Serial.println();
  }
  return pitchOutput;
}

void movePitchPID(){
  int newPitch = computePitchPID();
  pitchServoVal = constrain(newPitch, pitchMin, pitchMax);
  pitchServo.write(pitchServoVal);
  delay(50);
}

int calcTimeDiff(){
  static unsigned long tic = 0;
  static unsigned long toc = 0;
  tic = toc;
  toc = millis();
  return toc - tic;
}
