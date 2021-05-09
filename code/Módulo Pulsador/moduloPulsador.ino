
/*************************************************************
Programa que distingue 3 tipos de pulsaciones (corta, larga y doble) en un pulsador.
Comunicación I2C:
- Envía al máster: si hay pulsación, de qué tipo y de qué botón es.
- Direcciones: 6, 15
*************************************************************
Proyecto FCB1010 de Elena Nieto: https://github.com/elenieto/fcb1010
**************************************************************/

#include <twi.h>
#include <TinyWire.h>

#define SHORT_PULS 0
#define LONG_PULS 64
#define DOUBLE_PULS 128

// Dirección del Attiny en el bus I2C
const int address = 15;

// Número de pulsador
const int pulsNumber = 12;

//Asignación de pines
int buttonPin = 3;

//Variables: entradas y salidas
int last_in = HIGH;
int now_in;
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
      }
    } else {
      // Si se cumple la espera sin que haya habido otra pulsación
      if (shortPuls == true && millis() - startAwait > 300) {
        // Pulsación corta
        pulsation = SHORT_PULS;
        notification = 16;
        shortPuls = false;
      }
    }
  }
  last_in = now_in;
}

void onI2CRequest() {
  // Se forma un byte con la información que recibirá el máster
  byteSent = pulsation + pulsNumber + notification;
  TinyWire.write(byteSent);
  notification = 0;
}
