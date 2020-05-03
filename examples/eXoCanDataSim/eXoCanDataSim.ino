/* 
eXoCanDataSim.ino                                                            4/30/20

Simulates varying data from multiple sensors and sends data to the CAN and Serial busses.

****** REMEMBER No Serial USB with CAN *********

TJA1050 boards work, but must have a good +5V.  USB to UART adapters don't supply 
enough current, +5V is down to 3V at blue pill.  USB from PC works.                   1/7

just sending 8 bytes in a tight loop = 488uS msg to msg at br250K                     1/14
444uS msg, 44uS dead time

RAM:   [=         ]   5.4% (used 1108 bytes from 20480 bytes)
Flash: [==        ]  18.1% (used 23692 bytes from 131072 bytes)  working
 */

#include <arduino.h>
#include <eXoCAN.h> 

#define PRINT true
#define VERS "eXoCanDataSim 043020"

constexpr uint8_t totalMeas = 9;

enum zIndex
{
  Min,
  Second,
  Third,
  Max
};

const uint16_t auxBatLim[] = {0, 13, 15, 16};
const uint16_t batteryLim[] = {0, 13, 15, 16};
const uint16_t coolantTLim[] = {0, 210, 250, 250};
const uint16_t coolantPLim[] = {0, 5, 15, 20};
const uint16_t fuelPresLim[] = {0, 27, 32, 40};
const uint16_t fuelGalLim[] = {0, 2, 3, 16};
const uint16_t oilPresLim[] = {20, 45, 55, 60};
const uint16_t rpmLim[] = {0, 6000, 7500, 8000};
const uint16_t speedLim[] = {0, 80, 120, 120};

enum measType : uint8_t
{
  AuxBat,
  Battery,
  CoolantT,
  CoolantP,
  FuelPres,
  FuelGal,
  OilPres,
  RPM,
  Speed,
  EOM
}; // 0-9

enum measAdr : u_int8_t  // CAN Bus measurement addr (ID)
{
  adrAuxBat = 0x60,
  adrBattery,
  adrCoolantT,
  adrCoolantP, 
  adrFuelPres,
  adrFuelGal,
  adrOilPres,
  adrRPM,
  adrSpeed
}; // 0x60 - 0x68

MSG rxMsg;  // 
msgFrm frames[totalMeas]; // an array of tx message structures

eXoCAN can(STD_ID_LEN, BR250K, PORTA_11_12_WIRE_PULLUP);  // constuctor for eXoCAN lib

char cBuff[20]; // holds formatted string
void canTX(msgFrm frm, bool prt = false) //send a CAN Bus message
{
  uint8_t msgLen = frm.txMsgLen;
  can.transmit(frm.txMsgID, frm.txMsg.bytes, msgLen);
  if (prt)
  {
    sprintf(cBuff, "tx @%02x #%d \t", frm.txMsgID, msgLen);
    Serial.print(cBuff);
    for (int j = 0; j < msgLen; j++)
    {
      sprintf(cBuff, "%02x ", frm.txMsg.bytes[j]);
      Serial.print(cBuff);
    }
    Serial.println();
  }
}

uint8_t canRX(bool print = false) //check for a CAN Bus message
{
  int len, id = 0, fltIdx;
  len = can.receive(id, fltIdx, rxMsg.bytes);
  if (print)
  {
    if (len >= 0)
    {
      sprintf(cBuff, "RX @%02x #%d  %d\t", id, len, fltIdx);
      Serial.print(cBuff);
      for (int j = 0; j < len; j++)
      {
        sprintf(cBuff, "%02x ", rxMsg.bytes[j]);
        Serial.print(cBuff);
      }
      Serial.println();
    }
  }
  return id;
}

