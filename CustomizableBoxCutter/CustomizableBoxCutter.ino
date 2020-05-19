// This embedded program drives the behavior of the customizable box cutter.  
// This derives from, and expands up, the original 'simple box cutter'.
//
// 6/30/2019: First version

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
//
// Stepper Pins:
//   9  - Step
//   10 - Direction
//


// --------------------------------------------------------------------------
// Understanding the finger & valley goodness
// --------------------------------------------------------------------------

// This documentation is broken up into three sections where each 
// subsequent section builds on the prior and introduces some complexity
// that was glossed over in the prior section.  This aids in understanding
// what is happening into smaller sized chunks.
//
// Section 1:  Fingers & Valleys
// Section 2:  Initial cuts versus subsequent cuts
// Section 3:  Non-uniform sized cuts


// --------------------------------------------------------------------------
// Section 1:  Fingers & Valleys
// --------------------------------------------------------------------------

//
//  The Finger Joints look like this:
//
//        "Negative"              "Positive"
//    isCutFirst = False      isCutFirst = True
//    +------------------+    +---------------+
//    |     WOOD         |    |     WOOD      |
//    |                  |    |               |
//    |    [Finger]      |    |   [Valley]    |
//    |                  |    |               |
//    |          +-------+    |               |
//    |          |    +-------+               |
//    |          |    |                       |
//    | [Valley] |    |           [Finger]    |
//    |          |    |                       |
//    |          |    +-------+               |
//    |          +-------+    |               |
//    |                  |    |               |
//        ... 
//
// There are 2 values in use when you are doing a pair of Finger and Valley. 
// The first is how much to cut - this is the size of the valley.  The second
// is the amount to move giving you the size of the finger.
// The valley needs to be larger than the finger by just a little bit.
// If they were the exact same size then they would be too tight. 
// Depending on the material and the characteristics of the cutting, a few
// thousands of an inch should be sufficient.  This parameter (the additional
// thousandths of an inch to cut to allow them to fit) is called 'slop'.
//
// The units for all variables that are capturing distance are in thousandths
// of an inch.  So 500 is 0.5 inches.

// The menu on the device talks about "Positive" and "Negative" for the two
// mating pieces of wood. 
// 
//  When "Positive" then isCutFirst = True - which means we cut then move:
//    distanceRemainingToCut = initialDTMValley;
//    distanceRemainingToMove = subDTMFinger;
//

//
//  When "Negative" then isCutFirst = False - which means that we just move:
//    distanceRemainingToCut = 0;
//    distanceRemainingToMove = initialDTMFinger;
//



// --------------------------------------------------------------------------
// Section 2:  Initial cuts versus subsequent cuts
// --------------------------------------------------------------------------

// Positive and negative cuts are all good but how do we make the entire
// process repeatable given that we don't want to create an initial setup 
// that is simple and doesn't require any guess work on where to start?
//

// The jig is operated by the following steps:
//   0/  Clamp the wood into the gantry.  Make sure it is flush and square.
//   1/  Move the gantry (not the wood) so that the wood is flush against 
//       the table saw.
//   2/  Select the cut type, turn on the table saw.
// 
//   3/  Push the jig forward, then back.
//   4/  Hit the next button (on the right) on the jig.
//   5/  Goto step 2.
//  
// This keeps the process simple and repeatable.  Two important characteristics
// when moving wood over-and-over through a spinning blade!!
// 

// But this also complicates the software as we need to get the movement and
// cut values correct given that there are two situations.  This is because on
// the very first finger/valley the blade is to the left of where the wood is.
// To start the wood is put flush against the blade and, when we push the jig
// through the blade (step #3), empty air is cut.  The setup looks like this:
//
//  
//            B                             ^      
//            L                             |
//            A                             |
//       ----+D                        Human Pushes
//           |E                          the jig
//           |                              |
//           |                              |
//
//   -> Stepper movement ->
// 
// When you push the jig forward, and move the blade through the wood, 
// no wood is actually cut since the initial setup has the blade flush against
// the wood.
//

