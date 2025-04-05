#include <Servo.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <SPI.h>
#include <MFRC522.h>
#include <EEPROM.h>

#define SS_PIN 10
#define RST_PIN 9
#define SERVO_PIN 3
#define TOLL_AMOUNT 5
#define RECHARGE_AMOUNT 20
#define MAX_CARDS 5  // Maximum number of stored RFID cards

MFRC522 rfid(SS_PIN, RST_PIN);
Servo myServo;
LiquidCrystal_I2C lcd(0x3F, 16, 2);

const byte authorizedCards[MAX_CARDS][4] = {
    {0x39, 0xF4, 0x1E, 0xE8},  // Card 1
    {0xAC, 0xB8, 0xEC, 0x30},  // Card 2
   // {0x95, 0x25, 0x2C, 0x02},  // Card 3
    {0x93, 0xB8, 0x59, 0xDA},  // Card 4
    {0xAA, 0xBB, 0xCC, 0xDD}   // Card 5
};

const byte rechargeCardUID[4] = {0x93, 0x31, 0x7F, 0x4A};  // Recharge Card
bool rechargeMode = false;  // Flag to enable recharge mode

void setup() {
    Serial.begin(9600);
    SPI.begin();
    rfid.PCD_Init();
    myServo.attach(SERVO_PIN);
    lcd.begin();
    lcd.backlight();
    
    for (int i = 0; i < MAX_CARDS; i++) {
        if (EEPROM.read(i) == 255) {
            EEPROM.put(i * sizeof(int), 50);  // Default balance = 50
        }
    }
    lcd.setCursor(0, 0);
    lcd.print("Scan Your Card");
    Serial.println("Scan Your Card...");
}

int getCardIndex(byte *cardUID) {
    for (int i = 0; i < MAX_CARDS; i++) {
        if (memcmp(cardUID, authorizedCards[i], 4) == 0) {
            return i;
        }
    }
    return -1;
}

void resetBalances() {
    for (int i = 0; i < MAX_CARDS; i++) {
        EEPROM.put(i * sizeof(int), 50); // Reset balance to default 50
    }
    Serial.println("All balances reset to $50.");
}

void loop() {
    if (Serial.available()) {
        String command = Serial.readStringUntil('\n');
        command.trim();
        if (command == "RESET_BALANCE") {
            resetBalances();
        }
    }
    
    if (!rfid.PICC_IsNewCardPresent() || !rfid.PICC_ReadCardSerial()) {
        return;
    }
    
    Serial.print("Card UID: ");
    for (byte i = 0; i < 4; i++) {
        Serial.print(rfid.uid.uidByte[i], HEX);
        Serial.print(" ");
    }
    Serial.println();

    lcd.clear();

    if (memcmp(rfid.uid.uidByte, rechargeCardUID, 4) == 0) {
        lcd.setCursor(0, 0);
        lcd.print("Recharge Mode On");
        Serial.println("Tap a user card to recharge");
        rechargeMode = true;  // Enable recharge mode
        delay(2000);
        lcd.clear();
        lcd.setCursor(0, 0);
        lcd.print("Scan User Card");
        return;
    }
    
    int cardIndex = getCardIndex(rfid.uid.uidByte);
    if (cardIndex != -1) {
        int balance;
        EEPROM.get(cardIndex * sizeof(int), balance);
        
        if (rechargeMode) {
            balance += RECHARGE_AMOUNT;
            EEPROM.put(cardIndex * sizeof(int), balance);
            rechargeMode = false; // Reset recharge mode
            
            lcd.setCursor(0, 0);
            lcd.print("Card Recharged!");
            lcd.setCursor(0, 1);
            lcd.print("New Bal: $");
            lcd.print(balance);
            
            Serial.print("Card Recharged! New Balance: $");
            Serial.println(balance);
        } 
        else if (balance >= TOLL_AMOUNT) {
            balance -= TOLL_AMOUNT;
            EEPROM.put(cardIndex * sizeof(int), balance);
            
            lcd.setCursor(0, 0);
            lcd.print("Access Granted");
            lcd.setCursor(0, 1);
            lcd.print("Bal: $");
            lcd.print(balance);
            
            Serial.println("Access Granted");
            Serial.print("New Balance: $");
            Serial.println(balance);
            
            myServo.write(90);
            delay(3000);
            myServo.write(0);
        } 
        else {
            lcd.setCursor(0, 0);
            lcd.print("Insufficient Funds");
            lcd.setCursor(0, 1);
            lcd.print("Bal: $");
            lcd.print(balance);
            Serial.println("Insufficient Funds");
        }
    } 
    else {
        lcd.setCursor(0, 0);
        lcd.print("Access Denied");
        Serial.println("Access Denied");
    }
    
    delay(2000);
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Scan Your Card");
    
    rfid.PICC_HaltA();
    rfid.PCD_StopCrypto1();
}
