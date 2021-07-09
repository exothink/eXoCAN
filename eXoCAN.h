#pragma once

/*
eXoCAN.h      4/16/20
vers 1.0.1  02/06/2021
vers 1.0.3  04/15/2021

'eXoCAN' is working as a struck in the original 'eXoCAN.h' file
now working as a 'class'.  
C:\Users\jhe\Documents\PlatformIO\Projects\eXoCanInt\lib\eXoCAN

  // ******* FILTERS Index Rules ************
  // fltr indexes accumulate from the prior index, the bank in use should be contiguous from bank 0.
  // If you leave empty/missing filter banks, each absorbs two index values
  // '16b list' has four indexes
  // '16b mask' has two
  // 'list' filters get seached first even when the index is higher that a mask filter


extended IDs are working                                                                   4/19

constructor now does all the setup                                                         4/27
       bug fix: extended ID filtering wasn't working. Wrong shift + set IDE bit            4/15/21
*/
#include <Arduino.h>

//Register addresses
constexpr static uint32_t CANBase = 0x40006400;

constexpr static uint32_t mcr = CANBase + 0x000;  // master cntrl
constexpr static uint32_t msr = CANBase + 0x004;  // rx status
constexpr static uint32_t tsr = CANBase + 0x008;  // tx status
constexpr static uint32_t rf0r = CANBase + 0x00C; // rx fifo 0 info reg

constexpr static uint32_t ier = CANBase + 0x014; // interrupt enable

constexpr static uint32_t btr = CANBase + 0x01C; // bit timing and rate

constexpr static uint32_t ti0r = CANBase + 0x180;  // tx mailbox id
constexpr static uint32_t tdt0r = CANBase + 0x184; // tx data len and time stamp
constexpr static uint32_t tdl0r = CANBase + 0x188; // tx mailbox data[3:0]
constexpr static uint32_t tdh0r = CANBase + 0x18C; // tx mailbox data[7:4]

constexpr static uint32_t ri0r = CANBase + 0x1B0;  // rx fifo id reg
constexpr static uint32_t rdt0r = CANBase + 0x1B4; // fifo data len and time stamp
constexpr static uint32_t rdl0r = CANBase + 0x1B8; // rx fifo data low
constexpr static uint32_t rdh0r = CANBase + 0x1BC; // rx fifo data high

constexpr static uint32_t fmr = CANBase + 0x200;   // filter master reg
constexpr static uint32_t fm1r = CANBase + 0x204;  // filter mode reg
constexpr static uint32_t fs1r = CANBase + 0x20C;  // filter scale reg, 16/32 bits
constexpr static uint32_t ffa1r = CANBase + 0x214; //filter FIFO assignment
constexpr static uint32_t fa1r = CANBase + 0x21C;  // filter activation reg
constexpr static uint32_t fr1 = CANBase + 0x240;   // id/mask acceptance reg1
constexpr static uint32_t fr2 = CANBase + 0x244;   // id/mask acceptance reg2

constexpr static uint32_t scsBase = 0xE000E000UL;        // System Control Space Base Address
constexpr static uint32_t nvicBase = scsBase + 0x0100UL; // NVIC Base Address
constexpr static uint32_t iser = nvicBase + 0x000;       //  NVIC interrupt set (enable)
constexpr static uint32_t icer = nvicBase + 0x080;       // NVIC interrupt clear (disable)

constexpr static uint32_t scbBase = scsBase + 0x0D00UL;
constexpr static uint32_t vtor = scbBase + 0x008;

// GPIO/AFIO Regs
constexpr static uint32_t afioBase = 0x40010000UL;
constexpr static uint32_t mapr = afioBase + 0x004; // alternate pin function mapping

constexpr static uint32_t gpioABase = 0x40010800UL; // port A
constexpr static uint32_t crhA = gpioABase + 0x004; // cntrl reg for port b
constexpr static uint32_t odrA = gpioABase + 0x00c; // output data reg

constexpr static uint32_t gpioBBase = gpioABase + 0x400; // port B
constexpr static uint32_t crhB = gpioBBase + 0x004;      // cntrl reg for port b
constexpr static uint32_t odrB = gpioBBase + 0x00c;      // output data reg

// Clock
constexpr static uint32_t rcc = 0x40021000UL;
constexpr static uint32_t rccBase = 0x40021000UL;
constexpr static uint32_t apb1enr = rccBase + 0x01c;
constexpr static uint32_t apb2enr = rccBase + 0x018;

