#include <LiquidCrystal_I2C.h>

LiquidCrystal_I2C lcd(0x27, 16, 2);

byte letter_one[8] = {
  0b10001, 
  0b10001, 
  0b10001, 
  0b10001, 
  0b10101, 
  0b10001, 
  0b10001, 
  0b10001
};

byte letter_two[8] = {
  0b10101, 
  0b00100, 
  0b00100, 
  0b00100, 
  0b00100, 
  0b00100, 
  0b00100, 
  0b10101
};

void setup() {
  // put your setup code here, to run once:
  lcd.init();
  lcd.backlight();

  lcd.createChar(0, letter_one);
  lcd.createChar(1, letter_two);

  lcd.setCursor(0, 0);
  lcd.write(byte(0));
  lcd.write(byte(1));
}

void loop() {
  // put your main code here, to run repeatedly:

}
