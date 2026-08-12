//-----------------------------------------------------------------------------
//
// si47xxWBRX.c
//
// Contains the WB radio functions.
//
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
// Includes
//-----------------------------------------------------------------------------
#include <c8051f320.h>
#include <stddef.h>
#include "typedefs.h"
#include "portdef.h"
#include "propertydefs.h"
#include "commanddefs.h"
#include "si47xxWBRX.h"

//-----------------------------------------------------------------------------
// Defines
//-----------------------------------------------------------------------------
#define POWERUP_TIME 110    // Powerup delay in milliseconds

//-----------------------------------------------------------------------------
// Global variables
//-----------------------------------------------------------------------------
extern bit WaitSTCInterrupt;
extern bit PoweredUp;
extern bit SeekTuneInProc;
extern u8  idata cmd[8];
extern u8  idata rsp[15];
extern u8  chipFunction;

// This variables are used by the status commands.  Make sure to call those
// commands (wbRsqStatus, wbTuneStatus, wbAsqStatus, or wbSameStatus) prior to 
// access.
extern u8  xdata Status;
extern u8  xdata RsqInts;
extern u8  xdata STC;
extern u8  xdata SMUTE;
extern u8  xdata AFCRL;
extern u8  xdata Valid;
extern u16 xdata Freq;
extern u8  xdata RSSI;
extern u8  xdata ASNR;
extern u16 xdata AntCap;
extern u8  xdata FreqOff;
extern u8  xdata AsqInts;
extern u8  xdata Alert;
u8 xdata SameInts;
u8 xdata SameState;
u8 xdata SameMsgLen;
u8 xdata SameConf[8];
u8 xdata SameData[8];

//-----------------------------------------------------------------------------
// Function prototypes
//-----------------------------------------------------------------------------
void wait_ms(u16 ms);
void si47xx_command(u8 cmd_size, u8 idata *cmd, u8 reply_size, u8 idata *reply);
void si47xx_reset(void);
u8 getIntStatus(void);
void wbTuneFreq(u16 frequency);
static void wbTuneStatus(u8 intack);
static void wbRsqStatus(u8 intack);
static void wbAsqStatus(u8 intack);

//-----------------------------------------------------------------------------
// Take the Si47xx out of powerdown mode.
//-----------------------------------------------------------------------------
void si47xxWBRX_powerup(void)
{

    // Check if the device is already powered up.
    if (PoweredUp) {
    } else {
        // Put the ID for the command in the first byte.
        cmd[0] = POWER_UP;

		// Enable the GPO2OEN on the part.
        cmd[1] = POWER_UP_IN_GPO2OEN;

		// The device is being powered up in WB RX mode.
        cmd[1] |= POWER_UP_IN_FUNC_WBRX;

		// The opmode needs to be set to analog mode
        cmd[2] = POWER_UP_IN_OPMODE_RX_ANALOG;

        // Powerup the device
		si47xx_command(3, cmd, 8, rsp);
        wait_ms(POWERUP_TIME);               // wait for si47xx to powerup

        // Since we did not boot the part in query mode the result will not
        // contain the part information.

		PoweredUp = 1;
    }
}

//-----------------------------------------------------------------------------
// Place the Si47xx into powerdown mode.
//-----------------------------------------------------------------------------
void si47xxWBRX_powerdown(void)
{

	// Check to see if the device is powered up.  If not do not do anything.
    if(PoweredUp)
    {   
        // Set the powered up variable to 0
        PoweredUp = 0;

	    // Put the ID for the command in the first byte.
	    cmd[0] = POWER_DOWN;

	    // Invoke the command
		si47xx_command(1, cmd, 1, rsp);
    }
}

//-----------------------------------------------------------------------------
// This function will set up some general items on the hardware like
// initializing the STC interrupt.
//
//-----------------------------------------------------------------------------
static void si47xxWBRX_hardware_cfg(void)
{
	// Enable the STC, SAME, and Alert Tone interrupts.
    si47xx_set_property(GPO_IEN, GPO_IEN_STCIEN_MASK | 
								 GPO_IEN_SAMEIEN_MASK |
								 GPO_IEN_ASQIEN_MASK);
}

