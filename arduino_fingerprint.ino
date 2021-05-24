#include <Adafruit_Fingerprint.h>

#define TX_PIN 2
#define RX_PIN 3
#define ENROLL_TIMEOUT 30

SoftwareSerial serial(TX_PIN, RX_PIN);
Adafruit_Fingerprint finger = Adafruit_Fingerprint(&serial);

bool ledState = false;
bool sensorInitialized = false;
bool matchedFingerprint = false;

uint8_t stage = 0;
uint8_t prevStage = 0;
uint8_t lightCount = 0;
uint8_t enrollTimeout = 0;
uint8_t session = 0;

uint8_t fingerprintCount = 0;
uint8_t fingerprintCapacity = 0;
uint8_t nextId = 0;


unsigned long _50 = 0;
unsigned long _150 = 0;
unsigned long _1500 = 0;
unsigned long _1000 = 0;

static const struct SESSION {
  static const uint8_t INIT = 0;
  static const uint8_t READY = 1;
  static const uint8_t ENROLL = 2;
} SESSION;

static const struct STAGE {
  static const uint8_t ACTIVATE = 1;
  static const uint8_t ENROLL = 4;
  static const uint8_t EMPTY = 15;
} STAGE;




void setup() {
  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(LED_BUILTIN, ledState);
  Serial.begin(9600);
  Serial.println("Starting...");
  finger.begin(56700);
  sensorInitialized = finger.verifyPassword();

  if (!sensorInitialized) {
    Serial.println("Sensor is unavailable");

  } else {
    
    Serial.println("Sensor ready");

    finger.getParameters();
    fingerprintCapacity = finger.capacity;
    Serial.print("Fingerprint capacity: "); Serial.println(fingerprintCapacity);

    finger.getTemplateCount();
    fingerprintCount = finger.templateCount;
    Serial.print("Template count: "); Serial.println(fingerprintCount);

    for (uint8_t id = 1; id <= fingerprintCapacity; id++) {
      Serial.print("Getting fingerprint id #"); Serial.println(id);
      uint8_t i = finger.loadModel(id);
      Serial.print("ID result: "); Serial.println(i);

      if (i != FINGERPRINT_OK) {
        nextId = i;
        break;
      }
    }

    session = SESSION.READY;
    Serial.println("Ready!");
  }
  

}

void loop() {

  unsigned long ms = millis();

  // 50 Milliseconds
  if (ms - _50 >= 50) {
    if (sensorInitialized){
      listenToFingerprint();
    }
    _50 = ms;
  }

  // 150 Milliseconds
  if (ms - _150 >= 150) {

    if (!sensorInitialized && session == SESSION.READY) {
      ledState = !ledState;
      digitalWrite(LED_BUILTIN, ledState);
    } else if (session == SESSION.ENROLL) {
      if (lightCount < 4) {
        lightCount++;
        ledState = !ledState;
        digitalWrite(LED_BUILTIN, ledState);
      }
    }

    _150 = ms;
  }

  // 1000 Milliseconds
  if (ms - _1000 >= 1000) {

    if (session == SESSION.READY) {
      if (sensorInitialized) {
        ledState = !ledState;
        digitalWrite(LED_BUILTIN, ledState);
      }
      
    } else if (session == SESSION.ENROLL) {
      if (enrollTimeout >= ENROLL_TIMEOUT) {
        enrollTimeout = 0;
        session = SESSION.READY;
      }
      enrollTimeout++;
    
    }

    _1000 = ms;
  }
  
  // 1500 Milliseconds
  if (ms - _1500 >= 1500) {

    if (lightCount > 3) {
      lightCount = 0;
    }

    if (matchedFingerprint) {
      if (stage == prevStage) {
        matchedFingerprint = false;
        prevStage = 0;

        if (stage >= STAGE.EMPTY) {
          Serial.println("Delete all fingerprints");

        } else if (stage >= STAGE.ENROLL) {
          Serial.println("Enroll fingerprint");
          Serial.print("ID #"); Serial.println(nextId);
          
          session = SESSION.ENROLL;
          digitalWrite(LED_BUILTIN, LOW);
          
        } else if (stage >= STAGE.ACTIVATE) {
          Serial.println("Relay activated");

        } else {
          Serial.print("Invalid stage");

        }

        Serial.print("Timer: "); Serial.println(stage);
        stage = 0;
        
      }
      prevStage = stage;
    }
    
    _1500 = ms;
  }

}

uint8_t listenToFingerprint() {
  uint8_t p = finger.getImage();
  switch (p) {
    case FINGERPRINT_OK:
      Serial.println("Image taken");
      if (matchedFingerprint) {
        stage++;
      }
      break;
    case FINGERPRINT_NOFINGER:
//      Serial.println("No finger detected");
      return p;
    case FINGERPRINT_PACKETRECIEVEERR:
      Serial.println("Communication error");
      return p;
    case FINGERPRINT_IMAGEFAIL:
      Serial.println("Imaging error");
      return p;
    default:
      Serial.println("Unknown error");
      return p;
  }

  // OK success!

  p = finger.image2Tz();
  switch (p) {
    case FINGERPRINT_OK:
      // Serial.println("Image converted");
      break;
    case FINGERPRINT_IMAGEMESS:
      Serial.println("Image too messy");
      return p;
    case FINGERPRINT_PACKETRECIEVEERR:
      Serial.println("Communication error");
      return p;
    case FINGERPRINT_FEATUREFAIL:
      Serial.println("Could not find fingerprint features");
      return p;
    case FINGERPRINT_INVALIDIMAGE:
      Serial.println("Could not find fingerprint features");
      return p;
    default:
      Serial.println("Unknown error");
      return p;
  }

  // OK converted!
  p = finger.fingerSearch();
  if (p == FINGERPRINT_OK) {
    // Serial.println("Found a print match!");
  } else if (p == FINGERPRINT_PACKETRECIEVEERR) {
    Serial.println("Communication error");
    return p;
  } else if (p == FINGERPRINT_NOTFOUND) {
    Serial.println("Did not find a match");
    return p;
  } else {
    Serial.println("Unknown error");
    return p;
  }

  // found a match!
  Serial.print("Found ID #"); Serial.print(finger.fingerID);
  Serial.print(" with confidence of "); Serial.println(finger.confidence);
  stage++;
  matchedFingerprint = true;
  return finger.fingerID;
}
