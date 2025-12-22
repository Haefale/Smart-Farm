#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <EEPROM.h>

LiquidCrystal_I2C lcd(0x27, 16, 2);

const int soilPin = A0;
const int cdsPin = A1;
const int pumpIn1 = 9;
const int pumpIn2 = 8;
const int ledPin = 10;
const int btnUp = 2;
const int btnDown = 3;
const int btnMode = 4;

const int THRESHOLD_ADDR = 0;
const int CDS_THRESHOLD_ADDR = 4;

int cdsThreshold = 100;
int threshold = 30;
const int minThreshold = 0;
const int maxThreshold = 90;

unsigned long lastButtonTime = 0;
const unsigned long debounceDelay = 500;

bool pumpRunning = false;
bool ledRunning = false;
bool controlHumidity = true;

bool manualPumpOverride = false;
bool manualLedOverride = false;
bool systemManualMode = false;

void setup() {
    pinMode(pumpIn1, OUTPUT);
    pinMode(pumpIn2, OUTPUT);
    pinMode(ledPin, OUTPUT);
    pinMode(btnUp, INPUT_PULLUP);
    pinMode(btnDown, INPUT_PULLUP);
    pinMode(btnMode, INPUT_PULLUP);

    Serial.begin(9600);

    lcd.init();
    lcd.backlight();

    EEPROM.get(THRESHOLD_ADDR, threshold);
    EEPROM.get(CDS_THRESHOLD_ADDR, cdsThreshold);
    if (threshold < minThreshold || threshold > maxThreshold) {
        threshold = 30;
    }
    if (cdsThreshold < 0 || cdsThreshold > 1023) {
        cdsThreshold = 100;
    }

    lcd.setCursor(0, 0);
    lcd.print("      ");
    delay(1000);
    lcd.clear();
}

void checkButtons() {
    unsigned long now = millis();

    if (now - lastButtonTime > debounceDelay) {
        bool upPressed = digitalRead(btnUp) == LOW;
        bool downPressed = digitalRead(btnDown) == LOW;
        bool modePressed = digitalRead(btnMode) == LOW;

        bool valueChanged = false;

        if (modePressed) {
            controlHumidity = !controlHumidity;
            lastButtonTime = now;
            return;
        }

        if (upPressed && downPressed) {
            if (controlHumidity) {
                if (threshold > 0) {
                    threshold = 0;
                } else {
                    threshold = 80;
                }
                EEPROM.put(THRESHOLD_ADDR, threshold);
            } else {
                if (cdsThreshold > 0) {
                    cdsThreshold = 0;
                } else {
                    cdsThreshold = 500;
                }
                EEPROM.put(CDS_THRESHOLD_ADDR, cdsThreshold);
            }
            lastButtonTime = now;
            return;
        }

        if (upPressed) {
            if (controlHumidity && threshold < maxThreshold) {
                threshold += 1;
                valueChanged = true;
            } else if (!controlHumidity && cdsThreshold < 1023) {
                cdsThreshold += 50;
                valueChanged = true;
            }
            lastButtonTime = now;
        }

        if (downPressed) {
            if (controlHumidity && threshold > minThreshold) {
                threshold -= 1;
                valueChanged = true;
            } else if (!controlHumidity && cdsThreshold > 0) {
                cdsThreshold -= 50;
                valueChanged = true;
            }
            lastButtonTime = now;
        }

        if (valueChanged) {
            if (controlHumidity) {
                EEPROM.put(THRESHOLD_ADDR, threshold);
            } else {
                EEPROM.put(CDS_THRESHOLD_ADDR, cdsThreshold);
            }
        }
    }
}