// This requires the 'initial' finger (or valley) to differ from all others
// because the kerf distance needs to be taken into account.
//
// For the first finger/valley, the variables 'initialDTMFinger' or
// 'initialDTMValley' are used.  DTM stands for 'distance to move'.  
//
//    initialDTMValley  =  valleySize     +  slop         +  0;  
//    initialDTMFinger  =  fingerSize     +  -slop        +  kerf;  
//
// Because step #3 doesn't actually do anything (you cut empty air the first
// time) the initial distance to cut includes the full slot and slop.  The zero
// term there will make sense once you see the subDTMValley for non-initial
// finger/valleys.
//
// The initialDTMFinger is offset by the kerf term in order to position
// the cutter inside of the wood for the next step.

// 
// After that first initial finger or valley the blade is ready to pass through
// material into the valley of the second and all subsequent joints.  This is
// because when doing a negative cut we don't do anything but move to the
// starting point of the first valley.  When doing positive then we cut and
// then do a big move.  In both cases we are ready to pass the material through
// the blade as part of the subsequent valley.
//
// In this case the setup looks like this:
//  
//                -> Stepper movement ->
// 
//            B                           B              
//            L                           L                      ^ 
//            A                           A                      | 
//            D                           D                      | 
//            E              OR           E                 Human Pushes
//      +--------                   +-----------              the jig
//      |                           |                            |
//      |                           |                            |
//  +---+                           |             
//  |                               |         
//   
//
// Now step #3 will *not* cut empty air.  It will cut a kerf amount of wood.  
// Therefore all subsequent valley and finger distance to moves are 
// defined by:
//  
//    subDTMValley  = valleySize     +  slop         -  kerf;  
//    subDTMFinger  = fingerSize     +  -slop        +  kerf;
// 
// The term 'sub' is short for subsequent which captures all fingers and
// valleys after the first.


// --------------------------------------------------------------------------
// Section 3:  Non-uniform sized cuts
// --------------------------------------------------------------------------
//
// Final complication!  When you are doing the same sized fingers and 
// valleys then the process is simple.  When doing variable sized fingers
// and valleys the conceptual is still the same but the details are a
// bit more tricky.  
//                                              
// When doing the programmatic cut you set the individual stop points for
// each of the fingers or valleys.  Whether the stop point represents a 
// finger or valley depends upon whether you are doing the positive or
// negative piece.
//
// The numeric values shown below are the thous that have been set up 
// while programming.  
// 
//        Negative              Positive
//    +-----------------+    +---------------+
//    |                 |    |               |
//    | 200             |    |    200        |
//    |     Wood        |    |               |
//    |                 |    |               |
//    |         +-------+    |               |
//    |         |    +-------+               |
//    | 300     |    |                       |
//    |         |    |            300        |
//    |         |    |                       |
//    |         |    +-------+               |
//    |         +-------+    |               |
//    |                 |    |               |
//    |                 |    |               |
//    |                 |    |               |
//    | 450             |    |   450         |
//    |                 |    |               |
//    |                 |    |               |
//    |         +-------+    |               |
//    |         |    +-------+               |
//    | 300     |    |                       |
//    |         |    |           300         |
//    |         |    |                       |
//    |         |    +-------+               |
//    |         +-------+    |               |
//    |                 |    |               |
//    |                 |    |               |
//    | 200             |    |   200         |
//    |                 |    |               |
//    +-----------------+    +---------------+
// 
// In the example above the programmaticSlots would look like:
//  programmaticSlots[] = {
//     200,
//     300,
//     450,
//     300,
//     200,
//     0, 0, 0, ..., 0,
//  }
// 
// When doing the positive we would pass in 
//  valley = 200 (programmaticSlot[0]), finger = 300 (programmaticSlots[1])
//  valley = 450 (programmaticSlot[2]), finger = 300 (programmaticSlots[3])
//  valley = 200 (programmaticSlot[4]), finger = 200 (programmaticSlots[0])
//    (If the end of the piece of wood is reached before wrapping around 
//       then the wrap around doesn't matter.)
// 
// When doing the negative we would pass in 
//  valley = 0 (no programmaticSlot!),  finger = 200 (programmaticSlots[0])
//  valley = 300 (programmaticSlot[1]), finger = 450 (programmaticSlots[2])
//  valley = 300 (programmaticSlot[3]), finger = 200 (programmaticSlots[4])
//
// Thus the movement through the programmaticSlots is always by 2s for 
// a single valley and finger pair, but it is offset by one when 
// doing the negative.  

 
#include <LiquidCrystal.h>
#include <EEPROM.h>   

