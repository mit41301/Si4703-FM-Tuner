//-----------------------------------------------------------------------------
//
// si47xxFMTX.h
//
// Contains the function prototypes for the functions contained in si47xxFMRX.c
//
//-----------------------------------------------------------------------------
#ifndef _SI47XXFMTX_H_
#define _SI47XXFMTX_H_

//-----------------------------------------------------------------------------
// Globals
//-----------------------------------------------------------------------------
extern bit WaitSTCInterrupt;
extern bit PoweredUp;

//-----------------------------------------------------------------------------
// Function prototypes for si47xxFMTX.c
//-----------------------------------------------------------------------------
void si47xxFMTX_powerup(void);
void si47xxFMTX_powerdown(void);
void si47xxFMTX_initialize(void);
void si47xxFMTX_configure(void);
void si47xxFMTX_tune(u16 frequency);
void si47xxFMTX_tunePower(u8 voltage);
void si47xxFMTX_tuneMeasure(u16 frequency);
u8 si47xxFMTX_get_rnl(void);
void txAsqStatus(u8 intack);
void txRdsPs(u8 id, u8 char0, u8 char1, u8 char2, u8 char3);
void txRdsBuff(bit intack, bit mtbuff, bit ldbuff, bit fifo,
			   u16 rdsb, u16 rdsc, u16 rdsd);

//-----------------------------------------------------------------------------
// Function prototypes for FMTXScan.c
//-----------------------------------------------------------------------------
u8 si47xxFMTX_scan(void);

//-----------------------------------------------------------------------------
// Function prototypes for si47xx_low.c
//-----------------------------------------------------------------------------
void si47xx_set_property(u16 propNumber, u16 propValue);
void si47xx_getPartInformation(void);

#endif
