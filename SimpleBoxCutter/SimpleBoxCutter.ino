// This script drives the behavior of the simple box cutter.  
//
// 8/4/2017: First version
// 7/10/2018: Minor fixes - typos and first finger is too wide.
//   Recording the flat top blade that I like. 
//   Slop: 2, Kerf: 123

// Hardware:
// 1/  A sparkfun "Basic 20x4 Character LCD - Black on Green 5V".  This is
// an HD47780-based display.  
// 2/  A Nema 17 stepper motor.  https://www.sparkfun.com/products/9238  
// 3/  A Big Easy Driver.  https://www.sparkfun.com/products/12859

// LCD Pins:
//
// LCD RS pin to digital pin 12
// LCD Enable pin to digital pin 11
// LCD D4 pin to digital pin 5
// LCD D5 pin to digital pin 4
// LCD D6 pin to digital pin 3
// LCD D7 pin to digital pin 2
// LCD R/W pin to ground
// LCD VSS pin to ground
// LCD VCC pin to 5V
// 
// Input pins:
//   6  - Left button
//   7  - Right button
//   8  - Center button
//   13 - Home limit switch
//
// Stepper Pins:
//   9  - Step
//   10 - Direction
//

//
//  The Finger Joints look like this:
//
//        "Positive"               "Inverse"
//     isCutFirst = True        isCutFirst = False
//    +-----------------+    +---------------+
//    |                 |    |               |
//    |                 |    |               |
//    |                 |    |               |
//    |                 |    |               |
//    |         +-------+    |               |
//    |         |    +-------+               |
//    |         |    |                       |
//    |         |    |                       |
//    |         |    +-------+               |
//    |         +-------+    |               |
//    |                 |    |               |
//        ... 
//
// There are 2 values in use when you are doing a pair of Finger and Valley. 
// The first is how much to cut - this is the size of the valley.  The second
// is the amount to move giving you the size of the finger.
// The finger needs to be smaller than the valley by just a little bit.
// The value is determined by 


//  When "Positive":
//    distanceRemainingToCut = initialDistanceToCut;
//    distanceRemainingToMove = distanceToMove;
//
//  When "Inverse":
//    distanceRemainingToCut = 0;
//    distanceRemainingToMove = initialDistanceToMove;

//  Blade starts to the right of the wood here so the first "cut" is on empty space.
// 
//    initialDistanceToCut = slot + (slop);  
//    initialDistanceToMove = slot - (slop + slop) + kerf;  // 07/10/2018 - Added another slop here as first finger was too tight.
//    distanceToCut = slot + (slop + slop) - kerf;
//    distanceToMove = slot - (slop + slop) + kerf;
 
#include <LiquidCrystal.h>
#include <EEPROM.h>   

#define BUTTON_LEFT_PIN 6
#define BUTTON_RIGHT_PIN 7
#define BUTTON_DOWN_PIN 8
#define BUTTON_UP_PIN 13
#define STEP_PIN 10
#define DIRECTION_PIN 9
#define MOTOR_ENABLE_PIN A0

/* Home switch coded up partially, but never really used. */
#define HOME_LIMIT_SWITCH_PIN A1

// All units are in thousands of an inch.
// So, for example, 125 = 0.125"

// Default kerf size of blade
uint16_t kerf = 123;

uint16_t slot = 500;
uint16_t slop = 2;

// Computed based on the configuration options.  Set in determineMovementValues.
uint16_t initialDistanceToCut;
uint16_t initialDistanceToMove;
uint16_t distanceToCut;
uint16_t distanceToMove;

uint16_t distanceRemainingToCut = 0;
uint16_t distanceRemainingToMove = 0;

// Default speed for movement.  This is in microseconds and captures the amount of time
// to wait inbetween low and high pulses.  A larger number indicates slower speed.
uint16_t motorSpeed = 100;

// Initiali the LCD.  Pin numbers in the below match the earlier comments.
LiquidCrystal lcd(12, 11, 5, 4, 3, 2);