#define BUTTON_LEFT_PIN 6
#define BUTTON_RIGHT_PIN 7
#define BUTTON_DOWN_PIN 8
#define BUTTON_UP_PIN 13
#define STEP_PIN 10
#define DIRECTION_PIN 9
#define MOTOR_ENABLE_PIN A0

// All units are in thousands of an inch.
// So, for example, 125 = 0.125"

// Default kerf size of blade
uint16_t kerf = 123;

// Default movement can be less than the kerf of the blade if you want overlapping cuts.
uint16_t maxAdvance = 120;

#define MAX_PROGRAMMATIC_SLOTS 64
uint16_t uniformSlot;
uint16_t programmaticSlots[MAX_PROGRAMMATIC_SLOTS] = {
       125, 125, 125, 125, 125, 125, 125, 125,
       125, 125, 125, 125, 125, 125, 125, 125,
       125, 125, 125, 125, 125, 125, 125, 125,
       125, 125, 125, 125, 125, 125, 125, 125,
       0,   0,   0,   0,   0,   0,   0,   0,
       0,   0,   0,   0,   0,   0,   0,   0,
       0,   0,   0,   0,   0,   0,   0,   0,
       0,   0,   0,   0,   0,   0,   0,   0,
};
uint16_t programmaticIndex = 0;
uint16_t slop = 2;

// Computed based on the current options.  Set in determineMovementValues.
// DTM == Distance To Move.
uint16_t initialDTMValley;
uint16_t initialDTMFinger;
uint16_t subDTMValley;
uint16_t subDTMFinger;

uint16_t currentFingerSize;
uint16_t currentValleySize;

uint16_t distanceRemainingToCut = 0;
uint16_t distanceRemainingToMove = 0;

// Default speed for movement.  This is in microseconds and captures the amount of time
// to wait inbetween low and high pulses.  A larger number indicates slower speed.
uint16_t motorSpeed = 100;

// Default length of wood and number of slots.
uint32_t lengthOfWood = 4000;
uint16_t numberOfSlots = 6;

// Initialize the LCD.  Pin numbers in the below match the earlier comments.
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

#define MAIN_MENU 0
#define CUT_MENU 1
#define CONFIG_MENU 3
#define CONFIG_CHANGE_VALUE_MENU 4
#define UNIFORM_CUT_MENU 5
#define PROGRAMMATIC_CUT_MENU 6
#define CUT_TYPE_MENU 7

void registerThumbUp() { 
  for( int i=0; i < 6; i++ ) { 
    lcd.createChar(i, thumbUpIcon[i]);
  }
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
  delay(50);
  while( digitalRead(pin) == LOW ) { 
    delay(1);
  }
  delay(40);
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

  Serial.print( "going to writeSettings; kerf: ");
  Serial.print(kerf);
  Serial.print( ", motorSpeed: ");
  Serial.print(motorSpeed);
  Serial.print( ", slop: ");
  Serial.print(slop);
  Serial.print( ", lengthOfWood: ");
  Serial.print(lengthOfWood);
  Serial.print( ", numberOfSlots: ");
  Serial.print(numberOfSlots);
  Serial.print( ", maxAdvance: ");
  Serial.print(maxAdvance);
  Serial.print( ", uniformSlot: ");
  Serial.println(uniformSlot);
  
  EEPROM.write(0, byte(215));
  EEPROM.write(1, byte(kerf));
  EEPROM.write(2, byte(motorSpeed / 256));
  EEPROM.write(3, byte(motorSpeed % 256));
  EEPROM.write(4, byte(slop / 256));
  EEPROM.write(5, byte(slop % 256));

  EEPROM.put(6, lengthOfWood);

  EEPROM.write(10, byte(numberOfSlots / 256));
  EEPROM.write(11, byte(numberOfSlots % 256));

  EEPROM.write(12, byte(maxAdvance));

  EEPROM.write(13, byte(uniformSlot / 256));
  EEPROM.write(14, byte(uniformSlot % 256));

  for( int i=0; i < MAX_PROGRAMMATIC_SLOTS; i++ ) { 
    EEPROM.write(15 + (i*2), byte(programmaticSlots[i] / 256));
    EEPROM.write(16 + (i*2), byte(programmaticSlots[i] % 256));
  }

  Serial.print( "Programmatic slots, maxLength: ");
  Serial.print( MAX_PROGRAMMATIC_SLOTS );
  for( int i=0; i < MAX_PROGRAMMATIC_SLOTS; i++ ) { 
    Serial.print( ", S" );
    Serial.print( i );
    Serial.print( ": ");
    Serial.print(programmaticSlots[i]);
  }
  Serial.println("!");

  lcd.setCursor(1, 1);
  lcd.print("Saved settings");
  delay(1000);
}

