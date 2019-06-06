// This script drives the behavior of the simple box cutter.  
//
// 8/4/2017: First version
// 7/10/2018: Minor fixes - typos and first finger is too wide.
//   Recording the flat top blade that I like. 
//   Slop: 2, Kerf: 123
// 6/4/2019: Added the capability to specify the size of the material along
//   with the number of slots in order to determine the size of each slot.

// Hardware:
// 1/  A sparkfun "Basic 20x4 Character LCD - Black on Green 5V".  This is
//     an HD47780-based display.  
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
//        "Inverse"              "Positive"
//    isCutFirst = False     isCutFirst = True
//    +-----------------+    +---------------+
//    |                 |    |               |
//    |                 |    |               |
//    |     Wood        |    |               |
//    |                 |    |               |
//    |         +-------+    |               |
//    |         |    +-------+               |
//    |         |    |                       |
//    |         |    |            Wood       |
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
// The value is determined by the slop setting.  
//
// All measurements are in thousandths of an inch.  So 500 is 0.5 inches.

// The menu on the device talks about "Positive" and "Inverse" for the two
// mating pieces of wood. 
// 
//  When "Positive" then isCutFirst = True - which means we cut then move:
//    distanceRemainingToCut = initialDistanceToCut;
//    distanceRemainingToMove = distanceToMove;
//
//  Thus the start looks like this:
//         +---+
//         |
//     +---+

//
//  When "Inverse" then isCutFirst = False - which means that we just move:
//    distanceRemainingToCut = 0;
//    distanceRemainingToMove = initialDistanceToMove;
// 
//  Thus the start looks like this:
//     +---+
//     |   |
//     +   +---+
//

//    initialDistanceToCut = slot + (slop);  
//    initialDistanceToMove = slot - (slop + slop) + kerf;  
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

// Computed based on the current options.  Set in determineMovementValues.
uint16_t initialDistanceToCut;
uint16_t initialDistanceToMove;
uint16_t distanceToCut;
uint16_t distanceToMove;

uint16_t distanceRemainingToCut = 0;
uint16_t distanceRemainingToMove = 0;

// Default speed for movement.  This is in microseconds and captures the amount of time
// to wait inbetween low and high pulses.  A larger number indicates slower speed.
uint16_t motorSpeed = 100;

uint32_t lengthOfWood = 0;
uint16_t numberOfSlots = 0;

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
  for( int i=0; i < 6; i++ ) { 
    lcd.createChar(i, thumbUpIcon[i]);
  }
}

bool isHomeSwitchPressed() {
  return digitalRead( HOME_LIMIT_SWITCH_PIN ) == LOW;
}

bool isLeftButtonPressed() {
  return digitalRead( BUTTON_LEFT_PIN ) == LOW;
}

bool isRightButtonPressed() {
  return digitalRead( BUTTON_RIGHT_PIN ) == LOW;
}

bool isDownButtonPressed() {
  return digitalRead( BUTTON_DOWN_PIN ) == LOW;
}

bool isUpButtonPressed() {
  return digitalRead( BUTTON_UP_PIN ) == LOW;
}

void waitForButtonRelease(uint8_t pin) { 
  delay(10);
  while( digitalRead(pin) == LOW ) { 
    delay(1);
  }
  delay(50);
}
void waitForLeftButtonRelease() {
  waitForButtonRelease(BUTTON_LEFT_PIN);
}

void waitForDownButtonRelease() {
  waitForButtonRelease(BUTTON_DOWN_PIN);
}

void waitForRightButtonRelease() {
  waitForButtonRelease(BUTTON_RIGHT_PIN);
}

