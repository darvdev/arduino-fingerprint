#include <Adafruit_Fingerprint.h>

#define TX_PIN 2
#define RX_PIN 3
#define RELAY_PIN 4
#define ENROLL_TIMEOUT 30

SoftwareSerial serial(TX_PIN, RX_PIN);
Adafruit_Fingerprint finger = Adafruit_Fingerprint(&serial);

bool ledState = false;
bool relayState = false;
bool sensorInitialized = false;
bool matchedFingerprint = false;
bool placedFingerprint = false;
bool enrollStage2 = false;

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
  pinMode(RELAY_PIN, OUTPUT);
  digitalWrite(LED_BUILTIN, ledState);
  digitalWrite(RELAY_PIN, relayState);

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
      uint8_t f = finger.loadModel(id);
      Serial.print("ID result: "); Serial.println(f);

      if (f != FINGERPRINT_OK) {
        nextId = id;
        break;
      }
    }
    
  }
  
  Serial.println("Ready!");
  session = SESSION.READY;
}

void loop() {

  unsigned long ms = millis();

  // 50 Milliseconds
  if (ms - _50 >= 50) {

    switch (session) {
      case SESSION.READY:
        if (sensorInitialized){
          listenToFingerprint();
        }
        break;
      
      case SESSION.ENROLL:
        if (enrollStage2) {
          enrollFingerprint2();
        } else {
          enrollFingerprint1();
        }
        break;
    }

    _50 = ms;
  }

  // 150 Milliseconds
  if (ms - _150 >= 150) {

    switch (session) {
      case SESSION.READY:
        if (!sensorInitialized) {
          ledState = !ledState;
          digitalWrite(LED_BUILTIN, ledState);
        }

        if (matchedFingerprint && !ledState) {
          ledState = true;
          digitalWrite(LED_BUILTIN, ledState);
        }

        break;

      case SESSION.ENROLL:
        if (lightCount < 4) {
          lightCount++;
          ledState = !ledState;
          digitalWrite(LED_BUILTIN, ledState);
        }
        break;
    }

    _150 = ms;
  }

  // 1000 Milliseconds
  if (ms - _1000 >= 1000) {

    switch (session) {
      case SESSION.READY:
        if (sensorInitialized && !matchedFingerprint && !relayState) { //Blink led one second at a time
          ledState = !ledState;
          digitalWrite(LED_BUILTIN, ledState);
        }
        break;
      
      case SESSION.ENROLL:
        enrollTimeout++;
        if (enrollTimeout >= ENROLL_TIMEOUT) {
          Serial.println("Enroll timeout");
          enrollTimeout = 0;
          enrollStage2 = false;
          session = SESSION.READY;
        }
        break;

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
        Serial.print("Timer: "); Serial.println(stage);
        matchedFingerprint = false;
        prevStage = 0;

        if (stage >= STAGE.EMPTY) {
          Serial.println("Delete all fingerprints");

        } else if (stage >= STAGE.ENROLL) {
          Serial.println("Enroll fingerprint");
          Serial.print("ID #"); Serial.println(nextId);
          ledState = false;
          digitalWrite(LED_BUILTIN, ledState);
          Serial.print("Waiting for valid finger to enroll as #"); Serial.println(nextId);
          session = SESSION.ENROLL;
          
        } else if (stage >= STAGE.ACTIVATE) {
          relayState = !relayState;
          digitalWrite(RELAY_PIN, relayState);
          if (relayState) {
            ledState = true;
            digitalWrite(LED_BUILTIN, ledState);
            Serial.println("Relay activated");
            
          } else {
            Serial.println("Relay deactivated");
          } 

        } else {
          Serial.print("Invalid stage");

        }

        stage = 0;
        
      }
      prevStage = stage;
    }
    
    _1500 = ms;
  }

}

