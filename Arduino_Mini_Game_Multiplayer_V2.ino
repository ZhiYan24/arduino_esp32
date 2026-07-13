#include <EEPROM.h>

// matches each component to a pin

// Player 1 Buttons
#define P1_DOT 7
#define P1_DASH 8
#define P1_SUBMIT 9

// Player 2 Buttons
#define P2_DOT 12
#define P2_DASH 11
#define P2_SUBMIT 10


// Display Board
#define RED_LED 6
#define YELLOW_LED 5

int greenLEDs[5] = {0,1,2,3,4};


// Scoreboard
#define BUZZER 13

int whiteLEDs[3] = {A5,A4,A3};
int blueLEDs[3] = {A0,A1,A2};


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
  "BRICK",
  "DREAM",
  "EARTH",
  "FLAME",
  "GRAPE",
  "MOUSE",
  "NIGHT",
  "OCEAN",
  "QUEEN",
  "UNITY",
  "VIRUS",
  "YOUNG",
  "ZEBRA",
  "QUICK",
  "JUMPY",
  "FROST",
  "BLAZE",
  "PIXEL",
  "GIANT",
  "HAPPY",
  "SNAKE",
  "TIGER",
  "WHALE",
  "CANDY",
  "FUZZY",
  "JELLY",
  "KOALA",
  "XENON",
  "BEACH",
  "CHESS",
  "FIELD",
  "GRASS",
  "HEART",
  "INDEX",
  "LEMON",
  "MAGIC",
  "NORTH",
  "PIZZA",
  "RIVER",
  "SHEEP",
  "ZONES"
};

int wordNum = sizeof(words) / sizeof(words[0]);


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


//current game state: current letter, player 1/2 turn, score
String targetWord;

int currentLetter = 0;

int currentPlayer = 1;

int player1Score = 0;
int player2Score = 0;


// variables for edge detection: prev_state vs cur_state + millis() debounce check
int lastP1Dot = HIGH;
int lastP1Dash = HIGH;
int lastP1Submit = HIGH;

int lastP2Dot = HIGH;
int lastP2Dash = HIGH;
int lastP2Submit = HIGH;

unsigned long lastDotTime = 0;
unsigned long lastDashTime = 0;
unsigned long lastSubmitTime = 0;

int lastDotState = HIGH;
int lastDashState = HIGH;
int lastSubmitState= HIGH;

//setting delay to 200 so that when the buttons are pressed they aren't registered incorrectly 
//at one point we would click the red button once and the serial monitor would show that we clicked it twice
//we tried 50 --> 100 --> 300 then found 200 worked pretty well
const unsigned long debounceDelay = 200;


void setup() {

  //starts Serial Monitor
  // Serial.begin(9600);

  //sets up pin modes as inputs or outputs for buttons, LEDs, buzzer
  pinMode(P1_DOT,INPUT_PULLUP);
  pinMode(P1_DASH,INPUT_PULLUP);
  pinMode(P1_SUBMIT,INPUT_PULLUP);

  pinMode(P2_DOT,INPUT_PULLUP);
  pinMode(P2_DASH,INPUT_PULLUP);
  pinMode(P2_SUBMIT,INPUT_PULLUP);


  pinMode(RED_LED, OUTPUT);
  pinMode(BUZZER, OUTPUT);
  pinMode(YELLOW_LED,OUTPUT);

  for(int i = 0; i < 5; i++){
    pinMode(greenLEDs[i], OUTPUT);
  }

  for (int i = 0; i < 3; i++) {
    pinMode(whiteLEDs[i], OUTPUT);
    pinMode(blueLEDs[i], OUTPUT);
  }


  //pick a random word using a seed that changes each startup
  int seed;

  EEPROM.get(0, seed);
  seed++;
  EEPROM.put(0, seed);

  randomSeed(seed);

  int randomIndex = random(0, wordNum);
  targetWord = words[randomIndex];

  // Serial.println(targetWord);
}


void loop(){
//read buttons for player 1 vs 2 based on whose turn it is
    
  if(currentPlayer == 1){

    readButtons(P1_DOT, P1_DASH, P1_SUBMIT);

  }
  else{

    readButtons(P2_DOT, P2_DASH, P2_SUBMIT);

  }

}

