/*************************************************************
  Programa de los pedales de expresión. 
  Realiza una lectura del pin conectado al pedal de expresión. El valor,
  comprendido entre 0 y 1023 lo envía al máster. Si se detecta que el pedal
  está siendo accionado, se enciende su LED asociado.
 
  Comunicación I2C:
  - Envío al máster: lectura de pin en 2 bytes. Valores de 0 a 1023.
  - Direcciones: 7, 8.
*************************************************************
  Proyecto FCB1010 de Elena Nieto: https://github.com/elenieto/fcb1010
**************************************************************/
#include <twi.h>
#include <TinyWire.h>

const int address = 8;
//Asignación de pines
const int pot = 3;
const int led = 4;

boolean first = true;

int out = HIGH;
int readData;  
int lastReadData; //variable para el valor de la lectura anterior
byte dataSend[2];

void setup ()  {
  TinyWire.begin(address);
  TinyWire.onRequest(onI2CRequest);
  // TinyWire.onReceive( onI2CReceive );
  pinMode (led, OUTPUT);  // declaramos el led como salida
  /* los pin analogicos se declaran como entrada automaticamente */
}

void loop () {
  // lectura de valores  entre 0 y 1023
  readData = analogRead (pot);
  if (first) {
    lastReadData = readData;
    first = false;
  } else {
    // Si hay una variación significativa se enciende el led
    if ((readData > lastReadData + 20) || (readData < lastReadData - 0) ) {
      out = LOW;
    }
    else {
      out = HIGH;
    }
    lastReadData = readData;
  }
  digitalWrite(led, out);
}

// Envío del valor leído en dos bytes
void onI2CRequest() {
  dataSend[0] = (readData >> 8) & 0xFF;
  dataSend[1] = readData & 0xFF;
  TinyWire.send(dataSend, 2);
}