// Creating characters for display is pretty straightforward.
// Use this site:  http://www.quinapalus.com/hd44780udg.html
// for all of the heavy lifting.
byte thumbUpIcon[6][8] = {
 {B00100,B00011,B00100,B00011,B00100,B00011,B00010,B00001},
 {B00000,B00000,B00000,B00000,B00000,B00000,B00000,B00011},
 {B00000,B00000,B00000,B00000,B00000,B00000,B00001,B11110},
 {B00000,B01100,B10010,B10010,B10001,B01000,B11110,B00000},
 {B00010,B00010,B00010,B00010,B00010,B01110,B10000,B00000},
 {B00000,B00000,B00000,B00000,B00000,B10000,B01000,B00110}
};

void registerThumbUp() { 
  for( int i =0; i < 6; i++ ) { 
    lcd.createChar(i, thumbUpIcon[i]);
  }
}

bool isHomeSwitchHit() {
  return digitalRead( HOME_LIMIT_SWITCH_PIN ) == LOW;
}

bool isLeftButtonHit() {
  return digitalRead( BUTTON_LEFT_PIN ) == LOW;
}

bool isRightButtonHit() {
  return digitalRead( BUTTON_RIGHT_PIN ) == LOW;
}

bool isDownButtonHit() {
  return digitalRead( BUTTON_DOWN_PIN ) == LOW;
}

bool isUpButtonHit() {
  return digitalRead( BUTTON_UP_PIN ) == LOW;
}

void waitForLeftButtonUp() {
  delay(10);
  while( digitalRead( BUTTON_LEFT_PIN ) == LOW ) { 
    delay(1);
  }
  delay(50);
}

void waitForDownButtonUp() {
  delay(10);
  while( digitalRead( BUTTON_DOWN_PIN ) == LOW ) { 
    delay(1);
  }
  delay(50);
}

void waitForRightButtonUp() {
  delay(10);
  while( digitalRead( BUTTON_RIGHT_PIN ) == LOW ) { 
    delay(1);
  }
  delay(50);
}

void waitForUpButtonUp() {
  delay(10);
  while( digitalRead( BUTTON_UP_PIN ) == LOW ) { 
    delay(1);
  }
  delay(50);
}

void motorOn() {
  digitalWrite(MOTOR_ENABLE_PIN, LOW); 
}

void motorOff() {
  digitalWrite(MOTOR_ENABLE_PIN, HIGH);
}

void stepMotor(uint16_t d = motorSpeed) { 
  digitalWrite(STEP_PIN, HIGH);
  delayMicroseconds(d);
  digitalWrite(STEP_PIN, LOW);
  delayMicroseconds(d);
}

void homeStepper() { 
  // Move until the limit switch is pressed and then back off a little bit.
  
  digitalWrite(DIRECTION_PIN, LOW);
  while( ! isHomeSwitchHit() ) {  
    stepMotor();
  }

  delay(100);
  // Switch is now depressed and we need to back off until it goes low.
  digitalWrite(DIRECTION_PIN, HIGH);
 
  while( isHomeSwitchHit() ) { 
    stepMotor();
  }
}

bool moveStepper(uint16_t thousDistance, bool isLeft) { 
  if( isLeft ) { 
    digitalWrite(DIRECTION_PIN, LOW);
  } else {
    digitalWrite(DIRECTION_PIN, HIGH);
  }
  // The number of steps per thousandths of an inch
  int stepsPerThou = 4;
  int numberOfSteps = (int) (  ((int) thousDistance) * stepsPerThou );
  Serial.print("moveStepper; inches: " );
  Serial.print(thousDistance);
  Serial.print(", isLeft: " );
  Serial.print(isLeft);
  Serial.print(", numberOfSteps: " );
  Serial.println(numberOfSteps);
  for( int i = 0; i < numberOfSteps; i++ ) { 
    stepMotor();

    // If the limit switch is ever triggered then exit early.
    if( isHomeSwitchHit() ) { 
      Serial.println( "Home switch hit!" );
      return false;
    }  
  }
  return true;
}

