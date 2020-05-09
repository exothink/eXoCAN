/* 
 bpCanBlinkInterrupt.ino                                                                     4/24/20
 C:\Users\jhe\Documents\PlatformIO\mySTM32LIBS\eXoCAN\bpCanBlink.cpp

 This simple example requires two STM32f103 boards.  One of the boards is loaded
 with the 'frame' using txMsg = 0x069.  The second board gets loaded with the second
 frame where tsMsgID = 0x005 by uncommenting that section.

 On each board connect the CAN default RX and TX pins(PA11, PA12) together.  Then connect
 the boards together: +5V, Gnd., and Can(PA11/12 pair).

 When a board receives a CAN message it toggles its LED.  One board sends a message
 every second and the other every five seconds.

  RAM:   1180 bytes 
  Flash: 13028 bytes 

 working                                                                            4/25

*/
#include <arduino.h>
#include <eXoCAN.h>
#define bluePillLED PC13

// tx frame setup #1
int txMsgID = 0x069;
int rxMsgID = 0x005;   // needed for rx filtering
uint8_t txData[8]{0x00, 0x01, 0x23, 0x45, 0xab, 0xcd, 0xef, 0xff};
uint8_t txDataLen = 8;
uint32_t txDly = 5000; // mSec

//  ****** uncomment the following for the second stm32f103 board ******

// int txMsgID = 0x005;
// int rxMsgID = 0x069;   // only needed if using a filter
// uint8_t txData[8]{{0x01, 0xfe, 0xdc, 0xba, 0x11, 0x12, 0x34, 0x56};
// uint_8 txDataLen = 8;
// uint32_t txDly = 1000;  // mSec

int id, fltIdx;
uint8_t rxbytes[8];

// 11b IDs, 250k bit rate, no transceiver chip, portA pins 11,12, no external resistor
eXoCAN can(STD_ID_LEN, BR250K, PORTA_11_12_WIRE_PULLUP); 

void canISR() // get CAN bus frame passed by a filter into fifo0
{
    can.receive(id, fltIdx, rxbytes);  // empties fifo0 so that another another rx interrupt can take place
    digitalToggle(bluePillLED);
}

void setup()
{
  can.attachInterrupt(canISR);
  // can.filterMask16Init(0, rxMsgID, 0x7ff);   // filter bank 0, filter 0: allow ID = rxMsgID, mask = 0x7ff (must match)
                                                // without calling a filter, the default passes all IDs
  pinMode(bluePillLED, OUTPUT);
}

uint32_t last = 0;
void loop()
{
  if (millis() / txDly != last)                 // send every txDly, rx handled by the interrupt
  {
    last = millis() / txDly;
    can.transmit(txMsgID, txData, txDataLen);
  }
}
