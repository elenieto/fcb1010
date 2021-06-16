
/*************************************************************
  Programa principal del proyecto. Estructura:
  - Modo Preset
  - Modo Jam
  - Modo Sampler
  - Modo Libre
  - Configuración: SLD, LLD, CC, ML, EXP, DSP

  Comunicación I2C:
  - Envío a: pantalla LCD, displays y LEDs de los Módulos Pulsador LED.
  - Recibe: pulses de pulsadores y valores de los pedales de expresión.

  Bluetooth BLE:
  - Recibe JSON de configuración de CCs y de Modo Libre.
  - Envía comandos MIDI.

  Rx y Tx:
  - Recibe: comandos MIDI por conector DIN de 5 pines
  - Envía: comandos MIDI por conector DIN de 5 pines
*************************************************************
  Proyecto FCB1010 de Elena Nieto: https://github.com/elenieto/fcb1010
**************************************************************/

//  Librerías
#include <LiquidCrystal_I2C.h>
#include <Wire.h>
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>
#include <ArduinoJson.h>
#include <String.h>
#include <EEPROM.h>
#include <MIDI.h>

// Constantes
#define I2C_SDA 32
#define I2C_SCL 33
#define MIDI_SERVICE_UUID        "03b80e5a-ede8-4b33-a751-6ce34ec4c700"
#define MIDI_CHARACTERISTIC_UUID "7772e5db-3868-4112-a1a9-f2669d106bf3"
#define EEPROM_SIZE 216

// Direcciones y CC para Modo Preset, Jam y Sampler
const int expAddress [2] = {7, 8};
const int configAddress [6] = {2, 3, 4, 5, 10, 16};
const int configFmAddress [3] = {2, 3, 16};
const int buttonAddress [12] = {16, 2, 3, 4, 5, 10, 11, 12, 13, 14, 6, 15};
const int shortPresetJamCC [12] = {0, 1, 2, 3, 0, 4, 5, 6, 7, 18 , 8, 9};
const int longCC [12] = {0, 0, 0, 0, 0, 12, 13, 14, 15, 16, 10, 11};
const int shortSamplerCC [12] = {0, 1, 2, 3, 32, 4, 5, 6, 7, 18 , 8, 9};
const int doubleSamplerCC [12] = {0, 0, 0, 31, 30, 0, 0, 0, 29, 18, 0, 0};
const int doubleJamCC [12] = {22, 24, 26, 0, 0, 23, 25, 27, 0, 17, 0, 0};
const int displayAddress = 9;
int cc [33];
int indexCC = 0;

// Menú
int state = 1;
int configState = 0;

// Modo libre
int shortPuls[10][3];
int longPuls[10][3];
int doublePuls[10][3];
int freeExp[2][3];
boolean x = false;
boolean firstDone = false;
int addressFm = 40;
int addressExp = 212;

// LCD
int lcdColumns = 16;
int lcdRows = 2;
LiquidCrystal_I2C lcd(0x27, lcdColumns, lcdRows);
boolean printLcd = true;

boolean load = true;

// Pedales de expresión
volatile byte dataGet[2];
boolean expMin = true;
int counterMin = 0;
int counterMax = 0;
int minExpLast[2] = {1023, 1023};
int maxExpLast[2] = {0, 0};
int lastValueExp[2] = {0, 0};
boolean expChange = false;
float m[2];
int n[2];

// Displays
int displayNum;
int displayLet;
int displayBank = 1;

// Fragmentar byte
byte puls;
byte button;

// Configuración
boolean firstCaracter = false;
boolean secondCaracter = false;
const int firstCharAddress [2] = {16, 2};
const int secondCharAddress [4] = {10, 11, 12, 13};
const String letters [4] = {"A", "B", "C", "D"};
const int ledAddress[10] = {16, 2, 3, 4, 5, 10, 11, 12, 13, 14};
boolean leds [10] = {false, false, false, false, false, false, false, false, false, false};
boolean currentLeds [10] = {false, false, false, false, false, false, false, false, false, false};
int let;
int num;

// Nuevo dispositivo
int newAddress;
boolean addedAddress = false;
int dec;
int lastMidiPacket[3];
boolean firstTime = true;

// Paquete MIDI
uint8_t midiPacket[] = {
  0xB9,  // header
  0xFD,  // timestamp, not implemented
  0xB0,  // status -> CHANNEL + TYPE
  0x3c,  // 0x3c == 60 == middle c // CC
  0x7A   // velocity
};

// Midi conector DIN
struct Serial2MIDISettings : public midi::DefaultSettings
{
  static const long BaudRate = 31250;
  static const int8_t RxPin  = 16;
  static const int8_t TxPin  = 17;
};

MIDI_CREATE_CUSTOM_INSTANCE(HardwareSerial, Serial2, DIN_MIDI, Serial2MIDISettings);

//BLE
BLECharacteristic *pCharacteristic;
bool deviceConnected = false;