void setup()
{
  Serial.begin(921600);
  
  //                   adr          id type  bytes  data     hw config     delay
  frames[AuxBat] =  {adrAuxBat,  STD_ID_LEN, 0x01, {0x0c}, PORTB_8_9_XCVR, 10500}; // AuxBat = 0, adrAuxBat = 0x60
  frames[Battery] = {adrBattery, STD_ID_LEN, 0x01, {0x0d}, PORTB_8_9_XCVR, 9750};
  frames[CoolantT] = {adrCoolantT, STD_ID_LEN, 0x01, {0x20}, PORTB_8_9_XCVR, 6400};
  frames[CoolantP] = {adrCoolantP, STD_ID_LEN, 0x01, {0x00}, PORTB_8_9_XCVR, 6300}; 
  frames[FuelPres] = {adrFuelPres, STD_ID_LEN, 0x01, {0x2e}, PORTB_8_9_XCVR,  8100};
  frames[FuelGal] = {adrFuelGal, STD_ID_LEN, 0x01, {0x04}, PORTB_8_9_XCVR, 10250};
  frames[OilPres] = {adrOilPres, STD_ID_LEN, 0x01, {0x34}, PORTB_8_9_XCVR, 2800};
  frames[RPM] = {adrRPM, STD_ID_LEN, 0x02, {0x02, 0x03}, PORTB_8_9_XCVR, 130}; 
  frames[Speed] = {adrSpeed, STD_ID_LEN, 0x01, {0x05}, PORTB_8_9_XCVR, 2200}; // Speed = 8, adrSpeed = 0x68

  //can.filterMask16Init(0, 0x05, 0x7ff); // rx filter bank 0 definition. only allow ID == 5 into fifo0
  can.filterList16Init(2, 5, 10, 21, 0x3f0);  // rx bank 2. passes IDs == 5, 10, 21 and 0x3f0

  Serial.println(VERS);
}

uint32_t lastT[totalMeas] = {0}; // keeps track of time for sending different Can tx mgs
uint8_t n = 1;
void loop()
{
  for (n = AuxBat; n < EOM; n++) // sizeof(frames)/sizeof(frames[0])
  {
    if (millis() / frames[n].txDly != lastT[n])
    {
      static int inc = 50;
      switch (n)
      {
      case Speed:
        frames[Speed].txMsg.bytes[0] = (speedLim[Max] / 3) * (1 + sin(2 * PI * millis() / 60000));
        break;
      case RPM:
        if (frames[RPM].txMsg.int16[0] > 7490)
          inc = -253;
        if (frames[RPM].txMsg.int16[0] < 900)
          inc = 51;
        frames[RPM].txMsg.int16[0] += inc;
        break;
      case OilPres: // offset +/- delta
        frames[OilPres].txMsg.bytes[0] = ((oilPresLim[Second] + oilPresLim[Third]) / 2) + (6 * (sin(2 * PI * millis() / 50000)));
        break;
      case FuelPres: // offset +/- delta
        frames[FuelPres].txMsg.bytes[0] = ((fuelPresLim[Second] + fuelPresLim[Third]) / 2) + (3 * (sin(2 * PI * millis() / 30000)));
        break;
      case FuelGal:
        // offset +/- delta
        --frames[FuelGal].txMsg.bytes[0]; 
        if (frames[FuelGal].txMsg.bytes[0] < 2)
          frames[FuelGal].txMsg.bytes[0] = 16;
        break;
      case CoolantP: // offset +/- delta
        frames[CoolantP].txMsg.bytes[0] = ((coolantPLim[Second] + coolantPLim[Third]) / 2) + (6 * (sin(2 * PI * millis() / 60000)));
        break;
      case CoolantT: // offset +/- delta
        frames[CoolantT].txMsg.bytes[0] = coolantTLim[Second] + (20 * (sin(2 * PI * millis() / 60000)));
        break;
      case AuxBat: // offset +/- delta
        frames[AuxBat].txMsg.bytes[0] = ((auxBatLim[Second] + auxBatLim[Third]) / 2) + (2 * (sin(2 * PI * millis() / 60000)));
        break;
      case Battery: // offset +/- delta
        frames[Battery].txMsg.bytes[0] = ((batteryLim[Second] + batteryLim[Third]) / 2) + (1 * (sin(2 * PI * millis() / 60000)));
        break;
      default:
        break;
      }
      lastT[n] = millis() / frames[n].txDly;
      canTX(frames[n], PRINT); // send CAN Bus msg, print

      //canRX(PRINT);  // check/get a msg
    }
  }
}