void determineMovementValues( uint16_t valleySize, uint16_t fingerSize ) {
  // Put the valley and finger sizes in a global variable for the 
  // display to easily access.
  currentValleySize = valleySize;
  currentFingerSize = fingerSize;

  // See comments at the top of the file.
  initialDTMValley  =  valleySize   +  slop    + 0;  
  initialDTMFinger  =  fingerSize   + -slop    + kerf; 

  subDTMValley      =  valleySize   +  slop    + -kerf;
  subDTMFinger      =  fingerSize   + -slop    + kerf;
}


void readSettings() {
  byte kerfByte = EEPROM.read(1);
  kerf = kerfByte;
  
  motorSpeed = (EEPROM.read(2) * 256) + EEPROM.read(3);
  slop = (EEPROM.read(4) * 256) + EEPROM.read(5);

  EEPROM.get(6, lengthOfWood);

  numberOfSlots = (EEPROM.read(10) * 256) + EEPROM.read(11);

  byte maxAdvanceByte = EEPROM.read(12);
  maxAdvance = maxAdvanceByte;

  uniformSlot = (EEPROM.read(13) * 256) + EEPROM.read(14);

  for( int i=0; i < MAX_PROGRAMMATIC_SLOTS; i++ ) { 
    uint16_t s = (EEPROM.read(15 + (i * 2)) * 256) + EEPROM.read(16 + (i*2));
    programmaticSlots[i] = s;
  }

  Serial.print( "read in the settings; kerf: ");
  Serial.print(kerf);
  Serial.print( ", motorSpeed: ");
  Serial.print(motorSpeed);
  Serial.print( ", slop: ");
  Serial.print(slop);
  Serial.print( ", lengthOfWood: ");
  Serial.print(lengthOfWood);
  Serial.print( ", numberOfSlots: ");
  Serial.print(numberOfSlots);
  Serial.print( ", maxAdvance: ");
  Serial.print(maxAdvance);
  Serial.print( ", uniformSlot: ");
  Serial.print(uniformSlot);

  for( int i=0; i < MAX_PROGRAMMATIC_SLOTS; i++ ) { 
    Serial.print( ", S" );
    Serial.print( i );
    Serial.print( ": ");
    Serial.print(programmaticSlots[i]);
  }
  Serial.println("!");
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

  pinMode( MOTOR_ENABLE_PIN, OUTPUT );

  lcd.begin(20, 4);
  registerThumbUp();

  // If the eeprom on the arduino doesn't have settings stored there or
  // if you hold the up button when you turn on the unit, then we write 
  // the default values.
  if (EEPROM.read(0) != 215 || isUpButtonPressed()) { 
    Serial.println("Writing configuration setup");
    writeSettings(); 
    readSettings();
  } else {
    Serial.println("Reading configuration setup");
    readSettings();
  }
  Serial.println("Done with setup");
}

void showOptions(const char *left, const char *bottom, const char *right = NULL, const char *top = NULL, const int startLine = 1) { 
  char statusMessage[16];

  if( top ) { 
    showCentered(top, startLine);
  }
  if( left ) {
    snprintf(statusMessage, 16, "%s", left);
    lcd.setCursor(0, startLine+1);
    lcd.print(statusMessage);
  }
  if( right ) {
    showRightAligned(right, startLine+1);
  }
  if( bottom ) {
    showCentered(bottom, startLine+2);
  }
}

int state = 0;
bool isUniformCut = true;
int pos = 0;
bool refreshDisplay = true;

bool isCutFirst = true;