void loop() {
    int soilVal = analogRead(soilPin);
    int humidity = map(soilVal, 1023, 0, 0, 100);

    int cdsVal = analogRead(cdsPin);
    int lightValueMapped = cdsVal;

    bool isDark = cdsVal > cdsThreshold;
    if (!systemManualMode) {
        digitalWrite(ledPin, isDark ? LOW : HIGH);
        ledRunning = isDark;
    } else {
        ledRunning = (digitalRead(ledPin) == HIGH);
    }

    String dataToSend = "H," + String(humidity) + "%,L," + String(lightValueMapped) +
                        ",HT," + String(threshold) +
                        ",LT," + String(cdsThreshold) +
                        ",MP," + String(manualPumpOverride ? "1" : "0") +
                        ",ML," + String(manualLedOverride ? "1" : "0") +
                        ",SM," + String(systemManualMode ? "1" : "0") +
                        ",RP," + String(pumpRunning ? "1" : "0") +
                        ",RL," + String(ledRunning ? "1" : "0");
    Serial.println(dataToSend);
    delay(5);

    checkButtons();
    
    if (Serial.available() > 0) {
        String receivedData = Serial.readStringUntil('\n');
        receivedData.trim();

        if (receivedData.startsWith("SET_H_")) {
            int newThreshold = receivedData.substring(6).toInt();
            if (newThreshold >= minThreshold && newThreshold <= maxThreshold) {
                threshold = newThreshold;
                EEPROM.put(THRESHOLD_ADDR, threshold);
            }
        } else if (receivedData.startsWith("SET_L_")) {
            int newCdsThreshold = receivedData.substring(6).toInt();
            if (newCdsThreshold >= 0 && newCdsThreshold <= 1023) {
                cdsThreshold = newCdsThreshold;
                EEPROM.put(CDS_THRESHOLD_ADDR, cdsThreshold);
            }
        }
        else if (receivedData.equals("MANUAL_ON")) {
            systemManualMode = true;
            manualPumpOverride = false;
            manualLedOverride = false;
        } else if (receivedData.equals("MANUAL_OFF")) {
            systemManualMode = false;
            digitalWrite(pumpIn1, LOW);
            digitalWrite(pumpIn2, LOW);
            digitalWrite(ledPin, LOW);
            manualPumpOverride = false;
            manualLedOverride = false;
            pumpRunning = false;
            ledRunning = false;
        }

        if (systemManualMode) {
            if (receivedData.equals("PUMP_ON")) {
                manualPumpOverride = true;
                digitalWrite(pumpIn1, HIGH);
                digitalWrite(pumpIn2, LOW);
                pumpRunning = true;
            } else if (receivedData.equals("PUMP_OFF")) {
                manualPumpOverride = false;
                digitalWrite(pumpIn1, LOW);
                digitalWrite(pumpIn2, LOW);
                pumpRunning = false;
            }

            else if (receivedData.equals("LED_ON")) {
                manualLedOverride = true;
                digitalWrite(ledPin, HIGH);
                ledRunning = true;
            } else if (receivedData.equals("LED_OFF")) {
                manualLedOverride = false;
                digitalWrite(ledPin, LOW);
                ledRunning = false;
            }
        }
    }

    lcd.setCursor(0, 0);
    lcd.print("H:");
    lcd.print(humidity);
    lcd.print("% ");

    lcd.setCursor(7, 0);
    lcd.print("HT:");
    lcd.print(threshold);
    lcd.print("% ");

    lcd.setCursor(0, 1);
    lcd.print("L:");
    lcd.print(cdsVal);
    lcd.print("   ");

    lcd.setCursor(7, 1);
    lcd.print("LT:");
    lcd.print(cdsThreshold);
    lcd.print("   ");

    if (controlHumidity) {
        lcd.setCursor(14, 0);
        lcd.print("<-");
        lcd.setCursor(14, 1);
        lcd.print("   ");
    } else {
        lcd.setCursor(14, 1);
        lcd.print("<-");
        lcd.setCursor(14, 0);
        lcd.print("   ");
    }

    bool upPressed = digitalRead(btnUp) == LOW;
    bool modePressed = digitalRead(btnMode) == LOW;
    bool isButtonOverride = upPressed && modePressed;

    if (systemManualMode) {
    }
    else if (isButtonOverride) {
        digitalWrite(pumpIn1, HIGH);
        digitalWrite(pumpIn2, LOW);
        pumpRunning = true;
    }
    else {
        if (humidity < threshold) {
            digitalWrite(pumpIn1, HIGH);
            digitalWrite(pumpIn2, LOW);
            pumpRunning = true;
        } else {
            digitalWrite(pumpIn1, LOW);
            digitalWrite(pumpIn2, LOW);
            pumpRunning = false;
        }
    }

    delay(50);
}