// Helpers
#define MMIO32(x) (*(volatile uint32_t *)(x))
#define MMIO16(x) (*(volatile uint16_t *)(x))
#define MMIO8(x) (*(volatile uint8_t *)(x))

static inline volatile uint32_t &periphBit(uint32_t addr, int bitNum) // peripheral bit tool
{
  return MMIO32(0x42000000 + ((addr & 0xFFFFF) << 5) + (bitNum << 2)); // uses bit band memory
}

#define INRQ mcr, 0
#define INAK msr, 0
#define FINIT fmr, 0
#define fmpie0 1 // rx interrupt enable on rx msg pending bit

enum BusType : uint8_t
{
  PORTA_11_12_XCVR,
  PORTB_8_9_XCVR,
  PORTA_11_12_WIRE,
  PORTB_8_9_WIRE,
  PORTA_11_12_WIRE_PULLUP,
  PORTB_8_9_WIRE_PULLUP
};

enum BitRate : uint8_t
{
  BR125K = 15,
  BR250K = 7,
  BR500K = 3, // 500K and faster requires good electical design practice
  BR1M = 1
};

enum idtype : bool
{
  STD_ID_LEN,
  EXT_ID_LEN
};

union MSG {
  uint8_t bytes[8] = {0xff, 0xff, 0xff, 0x00, 0x00, 0xff, 0xff, 0xff};
  int16_t int16[4];
  int32_t int32[2];
  int64_t int64;
};

struct msgFrm
{
  int txMsgID = 0x68; //volatile
  idtype idLen = STD_ID_LEN;
  uint8_t txMsgLen = 0x08;
  MSG txMsg;
  //uint8_t txMsg[8];
  BusType busConfig = PORTA_11_12_XCVR;
  uint32_t txDly = 5000;
};

class eXoCAN
{
private:
  idtype _extIDs = STD_ID_LEN;
  idtype _rxExtended;
  void filter16Init(int bank, int mode, int a = 0, int b = 0, int c = 0, int d = 0); // 16b filters
  void filter32Init(int bank, int mode, u_int32_t a, u_int32_t b);                   //32b filters

protected:
public:
  eXoCAN(idtype addrType = STD_ID_LEN, int brp = BR125K, BusType hw = PORTA_11_12_XCVR) 
    {begin(addrType, brp, hw);}
  void begin(idtype addrType = STD_ID_LEN, int brp = BR125K, BusType hw = PORTA_11_12_XCVR);
  void begin(idtype addrType, int brp, bool singleWire, bool alt, bool pullup);
  void enableInterrupt();
  void disableInterrupt();
  void filterMask16Init(int bank, int idA = 0, int maskA = 0, int idB = 0, int maskB = 0x7ff); // 16b mask filters
  void filterList16Init(int bank, int idA = 0, int idB = 0, int idC = 0, int idD = 0);         // 16b list filters
  void filterMask32Init(int bank, u_int32_t id = 0, u_int32_t mask = 0);
  void filterList32Init(int bank, u_int32_t idA = 0, u_int32_t idB = 0); // 32b filters
  bool transmit(int txId, const void *ptr, unsigned int len);
  //int receive(volatile int *id, volatile int *fltrIdx, volatile void *pData);
  int receive(volatile int &id, volatile int &fltrIdx, volatile uint8_t pData[]);
  void attachInterrupt(void func());
  bool getSilentMode() { return MMIO32(btr) >> 31; }
  void setAutoTxRetry(bool val = true) { periphBit(mcr, 4) = !val; } // &= 0xffffffef | retry << 4;}     // if tx isn't ACK'd don't retry
  // void setSilentMode(bool silent) { MMIO32(btr) &= 0x7fffffff | silent << 31; } // bus listen only
  void setSilentMode(bool val) { periphBit(btr, 31) = val; }
  idtype getIDType() { return _extIDs; }
  idtype getRxIDType() { return _rxExtended; }
  ~eXoCAN() {}

  // uint8_t rxMsgCnt = 0; //num of msgs in fifo0
  // uint8_t rxFull = 0;
  // uint8_t rxOverflow = 0;

  uint8_t getRxMsgFifo0Cnt() {return MMIO32(rf0r) & (3 << 0);} //num of msgs
  uint8_t getRxMsgFifo0Full() {return MMIO32(rf0r) & (1 << 3);}
  uint8_t getRxMsgFifo0Overflow() {return MMIO32(rf0r) & (1 << 4);} // b4

  volatile int rxMsgLen = -1; // CAN parms
  volatile int id, fltIdx;
  volatile MSG rxData;  // was uint8_t 
};
