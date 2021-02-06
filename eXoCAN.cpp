#include "eXoCAN.h"

// vers 1.0.1  02/06/2021

void eXoCAN::begin(idtype addrType, int brp, BusType hw)
{
    bool alt, wire;
    bool pullUp = false;

    switch (hw)
    {
    case PORTA_11_12_XCVR:
        alt = false;  // default or alternate pins
        wire = false; // bus uses a xcvr chip
        break;
    case PORTB_8_9_XCVR:
        alt = true;
        wire = false;
        break;
    case PORTA_11_12_WIRE:
        alt = false;
        wire = true;
        break;
    case PORTB_8_9_WIRE:
        alt = true;
        wire = true;
        break;
    case PORTA_11_12_WIRE_PULLUP:
        alt = false;
        wire = true;
        pullUp = true;
        break;
    case PORTB_8_9_WIRE_PULLUP:
        alt = true;
        wire = true;
        pullUp = true;
        break;

    default:
        alt = false;
        wire = false;
        break;
    }

    begin(addrType, brp, wire, alt, pullUp);
}

void eXoCAN::begin(idtype addrType, int brp, bool singleWire, bool alt, bool pullup)
{
    uint8_t inp_float = 0b0100;
    uint8_t inp_pull = 0b1000;
    uint8_t alt_out = 0b1001;
    uint8_t alt_out_od = 0b1101;

    _extIDs = addrType;

    
    // set up CAN IO pins
    uint8_t swMode = singleWire ? alt_out_od : alt_out;
    uint8_t inputMode = pullup ? inp_pull : inp_float;

    if (alt)
    {
        MMIO32(apb2enr) |= (1 << 3) | (1 << 0); // enable gpioB = b3 and afio = b0 clks
        MMIO32(mapr) |= (2 << 13);              // alt func, CAN remap to B9+B8 
        MMIO32(crhB) &= 0xFFFFFF00;             // clear control bits for pins 8 & 9 of Port B
        MMIO32(crhB) |= inputMode;              // pin8 for rx, b0100 = b01xx, floating, bxx00 input
        periphBit(odrB, 8) = pullup;            // set input will pullup resistor for single wire with pullup mode
        MMIO32(crhB) |= swMode << 4;            // set output
    }
    else 
    {
        MMIO32(apb2enr) |= (1 << 2) | (1 << 0); // enable gpioA = b2 and afio = b0 clks
        MMIO32(mapr) &= 0xffff9fff;             // CAN map to default pins, PA11/12
        MMIO32(crhA) &= 0xFFF00FFF;             // clear control bits for pins 11 & 12 of Port A
        MMIO32(crhA) |= inputMode << 12;        // pin11 for rx, b0100 = b01xx, floating, bxx00 input
        periphBit(odrA, 11) = pullup;           //
        MMIO32(crhA) |= swMode << 16;           // set output
    }
    // set up CAN peripheral
    periphBit(rcc + 0x1C, 25) = 1;      // enable CAN1
    periphBit(mcr, 1) = 0;              // exit sleep
    MMIO32(mcr) |= (1 << 6) | (1 << 0); // set ABOM, init req (INRQ)
    while (periphBit(INAK) == 0)        // wait for hw ready
        ;
    MMIO32(btr) = (3 << 20) | (12 << 16) | (brp << 0); // 125K, 12/15=80% sample pt. prescale = 15
    // periphBit(ti0r, 2) = _extIDs;                      // 0 = std 11b ids, 1 = extended 29b ids
    periphBit(INRQ) = 0;                               // request init leave to Normal mode
    while (periphBit(INAK))                            // wait for hw
        ;
    filterMask16Init(0, 0, 0, 0, 0);                   // let all msgs pass to fifo0 by default
}

void eXoCAN::enableInterrupt()
{
    periphBit(ier, fmpie0) = 1U; // set fifo RX int enable request
    MMIO32(iser) = 1UL << 20;
}

void eXoCAN::disableInterrupt()
{
    periphBit(ier, fmpie0) = 0U;
    MMIO32(iser) = 1UL << 20;
}

void eXoCAN::filterMask16Init(int bank, int idA, int maskA, int idB, int maskB) // 16b mask filters
{
    filter16Init(bank, 0, idA, maskA, idB, maskB); // fltr 1,2 of flt bank n
}

void eXoCAN::filterList16Init(int bank, int idA, int idB, int idC, int idD) // 16b list filters
{
    filter16Init(bank, 1, idA, idB, idC, idD); // fltr 1,2,3,4 of flt bank n
}

void eXoCAN::filter16Init(int bank, int mode, int a, int b, int c, int d) // 16b filters
{
    periphBit(FINIT) = 1;                            // FINIT  'init' filter mode ]
    periphBit(fa1r, bank) = 0;                       // de-activate filter 'bank'
    periphBit(fs1r, bank) = 0;                       // fsc filter scale reg,  0 => 2ea. 16b
    periphBit(fm1r, bank) = mode;                    // fbm list mode = 1, 0 = mask
    MMIO32(fr1 + (8 * bank)) = (b << 21) | (a << 5); // fltr1,2 of flt bank n  OR  flt/mask 1 in mask mode
    MMIO32(fr2 + (8 * bank)) = (d << 21) | (c << 5); // fltr3,4 of flt bank n  OR  flt/mask 2 in mask mode
    periphBit(fa1r, bank) = 1;                       // activate this filter ]
    periphBit(FINIT) = 0;                            // ~FINIT  'active' filter mode ]
}