void setupForCut( bool cutFirst ) {
  motorOn();

  isCutFirst = cutFirst;
  
  if( isCutFirst ) { 
    distanceRemainingToCut = initialDTMValley;
    distanceRemainingToMove = subDTMFinger;
  } else {
    distanceRemainingToCut = 0;
    distanceRemainingToMove = initialDTMFinger;
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
    showCentered("Adv Box Jointer", 0);
    showCentered(fingerprint, 1);

    showOptions( 
       "Config",        /* Left   */
       "Uniform",       /* Bottom */
       "Prog",          /* Right  */
       NULL,            /* Top    */
       1 );
  }

  if (isLeftButtonPressed()) {          /* Left: Config */
    state = CONFIG_MENU;
    waitForLeftButtonRelease();
    refreshDisplay = true;
  } else if (isDownButtonPressed()) {   /* Down: Uniform Cut Menu */
    isUniformCut = true;
    state = CUT_TYPE_MENU;
    waitForDownButtonRelease();
    refreshDisplay = true;
  } else if (isRightButtonPressed()) {  /* Right: Programmatic Cut Menu */
    isUniformCut = false;
    state = CUT_TYPE_MENU;
    waitForRightButtonRelease();
    refreshDisplay = true;
  }
}

void showCentered(const char *s, int line) {
  char m[20];
  int len = snprintf(m, 20, "%s", s);
  lcd.setCursor((20 - len) / 2, line);
  lcd.print(m);
}

void showRightAligned(const char *s, int line) {
  char m[20];
  int len = snprintf(m, 14, "%s", s);
  lcd.setCursor(20-len, line);
  lcd.print(m);
}

#define MAX_CONFIG_OPTIONS 7
int configOption = 0;
const char *configOptions[MAX_CONFIG_OPTIONS] = {
   "Slop",
   "Kerf",
   "MaxAdvance",

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
   "Static Slot",
   "Length",
   "Fingers",
   "Programmed Slots",
};

void showConfigMenu() {
  if (refreshDisplay) {
    refreshDisplay = false;
    
    lcd.clear();  
    showCentered("Config Menu", 0);

    char statusMessage[20];
    snprintf(statusMessage, 20, "Set %s", configOptions[configOption]);
    showOptions( 
       statusMessage,  /* Left - This is the 'I want to change this option' button */
       "Next",         /* Bottom */
       "Back",         /* Right */
       "Prior",        /* Up */
       1
    );
  }
  if (isLeftButtonPressed()) {
    waitForLeftButtonRelease();
    state = CONFIG_CHANGE_VALUE_MENU;
    if( configOption == 6 ) { 
      motorOn();
    }
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
    state = MAIN_MENU;
    refreshDisplay = true;
  }
}

uint16_t recalculateSlotSize() {
  // Based on the length of the wood and the number of slots determine the
  // slot size.
  return lengthOfWood / numberOfSlots;
}

uint8_t configSlotCount = 0;
uint16_t configSlotDistance = 0;

void handleConfigProgrammaticSlots() {
  if (isUpButtonPressed()) {
    waitForUpButtonRelease();
    programmaticSlots[configSlotCount] = configSlotDistance;
    configSlotCount = configSlotCount + 1;
    refreshDisplay = true;
    configSlotDistance = programmaticSlots[configSlotCount];
    moveStepper(configSlotDistance, true);
  } else if( isLeftButtonPressed() ) { 
    delay(1);
    refreshDisplay = true;
    uint8_t distanceToMove = 0;
    if( configSlotDistance <=4 ) { 
      distanceToMove = configSlotDistance;
    } else {
      distanceToMove = 5;
    }
    configSlotDistance -= distanceToMove;
    moveStepper(distanceToMove, false);
  } else if( isRightButtonPressed() ) { 
    delay(1);
    refreshDisplay = true;
    uint8_t distanceToMove = 0;
    if( configSlotDistance >= 1996 ) { 
      distanceToMove = 2000 - configSlotDistance;
    } else {
      distanceToMove = 5;
    }
    configSlotDistance += distanceToMove;
    moveStepper(distanceToMove, true);

  } else if( isDownButtonPressed() ) { 
    for( int i=configSlotCount; i < MAX_PROGRAMMATIC_SLOTS; i++ ) { 
      programmaticSlots[i] = 0;
    }
    motorOff();
    refreshDisplay = true;
    waitForDownButtonRelease();
    state = CONFIG_MENU;
    writeSettings();
    refreshDisplay = true;
  } 
}

