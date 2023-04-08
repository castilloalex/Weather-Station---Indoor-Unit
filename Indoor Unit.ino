// Indoor unit pseudocode code
#include <LiquidCrystal.h>
#include <SPI.h>
#include <nRF24L01.h>
#include <RF24.h>
#include <dht_nonblocking.h>

#define CONTRAST_PIN 10
#define DHT_SENSOR_TYPE DHT_TYPE_22

const int rs = 9, en = 6, d4 = 5, d5 = 4, d6 = 3, d7 = 2;
LiquidCrystal lcd(rs, en, d4, d5, d6, d7);

const byte address[6] = "00001";
RF24 radio(7, 8);  // CE, CSN

const int DHT_SENSOR_PIN = A5;
DHT_nonblocking dht_sensor(DHT_SENSOR_PIN, DHT_SENSOR_TYPE);

const int tempUnitButton = A0, menuButton = A1, activityLEDPin = A2, warningLEDPin = A3;

unsigned long timeSinceLast = 0;

char currentOutdoorTemperature[32] = "-99";
char newOutdoorTemperature[32] = "-99";

float currentIndoorTemperature = -99;
float currentIndoorHumidity = -99;
float newIndoorTemperature = -99;
float newIndoorHumidity = -99;

// Default unit is C
int preferredTemperatureUnit = 1;

bool tempChangeMade = false;

int menuCounter = 1;

int tempUnitButtonState;
int tempUnitLastButtonState = LOW;  
unsigned long tempUnitLastDebounceTime = 0;

int menuButtonState;
int menuLastButtonState = LOW;  
unsigned long menuLastDebounceTime = 0;

unsigned long debounceDelay = 50;

// Prints the unit of temperature
void printTemperatureUnit() {
  if (preferredTemperatureUnit == 1) {
    lcd.print("C");
  } else if (preferredTemperatureUnit == 2) {
    lcd.print("F");
  } else {
    lcd.print("err");
  }
}

void displayTemperatureInformation() {
    lcd.clear();
    lcd.print("Outdoor: ");
    if (preferredTemperatureUnit == 2) {
      lcd.print((atof(currentOutdoorTemperature) * (9.0/5.0)) + 32);
    } else {
      lcd.print(currentOutdoorTemperature);
    }
    printTemperatureUnit();

    lcd.setCursor(0, 1);
    lcd.print("Indoor: ");
    if (preferredTemperatureUnit == 2) {
      lcd.print(((currentIndoorTemperature) * (9.0/5.0)) + 32);
    } else {
      lcd.print(currentIndoorTemperature);
    }
    printTemperatureUnit();
}

void displayTemperatureDeltas() {
  lcd.clear();
  lcd.print("It is ");
  if (currentIndoorTemperature < atof(currentOutdoorTemperature)) {
    if (preferredTemperatureUnit == 2) {
      lcd.print((((atof(currentOutdoorTemperature)) * (9.0/5.0)) + 32) - (((currentIndoorTemperature) * (9.0/5.0)) + 32));
    } else {
      lcd.print(atof(currentOutdoorTemperature) - currentIndoorTemperature);
    }
    printTemperatureUnit();
    lcd.setCursor(0, 1);
    lcd.print("hotter outside");
  } else {
    if (preferredTemperatureUnit == 2) {
      lcd.print((((currentIndoorTemperature) * (9.0/5.0)) + 32) - (((atof(currentOutdoorTemperature)) * (9.0/5.0)) + 32));
    } else {
      lcd.print(currentIndoorTemperature - atof(currentOutdoorTemperature));
    }
    printTemperatureUnit();
    lcd.setCursor(0, 1);
    lcd.print("colder outside");
  } 
}

// Checks for a button press of the button responsible for changing the temp unit
bool checkForTempUnitButtonPress() {
  bool buttonPressed = false;
  int reading = digitalRead(tempUnitButton);

  if (reading != tempUnitLastButtonState) {
    tempUnitLastDebounceTime = millis();
  }

  if ((millis() - tempUnitLastDebounceTime) > debounceDelay) {
    if (reading != tempUnitButtonState) {
      tempUnitButtonState = reading;

      if (tempUnitButtonState == HIGH) {
        if (preferredTemperatureUnit == 1) {
          preferredTemperatureUnit = 2;
        } else {
          preferredTemperatureUnit = 1;
        }
        buttonPressed = true;
      }
    }
  }

  tempUnitLastButtonState = reading;
  return buttonPressed;
}

// Checks for a button press of the button responsible for changing current menu
bool checkForMenuButtonPress() {
  bool buttonPressed = false;
  int reading = digitalRead(menuButton);

  if (reading != menuLastButtonState) {
    menuLastDebounceTime = millis();
  }

  if ((millis() - menuLastDebounceTime) > debounceDelay) {
    if (reading != menuButtonState) {
      menuButtonState = reading;

      if (menuButtonState == HIGH) {
        if (menuCounter == 1) {
          menuCounter = 2;
        } else {
          menuCounter = 1;
        }
        buttonPressed = true;
      }
    }
  }

  menuLastButtonState = reading;
  return buttonPressed;
}

void setup() {
  // put your setup code here, to run once:
  Serial.begin(9600);
  radio.begin();
  radio.openReadingPipe(0, address);
  radio.setPALevel(RF24_PA_MIN);
  radio.startListening();
  lcd.begin(16, 2);
  analogWrite(CONTRAST_PIN, 95);

  pinMode(activityLEDPin, OUTPUT);
  pinMode(warningLEDPin, OUTPUT);
  pinMode(tempUnitButton, INPUT);
  pinMode(menuButton, INPUT);
}

void loop() {
  // At the start of each loop, check for new data sent wirelessly from the outdoor unit
  if (radio.available()) {
    radio.read(&newOutdoorTemperature, sizeof(newOutdoorTemperature));
    digitalWrite(activityLEDPin, HIGH);
    digitalWrite(warningLEDPin, LOW);
    timeSinceLast = millis();
    Serial.println(newOutdoorTemperature);
  } else if (millis() - timeSinceLast > 60000) {
    // Red LED turns on it to warn user the last update was more than 1 min ago
    digitalWrite(warningLEDPin, HIGH);
  } else if (millis() - timeSinceLast > 500) {
    // Turns off activity LED 0.5 seconds after recieving data
    digitalWrite(activityLEDPin, LOW);
  }

  // Update outdoor temperature values from outdoor unit (if connection exists)
  if (strcmp(newOutdoorTemperature,currentOutdoorTemperature) != 0) {
    strcpy(currentOutdoorTemperature, newOutdoorTemperature);
    tempChangeMade = true;
  }

  // Update indoor temperature values using connected temp sensor
  dht_sensor.measure(&newIndoorTemperature, &newIndoorHumidity);
  if (newIndoorTemperature != currentIndoorTemperature) {
    currentIndoorTemperature = newIndoorTemperature;
    tempChangeMade = true;
  }

  // Checks for button press
  if (checkForTempUnitButtonPress() || checkForMenuButtonPress()) {
    tempChangeMade = true;
  }

  // Updates LCD to display appropriate data
  if (tempChangeMade) {
    if (menuCounter == 1) {
      displayTemperatureInformation();
    } else if (menuCounter == 2) {
      displayTemperatureDeltas();
    }
    tempChangeMade = false;
  }
  
}
