#include <Wire.h>
#include <Servo.h> 

//  SERVO SETUP (LIFT/TILT) 
Servo highTorqueServo;
const int servoPin = 4;
int basePosition = 1500;     // Starting point (Center)
int maxAngleOffset = 100;    // Maximum movement size (approx 45 degrees)

//  SERVO SETUP (MOP) 
Servo mopServo;      
const int mopServoPin = 12;   

//  SERVO SPEED CONTROL (LIFT/TILT) 
int stepDelay = 30;      // Milliseconds to wait (Higher = Slower)
int stepSize = 1;        // Microseconds to move per step (Lower = Smoother)


//  BACK LEFT 
const int BL_RPWM = 2;
const int BL_LPWM = 3;
const int BL_REN  = 44;
const int BL_LEN  = 45;

//  BACK RIGHT 
const int BR_RPWM = 9;
const int BR_LPWM = 8;
const int BR_REN  = 32;
const int BR_LEN  = 33;

//  FRONT LEFT 
const int FL_RPWM = 7;
const int FL_LPWM = 6;
const int FL_REN  = 42;
const int FL_LEN  = 43;

//  FRONT RIGHT 
const int FR_RPWM = 10;
const int FR_LPWM = 11;
const int FR_REN  = 38;
const int FR_LEN  = 39;

//  IR SENSORS 
const int IR_LEFT  = 50;
const int IR_RIGHT = 51;
const int IR_FRONT = 25; // New TCRT5000 sensor in front

//  SPEED SETTINGS 
int BASE_SPEED = 45;    
int TURN_SPEED = 100;   
int REVERSE_SPEED = 80; 
int PUSH_SPEED = 70;   

// Adjust these if behavior is inverted
const int BLACK = HIGH;
const int WHITE = LOW;
// TCRT5000 usually outputs LOW when it detects a reflection (white/obstacle)
const int OBSTACLE_DETECTED = HIGH; 

void setup() {
  Serial.begin(9600);

  //  Initialize Servos 
  highTorqueServo.attach(servoPin, 500, 2500); 
  highTorqueServo.writeMicroseconds(basePosition); // Set to starting position

  mopServo.attach(mopServoPin, 500, 2500); // Initialize Mop Servo
  mopServo.write(0); // Set mop to starting position

  // Initialize motor pins
  int allPins[] = {
    BL_RPWM, BL_LPWM, BL_REN, BL_LEN,
    BR_RPWM, BR_LPWM, BR_REN, BR_LEN,
    FL_RPWM, FL_LPWM, FL_REN, FL_LEN,
    FR_RPWM, FR_LPWM, FR_REN, FR_LEN
  };
  for (int i = 0; i < 16; i++) {
    pinMode(allPins[i], OUTPUT);
  }

  // Enable all motor drivers
  digitalWrite(BL_REN, HIGH); digitalWrite(BL_LEN, HIGH);
  digitalWrite(BR_REN, HIGH); digitalWrite(BR_LEN, HIGH);
  digitalWrite(FL_REN, HIGH); digitalWrite(FL_LEN, HIGH);
  digitalWrite(FR_REN, HIGH); digitalWrite(FR_LEN, HIGH);

  // Initialize IR pins
  pinMode(IR_LEFT, INPUT);
  pinMode(IR_RIGHT, INPUT);
  pinMode(IR_FRONT, INPUT); // Initialize front IR sensor
}