void eXoCAN::filterList32Init(int bank, int idA, int idB) //32b filters
{
    filter32Init(bank, 1, idA, idB);
}

void eXoCAN::filterMask32Init(int bank, int id, int mask) //32b filters
{
    filter32Init(bank, 0, id, mask);
}

void eXoCAN::filter32Init(int bank, int mode, int a, int b) //32b filters
{
    periphBit(FINIT) = 1;               // FINIT  'init' filter mode ]
    periphBit(fa1r, bank) = 0;          // de-activate filter 'bank'
    periphBit(fs1r, bank) = 1;          // fsc filter scale reg,  0 => 2ea. 16b,  1=>32b
    periphBit(fm1r, bank) = mode;       // fbm list mode = 1, 0 = mask
    MMIO32(fr1 + (8 * bank)) = a << 21; // the RXID/MASK to match ]
    MMIO32(fr2 + (8 * bank)) = b << 21; // must replace a mask of zeros so that everything isn't passed
    periphBit(fa1r, bank) = 1;          // activate this filter ]
    periphBit(FINIT) = 0;               // ~FINIT  'active' filter mode ]
}

//bool eXoCAN::transmit(int txId, const void *ptr, unsigned int len)
bool eXoCAN::transmit(int txId, const void *ptr, unsigned int len)
{
    //  uint32_t timeout = 10UL, startT = 0;
    // while (periphBit(tsr, 26) == 0) // tx not ready
    // {
    //     //     if(startT == 0)
    //     //         startT = millis();
    //     //     if((millis() - startT) > timeout)
    //     //     {
    //     //         Serial.println("time out");
    //     //         return false;
    //     //     }
    // }
    // TME0
    if (periphBit(tsr, 26) == 0) // tx mailbox 0 not ready)
        return false;

    if (_extIDs)
        MMIO32(ti0r) = (txId << 3)  + 0b100; // // set 29b extended ID.
    else
        MMIO32(ti0r) = (txId << 21) + 0b000; //12b std id

    MMIO32(tdt0r) = (len << 0);
    // this assumes that misaligned word access works
    MMIO32(tdl0r) = ((const uint32_t *)ptr)[0];
    MMIO32(tdh0r) = ((const uint32_t *)ptr)[1];

    periphBit(ti0r, 0) = 1; // tx request
    return true;
}

int eXoCAN::receive(volatile int &id, volatile int &fltrIdx, volatile uint8_t pData[])
{
    int len = -1;

    // rxMsgCnt = MMIO32(rf0r) & (3 << 0); //num of msgs
    // rxFull = MMIO32(rf0r) & (1 << 3);
    // rxOverflow = MMIO32(rf0r) & (1 << 4); // b4

    if (MMIO32(rf0r) & (3 << 0)) // num of msgs pending
    {
        _rxExtended = static_cast<idtype>((MMIO32(ri0r) & 1 << 2) >> 2);

        if (_rxExtended)
            id = (MMIO32(ri0r) >> 3); // extended id
        else
            id = (MMIO32(ri0r) >> 21);          // std id
        len = MMIO32(rdt0r) & 0x0F;             // fifo data len and time stamp
        fltrIdx = (MMIO32(rdt0r) >> 8) & 0xff;  // filter match index. Index accumalates from start of bank
        ((uint32_t *)pData)[0] = MMIO32(rdl0r); // 4 low rx bytes
        ((uint32_t *)pData)[1] = MMIO32(rdh0r); // another 4 bytes
        periphBit(rf0r, 5) = 1;                 // release the mailbox
    }
    return len;
}

void eXoCAN::attachInterrupt(void func()) // copy IRQ table to SRAM, point VTOR reg to it, set IRQ addr to user ISR
{
    static uint8_t newTbl[0xF0] __attribute__((aligned(0x100)));
    uint8_t *pNewTbl = newTbl;
    int origTbl = MMIO32(vtor);
    for (int j = 0; j < 0x3c; j++) // table length = 60 integers
        MMIO32((pNewTbl + (j << 2))) = MMIO32((origTbl + (j << 2)));

    uint32_t canVectTblAdr = reinterpret_cast<uint32_t>(pNewTbl) + (36 << 2); // calc new ISR addr in new vector tbl
    MMIO32(canVectTblAdr) = reinterpret_cast<uint32_t>(func);                 // set new CAN/USB ISR jump addr into new table
    MMIO32(vtor) = reinterpret_cast<uint32_t>(pNewTbl);                       // load vtor register with new tbl location
    enableInterrupt();
}

// void eXoCAN::attachInterrupt(void func()) // copy IRQ table to SRAM, point VTOR reg to it, set IRQ addr to user ISR
// {
//     static uint8_t xx[0xF0] __attribute__((aligned(0x100)));
//     uint8_t *px = xx;
//     int origTbl = MMIO32(vtor);
//     for (int j = 0; j < 0x3c; j++)
//         MMIO32((px + (j << 2))) = MMIO32((origTbl + (j << 2)));

//     uint32_t canVectTblAdr = (uint32_t)px + (36 << 2); // )USB_LP_CAN1_RX0_IRQn) + 16) << 2) isr addr location
//     MMIO32(canVectTblAdr) = (uint32_t)func;            // new vector table CAN/USB ISR jump addr
//     MMIO32(vtor) = (uint32_t)px;                       // put new location into vtor reg
// }