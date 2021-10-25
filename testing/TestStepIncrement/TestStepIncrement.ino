
//
// 10/22/2021
//
// After questioning the accuracy of the jig in terms of distance
// moved I decided to make a more robust measurement of the 
// distance traveled.  I also wanted to make sure that I was moving
// the smallest possible distance with every pulse.  As it turns
// out though the Sparkfun "Big Easy Driver" defaults to 1/16th steps
// when you don't access the MS1, MS2 and MS3 pins.  (They are 
// all tied high by default.)
//
// Using a Dial Indicator (https://www.amazon.com/gp/product/B002YPHT76/)
// I measured the precise thousands of an inch that the device
// moved over multiple distances to get - as close as I could determine -
// the number of steps required to move one thousandth of an inch.
//
// This script assumes you are using the big easy driver by sparkfun:
// https://www.sparkfun.com/products/12859
//


#define STEP_PIN 10
#define DIRECTION_PIN 9
#define ENABLE_PIN A0
#define MS1 A3
#define MS2 A4
#define MS3 A5

uint16_t motorSpeed = 500;
bool enabled = true;


void stepMotor(uint16_t d = motorSpeed) { 
  digitalWrite(STEP_PIN, HIGH);
  delayMicroseconds(d);
  digitalWrite(STEP_PIN, LOW);
  delayMicroseconds(d);
}

void moveStepper(int numberOfSteps, int directionValue = LOW, int ms1Value = LOW, int ms2Value = LOW, int ms3Value = LOW) {
  digitalWrite(MS1, ms1Value);
  digitalWrite(MS2, ms2Value);
  digitalWrite(MS3, ms3Value);
  digitalWrite(DIRECTION_PIN, directionValue);
  
  for( int i = 0; i < numberOfSteps; i++ ) { 
    stepMotor();
 }
}

int thousToSteps(int thous) { 
   float steps = thous * 4.19;
   
   return ceil(steps);
}

void resetMotorPins()
{
  digitalWrite(STEP_PIN, LOW);
  digitalWrite(DIRECTION_PIN, LOW);
  digitalWrite(MS1, LOW);
  digitalWrite(MS2, LOW);
  digitalWrite(MS3, LOW);

  if (enabled) { 
    digitalWrite(ENABLE_PIN, LOW);
  } else {
    digitalWrite(ENABLE_PIN, HIGH);
  }
}

void setup() {
  Serial.begin(9600);
  Serial.println("In setup");
  Serial.println( __DATE__ " " __TIME__ );
  pinMode(STEP_PIN, OUTPUT );
  pinMode(DIRECTION_PIN, OUTPUT); 
  pinMode(MS1, OUTPUT);
  pinMode(MS2, OUTPUT);
  pinMode(MS3, OUTPUT);
  pinMode(ENABLE_PIN, OUTPUT);

  resetMotorPins(); 
  
  Serial.println("Begin motor test");
  Serial.println();
  //Print function list for user selection
  Serial.println("Enter number for control option:");
  Serial.println("1. Turn at 1/8th microstep mode.");
  Serial.println("2. Reverse direction at 1/8th microstep mode.");
  Serial.println("3. Turn at 1/16th microstep mode.");
  Serial.println("4. Reverse turn at 1/16th microstep mode.");
  Serial.println("5. Toggle enable of motor.");
  Serial.println("6. Move forward 0.25 inches");
  Serial.println("7. Move backwards 0.25 inches");
  Serial.println("8. Increase distance to travel.");
  Serial.println("9. Decrease distance to travel.");
  Serial.println();
}

int distance_to_move = 500;

// 500 is 0.17"

void loop() {
  int available;
  while(available = Serial.available()) {
      Serial.print( "Bytes waiting for me: ");
      Serial.println(available);
      char user_input = Serial.read(); //Read user input and trigger appropriate function

      if (available > 1) {
        int additional = 1;
        while (additional < available) { 
          Serial.print( "Additional data: ");
          Serial.println( Serial.read());
          additional += 1; 
        }
      }
      
      if (user_input =='1')
      {
         Serial.println( "Moving stepper forward 1/8th step");
         moveStepper(distance_to_move, LOW, HIGH, HIGH, LOW);

         if (!enabled) { 
           Serial.println( "Enable is off; suppressing action." );
         }
      }
      else if(user_input =='2')
      {
         Serial.println( "Moving stepper backward 1/8th step");
         moveStepper(distance_to_move, HIGH, HIGH, LOW, LOW);
         
         if (!enabled) { 
           Serial.println( "Enable is off; suppressing action." );
         }
      }
      else if(user_input =='3')
      {
          Serial.println( "Moving stepper forward 1/16th step");
          moveStepper(distance_to_move, LOW, HIGH, HIGH, HIGH);         
          if (!enabled) { 
            Serial.println( "Enable is off; suppressing action." );
          }
      }
      else if(user_input =='4')
      {
          Serial.println( "Moving stepper backward 1/16th step");
          moveStepper(distance_to_move, HIGH, HIGH, HIGH, HIGH);         
          if (!enabled) { 
           Serial.println( "Enable is off; suppressing action." );
         }
      }
      else if(user_input == '5') 
      {
         Serial.print( "Changing enabled pin;  motors active: ");
         enabled = !enabled;
         if (enabled) { 
            Serial.println( "Yes");
         } else {
            Serial.println( "No");
         }
      } 
      else if(user_input == '6') { 
        int steps = thousToSteps(250);
        Serial.print( "Moving stepper forward 0.25 inches: ");
        Serial.println( steps);
        moveStepper(steps, LOW, HIGH, HIGH, HIGH);

         if (!enabled) { 
           Serial.println( "Enable is off; suppressing action." );
         }
      }
      else if(user_input == '7') {
        int steps = thousToSteps(250);
        Serial.print( "Moving stepper backward 0.25 inches: ");
        Serial.println( steps);
        moveStepper(steps, HIGH, HIGH, HIGH, HIGH);

        if (!enabled) { 
           Serial.println( "Enable is off; suppressing action." );
        }
      }
       else if(user_input == '8') { 
        distance_to_move += 100;
        Serial.print( "Distance to move is now: ");
        Serial.println(distance_to_move);
      }
      else if(user_input == '9') { 
        distance_to_move -= 100;
        Serial.print( "Distance to move is now: ");
        Serial.println(distance_to_move);
      }
      else
      {
        Serial.print("Invalid option entered;  read: '");
        Serial.print(user_input);
        Serial.println("'");
        Serial.println("Ensure serial mode has no newline enabled!");
      }
      resetMotorPins();
  }
}
