// [UNFIXED] far left 1023, far right 0 - opposite from expected

#define U 6
#define L 9
#define D 3
#define R 5
#define X A3
#define Y A2

void setup() {
  // put your setup code here, to run once:
  Serial.begin(9600);
  pinMode(U, OUTPUT);
  pinMode(L, OUTPUT);
  pinMode(D, OUTPUT);
  pinMode(R, OUTPUT);
  pinMode(X, INPUT);
  pinMode(Y, INPUT);
}

void loop() {
  // put your main code here, to run repeatedly:
  int rawX = analogRead(X);
  int levelX = map(rawX, 0, 1023, 0, 255);
  if (rawX > 512) {
    analogWrite(R, levelX - 127);
    analogWrite(L, 0);
  } else if (rawX < 512) {
    analogWrite(L, 512 - levelX);
    analogWrite(R, 0);
  }
  int rawY = analogRead(Y);
  int levelY = map(rawY, 0, 1023, 0, 255);
  if (rawY > 512) {
      analogWrite(U, levelY - 127);
      analogWrite(D, 0);
  } else if (rawY < 512) {
      analogWrite(D, 127 - levelY);
      analogWrite(U, 0);
  }
  Serial.println(rawX);
  Serial.println(rawY);
  Serial.println("\n");
}
