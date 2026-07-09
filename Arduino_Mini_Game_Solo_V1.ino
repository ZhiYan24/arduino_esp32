// matches component to digital pin
#define DOT_BUTTON 9
#define LINE_BUTTON 10
#define SUBMIT_BUTTON 11

#define BUZZER 12
#define RED_LED 8

int greenLEDs[5] = {3, 4, 5, 6, 7};

// string array for 5-letter words
String words[] = {
  "APPLE",
  "ROBOT",
  "PLANT",
  "HOUSE",
  "LIGHT",
  "STONE",
  "WATER",
  "TRAIN",
  "CLOUD",
  "BRICK"
};


String morseInput;

//matches Morse code to alphabet letter
String morseLetters[26] = {
  ".-", "-...", "-.-.", "-..", ".", 
  "..-.", "--.", "....", "..", ".---",
  "-.-", ".-..", "--", "-.", "---",
  ".--.", "--.-", ".-.", "...", "-",
  "..-", "...-", ".--", "-..-", "-.--",
  "--.."
};

char alphabet[26] = {
  'A','B','C','D','E','F','G','H','I','J',
  'K','L','M','N','O','P','Q','R','S','T',
  'U','V','W','X','Y','Z'
};


String targetWord;
int currentLetter = 0;


int lastDotState = HIGH;
int lastLineState = HIGH;
int lastSubmitState = HIGH;

unsigned long lastDotTime = 0;
unsigned long lastLineTime = 0;
unsigned long lastSubmitTime = 0;

const unsigned long debounceDelay = 200;


void setup() {

  // starts Serial Monitor
  Serial.begin(9600);

  //sets up pin modes as inputs or outputs
  pinMode(DOT_BUTTON, INPUT_PULLUP);
  pinMode(LINE_BUTTON, INPUT_PULLUP);
  pinMode(SUBMIT_BUTTON, INPUT_PULLUP);

  pinMode(RED_LED, OUTPUT);
  pinMode(BUZZER, OUTPUT);


  for(int i = 0; i < 5; i++){
    pinMode(greenLEDs[i], OUTPUT);
  }

  //pick a random word and print
  randomSeed(analogRead(A0));

  int randomIndex = random(0,10);
  targetWord = words[randomIndex];

  Serial.println(targetWord);
}



void loop() {

  int dotState = digitalRead(DOT_BUTTON);
  int lineState = digitalRead(LINE_BUTTON);
  int submitState = digitalRead(SUBMIT_BUTTON);

  unsigned long time = millis();

    // DOT button
  if (lastDotState == HIGH && dotState == LOW) {

    if (time - lastDotTime > debounceDelay) {
      morseInput += ".";
      Serial.println(morseInput);
      lastDotTime = millis();
    }

  }


  // LINE button
  if (lastLineState == HIGH && lineState == LOW) {

    if (time - lastLineTime > debounceDelay) {
      morseInput += "-";
      Serial.println(morseInput);
      lastLineTime = millis();
    }

  }


  // SUBMIT button
  if (lastSubmitState == HIGH && submitState == LOW) {

    if (time - lastSubmitTime > debounceDelay) {
      checkLetter();
      morseInput = "";
      Serial.println("Reset");
      lastSubmitTime = millis();
    }

  }


  // Update previous states
  lastDotState = dotState;
  lastLineState = lineState;
  lastSubmitState = submitState;

}




void checkLetter(){

  char decoded = decodeMorse(morseInput);


  Serial.print("Entered: ");
  Serial.println(decoded);



  if(decoded == targetWord[currentLetter]){

    digitalWrite(greenLEDs[currentLetter], HIGH);

    currentLetter++;


    if(currentLetter == 5){
      winGame();
    }

  }

  else{

    digitalWrite(RED_LED, HIGH);
    delay(500);
    digitalWrite(RED_LED, LOW);

  }

}



char decodeMorse(String code){

  for(int i=0; i<26; i++){

    if(code == morseLetters[i]){
      return alphabet[i];
    }

  }

  return '?';

}



void winGame(){

  tone(BUZZER, 523,200);
  delay(200);

  tone(BUZZER, 659,200);
  delay(200);

  tone(BUZZER, 784,200);
  delay(200);

  tone(BUZZER, 1047,500);

  while(true){
    // stop game
  }

}