void showCutTypeMenu() {
  if (refreshDisplay) {
    refreshDisplay = false;
    
    lcd.clear();  

    char statusMessage[20];
 
    lcd.clear();
    if( isUniformCut ) { 
      snprintf(statusMessage, 20, "Uniform: S: %d", uniformSlot);
    } else {
      snprintf(statusMessage, 20, "Programmatic");
    }
    showCentered(statusMessage, 0);
    showOptions( 
       "Positive",  /* Left */
       "Back",      /* Bottom */
       "Negative"   /* Right */
    );
  }

  if (isLeftButtonPressed() ) {   // Positive
    refreshDisplay = true;
    waitForLeftButtonRelease();
    state = CUT_MENU;
    if (isUniformCut) { 
      determineMovementValues(uniformSlot, uniformSlot);
    } else {
      programmaticIndex = 1;
      determineMovementValues(programmaticSlots[0], programmaticSlots[1]);
    }
    setupForCut(true);
  } else if (isDownButtonPressed()) { 
    refreshDisplay = true;
    waitForDownButtonRelease();
    state = MAIN_MENU;
  } else if (isRightButtonPressed() ) {  // Negative
    refreshDisplay = true;
    waitForRightButtonRelease();
    state = CUT_MENU;
    if (isUniformCut) { 
      determineMovementValues(uniformSlot, uniformSlot);
    } else {
      programmaticIndex = 0;
      determineMovementValues(0, programmaticSlots[0]);
    }
    setupForCut(false);
  }
}

void showConfigChangeValueMenu() {
  if (refreshDisplay) {
    refreshDisplay = false;
    
    lcd.clear();  

    char statusMessage[20];
 
    lcd.clear();
    if( configOption == 0 ) { 
      snprintf(statusMessage, 20, "Slop: %d", slop);
    } else if( configOption == 1) { 
      snprintf(statusMessage, 20, "Kerf: %d", kerf);   
    } else if( configOption == 2 ) { 
      snprintf(statusMessage, 20, "MaxAdvance: %d", maxAdvance);   
    } else if( configOption == 3 ) { 
      snprintf(statusMessage, 20, "Slot Size: %d", uniformSlot);   
    } else if( configOption == 4 ) { 
      snprintf(statusMessage, 20, "Length: %d", lengthOfWood);   
    } else if( configOption == 5 ) { 
      snprintf(statusMessage, 20, "NumSlots: %d", numberOfSlots);   
    } else if( configOption == 6 ) { 
      snprintf(statusMessage, 20, "C: %d, D: %d", configSlotCount, configSlotDistance);
    }
    showCentered(statusMessage, 0);
    if( configOption == 6 ) { 
      showOptions( "Left", "Done", "Right", "Set" );
    } else { 
      showOptions( "Back", "Down", "Cancel", "Up" );
    }
  }

  if( configOption == 6 ) { 
    handleConfigProgrammaticSlots();
  } else {
    if( isUpButtonPressed() ) {
      // We don't wait for the button up when we are modifying the values that
      // we expect to be very large.  This allows us to move quickly around in
      // setting the value between large distances.
      if( configOption != 4 ) { 
        waitForUpButtonRelease();
      }
      if( configOption == 0 ) { 
        slop = slop + 1;
      } else if( configOption == 1 ) { 
        kerf = kerf + 1;
      } else if ( configOption == 2 ) { 
        maxAdvance = maxAdvance + 1;
      } else if ( configOption == 3 ) { 
        uniformSlot = uniformSlot + 1;
      } else if ( configOption == 4 ) { 
        lengthOfWood = lengthOfWood + 1;
        uniformSlot = recalculateSlotSize();
      } else if ( configOption == 5 ) { 
        numberOfSlots = numberOfSlots + 1;
        uniformSlot = recalculateSlotSize();
      }
      refreshDisplay = true;
    } else if (isLeftButtonPressed()) { 
      state = CONFIG_MENU;
      waitForLeftButtonRelease();
      writeSettings();
      refreshDisplay = true;
    } else if (isDownButtonPressed()) { 

      // We don't wait for the button to be released when we are modifying the
      // values that we expect to be very large.  This allows us to zoom around
      // in setting the value.
      if( configOption != 4 ) { 
        waitForDownButtonRelease();
      }
      if( configOption == 0 ) { 
        slop = slop - 1;
      } else if ( configOption == 1 ) { 
        kerf = kerf - 1;
      } else if ( configOption == 2 ) { 
        maxAdvance = maxAdvance - 1;
      } else if ( configOption == 3 ) { 
        uniformSlot = uniformSlot - 1;
      } else if ( configOption == 4 ) { 
        lengthOfWood = lengthOfWood - 1;
        uniformSlot = recalculateSlotSize();
      } else if ( configOption == 5 ) { 
        numberOfSlots = numberOfSlots - 1;
        uniformSlot = recalculateSlotSize();
      }
      refreshDisplay = true;
    } else if (isRightButtonPressed()) { 
      waitForRightButtonRelease();
      lcd.setCursor(1, 1);
      lcd.print("   CANCELED    ");
      delay(1000);
      state = CONFIG_MENU;
      refreshDisplay = true;
    }
  }
}


