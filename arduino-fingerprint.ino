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
  static const uint8_t ENROLL = 5;
  static const uint8_t EMPTY = 10;
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
  }
  
  Serial.println("Program ready!");
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

        else if (matchedFingerprint && ledState) {
          ledState = false;
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
          Serial.print("Waiting for valid finger to enroll as #"); Serial.println(nextId);
          ledState = false;
          digitalWrite(LED_BUILTIN, ledState);
          session = SESSION.ENROLL;
          
        } else if (stage >= STAGE.ACTIVATE) {
          relayState = !relayState;
          digitalWrite(RELAY_PIN, relayState);
          
          if (relayState) {
            ledState = true;
            digitalWrite(LED_BUILTIN, ledState); 
          }
          Serial.println(relayState ? "Relay activated" : "Relay deactivated");

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
  if (p != FINGERPRINT_OK) {
    return;
  }


  Serial.println("Placed a finger");
  
  if (matchedFingerprint) {
    stage++;
    Serial.print("Stage "); Serial.println(stage);
    // ledState = relayState ? false : true;
    // digitalWrite(LED_BUILTIN, ledState);
    ledState = true;
    digitalWrite(LED_BUILTIN, ledState);
  }

  // OK success!
  p = finger.image2Tz();

  if (p != FINGERPRINT_OK) {
    Serial.print("Convert fingerprint error: "); Serial.println(p);
    return;
  }

  

  // OK converted!
  p = finger.fingerSearch();

  if (p != FINGERPRINT_OK) {
    if (p == FINGERPRINT_NOTFOUND) {
      Serial.println("Fingerprint no match found!");
    } else {
      Serial.print("Search fingerprint error: "); Serial.println(p);
    }
    return;
  }

  // found a match!
  Serial.print("Found ID: "); Serial.print(finger.fingerID); Serial.print(" confidence: "); Serial.println(finger.confidence);

  if (relayState) {
    relayState = false;
    digitalWrite(RELAY_PIN, relayState);
  } else {

    if (!matchedFingerprint) {
      stage = 1;
      Serial.println("Add 1st stage");
      ledState = false;
      digitalWrite(LED_BUILTIN, ledState);
      delay(500);
      ledState = true;
      digitalWrite(LED_BUILTIN, ledState);
      delay(500);
      matchedFingerprint = true;
    }
    
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

void getEnrollId() {
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