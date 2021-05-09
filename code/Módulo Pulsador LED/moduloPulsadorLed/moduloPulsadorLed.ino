
/*************************************************************
Programa que distingue 3 tipos de pulsaciones (corta, larga y doble)
en un pulsador y enciende el led en consecuencia.
- Corta: cambio de estado
- Larga: parpadeo largo
- Doble: parapadeo corto

Comunicación I2C:
- Envía al máster: si hay pulsación, de qué tipo y de qué botón es.
- Recibe del máster: apagado y encendido de LED.
- Direcciones: 16, 2, 3, 4, 5, 10, 11, 12, 13, 14
*************************************************************
Proyecto FCB1010 de Elena Nieto: https://github.com/elenieto/fcb1010
**************************************************************/

#include <twi.h>
#include <TinyWire.h>

#define SHORT_PULS 0
#define LONG_PULS 64
#define DOUBLE_PULS 128

// Dirección del Attiny en el bus I2C
const int address = 14;

// Número de pulsador
const int pulsNumber = 10;

//Asignación de pines
int buttonPin = 3;
int ledPin = 4;

//Variables: entradas y salidas
int last_in = HIGH;
int now_in;
int out = HIGH;
int pulsation = 0;

// Variables de tiempo
const long debounceTime = 100;
long lastChange = 0;
long penultChange = 0;
long startAwait = 0;

//Otras variables
boolean change = false;
boolean shortPuls = false;
int byteSent;
int notification = 0;

void setup()
{
  TinyWire.begin(address);
  TinyWire.onRequest(onI2CRequest);
  TinyWire.onReceive( onI2CReceive );
  pinMode(ledPin, OUTPUT);
}

void loop()
{
  // Lectura del pin del pulsador
  now_in =  digitalRead(buttonPin);
  
  // Si el nivel leído es bajo, el anterior era alto y ha pasado el tiempo de rebote
  if (now_in == LOW && last_in == HIGH && millis() - lastChange > debounceTime)
  {
    // Flanco de bajada
    change = true;
    penultChange = lastChange;
    lastChange = millis();
  }
  else {
      // Si el nivel leído es alto, el anterior era bajo, ha pasado el tiempo de rebote 
      // y ha habido un flanco de bajada anterior
    if (now_in == HIGH && last_in == LOW && millis() - lastChange > debounceTime && change == true) {
      // Flanco de subida
      change = false;
      // Si el tiempo entre flanco de subida y de bajada es menor que 400ms -> pulsación corta o doble
      if (millis() - lastChange < 400) {
        // Si el tiempo entre la pulsación actual y la anterior es menor que 500ms
        if (lastChange - penultChange < 500) {
          // Pulsación doble
          shortPuls = false;
          notification = 16;
          pulsation = DOUBLE_PULS;
          digitalWrite(ledPin, !out);
          delay(100);
          digitalWrite(ledPin, out);
          delay(100);
        }
        else {
          // Empieza la espera para saber si estamos ante una pulsación corta o la primera pulsación
          // de una doble. 
          startAwait = millis();
          shortPuls = true;
        }
      } else {
        // Pulsación larga
        pulsation = LONG_PULS;
        notification = 16;
        // Parpadeo
        digitalWrite(ledPin, !out);
        delay(100);
        digitalWrite(ledPin, out);
        delay(100);
        digitalWrite(ledPin, !out);
        delay(100);
        digitalWrite(ledPin, out);
        delay(100);
        digitalWrite(ledPin, !out);
        delay(100);
        digitalWrite(ledPin, out);
        delay(100);
        digitalWrite(ledPin, !out);
        delay(100);
      }
    } else {
      // Si se cumple la espera sin que haya habido otra pulsación
      if (shortPuls == true && millis() - startAwait > 300) {
        // Pulsación corta
        pulsation = SHORT_PULS;
        notification = 16;
        out = !out;
        shortPuls = false;
      }
    }
  }
  digitalWrite(ledPin, out); // envía a la salida  ́led ́el valor leído
  last_in = now_in;
}

void onI2CRequest() {
  // Se forma un byte con la información que recibirá el máster
  byteSent = pulsation + pulsNumber + notification;
  TinyWire.write(byteSent);
  notification = 0;
}

// ESP32 enciende o apaga LED
void onI2CReceive(int howMany) {
  while (TinyWire.available() > 0) {
    out = TinyWire.read() ? LOW : HIGH;
  }
}