void drawThumbUp() { 
 lcd.setCursor(16,3);
 lcd.write((uint8_t)0);
 lcd.setCursor(16,2);
 lcd.write((uint8_t)1);
 lcd.setCursor(17,3);
 lcd.write((uint8_t)2);
 lcd.setCursor(17,2);
 lcd.write((uint8_t)3);
 lcd.setCursor(18,3);
 lcd.write((uint8_t)4);
 lcd.setCursor(18,2);
 lcd.write((uint8_t)5);
}

void writeSettings() {
  uint8_t kerfInByte = (uint8_t) kerf;
  
  EEPROM.write(0, byte(215));
  EEPROM.write(1, byte(kerf));
  EEPROM.write(2, byte(motorSpeed / 256));
  EEPROM.write(3, byte(motorSpeed % 256 ));
  EEPROM.write(4, byte(slop / 256));
  EEPROM.write(5, byte(slop % 256 ));

  lcd.setCursor(1, 1);
  lcd.print("Saved settings");
  delay(5000);

  determineMovementValues();
}

void determineMovementValues() {
   // Blade starts to the right of the wood here so the first "cut" is on empty space.
  initialDistanceToCut = slot + (slop);  
  initialDistanceToMove = slot - (slop + slop) + kerf;  // 07/10/2018 - Added another slop here as first finger was too tight.
  distanceToCut = slot + (slop + slop) - kerf;
  distanceToMove = slot - (slop + slop) + kerf;
}

void readSettings() {
  byte kerfByte = EEPROM.read(1);
  kerf = kerfByte;
  
  motorSpeed = (EEPROM.read(2) * 256) + EEPROM.read(3);
  slop = (EEPROM.read(4) * 256) + EEPROM.read(5);
}

void setup() {
  Serial.begin(9600);
  Serial.println("In setup");
  Serial.println( __DATE__ " " __TIME__ );
  pinMode( STEP_PIN, OUTPUT );
  pinMode( DIRECTION_PIN, OUTPUT ); 
  
  pinMode( BUTTON_LEFT_PIN, INPUT_PULLUP );
  pinMode( BUTTON_RIGHT_PIN, INPUT_PULLUP );
  pinMode( BUTTON_DOWN_PIN, INPUT_PULLUP );
  pinMode( BUTTON_UP_PIN, INPUT_PULLUP );

  pinMode( HOME_LIMIT_SWITCH_PIN, INPUT_PULLUP );

  pinMode( MOTOR_ENABLE_PIN, OUTPUT );

  lcd.begin(20, 4);
  registerThumbUp();

  // If the eeprom on the arduino doesn't have settings stored there then
  // we just write the default values.
  if (EEPROM.read(0) != 215 || isUpButtonHit()) { 
    Serial.println("Writing configuration setup");
    writeSettings(); 
  } else {
    Serial.println("Reading configuration setup");
    readSettings();
    determineMovementValues();
  }
  Serial.println("Done with setup");
}

void showOptions( const char *left, const char *bottom, const char *right = NULL, const char *top = NULL, const int startLine = 1) { 
  char statusMessage[14];

  if( top ) { 
    int len = snprintf(statusMessage, 14, "%s", top);
    lcd.setCursor((int)(20-len)/2, startLine);
    lcd.print(statusMessage);
  }
  if( left ) {
    int len = snprintf(statusMessage, 14, "%s", left);
    lcd.setCursor(0, startLine+1);
    lcd.print(statusMessage);
  }
  if( right ) {
    int len = snprintf(statusMessage, 14, "%s", right);
    lcd.setCursor(20-len, startLine+1);
    lcd.print(statusMessage);
  }
  if( bottom ) {
    int len = snprintf(statusMessage, 14, "%s", bottom);
    lcd.setCursor((int)(20-len)/2, startLine+2);
    lcd.print(statusMessage);
  }
}

int state = 0;
int pos = 0;
bool refreshDisplay = true;

//
// isCutFirst = false means that we do not cut first - instead we move.  When
// we move initially we leave a finger like this:  
//
//  +---+
//  |   |
//  +   +---+
//
// But when isCutFirst = true then we have cut our first slot and it looks like this:
//      +---+
//      |
//  +---+
//
bool isCutFirst = true;