// Carga de ficheros de configuración recibidos por Bluetooth BLE
class MyCallbacks: public BLECharacteristicCallbacks {
    // Valores recibidos por Bluetooth
    void onWrite(BLECharacteristic *pCharacteristic) {
      std::string value = pCharacteristic->getValue();
      if (value.length() > 0) {
        const size_t capacity = JSON_OBJECT_SIZE(40) + 160;
        DynamicJsonDocument doc(capacity);
        const char* json = value.c_str();

        // Deserialización del JSON
        deserializeJson(doc, json);

        // EFFECTS
        cc[0] = (int) doc["AMP"]; // "12"
        cc[1] = (int) doc["NG"]; // "13"
        cc[2] = (int) doc["STOMP"]; // "14"
        cc[3] = (int) doc["DLY"]; // "15"
        cc[4] = (int) doc["MOD"]; // "16"
        cc[5] = (int) doc["RVB"]; // "17"
        cc[6] = (int) doc["FILT"]; // "18"
        cc[7] = (int) doc["FX"]; // "19"

        // PRESET
        cc[8] = (int) doc["PR+"]; // "20"
        cc[9] = (int) doc["PR-"]; // "21"
        cc[10] = (int) doc["PG+"]; // "22"
        cc[11] = (int) doc["PG-"]; // "23"
        cc[12] = (int) doc["A"]; // "24"
        cc[13] = (int) doc["B"]; // "25"
        cc[14] = (int) doc["C"]; // "26"
        cc[15] = (int) doc["D"]; // "27"

        // UTILITY
        cc[16] = (int) doc["TUN"]; // "28"
        cc[17] = (int) doc["MTR"]; // "29"
        cc[18] = (int) doc["TAP"]; // "30"

        // EXP
        cc[19] = (int) doc["VOL"]; // "31"
        cc[20] = (int) doc["WAH"]; // "32"
        cc[21] = (int) doc["PTCH"]; // "33"

        // JAM
        cc[22] = (int) doc["SP+"]; // "34"
        cc[23] = (int) doc["SP-"]; // "35"
        cc[24] = (int) doc["PT+"]; // "36"
        cc[25] = (int) doc["PT-"]; // "37"
        cc[26] = (int) doc["VL+"]; // "38"
        cc[27] = (int) doc["VL-"]; // "39"
        cc[28] = (int) doc["PLJ"]; // "40"

        // SAMPLER
        cc[29] = (int) doc["REC"]; // "41"
        cc[30] = (int) doc["UNDO"]; // "42"
        cc[31] = (int) doc["PLS"]; // "43"
        cc[32] = (int) doc["DUB"]; // "44"

        // Modo Libre
        for (int i = 0; i < 10; i++) {
          for (int z = 0; z < 3; z++) {
            char shortIndex[20];
            char longIndex[20];
            char doubleIndex[20];
            char numAux[17];
            strcpy (shortIndex, "S");
            strcpy (longIndex, "L");
            strcpy (doubleIndex, "D");

            char* num = itoa(i, numAux, 10);
            strncat (shortIndex, num, 1);
            strncat (longIndex, num, 1);
            strncat (doubleIndex, num, 1);

            shortPuls[i][z] = (int) doc[shortIndex][z];
            longPuls[i][z] = (int) doc[longIndex][z];
            doublePuls[i][z] = (int) doc[doubleIndex][z];
          }
        }
        for (int z = 0; z < 3; z++) {
          freeExp[0][z] = (int) doc["EXP1"][z];
          freeExp[1][z] = (int) doc["EXP2"][z];
        }
      }
    }
};

class MyServerCallbacks: public BLEServerCallbacks {
    void onConnect(BLEServer* pServer) {
      deviceConnected = true;
    };

    void onDisconnect(BLEServer* pServer) {
      deviceConnected = false;
    }
};

void setup() {
  Wire.begin(I2C_SDA, I2C_SCL, 100000);
  Serial.begin(115200);
  lcd.init();
  lcd.backlight();
  EEPROM.begin(EEPROM_SIZE);
  DIN_MIDI.begin(MIDI_CHANNEL_OMNI);
  BLEDevice::init("MIDI");

  // Se crea del servidor BLE
  BLEServer *pServer = BLEDevice::createServer();
  pServer->setCallbacks(new MyServerCallbacks());
  BLEDevice::setEncryptionLevel((esp_ble_sec_act_t)ESP_LE_AUTH_REQ_SC_BOND);

  // Se crea el servicio BLE
  BLEService *pService = pServer->createService(BLEUUID(MIDI_SERVICE_UUID));

  // Se crea la característica BLE
  pCharacteristic = pService->createCharacteristic(
                      BLEUUID(MIDI_CHARACTERISTIC_UUID),
                      BLECharacteristic::PROPERTY_READ   |
                      BLECharacteristic::PROPERTY_NOTIFY |
                      BLECharacteristic::PROPERTY_WRITE
                    );

  pCharacteristic->setCallbacks(new MyCallbacks());

  pCharacteristic->setAccessPermissions(ESP_GATT_PERM_READ_ENCRYPTED | ESP_GATT_PERM_WRITE_ENCRYPTED);

  // https://www.bluetooth.com/specifications/gatt/viewer?attributeXmlFile=org.bluetooth.descriptor.gatt.client_characteristic_configuration.xml
  // Se crea el descriptor BLE
  pCharacteristic->addDescriptor(new BLE2902());

  // Empieza el servicio
  pService->start();

  BLESecurity *pSecurity = new BLESecurity();
  pSecurity->setAuthenticationMode(ESP_LE_AUTH_REQ_SC_BOND);
  pSecurity->setCapability(ESP_IO_CAP_NONE);
  pSecurity->setInitEncryptionKey(ESP_BLE_ENC_KEY_MASK | ESP_BLE_ID_KEY_MASK);

  pServer->getAdvertising()->addServiceUUID(MIDI_SERVICE_UUID);
  pServer->getAdvertising()->start();
}