void readButtons(int dotPin, int dashPin, int submitPin){
  //reading buttons states
  int dotState = digitalRead(dotPin);
  int dashState = digitalRead(dashPin);
  int submitState = digitalRead(submitPin);

  unsigned long currentTime = millis();

  // DOT BUTTON
  if(lastDotState == HIGH && dotState == LOW){
    if(currentTime - lastDotTime > debounceDelay){
      morseInput += ".";
      // Serial.println(morseInput);
      lastDotTime = currentTime;

    }

  }

  // DASH BUTTON
  if(lastDashState == HIGH && dashState == LOW){
    if(currentTime - lastDashTime > debounceDelay){
      morseInput += "-";
      // Serial.println(morseInput);
      lastDashTime = currentTime;
    }

  }

  // SUBMIT BUTTON
  if(lastSubmitState == HIGH && submitState == LOW){
    if(currentTime - lastSubmitTime > debounceDelay){
      checkLetter();
      morseInput = "";      
      // Serial.println("Reset");
      lastSubmitTime = currentTime;
    }

  }

  // update previous states
  lastDotState = dotState;
  lastDashState = dashState;
  lastSubmitState = submitState;

}

void checkLetter(){

  char decoded = decodeMorse(morseInput);


  // Serial.print("Entered: ");
  // Serial.println(decoded);



  // Correct letter

  if(decoded == targetWord[currentLetter]){

    digitalWrite(greenLEDs[currentLetter], HIGH);


    currentLetter++;


    // Completed the whole word
    if(currentLetter == 5){

      winRound();

    }

  }


  // Letter exists somewhere else in the word
  //index of retursn 0-4 if in the word, else return -1

  else if(targetWord.indexOf(decoded) != -1){
    flashYellow();
    switchPlayer();
  }

  // Letter does not exist in word

  else{
    flashRed();
    switchPlayer();
  }

}

// check back here if not registering correctly, may be due to delay
void flashYellow(){

  digitalWrite(YELLOW_LED, HIGH);

  delay(500);

  digitalWrite(YELLOW_LED, LOW);

}

void flashRed(){

  digitalWrite(RED_LED, HIGH);

  delay(500);

  digitalWrite(RED_LED, LOW);

}

void switchPlayer(){

  if(currentPlayer == 1){

    currentPlayer = 2;

  }

  else{

    currentPlayer = 1;

  }


  // Serial.print("Player ");

  // Serial.print(currentPlayer);

  // Serial.println("'s turn");

}

char decodeMorse(String code){

  for(int i=0; i<26; i++){

    if(code == morseLetters[i]){

      return alphabet[i];

    }

  }

  return '?';

}

void startRound(){

  // Reset word progress

  currentLetter = 0;

  morseInput = "";


  // Reset green LEDs

  for(int i = 0; i < 5; i++){

    digitalWrite(greenLEDs[i], LOW);

  }


  // Turn off feedback LEDs

  digitalWrite(RED_LED, LOW);

  digitalWrite(YELLOW_LED, LOW);



  // Choose random word

  int randomIndex = random(0, wordNum);

  targetWord = words[randomIndex];


  // Serial.print("New Word: ");

  // Serial.println(targetWord);



  // Losing player starts next round
  // currentPlayer was the winner, so switch it

  switchPlayer();


}

void givePoint(){


  if(currentPlayer == 1){


    digitalWrite(whiteLEDs[player1Score], HIGH);

    player1Score++;


    // Serial.print("Player 1 Score: ");

    // Serial.println(player1Score);


  }


  else{


    digitalWrite(blueLEDs[player2Score], HIGH);

    player2Score++;


    // Serial.print("Player 2 Score: ");

    // Serial.println(player2Score);


  }

}

void winRound(){


  // Serial.println("ROUND WON!");



  // Short victory tune

  tone(BUZZER, 523,200);
  delay(200);

  tone(BUZZER, 659,200);
  delay(200);

  tone(BUZZER, 784,300);
  delay(300);

  tone(BUZZER, 1047,500);



  givePoint();



  // Check if someone won the whole game

  if(player1Score == 3 || player2Score == 3){

    winGame();

  }


  else{


    // Start another round

    startRound();


  }


}

void winGame(){


  // Serial.println("GAME OVER!");



  // Longer celebration melody

  tone(BUZZER,523,250);
  delay(250);

  tone(BUZZER,659,250);
  delay(250);

  tone(BUZZER,784,250);
  delay(250);

  tone(BUZZER,1047,400);
  delay(400);

  tone(BUZZER,784,250);
  delay(250);

  tone(BUZZER,1047,700);


// Flash winner's score LEDs

if(player1Score == 3){

  for(int j = 0; j < 5; j++){   // flash 5 times

    for(int i = 0; i < 3; i++){
      digitalWrite(whiteLEDs[i], LOW);
    }

    delay(200);

    for(int i = 0; i < 3; i++){
      digitalWrite(whiteLEDs[i], HIGH);
    }

    delay(200);

  }

}


else if(player2Score == 3){

  for(int j = 0; j < 5; j++){   // flash 5 times

    for(int i = 0; i < 3; i++){
      digitalWrite(blueLEDs[i], LOW);
    }

    delay(200);

    for(int i = 0; i < 3; i++){
      digitalWrite(blueLEDs[i], HIGH);
    }

    delay(200);

  }

}



  // Stop the game

  while(true){

  }


}

}
