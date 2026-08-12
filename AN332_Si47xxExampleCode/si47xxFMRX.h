//-----------------------------------------------------------------------------
//
// si47xxFMRX.h
//
// Contains the function prototypes for the functions contained in si47xxFMRX.c
//
//-----------------------------------------------------------------------------
#ifndef _SI47XXFMRX_H_
#define _SI47XXFMRX_H_

//-----------------------------------------------------------------------------
// Globals
//-----------------------------------------------------------------------------
extern bit WaitSTCInterrupt;
extern bit PoweredUp;

//-----------------------------------------------------------------------------
// Function prototypes for si47xxFMRX.c
//-----------------------------------------------------------------------------
void si47xxFMRX_powerup(void);
void si47xxFMRX_powerdown(void);
void si47xxFMRX_initialize(void);
void si47xxFMRX_configure(void);
void si47xxFMRX_set_volume(u8 volume);
void si47xxFMRX_mute(u8 mute);
u8 si47xxFMRX_tune(u16 frequency);
u8 si47xxFMRX_seek(u8 seekup, u8 seekmode);
u16 si47xxFMRX_get_frequency(void);
u8 si47xxFMRX_get_rssi(void);
u8 quickAFTune(u16 freq);
void fmRdsStatus(u8 intack, u8 mtfifo);

//-----------------------------------------------------------------------------
// Function prototypes for autoseek.c
//-----------------------------------------------------------------------------
u8 si47xxFMRX_autoseek(void);

//-----------------------------------------------------------------------------
// Function prototypes for si47xx_low.c
//-----------------------------------------------------------------------------
void si47xx_set_property(u16 propNumber, u16 propValue);
void si47xx_getPartInformation(void);

#endif