void loop() {
  // Carga de variables en la primera vuelta del bucle
 if (load) {
    int memoryAddress = 0;
    // Valores de CC

    for (int i = 0; i < 33; i++) {
      cc[i] = EEPROM.read(memoryAddress);
      memoryAddress++;
    }
      
    // Valores de pedales EXP
    for (int i = 0; i < 2; i++) {
      m[i] = (float) EEPROM.read(memoryAddress) / 100;
      memoryAddress++;
      n[i] = EEPROM.read(memoryAddress);
      memoryAddress++;
    }

    // Pulsadores Modo Libre
    memoryAddress = 40;
    while (memoryAddress < 130) {
      for (int i = 0; i < 10; i++) {
        for (int z = 0; z < 3; z++) {
          shortPuls[i][z] = (int) EEPROM.read(memoryAddress);
          memoryAddress++;
          longPuls[i][z] = (int) EEPROM.read(memoryAddress);
          memoryAddress++;
          doublePuls[i][z] = (int) EEPROM.read(memoryAddress);
          memoryAddress++;
        }
      }
    }
    // Último preset cargado
    displayNum = (int) EEPROM.read(210);
    displayLet = (int) EEPROM.read(211);

    // Carga del estado de los LEDs
    if ((displayNum == 1 || displayNum == 2) && (displayLet > 9 && displayLet < 15)) {
      int memoryAddress = 130 + (40 * (displayNum - 1)) + (10 * (displayLet - 10));
      for (int i = 0; i < 10; i++) {
        currentLeds[i] = EEPROM.read(memoryAddress);
        memoryAddress++;
      }
      lightLeds();
      displays();
    }

    // Exp Modo Libre
    memoryAddress = 212;
    for (int z = 0; z < 2; z++) {
      for (int i = 0; i < 2; i++) {
        freeExp[i][z] = (int) EEPROM.read(memoryAddress);
        memoryAddress++;
      }
    }
    freeExp[0][2] = 0;
    freeExp[1][2] = 0;

    load = false;
  }

  if (DIN_MIDI.read())
  {
    // Thru on A has already pushed the input message to out A.
    // Forward the message to out B as well.
    DIN_MIDI.send(DIN_MIDI.getType(),
                  DIN_MIDI.getData1(),
                  DIN_MIDI.getData2(),
                  DIN_MIDI.getChannel());
  }

  switch (state)
  {
    case 1:
      if (printLcd) printlcd(1);
      preset(); // preset
      pedalExp();
      addedDsp();
      break;
    case 2:
      if (printLcd) printlcd(2);
      jam(); //jam
      pedalExp();
      addedDsp();
      break;
    case 3:
      if (printLcd) printlcd(3);
      sampler(); //sampler
      pedalExp();
      addedDsp();
      break;
    case 4:
      if (printLcd) printlcd(4);
      freeMode(); //Modo Libre
      pedalExp();
      addedDsp();
      break;
    case 5:
      switch (configState) {
        case 0:
          if (printLcd) {
            lcd.setCursor(0, 0);
            lcd.print("1:SLD 2:LLD 3:CC");
            lcd.setCursor(0, 1);
            lcd.print("4:ML 5:EXP A:DSP");
            printLcd = false;
          }
          initConfig();
          break;
        case 1:
          configLeds(); // Guardar leds
          break;
        case 2:
          loadLeds(); //Cargar leds
          break;
        case 3:
          configCc(); // Guardar CC
          break;
        case 4:
          configFm(); // Guardar configuración Modo Libre
          break;
        case 5:
          calibrationExp(); // Calibrar pedales
          break;
        case 6:
          newDsp(); // Añadir nuevo dispositivo
          break;
        default:
          break;
      }
      break;
    default:
      break;
  }
}

void preset() {
  // Se reciben pulsaciones de los 12 pulsadores
  for (int i = 0; i < 12; i++) {
    Wire.requestFrom(buttonAddress[i], 1);   // Recibe byte a byte
    while (Wire.available()) {
      byte receivedByte = Wire.read();
      fragmentation(receivedByte);
      // Si notifica una pulsación
      if (bitRead(puls, 0) && !bitRead(puls, 1) && button != 0) {
        if (bitRead(puls, 2)) {
          // Pulsación larga
          if (button < 6) {
            // Cambio de modo
            state = button;
            printLcd = true;
            if (button == 4) turnOffLeds();
          } else {
            // Envío comando MIDI
            indexCC = longCC[button - 1];
            sendMidiCC(indexCC);
            // Cambio de preset
            switch (button) {
              case 10:
                break;
              case 11: nextPage();
                break;
              case 12: previousPage();
                break;
              default: {
                  displayLet = buttonAddress[i];
                  displays();
                  loadPreset();
                }
                break;
            }
          }
        } else {
          if (!bitRead(puls, 3)) {
            // Pulsación corta: envío comando MIDI
            indexCC = shortPresetJamCC[button - 1];
              sendMidiCC(indexCC);
            if (button == 11) {
              nextPreset();
            }
            else {
              if (button == 12) previousPreset();
            }
          } else {
            if (button == 11) {
              // Conmutación de pedal de expresión
              expChange = !expChange;
              printLcd = true;
            } else {
              if (button == 10) {
                sendMidiCC(17);
              }
            }
          }
        }
      }
    }
  }
}