//-----------------------------------------------------------------------------
// Set up general configuration properties:
//      Valid SNR and RSSI thresholds
//-----------------------------------------------------------------------------
static void si47xxWBRX_general_cfg(void)
{
    //Typically the settings used for determining if a channel is valid are
	//adjusted by the designer and not exposed to the end user. They should
	//be adjusted here.
    
    // The max tune error is normally left in its default state.  The designer
    // can change if desired.
    //  si47xx_set_property(WB_MAX_TUNE_ERROR, 8);
 
    si47xx_set_property(WB_VALID_SNR_THRESHOLD, 3);
    si47xx_set_property(WB_VALID_RSSI_THRESHOLD, 20);
}


//-----------------------------------------------------------------------------
// Configures the device for normal operation
//-----------------------------------------------------------------------------
void si47xxWBRX_configure(void)
{
    // Configure all other registers
    si47xxWBRX_hardware_cfg();
    si47xxWBRX_general_cfg();
    
	// Turn on the Headphone Amp and analog out.
	M_INPUT_AD = 1;
	M_OUTPUT_AD = 0;
	GP1 = 1;
}

//-----------------------------------------------------------------------------
// Resets the part and initializes registers to the point of being ready for
// the first tune or seek.
//-----------------------------------------------------------------------------
void si47xxWBRX_initialize(void)
{
    // Zero status registers.
	PoweredUp = 0;

    // Perform a hardware reset, power up the device, and then perform the
    // initial configuration.
    si47xx_reset();
    si47xxWBRX_powerup();
    si47xxWBRX_configure();
}

//-----------------------------------------------------------------------------
// Set the volume and mute/unmute status
//
// Inputs:
//      volume:    a 6-bit volume value
//
// Note: It is assumed that if the volume is being adjusted, the device should
// not be muted.
//-----------------------------------------------------------------------------
void si47xxWBRX_set_volume(u8 volume)
{
    // Turn off the mute
    si47xx_set_property(RX_HARD_MUTE, 0);

    // Set the volume to the passed value
    si47xx_set_property(RX_VOLUME, (u16)volume & RX_VOLUME_MASK);
}

//-----------------------------------------------------------------------------
// Mute/unmute audio
//
// Inputs:
//      mute:  0 = output enabled (mute disabled)
//             1 = output muted
//-----------------------------------------------------------------------------
void si47xxWBRX_mute(u8 mute)
{
    if(mute)
    	si47xx_set_property(RX_HARD_MUTE, 
                                RX_HARD_MUTE_RMUTE_MASK | RX_HARD_MUTE_LMUTE_MASK);
    else
    	si47xx_set_property(RX_HARD_MUTE, 0);
}

//-----------------------------------------------------------------------------
// Tunes to a station number using the current band and spacing settings.
//
// Inputs:
//      frequency:  frequency between 162.4MHz and 162.55MHz in 2.5kHz units. 
//For example 162.4MHz MHz = 64960 and 162.55MHz = 65020
//
// Returns:
//      The RSSI level found during the tune.
//-----------------------------------------------------------------------------
u8 si47xxWBRX_tune(u16 frequency)
{
	// Enable the bit used for the interrupt of STC.
	SeekTuneInProc = 1;

	// Call the tune command to start the tune.
 	WaitSTCInterrupt = 1;
    wbTuneFreq(frequency);

    // wait for the interrupt before continuing
    // If you do not wish to use interrupts but wish to poll the part
    // then comment out this line.
    while (WaitSTCInterrupt); // Wait for interrupt to clear the bit

    // Wait for stc bit to be set
    while (!(getIntStatus() & STCINT));

	// Clear the STC bit and get the results of the tune.
    wbTuneStatus(1);

	// Disable the bit used for the interrupt of STC.
	SeekTuneInProc = 0;

    // Return the RSSI level
    return RSSI;
}


//-----------------------------------------------------------------------------
// Returns the current tuned frequency of the part
//
// Returns:
//      frequency between 162.4MHz and 162.55MHz in 2.5kHz units. 
//For example 162.4MHz MHz = 64960 and 162.55MHz = 65020
//-----------------------------------------------------------------------------
u16 si47xxWBRX_get_frequency()
{
	// Get the tune status which contains the current frequency
    wbTuneStatus(0);

    // Return the frequency
    return Freq;
}

