//-----------------------------------------------------------------------------
//
// si47xxWBRX.h
//
// Contains the function prototypes for the functions contained in si47xxWBRX.c
//
//-----------------------------------------------------------------------------
#ifndef _SI47XXWBRX_H_
#define _SI47XXWBRX_H_

//-----------------------------------------------------------------------------
// Globals
//-----------------------------------------------------------------------------
extern bit WaitSTCInterrupt;
extern bit PoweredUp;

//-----------------------------------------------------------------------------
// Function prototypes for si47xxWBRX.c
//-----------------------------------------------------------------------------
void si47xxWBRX_powerup(void);
void si47xxWBRX_powerdown(void);
void si47xxWBRX_initialize(void);
void si47xxWBRX_configure(void);
void si47xxWBRX_set_volume(u8 volume);
void si47xxWBRX_mute(u8 mute);
u8 si47xxWBRX_tune(u16 frequency);
u16 si47xxWBRX_get_frequency(void);
u8 si47xxWBRX_get_rssi(void);
u8 si47xxWBRX_get_alert(u8 intack, u8 *asqInts);
void wbSameStatus(u8 intack, u8 clrbuf, u8 readaddr);

//-----------------------------------------------------------------------------
// Function prototypes for si47xx_low.c
//-----------------------------------------------------------------------------
void si47xx_set_property(u16 propNumber, u16 propValue);
void si47xx_getPartInformation(void);

#endif