void sampler() {
  // Se reciben pulsaciones de los 12 botones
  for (int i = 0; i < 12; i++) {
    Wire.requestFrom(buttonAddress[i], 1);   // Recibe byte a byte
    while (Wire.available()) {
      byte receivedByte = Wire.read();
      fragmentation(receivedByte);
      // Si notifica una pulsación
      if (bitRead(puls, 0) && !bitRead(puls, 1) && button != 0) {
        if (bitRead(puls, 2)) {
          // Pulsación larga
          if (button < 6) {
            // Cambio de modo
            state = button;
            printLcd = true;
            if (button == 4) turnOffLeds();
          } else {
            // Envío comando MIDI
            indexCC = longCC[button - 1];
            sendMidiCC(indexCC);
            // Cambio de preset
            switch (button) {
              case 10:
                break;
              case 11: nextPage();
                break;
              case 12: previousPage();
                break;
              default: {
                  displayLet = buttonAddress[i];
                  displays();
                  loadPreset();
                }
                break;
            }
          }
        } else {
          if (!bitRead(puls, 3)) {
            // Pulsación corta: envío comando MIDI
            indexCC = shortSamplerCC[button - 1];
            sendMidiCC(indexCC);
            if (button == 11) nextPreset();
            else {
              if (button == 12) previousPreset();
            }
          } else {
            // Pulsación doble
            if (button == 11) {
              // Conmutación de pedal de expresión
              expChange = !expChange;
              printLcd = true;
            } else {
              // Envío comando MIDI
              indexCC = doubleSamplerCC[button - 1];
              if (indexCC != 0) sendMidiCC(indexCC);
            }
          }
        }
      }
    }
  }
}

void jam() {
  // Se reciben pulsaciones de los 12 botones
  for (int i = 0; i < 12; i++) {
    Wire.requestFrom(buttonAddress[i], 1);   // Recibe byte a byte
    while (Wire.available()) {
      byte receivedByte = Wire.read();
      fragmentation(receivedByte);
      // Si notifica una pulsación
      if (bitRead(puls, 0) && !bitRead(puls, 1) && button != 0) {
        if (bitRead(puls, 2)) {
          // Pulsación larga
          if (button < 6) {
            state = button;
            printLcd = true;
            if (button == 4) turnOffLeds();
          }
          else {
            indexCC = longCC[button - 1];
            sendMidiCC(indexCC);
            // Cambio de preset
            switch (button) {
              case 10:
                break;
              case 11: nextPage();
                break;
              case 12: previousPage();
                break;
              default: {
                  displayLet = buttonAddress[i];
                  displays();
                  loadPreset();
                }
                break;
            }
          }
        }
        else {
          if (!bitRead(puls, 3)) {
            // Pulsación corta: enviar comando MIDI
            indexCC = shortPresetJamCC[button - 1];
            sendMidiCC(indexCC);
            if (button == 11) nextPreset();
            else {
              if (button == 12) previousPreset();
            }
          }
          else {
            // Pulsación doble
            if (button == 11) {
              // Conmutación de pedal de expresión
              expChange = !expChange;
              printLcd = true;
            } else {
              // Enviar comando MIDI
              indexCC = doubleJamCC[button - 1];
              if (indexCC != 0) sendMidiCC(indexCC);
            }
          }
        }
      }
    }
  }
}

void freeMode() {
  // Se reciben pulsaciones de los 12 botones
  for (int i = 0; i < 12; i++) {
    Wire.requestFrom(buttonAddress[i], 1);   // Recibe byte a byte
    while (Wire.available()) {
      byte receivedByte = Wire.read();
      fragmentation(receivedByte);
      // Si notifica una pulsación
      if (bitRead(puls, 0) && !bitRead(puls, 1) && button != 0) {
        if (bitRead(puls, 2)) {
          // Pulsación larga
          if (button < 6) {
            // Cambio de modo
            state = button;
            printLcd = true;
          }
          else sendMidi(longPuls[i][0], longPuls[i][1], longPuls[i][2]);
        }
        else {
          // Pulsación corta
          if (!bitRead(puls, 3)) sendMidi(shortPuls[i][0], shortPuls[i][1], shortPuls[i][2]);
          // Pulsación doble
          else sendMidi(doublePuls[i][0], doublePuls[i][1], doublePuls[i][2]);
        }
      }
    }
  }
}