//-----------------------------------------------------------------------------
// Returns the current tuned frequency of the part
//
// Returns:
//      RSSI in dBuV
//-----------------------------------------------------------------------------
u8 si47xxWBRX_get_rssi()
{
	// Get the tune status which contains the current frequency
	wbRsqStatus(0);

    // Return the RSSI level
    return RSSI;
}

//-----------------------------------------------------------------------------
// Returns Returns status information about the 1050kHz alert tone. 
//
// Returns:
//      1050kHz alert tone status
//-----------------------------------------------------------------------------
u8 si47xxWBRX_get_alert(u8 intack, u8 *asqInts)
{
	// Get the tune status which contains the current frequency
	wbAsqStatus(intack);

	// Put the asqInterrupt status if required.
	if(asqInts != 0)
		*asqInts = AsqInts;

    // Return the alert status
    return Alert;
}

//-----------------------------------------------------------------------------
// Helper function that sends the WB_TUNE_FREQ command to the part
//
// Inputs:
// 	frequency between 162.4MHz and 162.55MHz in 2.5kHz units. 
//For example 162.4MHz MHz = 64960 and 162.55MHz = 65020
//-----------------------------------------------------------------------------
static void wbTuneFreq(u16 frequency)
{
    // Put the ID for the command in the first byte.
    cmd[0] = WB_TUNE_FREQ;

	// Initialize the reserved section to 0
    cmd[1] = 0;

	// Put the frequency in the second and third bytes.
    cmd[2] = (u8)(frequency >> 8);
	cmd[3] = (u8)(frequency & 0x00FF);

	// Set the antenna calibration value.
    cmd[4] = (u8)0;  // Auto

    // Invoke the command
	si47xx_command(5, cmd, 1, rsp);
}



//-----------------------------------------------------------------------------
// Helper function that sends the WB_TUNE_STATUS command to the part
//
// Inputs:
//  intack: If non-zero the interrupt for STCINT will be cleared.
//
// Outputs:  // These are global variables and are set by this method
//  STC:    The seek/tune is complete
//  AFCRL:  The AFC is railed if this is non-zero
//  Valid:  The station is valid if this is non-zero
//  Freq:   The current frequency
//  RSSI:   The RSSI level read at tune.
//  ASNR:   The audio SNR level read at tune.
//  AntCap: The current level of the tuning capacitor.
//-----------------------------------------------------------------------------
static void wbTuneStatus(u8 intack)
{
    // Put the ID for the command in the first byte.
    cmd[0] = WB_TUNE_STATUS;

	// Put the flags if the bit was set for the input parameters.
	cmd[1] = 0;
    if(intack) cmd[1] |= WB_TUNE_STATUS_IN_INTACK;

    // Invoke the command
	si47xx_command(2, cmd, 8, rsp);

    // Parse the results
    STC    = !!(rsp[0] & STCINT);
    AFCRL  = !!(rsp[1] & WB_TUNE_STATUS_OUT_AFCRL);
    Valid  = !!(rsp[1] & WB_TUNE_STATUS_OUT_VALID);
    Freq   = ((u16)rsp[2] << 8) | (u16)rsp[3];
    RSSI   = rsp[4];
    ASNR   = rsp[5];
    AntCap = rsp[7];   
}

//-----------------------------------------------------------------------------
// Helper function that sends the WB_RSQ_STATUS command to the part
//
// Inputs:
//  intack: If non-zero the interrupt for STCINT will be cleared.
//
// Outputs:
//  Status:  Contains bits about the status returned from the part.
//  RsqInts: Contains bits about the interrupts that have fired related to RSQ.
//  AFCRL:   The AFC is railed if this is non-zero
//  Valid:   The station is valid if this is non-zero
//  RSSI:    The RSSI level read at tune.
//  ASNR:    The audio SNR level read at tune.
//  FreqOff: The frequency offset in kHz of the current station from the tuned 
//           frequency.
//-----------------------------------------------------------------------------
static void wbRsqStatus(u8 intack)
{
    // Put the ID for the command in the first byte.
    cmd[0] = WB_RSQ_STATUS;

	// Put the flags if the bit was set for the input parameters.
	cmd[1] = 0;
	if(intack) cmd[1] |= WB_RSQ_STATUS_IN_INTACK;

    // Invoke the command
	si47xx_command(2, cmd, 8, rsp);

    // Parse the results
	Status  = rsp[0];
    RsqInts = rsp[1];
    AFCRL   = !!(rsp[2] & WB_RSQ_STATUS_OUT_AFCRL);
    Valid   = !!(rsp[2] & WB_RSQ_STATUS_OUT_VALID);
    RSSI    = rsp[4];
    ASNR    = rsp[5];
    FreqOff = rsp[7];   
}