void listenToFingerprint() {
  uint8_t p = finger.getImage();
  
  switch (p) {
    case FINGERPRINT_OK:
      Serial.println("Image taken");
      if (matchedFingerprint) {
        stage++;
        // ledState = false;
        // digitalWrite(LED_BUILTIN, ledState);
      }
      break;
    case FINGERPRINT_NOFINGER:
//      Serial.println("No finger detected");
      return;
    case FINGERPRINT_PACKETRECIEVEERR:
      Serial.println("Communication error");
      return;
    case FINGERPRINT_IMAGEFAIL:
      Serial.println("Imaging error");
      return;
    default:
      Serial.println("Unknown error");
      return;
  }

  // OK success!

  p = finger.image2Tz();
  switch (p) {
    case FINGERPRINT_OK:
      // Serial.println("Image converted");
      break;
    case FINGERPRINT_IMAGEMESS:
      Serial.println("Image too messy");
      return;
    case FINGERPRINT_PACKETRECIEVEERR:
      Serial.println("Communication error");
      return;
    case FINGERPRINT_FEATUREFAIL:
      Serial.println("Could not find fingerprint features");
      return;
    case FINGERPRINT_INVALIDIMAGE:
      Serial.println("Could not find fingerprint features");
      return;
    default:
      Serial.println("Unknown error");
      return;
  }

  // OK converted!
  p = finger.fingerSearch();
  if (p == FINGERPRINT_OK) {
    // Serial.println("Found a print match!");
  } else if (p == FINGERPRINT_PACKETRECIEVEERR) {
    Serial.println("Communication error");
    return;
  } else if (p == FINGERPRINT_NOTFOUND) {
    Serial.println("Did not find a match");
    return;
  } else {
    Serial.println("Unknown error");
    return;
  }

  // found a match!
  Serial.print("Found ID #"); Serial.print(finger.fingerID);
  Serial.print(" with confidence of "); Serial.println(finger.confidence);

  if (relayState) {
    relayState = false;
    digitalWrite(RELAY_PIN, relayState);
  } else {
    stage++;
    ledState = true;
    digitalWrite(LED_BUILTIN, ledState);
    matchedFingerprint = true;
  }
  
  return;// finger.fingerID;
}

void enrollFingerprint1() {
  int p = finger.getImage();
  if (p != FINGERPRINT_OK) {
    return;
  }

  // OK success!
  p = finger.image2Tz(1);

  if (p == FINGERPRINT_OK) {
    Serial.println("Remove finger");
    delay(2000);
    p = 0;
    while (p != FINGERPRINT_NOFINGER) {
      p = finger.getImage();
    }
    Serial.println("Place same finger again");
    enrollStage2 = true;
  } else {
    Serial.println("Failed to enroll fingerprint stage 1");
    session = SESSION.READY;
  }

}

void enrollFingerprint2() {
  
  int p = finger.getImage();

  if (p != FINGERPRINT_OK) {
    return;
  }

  // OK success!
  p = finger.image2Tz(2);
  if (p != FINGERPRINT_OK) {
    Serial.println("Failed to enroll fingerprint stage 2");
    session = SESSION.READY;
    enrollStage2 = false;
    return;
  }

  

  // OK converted!
  Serial.print("Creating model for #");  Serial.println(nextId);

  bool success = false;
  String m = "";
  p = finger.createModel();

  if (p == FINGERPRINT_OK) {
    Serial.println("Prints matched!");
    success = true;
  } else if (p == FINGERPRINT_PACKETRECIEVEERR) {
    m = "Communication error";
  } else if (p == FINGERPRINT_ENROLLMISMATCH) {
    m = "Fingerprints did not match";
  } else {
    m = "Unknown error";
  }

  if (!success) {
    Serial.println(m);
    session = SESSION.READY;
    enrollStage2 = false;
    return;
  }

  p = finger.storeModel(nextId);

  if (p == FINGERPRINT_OK) {
    Serial.println("Stored!");
  } else if (p == FINGERPRINT_PACKETRECIEVEERR) {
    Serial.println("Communication error");
  } else if (p == FINGERPRINT_BADLOCATION) {
    Serial.println("Could not store in that location");
  } else if (p == FINGERPRINT_FLASHERR) {
    Serial.println("Error writing to flash");
  } else {
    Serial.println("Unknown error");
  }


  session = SESSION.READY;
  enrollStage2 = false;
  return;
}