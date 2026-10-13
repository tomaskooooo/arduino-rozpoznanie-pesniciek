// LCD data-wire finder. Tries all 24 orderings of the four data wires (Mega 24-27).
// Whichever ordering matches the real wiring shows "Hello" / "Try <n>" readable; the rest look garbled.
#include <LiquidCrystal.h>

const uint8_t RS = 22, E = 23;
const uint8_t PINS[4] = {24, 25, 26, 27};

// Next lexicographic permutation of idx[0..3]; returns false after the last one.
bool nextPerm(uint8_t* a) {
  int i = 2;
  while (i >= 0 && a[i] > a[i + 1]) i--;
  if (i < 0) return false;
  int j = 3;
  while (a[j] < a[i]) j--;
  uint8_t t = a[i]; a[i] = a[j]; a[j] = t;
  for (int l = i + 1, r = 3; l < r; l++, r--) { t = a[l]; a[l] = a[r]; a[r] = t; }
  return true;
}

void setup() {
  Serial.begin(115200);
  delay(1000);
}

void loop() {
  uint8_t p[4] = {0, 1, 2, 3};
  uint8_t n = 1;
  do {
    // LCD D4..D7 are assumed to be on Mega pins PINS[p[0]]..PINS[p[3]]
    LiquidCrystal lcd(RS, E, PINS[p[0]], PINS[p[1]], PINS[p[2]], PINS[p[3]]);
    lcd.begin(16, 2);
    lcd.clear();
    lcd.print("Hello");
    lcd.setCursor(0, 1);
    lcd.print("Try ");
    lcd.print(n);
    Serial.print(F("Try "));
    Serial.print(n);
    Serial.print(F(": D4="));
    Serial.print(PINS[p[0]]);
    Serial.print(F(" D5="));
    Serial.print(PINS[p[1]]);
    Serial.print(F(" D6="));
    Serial.print(PINS[p[2]]);
    Serial.print(F(" D7="));
    Serial.println(PINS[p[3]]);
    n++;
    delay(4000);
  } while (nextPerm(p));
}
