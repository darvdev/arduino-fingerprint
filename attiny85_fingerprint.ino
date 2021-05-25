#include <Adafruit_Fingerprint.h>
#include <SoftSerial.h>

#define TX_PIN 3
#define RX_PIN 4
#define RELAY_PIN 5
#define BUILTIN_LED 1

unsigned long oneSecMillis = 0;
unsigned long sensorFailedMillis = 0;

bool sensorInitialize = false;
bool ledState = true;

SoftSerial serial(TX_PIN, RX_PIN);
Adafruit_Fingerprint finger = Adafruit_Fingerprint(&serial);

void setup() {
  pinMode(BUILTIN_LED, OUTPUT);
  pinMode(RELAY_PIN, OUTPUT);
  digitalWrite(BUILTIN_LED, ledState);
  finger.begin(57600);
  sensorInitialize = finger.verifyPassword();
}

void loop() {
  unsigned long ms = millis();
  if (ms - oneSecMillis >= 1000) {
    if (sensorInitialize){
      ledState = !ledState;
      digitalWrite(BUILTIN_LED, ledState);
    }
    oneSecMillis = ms;
  }

  if (ms - sensorFailedMillis >= 150){
    if (!sensorInitialize){
      ledState = !ledState;
      digitalWrite(BUILTIN_LED, ledState);
    }
    sensorFailedMillis = ms;
  }

  
  
}