/* Calibración de los pedales de expresión. Se guarda el nivel máximo y mínimo.*/
void calibrationExp() {
  for (int i = 0; i < 2; i++) {
    Wire.requestFrom(expAddress[i], 2);
    // Recibe 2 bytes del Módulo Pedal de Expresión
    for (int i = 0; i < 2; i++) dataGet[i] = Wire.read();
    int receivedByte = dataGet[0];
    receivedByte = receivedByte << 8 | dataGet[1];
    if (counterMin < 10000) {
      if (counterMin == 0) {
        lcd.setCursor(0, 0);
        lcd.print("   CALIBRANDO   ");
        lcd.setCursor(0, 1);
        lcd.print("     MINIMO     ");
      }
      if (receivedByte < minExpLast[i]) {
        minExpLast[i] = receivedByte;
      }
      counterMin++;
    }
    else {
      if (counterMax < 10000) {
        if (counterMax == 0) {
          lcd.setCursor(0, 1);
          lcd.print("     MAXIMO     ");
        }
        if (receivedByte > maxExpLast[i]) maxExpLast[i] = receivedByte;
        counterMax++;
      } else {
        lcd.setCursor(0, 1);
        lcd.print("    GUARDADO    ");
        counterMin = 0;
        counterMax = 0;
        int address = 33;
        for (int i = 0; i < 2; i++) {
          m[i] = 127 / ((float) maxExpLast[i] - (float) minExpLast[i]);
          n[i] = (float) m[i] * (int) minExpLast[i];
          int savedM = m[i] * 100;
          if (savedM < 256) {
            EEPROM.write(address, (byte) savedM);
            address++;
            EEPROM.write(address, (byte) n[i]);
            address++;
            EEPROM.commit();
          }
        }
        delay(2000);
        lcd.clear();
        state = 1;
        configState = 0;
      }
    }
  }
}

/* Pedal de Expresión. Recibe el valor del Módulo Pedal de Expresión y envía el comando
  MIDI correspondiente*/
void pedalExp() {
  for (int i = 0; i < 2; i++) {
    Wire.requestFrom(expAddress[i], 2);
    // Recibe 2 bytes del Módulo Pedal de Expresión
    for (int i = 0; i < 2; i++) dataGet[i] = Wire.read();
    int receivedByte = dataGet[0];
    receivedByte = receivedByte << 8 | dataGet[1];
    int value = m[i] * receivedByte - n[i];
    // Si hay variación del pedal
    if (value > lastValueExp[i] + 5 || value < lastValueExp[i] - 5) {
      // Delimita el valor entre 0 y 127
      if (value < 0) value = 0;
      else {
        if (value > 127) value = 127;
      }
      lastValueExp[i] = value;
      // Si la máquina está en Modo Libre
      if (state == 4) {
        // Se identifica el comando MIDI que debe enviar el pedal
        int midiType = freeExp[i][0] >> 4;
        int x = midiType << 4;
        int midiChannel = freeExp[i][0] - x;
        if (midiType != 0) {
          switch (midiType) {
            case 10:
              // Aftertouch Polifónico
              sendMidi(freeExp[i][0], freeExp[i][1], value);
              break;
            case 11:
              // Control Change
              sendMidi(freeExp[i][0], freeExp[i][1], value);
              break;
            case 13:
              // Aftertouch Canal
              sendMidi(freeExp[i][0], value, 0);
              break;
            case 14:
              // Pitch Bend
              sendMidi(freeExp[i][0], value, 0);
              break;
            default:
              break;
          }
        }
      } else {
        // Si la máquina no está en Modo Libre se envía el CC correspondiente
        if (i == 0) {
          indexCC = expChange ? 21 : 20;
        } else {
          indexCC = 19;
        }
        midiPacket[3] = cc[indexCC];
        midiPacket[4] = value;
        pCharacteristic->setValue(midiPacket, 5); // packet, length in bytes
        pCharacteristic->notify();
        DIN_MIDI.sendControlChange(midiPacket[3], midiPacket[4], 1);
      }
    }
  }
}

// Pantalla inicial del menú de configuración. Selección de submenú.
void initConfig() {
  for (int i = 0; i < 6; i++) {
    Wire.requestFrom(configAddress[i], 1);   // Recibe byte a byte
    while (Wire.available()) {
      byte receivedByte = Wire.read();
      fragmentation(receivedByte);
      if (bitRead(puls, 0) && !bitRead(puls, 1) && button != 0) {
        if (button < 7) {
          configState = button;
          printLcd = true;
          if (button == 6 || button == 2 ) turnOffLeds();
        }
      }
    }
  }
}

// Guardado de la configuración cargada de Control Change.
void configCc() {
  for (int i = 0; i < 33; i++) {
    if (EEPROM.read(i) != cc[i]) {
      EEPROM.write(i, (byte) cc[i]);
      EEPROM.commit();
    }
  }
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("    GUARDADO    ");
  delay (3000);
  state = 1;
  configState = 0;
}

// Guardado de la configuración cargada del Modo Libre.
void configFm() {
  // Guardado de los comandos de los Pedales de Expresión
  for (int z = 0; z < 2; z++) {
    for (int i = 0; i < 2; i++) {
      if (EEPROM.read(addressExp) != freeExp[i][z]) {
        EEPROM.write(addressExp, (byte) freeExp[i][z]);
        EEPROM.commit();
        addressExp++;
      }
    }
  }
  // Guardado de los comandos asociados a los pulsadores
  for (int i = 0; i < 10; i++) {
    for (int z = 0; z < 3; z++) {
      if (EEPROM.read(addressFm) != shortPuls[i][z]) {
        EEPROM.write(addressFm, (byte) shortPuls[i][z]);
        EEPROM.commit();
      }
      addressFm++;
      if (EEPROM.read(addressFm) != longPuls[i][z]) {
        EEPROM.write(addressFm, (byte) longPuls[i][z]);
        EEPROM.commit();
      }
      addressFm++;
      if (EEPROM.read(addressFm) != doublePuls[i][z]) {
        EEPROM.write(addressFm, (byte) doublePuls[i][z]);
        EEPROM.commit();
      }
      addressFm++;
    }
  }
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("    GUARDADO    ");
  delay (2000);
  state = 1;
  configState = 0;
}