void waitForUpButtonRelease() {
  waitForButtonRelease(BUTTON_UP_PIN);
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
  while( ! isHomeSwitchPressed() ) {  
    stepMotor();
  }

  delay(100);
  // Switch is now depressed and we need to back off until it goes low.
  digitalWrite(DIRECTION_PIN, HIGH);
 
  while( isHomeSwitchPressed() ) { 
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
  unsigned long int stepsPerThou = 4;
  unsigned long int numberOfSteps = (unsigned long int) (((unsigned long int) thousDistance) * stepsPerThou );
  Serial.print("moveStepper; inches: " );
  Serial.print(thousDistance);
  Serial.print(", isLeft: " );
  Serial.print(isLeft);
  Serial.print(", numberOfSteps: " );
  Serial.println(numberOfSteps);
  for( unsigned long int i = 0; i < numberOfSteps; i++ ) { 
    stepMotor();

    // If the limit switch is ever triggered then exit early.
    if( isHomeSwitchPressed() ) { 
      Serial.println( "Home switch Pressed!" );
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
  EEPROM.write(3, byte(motorSpeed % 256));
  EEPROM.write(4, byte(slop / 256));
  EEPROM.write(5, byte(slop % 256));

  EEPROM.write(6, byte((lengthOfWood >> 16) / 256));
  EEPROM.write(7, byte((lengthOfWood >> 16 ) % 256));
  EEPROM.write(8, byte((lengthOfWood && 0xFFFF) / 256));
  EEPROM.write(9, byte((lengthOfWood && 0xFFFF) % 256));

  EEPROM.write(10, byte(numberOfSlots / 256));
  EEPROM.write(11, byte(numberOfSlots % 256));

  lcd.setCursor(1, 1);
  lcd.print("Saved settings");
  delay(5000);

  determineMovementValues(slot);
}

void determineMovementValues( uint16_t slotSize ) {
  // The 'Initial' values here are for the first finger-valley or
  // valley-finger on the wood.  The blade always starts adjacent to the wood
  // with the first movement of 'kerf' distance being a move that would cut
  // off 'kerf' from the edge of the wood (if it were a valley).  

  // The initial distance to cut is smaller, by a single slop setting, because
  // the valley is adjacent to empty space.
  initialDistanceToCut = slotSize + (slop);  
  initialDistanceToMove = slotSize - (slop + slop) + kerf; 

  distanceToCut = slotSize + (slop + slop) - kerf;
  distanceToMove = slotSize - (slop + slop) + kerf;
}

void readSettings() {
  byte kerfByte = EEPROM.read(1);
  kerf = kerfByte;
  
  motorSpeed = (EEPROM.read(2) * 256) + EEPROM.read(3);
  slop = (EEPROM.read(4) * 256) + EEPROM.read(5);

  lengthOfWood = ((EEPROM.read(6) * 256) + EEPROM.read(7));
  lengthOfWood = lengthOfWood << 16;
  lengthOfWood = lengthOfWood + ((EEPROM.read(8) * 256) + EEPROM.read(9) );

  numberOfSlots = (EEPROM.read(10) * 256) + EEPROM.read(11);
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

  // If the eeprom on the arduino doesn't have settings stored there or
  // if you hold the up button when you turn on the unit, then we write 
  // the default values.
  if (EEPROM.read(0) != 215 || isUpButtonPressed()) { 
    Serial.println("Writing configuration setup");
    writeSettings(); 
  } else {
    Serial.println("Reading configuration setup");
    readSettings();
    determineMovementValues(slot);
  }
  Serial.println("Done with setup");
}

void showOptions( const char *left, const char *bottom, const char *right = NULL, const char *top = NULL, const int startLine = 1) { 
  char statusMessage[14];

  if( top ) { 
    showCentered(top, startLine);
  }
  if( left ) {
    snprintf(statusMessage, 14, "%s", left);
    lcd.setCursor(0, startLine+1);
    lcd.print(statusMessage);
  }
  if( right ) {
    showRightAligned(right, startLine+1);
    int len = snprintf(statusMessage, 14, "%s", right);
    lcd.setCursor(20-len, startLine+1);
    lcd.print(statusMessage);
  }
  if( bottom ) {
    showCentered(bottom, startLine+2);
  }
}

int state = 0;
int pos = 0;
bool refreshDisplay = true;

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

  if (isLeftButtonPressed()) {   /* Config */
    state = 3;
    waitForLeftButtonRelease();
    refreshDisplay = true;
  } else if (isRightButtonPressed()) {  /* Inverse */
    setupForCut(false);
    state = 1;
    waitForRightButtonRelease();
    refreshDisplay = true;
  } else if (isDownButtonPressed()) {   /* Positive */
    setupForCut(true);
    state = 1;
    waitForDownButtonRelease();
    refreshDisplay = true;
  }
}

void showCentered(const char *s, int line) {
  char m[20];
  int len = snprintf(m, 20, "%s", s);
  lcd.setCursor( (int) ((20 - len) / 2), line);
  lcd.print(m);
}

void showRightAligned(const char *s, int line) {
  char m[20];
  int len = snprintf(m, 14, "%s", s);
  lcd.setCursor(20-len, line);
  lcd.print(m);
}

void showMoveMenu() {
  if (refreshDisplay) {
    refreshDisplay = false;
    
    lcd.clear();  
    showCentered("Move Menu", 0);
    showOptions( "Left", "Back", "Right" );
  }
  if (isLeftButtonPressed()) {
    moveStepper(5, 1);
  } else if (isRightButtonPressed()) { 
    moveStepper(5, 0);
  } else if (isDownButtonPressed()) { 
    state = 0;
    waitForDownButtonRelease();
    refreshDisplay = true;
  } 
}

#define MAX_CONFIG_OPTIONS 5
int configOption = 0;
const char *configOptions[MAX_CONFIG_OPTIONS] = {
   "Slop",
   "Kerf",

   // There are two ways to set the size of each of the finger. 
   // 
   // 1/  Set the slot size.  This is a direct method and done independent of
   // the length of the wood or the number of fingers that you want.  
   // 
   // 2/  Set the length of the piece of wood that will be joined along with
   // the number of fingers that you want.  This is 'length' and 'fingers'.  
   // Setting one of these values will recompute the slot size which is then 
   // used from that point forward.  
   // 
   "Slot",
   "Length",
   "Fingers",
};

void showConfigMenu() {
  if (refreshDisplay) {
    refreshDisplay = false;
    
    lcd.clear();  
    showCentered("Config Menu", 0);

    char statusMessage[20];
    snprintf(statusMessage, 20, "Set %s", configOptions[configOption]);
    showOptions( statusMessage, "Next", "Back", "Prior", 1 );

    // Always show the slot size here as it can be set by a number of
    // different methods.  This is on the 0th or first line which 
    // is empty.
    snprintf(statusMessage, 20, "Slot Size: %d", slot);   
    lcd.setCursor(2, 0);
    lcd.print(statusMessage);
  }
  if (isLeftButtonPressed()) {
    waitForLeftButtonRelease();
    state = 4;
    refreshDisplay = true;
  } else if (isDownButtonPressed()) { 
    configOption++;
    if( configOption == MAX_CONFIG_OPTIONS ) { 
      configOption = 0;
    }
    waitForDownButtonRelease();
    refreshDisplay = true;
  } else if (isUpButtonPressed()) { 
    configOption--;
    if( configOption < 0 ) { 
      configOption = MAX_CONFIG_OPTIONS-1;
    }
    waitForUpButtonRelease();
    refreshDisplay = true;
  } else if (isRightButtonPressed()) { 
    waitForRightButtonRelease();
    state = 0;
    refreshDisplay = true;
  }
}

void recalculateSlotSize() {
  // Based on the length of the wood and the number of slots determine the
  // slot size.
  slot = lengthOfWood / numberOfSlots;
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
  if (isUpButtonPressed()) {
    // We don't wait for the button up when we are modifying the values that
    // we expect to be very large.  This allows us to zoom around in setting
    // the value.
    if (configOption != 3 ) { 
       waitForUpButtonRelease();
    }
    if( configOption == 0 ) { 
      slop = slop + 1;
    } else if ( configOption == 1 ) { 
      kerf = kerf + 1;
    } else if ( configOption == 2 ) { 
      slot = slot + 1;
    } else if ( configOption == 3 ) { 
      lengthOfWood = lengthOfWood + 1;
      recalculateSlotSize();
    } else if ( configOption == 4 ) { 
      numberOfSlots = numberOfSlots + 1;
      recalculateSlotSize();
    }
    refreshDisplay = true;
  } else if (isLeftButtonPressed()) { 
    state = 3;
    waitForLeftButtonRelease();
    writeSettings();
    refreshDisplay = true;
  } else if (isDownButtonPressed()) { 

    // We don't wait for the button to be released when we are modifying the
    // values that we expect to be very large.  This allows us to zoom around
    // in setting the value.
    if( configOption != 3 ) { 
       waitForDownButtonRelease();
    }
    if( configOption == 0 ) { 
      slop = slop - 1;
    } else if ( configOption == 1 ) { 
      kerf = kerf - 1;
    } else if ( configOption == 2 ) { 
      slot = slot - 1;
    } else if ( configOption == 3 ) { 
      lengthOfWood = lengthOfWood - 1;
      recalculateSlotSize();
    } else if ( configOption == 4 ) { 
      numberOfSlots = numberOfSlots - 1;
      recalculateSlotSize();
    }
    refreshDisplay = true;
  } else if (isRightButtonPressed()) { 
    waitForRightButtonRelease();
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
   
  if (isRightButtonPressed()) {
    refreshDisplay = true;
    waitForRightButtonRelease();
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

  } else if (isLeftButtonPressed()) { 
    waitForLeftButtonRelease();
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