void setupForCut( bool cutFirst ) {
  isCutFirst = cutFirst;
  motorOn();
  
  if( isCutFirst ) { 
    distanceRemainingToCut = initialDistanceToCut;
    distanceRemainingToMove = distanceToMove;
  } else {
    distanceRemainingToCut = 0;
    distanceRemainingToMove = initialDistanceToMove;
  }
}

void showMainMenu() {
  if (refreshDisplay) {
    motorOff();
    refreshDisplay = false;
    lcd.clear();
    char statusMessage[20];
    const char fingerprint[] = __DATE__;
  
    lcd.clear();
    int len = snprintf(statusMessage, 20, "Box Jointer");
    lcd.setCursor((int)(20-len)/2, 0);
    lcd.print(statusMessage);
    len = snprintf(statusMessage, 20, "%s", fingerprint);
    lcd.setCursor((int)(20-len)/2, 1);
    lcd.print(statusMessage);

    showOptions( "Config",  /* Left   */
       "Pos",               /* Bottom */
       "Inv",               /* Right  */
       NULL,                /* Top    */
       1 );
  }

  if (isLeftButtonHit()) {   /* Config */
    state = 3;
    waitForLeftButtonUp();
    refreshDisplay = true;
  } else if (isRightButtonHit()) {  /* Inverse */
    setupForCut(false);
    state = 1;
    waitForRightButtonUp();
    refreshDisplay = true;
  } else if (isDownButtonHit()) {   /* Positive */
    setupForCut(true);
    state = 1;
    waitForDownButtonUp();
    refreshDisplay = true;
  }
}

void showMoveMenu() {
  if (refreshDisplay) {
    refreshDisplay = false;
    
    lcd.clear();  
    lcd.setCursor((int)(20-13)/2, 0);
    lcd.print("Move Menu");
    showOptions( "Left", "Back", "Right" );
  }
  if (isLeftButtonHit()) {
    moveStepper(5, 1);
  } else if (isRightButtonHit()) { 
    moveStepper(5, 0);
  } else if (isDownButtonHit()) { 
    state = 0;
    waitForDownButtonUp();
    refreshDisplay = true;
  } 
}

int configOption = 0;
const char *configOptions[] = {
   "Slop",
   "Kerf",
   "Slot",
};

void showConfigMenu() {
  if (refreshDisplay) {
    refreshDisplay = false;
    
    lcd.clear();  
    char statusMessage[20];
    int len = snprintf(statusMessage, 20, "     Config Menu" );
    lcd.setCursor(0, 0);
    lcd.print(statusMessage);

    snprintf(statusMessage, 20, "Set %s", configOptions[configOption]);
    showOptions( statusMessage, "Next", "Back", "Prior" );
  }
  if (isLeftButtonHit()) {
    waitForLeftButtonUp();
    state = 4;
    refreshDisplay = true;
  } else if (isDownButtonHit()) { 
    configOption++;
    if( configOption == 3 ) { 
      configOption = 0;
    }
    waitForDownButtonUp();
    refreshDisplay = true;
  } else if (isUpButtonHit()) { 
    configOption--;
    if( configOption < 0 ) { 
      configOption = 2;
    }
    waitForUpButtonUp();
    refreshDisplay = true;
  } else if (isRightButtonHit()) { 
    waitForRightButtonUp();
    state = 0;
    refreshDisplay = true;
  }
}


void showConfigChangeMenu() {
  if (refreshDisplay) {
    refreshDisplay = false;
    
    lcd.clear();  

    char statusMessage[20];
 
    lcd.clear();
    int len;
    if( configOption == 0 ) { 
      len = snprintf(statusMessage, 20, "Slop: %d", slop);
    } else if( configOption == 1) { 
      len = snprintf(statusMessage, 20, "Kerf: %d", kerf);   
    } else if( configOption == 2 ) { 
      len = snprintf(statusMessage, 20, "Slot Size: %d", slot);   
    }
    lcd.setCursor((int)(20-len)/2, 0);
    lcd.print(statusMessage);
    showOptions( "Back", "Down", "Cancel", "Up" );
  }
  if (isUpButtonHit()) {
    waitForUpButtonUp();
    if( configOption == 0 ) { 
      slop = slop + 1;
    } else if ( configOption == 1 ) { 
      kerf = kerf + 1;
    } else if ( configOption == 2 ) { 
      slot = slot + 1;
    }
    refreshDisplay = true;
  } else if (isLeftButtonHit()) { 
    state = 3;
    waitForLeftButtonUp();
    writeSettings();
    refreshDisplay = true;
  } else if (isDownButtonHit()) { 
    waitForDownButtonUp();
    if( configOption == 0 ) { 
      slop = slop - 1;
    } else if ( configOption == 1 ) { 
      kerf = kerf - 1;
    } else if ( configOption == 2 ) { 
      slot = slot - 1;
    }
    refreshDisplay = true;
  } else if (isRightButtonHit()) { 
    waitForRightButtonUp();
    lcd.setCursor(1, 1);
    lcd.print("  CANCELED     ");
    delay(2000);
    state = 3;
    refreshDisplay = true;
  }
}


