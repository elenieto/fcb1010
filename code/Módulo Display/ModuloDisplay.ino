

/*************************************************************
  Programa del Módulo Display. Basado en el programa de Tauno Erik 
  (https://www.hackster.io/taunoerik/selfmade-i2c-7-segment-display-with-attiny85-a637d0)
  
  Comunicación I2C:
  - Recibe: los valores que tiene que mostrar en los 3 displays de 7 segmentos
  - Dirección: 9
*************************************************************
  Proyecto FCB1010 de Elena Nieto: https://github.com/elenieto/fcb1010
**************************************************************/

#include <twi.h>
#include <TinyWire.h>

byte own_address = 9; //Slave i2c address
// ATtiny PB1, PB3, PB4
int latchPin = 1; // Pin connected to ST_CP of 74HC595
int dataPin  = 3; // Pin connected to    DS of 74HC595
int clockPin = 4; // Pin connected to SH_CP of 74HC595

// Array que almacena los dígitos
byte numArray[15];
uint8_t    i = 0;
uint8_t   ii = 0;
uint8_t  iii = 0;
void setup() {
 // Setup TinyWire I2C slave
 TinyWire.begin(own_address);
 TinyWire.onReceive( onI2CReceive );
 pinMode(latchPin, OUTPUT);
/*  
* 7 segmentos:
*  |--A--|
*  F     B
*  |--G--|
*  E     C
*  |--D--|   
*         H
*         
*  GF+AB
*    #
*  ED+CH
*
*  0b00000000
*    ABEDCHFG
*  
*  0- High
*  1- Low
*/
  numArray[0] = 0b00000101; 
  numArray[1] = 0b01110111; 
  numArray[2] = 0b00001110; 
  numArray[3] = 0b00100110; 
  numArray[4] = 0b01110100; 
  numArray[5] = 0b10100100; 
  numArray[6] = 0b10000100; 
  numArray[7] = 0b00110111; 
  numArray[8] = 0b00000100; 
  numArray[9] = 0b00110100;
  numArray[10] = 0b00010100; // A
  numArray[11] = 0b11000100; // B
  numArray[12] = 0b10001101; // C
  numArray[13] = 0b01000110; // D
  numArray[14] = 0b11111110; // Todos apagados
 digitalWrite(latchPin, 0);
 shiftOut(dataPin, clockPin, 0b11111110);
 shiftOut(dataPin, clockPin, 0b11111110);
 shiftOut(dataPin, clockPin, 0b11111110);
 digitalWrite(latchPin, 1);
 delay(400);
} 
void loop() {
} 

void onI2CReceive(int howMany){
 digitalWrite(latchPin, 0);
 while(1 < TinyWire.available()){
   int c = TinyWire.read();
   shiftOut(dataPin, clockPin, numArray[c]);
 }
 // último
 int x = TinyWire.read();
 shiftOut(dataPin, clockPin, numArray[x]);
 digitalWrite(latchPin, 1);
}
/********************************
* 
********************************
void numsOut(int i, int ii, int iii){
 digitalWrite(latchPin, 0);
 shiftOut(dataPin, clockPin, numArray[iii]);  //x0xx
 shiftOut(dataPin, clockPin, numArray[ii]);   //xx0x
 shiftOut(dataPin, clockPin, numArray[i]);    //xxx0
 digitalWrite(latchPin, 1);
 }
/*************************************
* 
*************************************/
void shiftOut(int myDataPin, int myClockPin, byte myDataOut) {
 /* Desplaza 8 bits empezando por el más significativo en el flanco de subida
 del reloj */
 int i=0;
 int pinState;
 pinMode(myClockPin, OUTPUT);
 pinMode(myDataPin, OUTPUT);
 digitalWrite(myDataPin, 0);
 digitalWrite(myClockPin, 0);
 // Se realiza una cuenta descendente 
 for (i=7; i>=0; i--)  {
   digitalWrite(myClockPin, 0);
   // Recorre todas las posiciones y será true cuando haya un 1 en esa posición
   if ( myDataOut & (1<<i) ) {
     pinState= 1;
   }
   else {  
     pinState= 0;
   }
   // Pone el pin de datos a 0 ó 1 según corresponga
   digitalWrite(myDataPin, pinState);
   //Flanco de subida de reloj 
   digitalWrite(myClockPin, 1);
   //Se pone el pin de datos a 0 para evitar desbordamientos
   digitalWrite(myDataPin, 0);
 }
// Fin del desplazamiento
digitalWrite(myClockPin, 0);
}
