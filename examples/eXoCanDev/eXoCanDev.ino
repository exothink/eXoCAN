/* 
eXoCanDev.ino                                                                        5/3/20

Connect two boards and set msgNum = X.  They will communicate via CAN and send tx/rx
info via PA9 serial output.

****** REMEMBER No Serial USB with CAN *********
adding build flags to pio.ini for serial usb cdc kills CAN bus

TI Can boards DO NOT work!!!!  got one working by wiring RS pin low.                  1/5

TJA1050 boards work, but must have a good +5V.  USB to UART adapters don't supply 
enough current, +5V is down to 3V at blue pill.  USB from PC works.                   1/7

just sending 8 bytes in a tight loop = 488uS msg to msg at br250K                     1/14
444uS msg, 44uS dead time

using: can.begin(frame[frameIdx].idLen, BR250K, frame[frameIdx].busConfig);           4/15/21
debugged 32b filters with extended IDs.  OK now
     

RAM:   [=         ]   6.3% (used 1296 bytes from 20480 bytes)   with **watchdog** - adds 1792 bytes
Flash: [=         ]  13.9% (used 18264 bytes from 131072 bytes)

RAM:   [=         ]   6.3% (used 1292 bytes from 20480 bytes)   **without watchdog**
Flash: [=         ]  12.6% (used 16472 bytes from 131072 bytes)
 */

#include <arduino.h>
#include <eXoCAN.h>
#include <IWatchdog.h>

const int rxMsgID = 0x69; // used by CAN rx filter
//                           can.filterMask16Init(1, rxMsgID, 0x000); // 0x00 is don't care.  All IDs are rcvd.

eXoCAN can(STD_ID_LEN, BR250K, PORTA_11_12_XCVR); // constructor

// extendedId(0x18da'f110);  //29 bit 'extended' addr
// stdId(0x7e8);            // 11 bit 'std' addr

template <typename T>
void cpArray(T from[], T to[], int len) // copies one array to another
{
  for (int i = 0; i < len; i++)
    to[i] = from[i];
}

struct msgFrm frame[1]; // an array of tx structures

uint8_t frmData[5][8] = {
    // arrays of tx msg data fields
    {0x00, 0xff, 0xff, 0x00, 0x00, 0xff, 0xff, 0xff}, // [0][]
    {0x01, 0xa5, 0x00, 0x00, 0x5a, 0x5a, 0x5a, 0xff}, // [1][]
    {0x02, 0xe7, 0xe7, 0xe7, 0xe7, 0xe7, 0xe7, 0xe7}, // [2][]
    {0x03, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07}, // [3][]
    {0x04, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}  // [4][]
};

uint8_t frameIdx = 0; // index of tx frame[] to send. In this example it isn't useful as there is only one 'frame[1]'
uint8_t msgNum = 1;   // index of tx data to send   <<<<<<----------<<<<<<

void initFrame()
{
  int frmDataIdx = 0; // index of desired frmData[frmDataIdx]
  switch (msgNum)
  {
  case 0:
    frmDataIdx = 0;
    frame[frameIdx].txMsgID = 0x05;
    frame[frameIdx].idLen = STD_ID_LEN;
    frame[frameIdx].txDly = 500;
    frame[frameIdx].busConfig = PORTA_11_12_WIRE_PULLUP; //PORTB_8_9_WIRE_PULLUP;
    break;
  case 1:
    frmDataIdx = 1;
    frame[frameIdx].txMsgID = 0x7ff; //daf110; // 0x69;
    frame[frameIdx].idLen = STD_ID_LEN;
    frame[frameIdx].txDly = 1000;
    frame[frameIdx].busConfig = PORTA_11_12_XCVR; //PORTA_11_12_XCVR;
    break;
  case 2:
    frmDataIdx = 2;
    frame[frameIdx].txMsgID = 0x69;
    frame[frameIdx].idLen = STD_ID_LEN;
    frame[frameIdx].txDly = 1500;
    frame[frameIdx].busConfig = PORTB_8_9_XCVR;
    break;
  case 3:
    frmDataIdx = 3;
    frame[frameIdx].txMsgID = 0x69;
    frame[frameIdx].idLen = EXT_ID_LEN;
    frame[frameIdx].txDly = 4000;
    frame[frameIdx].busConfig = PORTB_8_9_XCVR;
    break;
  case 4:
    frmDataIdx = 4;
    frame[frameIdx].txMsgID = 0x00232461; //0x69;
    frame[frameIdx].idLen = EXT_ID_LEN;
    frame[frameIdx].txDly = 1100;
    frame[frameIdx].busConfig = PORTA_11_12_XCVR;
    break;

  default:
    break;
  }
  int mLen = sizeof(frmData[0]) / sizeof(frmData[0][0]);
  cpArray(frmData[frmDataIdx], frame[frameIdx].txMsg.bytes, mLen); // copy '8' values
}

char cBuff[46]; // holds formatted string