// Guardado del estado de los LEDs asociados a un preset
void configLeds() {
  if (!firstCaracter) {
    if (printLcd) {
      lcd.setCursor(0, 0);
      lcd.print(" GUARDADO LEDS  ");
      lcd.setCursor(0, 1);
      lcd.print("Preset:         ");
      printLcd = false;
    }
    // Se introduce el primer carácter (número 1 ó 2)
    for (int i = 0; i < 2; i++) {
      Wire.requestFrom(firstCharAddress[i], 1);   // Recibe byte a byte
      while (Wire.available()) {
        byte receivedByte = Wire.read();
        fragmentation(receivedByte);
        if (bitRead(puls, 0) && !bitRead(puls, 1) && button != 0) {
          num = button;
          firstCaracter = true;
          // Se guarda el carácter introducido
          if (EEPROM.read(210) != (byte) num) {
            EEPROM.write(210, (byte) num);
            EEPROM.commit();
          }
          lcd.setCursor(0, 1);
          lcd.print("Preset: " + (String) num + "       ");
        }
      }
    }
  } else {
    // Se introduce el segundo carácter (letra A-D)
    if (!secondCaracter) {
      for (int i = 0; i < 4; i++) {
        Wire.requestFrom(secondCharAddress[i], 1);   // Recibe byte a byte
        while (Wire.available()) {
          byte receivedByte = Wire.read();
          fragmentation(receivedByte);
          if (bitRead(puls, 0) && !bitRead(puls, 1) && button != 0) {
            let = secondCharAddress[i];
            // Se guarda el carácter introducido
            if ((int) EEPROM.read(211) != let) {
              EEPROM.write(211, (byte) let);
              EEPROM.commit();
            }
            lcd.setCursor(0, 1);
            lcd.print("Preset: " + (String) num + (String)letters[let - 10] + "      ");
            delay(1000);
            printLcd = true;
            secondCaracter = true;
          }
        }
      }
    } else {
      if (printLcd) {
        turnOffLeds();
        lcd.setCursor(0, 1);
        lcd.print("SELECCIONA LEDS ");
        printLcd = false;
      }
      // Selección de LEDs activos asociados al preset
      for (int i = 0; i < 12; i++) {
        Wire.requestFrom(buttonAddress[i], 1);   // Recibe byte a byte
        while (Wire.available()) {
          byte receivedByte = Wire.read();
          fragmentation(receivedByte);
          // Si hay pulsación
          if (bitRead(puls, 0) && !bitRead(puls, 1) && button != 0) {
            if (!bitRead(puls, 2) && !bitRead(puls, 3)) {
              // Si se acciona el pulsador 6 se guardan los LEDs
              if (button == 11) {
                // Cálculo de la dirección en la que se guarda el estado de los LEDs
                int address = 130 + (40 * (num - 1)) + (10 * (let - 10));
                // Salva del estado de los LEDs
                for (int i = 0; i < 10; i++) {
                  if ((int) EEPROM.read(address) != leds[i]) {
                    EEPROM.write(address, (byte) leds[i]);
                    EEPROM.commit();
                  }
                  address++;
                }
                lcd.setCursor(0, 1);
                lcd.print("    GUARDADO    ");
                for (int i=0; i<10; i++) {leds[i] = false;}
                printLcd = true;
                displayNum = (byte) num;
                displayLet = (byte) let;
                displays();
                firstCaracter = false;
                secondCaracter = false;
                state = 1;
                configState = 0;
                delay (2000);
              }
              else {
                if (i < 10) leds[i] = !leds[i];
              }
            }
          }
        }
      }
    }
  }
}