uint16_t d() {
  Serial.print( "computing d; distanceRemainingToCut: ");
  Serial.print( distanceRemainingToCut );
  Serial.print( ", distanceRemainingToMove: ");
  Serial.print( distanceRemainingToMove );
  Serial.print( ", kerf: " );
  Serial.print( kerf );
  Serial.print( ", maxAdvance: " );
  Serial.println( maxAdvance );
  if (distanceRemainingToCut > maxAdvance) { 
    Serial.println("Returning Kerf" );
    return maxAdvance;
  } else {
    Serial.println("Less to cut!  Returning distanceRemainingToCut!" );
    return distanceRemainingToCut;
  }
}


uint16_t getNextProgrammaticSlotSize() {
  programmaticIndex = programmaticIndex + 1;
  if (programmaticIndex >= MAX_PROGRAMMATIC_SLOTS ||
      programmaticSlots[programmaticIndex] == 0) { 
    Serial.println("Reached the end of the programmatic slots!");
    programmaticIndex = 0;
  }
  return programmaticSlots[programmaticIndex];
}

void showCutMenu() {
  if (refreshDisplay) {
    refreshDisplay = false;
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("+---+   ");
    lcd.setCursor(0, 1);
    lcd.print("|   |___+");

    // Show the slot size as "([d])" on the user interface.
    char distanceMessage[12];
    if (isUniformCut) { 
      snprintf(distanceMessage, 6, "(%d)", uniformSlot);
      lcd.setCursor(14,2);
    } else {
      snprintf(distanceMessage, 12, "(%d,%d)", 
        currentValleySize,
        currentFingerSize);
      lcd.setCursor(9,2);
    } 
    lcd.print(distanceMessage);

    if (distanceRemainingToCut == 0) { 
      lcd.setCursor(5, 1);
      lcd.print("*");
    } else if ( distanceRemainingToCut < subDTMValley / 3 ) { 
      lcd.setCursor(3, 1);
      lcd.print("^");
    } else if ( distanceRemainingToCut < ( 2*subDTMValley / 3) ) { 
      lcd.setCursor(2, 1);
      lcd.print("^");
    } else {
      lcd.setCursor(1, 1);
      lcd.print("^");
    }
    if( distanceRemainingToCut > 0 ) { 
      char statusMessage[10];
      snprintf(statusMessage, 10, "X %d %d", (int)(distanceRemainingToCut), (int)(subDTMValley) );
      lcd.setCursor(10, 0);
      lcd.print(statusMessage);
    } else {
      char statusMessage[10];
      snprintf(statusMessage, 10, "-> %d", (int)(distanceRemainingToMove) );
      lcd.setCursor(10, 0);
      lcd.print(statusMessage);
    }
 
    showOptions( 
      "Back", /* Left */
      NULL,   /* Bottom */
      "Next", /* Right */
      NULL,   /* Top */
      2 );
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

      if (!isUniformCut) { 
        uint16_t valleySize = getNextProgrammaticSlotSize();
        uint16_t fingerSize = getNextProgrammaticSlotSize();

        determineMovementValues(
          valleySize, 
          fingerSize);
      }

      distanceRemainingToCut = subDTMValley;
      distanceRemainingToMove = subDTMFinger;
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
    state = MAIN_MENU;
    refreshDisplay = true;
  }
}

void loop() {
  if (state == MAIN_MENU) { 
    showMainMenu();
  } else if (state == CUT_MENU) {     
    showCutMenu();
  } else if (state == CONFIG_MENU) {
    showConfigMenu();
  } else if (state == CONFIG_CHANGE_VALUE_MENU) {
    showConfigChangeValueMenu();
  } else if (state == CUT_TYPE_MENU) {
    showCutTypeMenu();
  }
}
