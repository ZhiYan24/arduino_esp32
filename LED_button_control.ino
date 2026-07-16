#define LED 7
#define button 2

int LED_state = LOW;

void setup() {
  // put your setup code here, to run once:
  pinMode(LED, OUTPUT);
  pinMode(button, INPUT_PULLUP);
  Serial.begin(9600);
}

void loop() {
  // put your main code here, to run repeatedly:
  int button_state = digitalRead(button);
  Serial.println(button_state);
  if (button_state == LOW) {
    if (LED_state == LOW) {
      LED_state = HIGH;
    }
    else {
      LED_state = LOW;
    }
    digitalWrite(LED, LED_state);
    delay(5000);
  }
}