// Carga de preset con sus LEDs asociados.
void loadLeds() {
  // Se introduce el primer carácter (1 ó 2)
  if (!firstCaracter) {
    if (printLcd) {
      lcd.setCursor(0, 0);
      lcd.print("  CARGADO LEDS  ");
      lcd.setCursor(0, 1);
      lcd.print("Preset:         ");
      printLcd = false;
    }
    for (int i = 0; i < 2; i++) {
      Wire.requestFrom(firstCharAddress[i], 1);   // Recibe byte a byte
      while (Wire.available()) {
        byte receivedByte = Wire.read();
        fragmentation(receivedByte);
        // Si notifica pulsación
        if (bitRead(puls, 0) && !bitRead(puls, 1) && button != 0) {
          num = button;
          firstCaracter = true;
          // Guarda el primer carácter
          if (EEPROM.read(210) != (byte) num) {
            EEPROM.write(210, (byte) num);
            EEPROM.commit();
          }
          lcd.setCursor(0, 1);
          lcd.print("Preset: " + (String) num + "       ");
        }
      }
    }
  } else {
    // Se introduce el segundo carácter (A-D)
    if (!secondCaracter) {
      for (int i = 0; i < 4; i++) {
        Wire.requestFrom(secondCharAddress[i], 1);   // Recibe byte a byte
        while (Wire.available()) {
          byte receivedByte = Wire.read();
          fragmentation(receivedByte);
          // Si notifica pulsación
          if (bitRead(puls, 0) && !bitRead(puls, 1) && button != 0) {
            let = secondCharAddress[i];
            // Guarda el segundo carácter
            if ((int) EEPROM.read(211) != let) {
              EEPROM.write(211, (byte) let);
              EEPROM.commit();
            }
            lcd.setCursor(0, 1);
            lcd.print("Preset: " + (String) num + (String)letters[let - 10] + "      ");
            delay(1000);
            secondCaracter = true;
          }
        }
      }
    } else {
      // Lectura del estado de los LEDs
      int savedLedsAddress = 130 + (40 * (num - 1)) + (10 * (let - 10));
      for (int i = 0; i < 10; i++) {
        currentLeds[i] = EEPROM.read(savedLedsAddress);
        savedLedsAddress++;
      }
      lightLeds();
      displayNum = num;
      displayLet = let;
      displayBank = 1;
      displays();
      lcd.setCursor(0, 1);
      lcd.print(" PRESET CARGADO ");
      printLcd = true;
      delay (2000);
      firstCaracter = false;
      secondCaracter = false;
      state = 1;
      configState = 0;
    }
  }
}

// Añadir un nuevo dispositivo al bus I2C.
void newDsp() {
  // Se introduce el primer carácter
  if (!firstCaracter) {
    if (printLcd) {
      lcd.setCursor(0, 0);
      lcd.print("  NUEVO MODULO  ");
      lcd.setCursor(0, 1);
      lcd.print("Direcc. 0-99:   ");
      printLcd = false;
    }
    for (int i = 0; i < 10; i++) {
      Wire.requestFrom(ledAddress[i], 1);   // Recibe byte a byte
      while (Wire.available()) {
        byte receivedByte = Wire.read();
        fragmentation(receivedByte);
        // Si notifica pulsación
        if (bitRead(puls, 0) && !bitRead(puls, 1) && button != 0) {
          if (!bitRead(puls, 2) && !bitRead(puls, 3)) {
            dec = i * 10;
            firstCaracter = true;
            lcd.setCursor(0, 1);
            lcd.print("Direcc. 0-99:  " + (String) dec);
          }
        }
      }
    }
  } else {
    // Se introduce el segundo carácter
    for (int i = 0; i < 10; i++) {
      Wire.requestFrom(ledAddress[i], 1);   // Recibe byte a byte
      while (Wire.available()) {
        byte receivedByte = Wire.read();
        fragmentation(receivedByte);
        // Si notifica pulsación
        if (bitRead(puls, 0) && !bitRead(puls, 1) && button != 0) {
          newAddress = dec + i;
          lcd.setCursor(0, 1);
          lcd.print("Direcc. 0-99: " + (String) newAddress);
          delay(1000);
          lcd.setCursor(0, 1);
          lcd.print(" MODULO ANADIDO ");
          delay (2000);
          printLcd = true;
          firstCaracter = false;
          addedAddress = true;
          state = 1;
          configState = 0;
          lightLeds();
        }
      }
    }
  }
}

/* Método que recibe la información del Módulo Adicional. Realiza la lectura y envía el comando MIDI*/
void addedDsp() {
  if (addedAddress) {
    Wire.requestFrom(newAddress, 2);   // Recibe byte a byte
    for (int i = 0; i < 3; i++) {
      midiPacket[i + 2] = Wire.read();
    }
    if (firstTime) {
      lastMidiPacket[0] =  midiPacket[2];
      lastMidiPacket[1] =  midiPacket[3];
      lastMidiPacket[2] =  midiPacket[4];
      firstTime = false;
      pCharacteristic->setValue(midiPacket, 5); // packet, length in bytes
      pCharacteristic->notify();
    } else {
      if (midiPacket[2] != lastMidiPacket[0] || midiPacket[3] != lastMidiPacket[1] || midiPacket[4] != lastMidiPacket[2]) {
        lastMidiPacket[0] =  midiPacket[2];
        lastMidiPacket[1] =  midiPacket[3];
        lastMidiPacket[2] =  midiPacket[4];
        pCharacteristic->setValue(midiPacket, 5); // packet, length in bytes
        pCharacteristic->notify();
      }
    }
    sendMidi(midiPacket[2], midiPacket[3], midiPacket[4]);
  }
}

// Fragmentación del byte para extraer: si notifica pulsación, pulsador y tipo de pulsación.
void fragmentation(byte receivedByte) {
  byte x;
  puls = receivedByte >> 4;
  x = puls << 4;
  button = receivedByte - x;
}

// Mostrar displays
void displays() {
  Wire.beginTransmission(displayAddress);
  Wire.write(displayLet); // sends one byte
  Wire.write(displayNum);
  Wire.write(displayBank);
  Wire.endTransmission(); // stop transmitting
}