void loop() {
  //  READ IR SENSORS 
  int left  = digitalRead(IR_LEFT);
  int right = digitalRead(IR_RIGHT);

  //  STOPPING LOGIC: BOTH SENSORS DETECT BLACK 
  if (left == BLACK && right == BLACK) {
    Serial.println("DOUBLE BLACK DETECTED! Stopping bot and sweeping tilt servo smoothly...");
    
    // 1. Stop the bot
    stopBot();
    
    // 2. Perform Smooth Servo Tilt (To max offset OR until IR detects obstacle)
    int finalTiltPosition = basePosition; // Variable to remember where it stops
    
    for (int currentPos = basePosition; currentPos <= (basePosition + maxAngleOffset); currentPos += stepSize) {
      highTorqueServo.writeMicroseconds(currentPos); 
      finalTiltPosition = currentPos; // Update the stopping position
      delay(stepDelay); 
      
      // FIX: Ignore the IR sensor for the first 20 microseconds of movement.
      // This gives the servo a tiny "blind zone" to lift off the resting surface
      // so the sensor doesn't instantly trigger on the next cycle.
      if (currentPos > (basePosition + 20)) {
        if (digitalRead(IR_FRONT) == OBSTACLE_DETECTED) {
          Serial.println("Obstacle/White detected! Stopping tilt early.");
          break; // Stop tilting immediately
        }
      }
    }
    
    // 3. MOP SWEEPING FOR 15 SECONDS
    Serial.println("Tilt complete. Running mop for 15 seconds...");
    unsigned long mopStartTime = millis();
    
    // Run this loop as long as the difference between current time and start time is less than 15000ms (15 sec)
    while (millis() - mopStartTime < 15000) {
      
      // Sweep Forward 180 degrees (Original fast speed)
      for (int angle = 0; angle <= 180; angle += 4) { 
        mopServo.write(angle); 
        delay(10); 
      }
      delay(150); // Momentum pause

      // Sweep Backward to 0 degrees (Original fast speed)
      for (int angle = 180; angle >= 0; angle -= 4) { 
        mopServo.write(angle); 
        delay(10);             
      }
      delay(150); // Momentum pause
    }

    Serial.println("Mop sweep finished. Returning tilt servo to base...");
    
    // 4. Return Tilt Servo smoothly starting from wherever it stopped
    for (int currentPos = finalTiltPosition; currentPos >= basePosition; currentPos -= stepSize) {
      highTorqueServo.writeMicroseconds(currentPos);
      delay(stepDelay); 
    }
    
    delay(1000); // Pause for 1 second at the starting point
    
    // 5. Resume movement
    Serial.println("Servo sequence complete. Pushing hard to cross the mark...");
    goForward(PUSH_SPEED); // Apply the higher burst of power
    delay(600);            // Slightly longer delay to ensure it crosses the line
  } 
  // --- STANDARD LINE FOLLOWING LOGIC ---
  else if (left == WHITE && right == WHITE) {
    // Line is centered between sensors → go straight
    goForward(BASE_SPEED);
  }
  else if (left == BLACK && right == WHITE) {
    // Line drifting left → pivot left
    turnLeft();
  }
  else if (left == WHITE && right == BLACK) {
    // Line drifting right → pivot right
    turnRight();
  }
}

//  STOP ALL MOTORS 
void stopBot() {
  analogWrite(BL_RPWM, 0);
  analogWrite(BL_LPWM, 0);
  analogWrite(FL_RPWM, 0);
  analogWrite(FL_LPWM, 0);
  
  analogWrite(BR_RPWM, 0);
  analogWrite(BR_LPWM, 0);
  analogWrite(FR_RPWM, 0);
  analogWrite(FR_LPWM, 0);
}

//  ALL WHEELS FORWARD 
void goForward(int speed) {
  // Left side forward
  analogWrite(BL_RPWM, 0);
  analogWrite(BL_LPWM, speed);
  analogWrite(FL_RPWM, 0);
  analogWrite(FL_LPWM, speed);
  // Right side forward
  analogWrite(BR_RPWM, speed);
  analogWrite(BR_LPWM, 0);
  analogWrite(FR_RPWM, speed);
  analogWrite(FR_LPWM, 0);
}

//  PIVOT LEFT: left wheels REVERSE, right wheels FORWARD 
void turnLeft() {
  // Left side REVERSE
  analogWrite(BL_RPWM, REVERSE_SPEED);
  analogWrite(BL_LPWM, 0);
  analogWrite(FL_RPWM, REVERSE_SPEED);
  analogWrite(FL_LPWM, 0);
  // Right side FORWARD
  analogWrite(BR_RPWM, TURN_SPEED);
  analogWrite(BR_LPWM, 0);
  analogWrite(FR_RPWM, TURN_SPEED);
  analogWrite(FR_LPWM, 0);
}

//  PIVOT RIGHT: left wheels FORWARD, right wheels REVERSE 
void turnRight() {
  // Left side FORWARD
  analogWrite(BL_RPWM, 0);
  analogWrite(BL_LPWM, TURN_SPEED);
  analogWrite(FL_RPWM, 0);
  analogWrite(FL_LPWM, TURN_SPEED);
  // Right side REVERSE
  analogWrite(BR_RPWM, 0);
  analogWrite(BR_LPWM, REVERSE_SPEED);
  analogWrite(FR_RPWM, 0);
  analogWrite(FR_LPWM, REVERSE_SPEED);
}