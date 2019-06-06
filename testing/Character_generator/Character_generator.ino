#include <LiquidCrystal.h>

// initialize the library
LiquidCrystal lcd(12, 11, 5, 4, 3, 2);

byte customChar[8] = {
    0x0,0xa,0x1f,0x1f,0xe,0x4,0x0
};
            
void setup()
{
  // create a new custom character
  lcd.createChar(0, customChar);
  
  // set up number of columns and rows
  lcd.begin(16, 2);

  // print the custom char to the lcd
  // why typecast? see: http://arduino.cc/forum/index.php?topic=74666.0
  lcd.write((uint8_t)0);
}

void loop()
{
  
}