// Envío de MIDI para los Modos Preset, Sampler y Jam.
void sendMidiCC(int indexCC) {
  midiPacket[3] = cc[indexCC];
  pCharacteristic->setValue(midiPacket, 5); // packet, length in bytes
  pCharacteristic->notify();
  DIN_MIDI.sendControlChange(midiPacket[3], midiPacket[4], 1);
}

// Envío de comando MIDI para Modo Libre y Módulo Añadido
void sendMidi(int firstValue, int secondValue, int thirdValue) {
  // Se extrae del primer Byte el canal y el tipo de comando.
  int midiType = firstValue >> 4;
  int x = midiType << 4;
  int midiChannel = firstValue - x;
  switch (midiType) {
    case 8:
      DIN_MIDI.sendNoteOff(secondValue, thirdValue, midiChannel);
      break;
    case 9:
      DIN_MIDI.sendNoteOn(secondValue, thirdValue, midiChannel);
      break;
    case 10:
      DIN_MIDI.sendPolyPressure(secondValue, thirdValue, midiChannel);
      break;
    case 11:
      DIN_MIDI.sendControlChange(secondValue, thirdValue, midiChannel);
      break;
    case 12:
      DIN_MIDI.sendProgramChange(secondValue, midiChannel);
      break;
    case 13:
      DIN_MIDI.sendAfterTouch(secondValue, midiChannel);
      break;
    case 14:
      { // Calibra el valor entre 0-127 para que esté entre -8192 y 8192
        float pitchM = 16384 / 127;
        int pitchValue = secondValue * pitchM - 8192;
        midiPacket[3] = (pitchValue >> 8) & 0xFF;
        midiPacket[4] = pitchValue & 0xFF;
        pCharacteristic->setValue(midiPacket, 5); // packet, length in bytes
        pCharacteristic->notify();
        DIN_MIDI.sendPitchBend(pitchValue, midiChannel);
      }
      break;
    default:
      break;
  }
  // Envío por Bluetooth
  if (midiType != 14) {
    midiPacket[2] = firstValue;
    midiPacket[3] = secondValue;
    midiPacket[4] = thirdValue;
    pCharacteristic->setValue(midiPacket, 5); // packet, length in bytes
    pCharacteristic->notify();
  }
}

// Iluminación de LEDs asociados al preset
void lightLeds() {
  for (int i = 0; i < 10; i++) {
    Wire.beginTransmission(ledAddress[i]);
    Wire.write(currentLeds[i]);
    Wire.endTransmission();
  }
}

// Apagar luces LED
void turnOffLeds() {
  for (int i = 0; i < 10; i++) {
    Wire.beginTransmission(ledAddress[i]);
    Wire.write(false);
    Wire.endTransmission();
  }
}

// Mostrar pantalla LCD de los distintos modos
void printlcd(int progMode) {
  lcd.setCursor(0, 0);
  switch (progMode) {
    case 1:
      lcd.print("MODO PRESET     ");
      break;
    case 2:
      lcd.print("MODO JAM        ");
      break;
    case 3:
      lcd.print("MODO SAMPLER    ");
      break;
    case 4:
      lcd.print("MODO LIBRE      ");
      break;
    default:
      break;
  }
  if (progMode != 4) {
    lcd.setCursor(0, 1);
    if (expChange) lcd.print("PITCH   VOLUM   ");
    else lcd.print("WAH     VOLUM   ");
  }
  printLcd = false;
}

// Siguiente preset 1A...D -> 2A....D -> .... 4D-> 1A
void nextPreset() {
  if (displayLet == 13) {
    displayLet = 10;
    if (displayNum == 8) {
      displayNum = 1;
      if (displayBank == 4) {
        displayBank = 1;
      } else displayBank++;
    } else displayNum++;
  } else displayLet++;
  displays();
  loadPreset();
}

// Anterior preset 1A -> 8D...A -> 7D...A ->...1A
void previousPreset() {
  if (displayLet == 10) {
    displayLet = 13;
    if (displayNum == 1) {
      displayNum = 8;
      if (displayBank == 1) {
        displayBank = 4;
      } else displayBank--;
    } else displayNum--;
  } else displayLet--;
  displays();
  loadPreset();
}

// Siguiente página 1->2->...->7->8->1
void nextPage() {
  displayLet = 14;
  if (displayNum == 8) displayNum = 1;
  else displayNum++;
  displays();
}

// Página anterior 8->7->...->2->1->8
void previousPage() {
  displayLet = 14;
  if (displayNum == 1) displayNum = 8;
  else displayNum--;
  displays();
}

// Carga de preset: iluminación de LEDs y guardado de preset actual
void loadPreset() {
  if ((displayNum == 1 || displayNum == 2) && (displayLet > 9 && displayLet < 15)) {
    int memoryAddress = 130 + (40 * (displayNum - 1)) + (10 * (displayLet - 10));
    for (int i = 0; i < 10; i++) {
      currentLeds[i] = EEPROM.read(memoryAddress);
      memoryAddress++;
    }
    if ((int) EEPROM.read(210) != displayNum) {
      EEPROM.write(210, (byte) displayNum);
      EEPROM.commit();
    }
    if ((int) EEPROM.read(211) != displayLet) {
      EEPROM.write(211, (byte) displayLet);
      EEPROM.commit();
    }
    lightLeds();
  }
}