uint16_t d() {
  Serial.print( "computing d; distanceRemainingToCut: ");
  Serial.print( distanceRemainingToCut );
  Serial.print( ", distanceRemainingToMove: ");
  Serial.print( distanceRemainingToMove );
  Serial.print( ", kerf: " );
  Serial.println( kerf );
  if (distanceRemainingToCut > kerf) { 
    Serial.println("Returning Kerf" );
    return kerf;
  } else {
    Serial.println("Less to cut!  Returning distanceRemainingToCut!" );
    return distanceRemainingToCut;
  }
}


void showCutMenu() {
  if (refreshDisplay) {
    refreshDisplay = false;
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("+---+   ");
    lcd.setCursor(0, 1);
    lcd.print("|   |___+");
    if (distanceRemainingToCut == 0) { 
      lcd.setCursor(5, 1);
      lcd.print("*");
    } else if ( distanceRemainingToCut < distanceToCut / 3 ) { 
      lcd.setCursor(3, 1);
      lcd.print("^");
    } else if ( distanceRemainingToCut < ( 2*distanceToCut / 3) ) { 
      lcd.setCursor(2, 1);
      lcd.print("^");
    } else {
      lcd.setCursor(1, 1);
      lcd.print("^");
    }
    if( distanceRemainingToCut > 0 ) { 
      char statusMessage[10];
      snprintf(statusMessage, 10, "X %d %d", (int)(distanceRemainingToCut), (int)(distanceToCut) );
      lcd.setCursor((int)(20-10), 0);
      lcd.print(statusMessage);
    } else {
      char statusMessage[10];
      snprintf(statusMessage, 10, "-> %d", (int)(distanceRemainingToMove) );
      lcd.setCursor((int)(20-10), 0);
      lcd.print(statusMessage);
    }
 
    showOptions( "Back", NULL, "Next", NULL, 2 );
    Serial.print( distanceRemainingToCut );
    Serial.println ( " distance remaining"); 
  }
   
  if (isRightButtonHit()) {
    refreshDisplay = true;
    waitForRightButtonUp();
    if (distanceRemainingToCut == 0) { 
      Serial.print( "At cusp of next - moving forward big: "); 
      Serial.println(distanceRemainingToMove);
      moveStepper(distanceRemainingToMove, 1);
      distanceRemainingToCut = distanceToCut;
      distanceRemainingToMove = distanceToMove;
      Serial.print( "Done;  new distanceRemainingToCut: "); 
      Serial.println(distanceRemainingToCut);
    } else {  
      uint16_t m = d();
      Serial.print( "doing a cut pass;  distanceRemainingToCut: " );
      Serial.print( distanceRemainingToCut );
      Serial.print( ", d: " );
      Serial.println(m);
      distanceRemainingToCut -= m;
      moveStepper(m, 1);
    }

  } else if (isLeftButtonHit()) { 
    waitForLeftButtonUp();
    state = 0;
    refreshDisplay = true;
  }
}

void loop() {
  if (state == 0) { 
    showMainMenu();
  } else if (state == 1) {     
    showCutMenu();
  } else if (state == 2) {
    showMoveMenu();
  } else if (state == 3) {
    showConfigMenu();
  } else if (state == 4) {
    showConfigChangeMenu();
  }
}

