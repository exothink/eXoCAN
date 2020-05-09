/* 
eXoCanDev.ino                                                                        5/3/20

from: eXoCanIntArd                                                                   4/17/20

****** REMEMBER No Serial USB with CAN *********
adding build flags to pio.ini for serial usb cdc kills CAN bus

TI Can boards DO NOT work!!!!  got one working by wiring RS pin low.                  1/5

TJA1050 boards work, but must have a good +5V.  USB to UART adapters don't supply 
enough current, +5V is down to 3V at blue pill.  USB from PC works.                   1/7

added 'Union' for msg data type conversions                                           1/11

just sending 8 bytes in a tight loop = 488uS msg to msg at br250K                     1/14
444uS msg, 44uS dead time
     

RAM:   [=         ]   6.3% (used 1296 bytes from 20480 bytes)   with **watchdog** - adds 1792 bytes
Flash: [=         ]  13.9% (used 18264 bytes from 131072 bytes)

RAM:   [=         ]   6.3% (used 1292 bytes from 20480 bytes)   **without watchdog**
Flash: [=         ]  12.6% (used 16472 bytes from 131072 bytes)
 */
 
#include <arduino.h>
#include <eXoCAN.h>
#include <IWatchdog.h>

const int rxMsgID = 0x69; // used by CAN rx filter

eXoCAN can(STD_ID_LEN, BR250K, PORTA_11_12_WIRE_PULLUP);  // constructor

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

uint8_t frameIdx = 0; // index of tx frame[] to send

uint8_t msgNum = 1;   // index of tx data to send   <<<<<<----------<<<<<<

void initFrame()
{
  int frmDataIdx = 1;
  switch (msgNum)
  {
  case 0:
    frmDataIdx = 0;
    frame[frameIdx].txMsgID = 0x05;
    frame[frameIdx].idLen = STD_ID_LEN;
    frame[frameIdx].txDly = 500;
    frame[frameIdx].busConfig = PORTB_8_9_WIRE_PULLUP;
    break;
  case 1:
    frmDataIdx = 1;
    frame[frameIdx].txMsgID = 0x7ff; //daf110; // 0x69;
    frame[frameIdx].idLen = STD_ID_LEN;
    frame[frameIdx].txDly = 5000;
    frame[frameIdx].busConfig = PORTA_11_12_WIRE_PULLUP;
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
    frame[frameIdx].txMsgID = 0x18daf110; //0x69;
    frame[frameIdx].idLen = EXT_ID_LEN;
    frame[frameIdx].txDly = 5000;
    frame[frameIdx].busConfig = PORTB_8_9_XCVR;
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
  //len = can.receive(&id, &fltIdx, rxMsg.bytes);  // use for non-interrupt, i.e. polling mode

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
        sprintf(cBuff, " cnt %d, full %d, overflow %d ", 
                can.getRxMsgFifo0Cnt(), can.getRxMsgFifo0Full(), can.getRxMsgFifo0Overflow());
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
  Serial.begin(112500);
  initFrame(); // fill tx frame with data
  //can.setIDType(STD);                // send standard or extended IDs
  //can.begin(STD_ID_LEN, BR250K, PORTA_11_12_WIRE_PULLUP); // can.init(RATE, SINGLE_WIRE, frame[frameIdx].altPins);
  //can.begin(frame[0].idLen, BR250K, frame[0].busConfig);
  can.setAutoTxRetry(true);  // CAN hw keeps sending last tx until someone ACK's it
  can.attachInterrupt(canISR);
  //  can.filterList16Init(0, 0, 0, 0, 0x0);
  //  can.filterList16Init(1, 0, 0, 0, 0);
  // can.filterList16Init(2, 0x69, 0, 0, 5);  // 4/19
  // can.filterMask16Init(1, rxMsgID, 0x7ff);
  can.filterMask16Init(1, rxMsgID, 0x000); // 0x00 is don't care.  All IDs are rcvd.

  Serial.println("\n*** " __DATE__ "  @" __TIME__ " ***\n"); 
  pinMode(PC13, OUTPUT); // blue pill LED

  can.enableInterrupt(); // enable rx interrupt

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
    frame[0].txMsg.int32[1] = millis();    // high 4 bytes of CAN msg
    frame[0].txMsg.int32[0] = 0;           // first four bytes = 0
    // frame[frameIdx].txMsg.int64 = 0x0807060504030201;
    Serial.println();
    canSend(frame[frameIdx], true);
    Serial.println();
  }

  // ----------------------------------rx----------------------------
  canRead(true);
  // IWatchdog.reload();
} // end of loop
