#define MAX 100

void setup() {
  // put your setup code here, to run once:
  Serial.begin(9600);
}

void loop() {
  // put your main code here, to run repeatedly:
  static char message[MAX];
  static int cur_pos = 0;

  if (Serial.available() > 0) {
    char cur_char = Serial.read();

    // if over 100, break, print 100, reset cur_pos, print remainder
    if (cur_char == '\n' || cur_pos == MAX) {
      message[cur_pos] = '\0';
      Serial.println(message);
      cur_pos = 0;
    } else {
      message[cur_pos] = cur_char;
      cur_pos++;
    }
  }
}
