// This script just moves the motor a fixed number of steps.
// From this we can determine the number of steps per ince.

// Based on this super scientific results 10,000 steps is 2.5 inches.

#define STEP_PIN 10
#define DIRECTION_PIN 9

uint16_t motorSpeed = 100;

void stepMotor(uint16_t d = motorSpeed) { 
  digitalWrite(STEP_PIN, HIGH);
  delayMicroseconds(d);
  digitalWrite(STEP_PIN, LOW);
  delayMicroseconds(d);
}

void moveStepper(int numberOfSteps) {
  for( int i = 0; i < numberOfSteps; i++ ) { 
    stepMotor();
 }
}

void setup() {
  Serial.begin(9600);
  Serial.println("In setup");
  Serial.println( __DATE__ " " __TIME__ );
  pinMode( STEP_PIN, OUTPUT );
  pinMode( DIRECTION_PIN, OUTPUT); 

  moveStepper(10000);
  
  Serial.println("Done with setup");
}

void loop() {}