//-----------------------------------------------------------------------------
// Helper function that sends the WB_ASQ_STATUS command to the part
//
// Inputs:
//  intack: If non-zero the interrupt for STCINT will be cleared.
//
// Outputs:
//  Status:  Contains bits about the status returned from the part.
//  AsqInts: Contains bits about the interrupts that have fired related to ASQ.
//  Alert:   The1050Hz alert tone is currently present if this is non-zero
//-----------------------------------------------------------------------------
static void wbAsqStatus(u8 intack)
{
    // Put the ID for the command in the first byte.
    cmd[0] = WB_ASQ_STATUS;

	// Put the flags if the bit was set for the input parameters.
	cmd[1] = 0;
	if(intack) cmd[1] |= WB_ASQ_STATUS_IN_INTACK;

    // Invoke the command
	si47xx_command(2, cmd, 3, rsp);

    // Parse the results
	Status  = rsp[0];
    AsqInts = rsp[1];
    Alert   = !!(rsp[2] & WB_ASQ_STATUS_OUT_ALERT);
    
}

void wbSameStatus(u8 intack, u8 clrbuf, u8 readaddr)
{
	// Put the ID for the command in the first byte.
	cmd[0] = WB_SAME_STATUS;

	// Put the flags for clear buffer and intack if necessary.
	cmd[1] = 0;
	if(intack) cmd[1] |= WB_SAME_STATUS_IN_INTACK;
	if(clrbuf) cmd[1] |= WB_SAME_STATUS_IN_CLRBUF;

	// Write the readaddress to the third byte.
	cmd[2] = readaddr;

	// Invoke the command
	si47xx_command(3, cmd, 14, rsp);

	// Parse the results
	Status      = rsp[0];
	SameInts    = rsp[1];
	SameState   = rsp[2];
	SameMsgLen  = rsp[3];
	SameConf[0] = (rsp[5] & WB_SAME_STATUS_OUT_CONF0) >> WB_SAME_STATUS_OUT_CONF0_SHFT;
	SameConf[1] = (rsp[5] & WB_SAME_STATUS_OUT_CONF1) >> WB_SAME_STATUS_OUT_CONF1_SHFT;
	SameConf[2] = (rsp[5] & WB_SAME_STATUS_OUT_CONF2) >> WB_SAME_STATUS_OUT_CONF2_SHFT;
	SameConf[3] = (rsp[5] & WB_SAME_STATUS_OUT_CONF3) >> WB_SAME_STATUS_OUT_CONF3_SHFT;
	SameConf[4] = (rsp[4] & WB_SAME_STATUS_OUT_CONF4) >> WB_SAME_STATUS_OUT_CONF4_SHFT;
	SameConf[5] = (rsp[4] & WB_SAME_STATUS_OUT_CONF5) >> WB_SAME_STATUS_OUT_CONF5_SHFT;
	SameConf[6] = (rsp[4] & WB_SAME_STATUS_OUT_CONF6) >> WB_SAME_STATUS_OUT_CONF6_SHFT;
	SameConf[7] = (rsp[4] & WB_SAME_STATUS_OUT_CONF7) >> WB_SAME_STATUS_OUT_CONF7_SHFT;
	SameData[0] = rsp[6];
	SameData[1] = rsp[7];
	SameData[2] = rsp[8];
	SameData[3] = rsp[9];
	SameData[4] = rsp[10];
	SameData[5] = rsp[11];
	SameData[6] = rsp[12];
	SameData[7] = rsp[13];
}