inline void canSend(msgFrm frm, bool prt = false) //send a CAN Bus message
{
  bool status = can.transmit(frm.txMsgID, frm.txMsg.bytes, frm.txMsgLen);
  if (prt)
  {
    if (status == false)
    {
      Serial.println("   *** TX ERROR ***");
      Serial.println("   mailbox not empty ");
    }
    else
    {
      if (can.getIDType())
        sprintf(cBuff, "tx @%08x #%u \t", frm.txMsgID, frm.txMsgLen);
      else
        sprintf(cBuff, "tx @%03x #%u \t", frm.txMsgID, frm.txMsgLen);
      Serial.print(cBuff);
      for (u_int8_t j = 0; j < frm.txMsgLen; j++)
      {
        sprintf(cBuff, "%02X ", frm.txMsg.bytes[j]);
        Serial.print(cBuff);
      }
      Serial.println();
    }
  }
}

inline void canRead(bool print = false) //check for a CAN Bus message
{
  //len = can.receive(id, fltIdx, rxMsg.bytes);  // comment out when using rx interrupt
  uint8_t cnt = can.getRxMsgFifo0Cnt();
  uint8_t full = can.getRxMsgFifo0Full();
  uint8_t ovr = can.getRxMsgFifo0Overflow();

  if (can.rxMsgLen >= 0)
  {
    if (print)
    {
      if (can.getRxIDType())
        sprintf(cBuff, "RX @%08x #%d  %d\t", can.id, can.rxMsgLen, can.fltIdx);
      else
        sprintf(cBuff, "RX @%03x #%d  %d\t", can.id, can.rxMsgLen, can.fltIdx);

      Serial.print(cBuff);
      for (uint8_t j = 0; j < can.rxMsgLen; j++)
      {
        sprintf(cBuff, "%02x ", can.rxData.bytes[j]);
        Serial.print(cBuff);
      }
      if (can.getRxMsgFifo0Cnt()) // check if loop is able to keep up with bus messages
      {
        sprintf(cBuff, " cnt %d, full %d, overflow %d ", cnt, full, ovr);
        Serial.print(cBuff);
      }
      Serial.println();
    }
    digitalToggle(PC13); // blink LED
    can.rxMsgLen = -1;
  }
  // return id;
}

void canISR() // get bus msg frame passed by a filter to FIFO0
{
  can.rxMsgLen = can.receive(can.id, can.fltIdx, can.rxData.bytes); // get CAN msg
}

// &&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&
void setup()
{
  Serial.begin(921600);
  initFrame(); // fill tx frame with data
  //can.setIDType(STD);                // send standard or extended IDs
  //can.begin(STD_ID_LEN, BR250K, PORTA_11_12_WIRE_PULLUP); // can.init(RATE, SINGLE_WIRE, frame[frameIdx].altPins);
  //can.begin(frame[0].idLen, BR250K, frame[0].busConfig);  // frame[frameIdx].busConfig
  can.begin(frame[frameIdx].idLen, BR250K, frame[frameIdx].busConfig); // CAN was constructed with user parms, this sets new parms
  can.setAutoTxRetry(true);                                            // CAN hw keeps sending last tx until someone ACK's it
  can.attachInterrupt(canISR);

  // can.filterList16Init(0, 0, 0, 0, 0);    // works 4/15/21
  // can.filterList16Init(1, 0, 0, 0, 0);    // works iff bank0 is setup.   4/15/21
  // can.filterList16Init(2, 0x68, 0, 0, 5); // 4/19,   ????? 4/15/21
  // can.filterList16Init(3, 0x69, 0, 0, 5);

  // can.filterMask16Init(0, 0x68, 0xfffe); // 0x00 is don't care.  All IDs are rcvd. (1, rxMsgID, 0x000)
  // can.filterMask16Init(1, 55, 0x7fff);
  // can.filterMask16Init(2, 45, 0x7fff);
  // can.filterMask16Init(3, rxMsgID, 0x7fff);

  //can.filterList32Init(0, 0x00232461);
  can.filterMask32Init(0, 0x00232461, 0x1fffffff);

  Serial.println("\n*** " __DATE__ "  @" __TIME__ " ***\n");
  pinMode(PC13, OUTPUT); // blue pill LED

  //// can.enableInterrupt(); // enable rx interrupt

  // IWatchdog.begin(4000000UL);   // 4s.  the tx can hang if it doesn't get an ACK
}

int lastPending = 0;
uint32_t last = 0;
void loop()
{
  //----------------------------------tx-------------------/-------------
  if (millis() / frame[frameIdx].txDly != last)
  {
    last = millis() / frame[frameIdx].txDly;
    frame[0].txMsg.int32[1] = millis(); // last 4 bytes of CAN msg
    frame[0].txMsg.int32[0] = 0;        // first four bytes = 0
    // frame[frameIdx].txMsg.int64 = 0x0807060504030201;
    Serial.println();
    canSend(frame[frameIdx], true);
    Serial.println();
  }

  delay(120);
  // ----------------------------------rx----------------------------
  canRead(true);
  // IWatchdog.reload();
} // end of loop
