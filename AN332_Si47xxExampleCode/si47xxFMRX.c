//-----------------------------------------------------------------------------
//
// si47xxFMRX.c
//
// Contains the FM radio functions with the exceptions of autoseek and rds.
//
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
// Includes
//-----------------------------------------------------------------------------
#include <c8051f320.h>
#include <stddef.h>
#include "typedefs.h"
#include "portdef.h"
#include "commanddefs.h"
#include "propertydefs.h"
#include "si47xxFMRX.h"

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
// commands (fmRsqStatus, fmTuneStatus, or fmRdsStatus) prior to access.
extern u8  xdata Status;
extern u8  xdata RsqInts;
extern u8  xdata STC;
extern u8  xdata SMUTE;
extern u8  xdata BLTF;
extern u8  xdata AFCRL;
extern u8  xdata Valid;
extern u8  xdata Pilot;
extern u8  xdata Blend;
extern u16 xdata Freq;
extern u8  xdata RSSI;
extern u8  xdata ASNR;
extern u16 xdata AntCap;
extern u8  xdata FreqOff;
u8  xdata RdsInts;
u8  xdata RdsSync;
u8  xdata GrpLost;
u8  xdata RdsFifoUsed;
u16 xdata BlockA;
u16 xdata BlockB;
u16 xdata BlockC;
u16 xdata BlockD;
u8  xdata BleA;
u8  xdata BleB;
u8  xdata BleC;
u8  xdata BleD;



typedef enum {USA, EUROPE, JAPAN} country_enum; // Could be expanded

//-----------------------------------------------------------------------------
// Function prototypes
//-----------------------------------------------------------------------------
void wait_ms(u16 ms);
void si47xx_command(u8 cmd_size, u8 idata *cmd, u8 reply_size, u8 idata *reply);
void si47xx_reset(void);
u8 getIntStatus(void);
void fmTuneFreq(u16 frequency);
static void fmSeekStart(u8 seekUp, u8 wrap);
static void fmTuneStatus(u8 cancel, u8 intack);
static void fmRsqStatus(u8 intack);

//-----------------------------------------------------------------------------
// Take the Si47xx out of powerdown mode.
//-----------------------------------------------------------------------------
void si47xxFMRX_powerup(void)
{

    // Check if the device is already powered up.
    if (PoweredUp) {
    } else {
        // Put the ID for the command in the first byte.
        cmd[0] = POWER_UP;

		// Enable the GPO2OEN on the part because it will be used to determine
        // RDS Sync timing.
        cmd[1] = POWER_UP_IN_GPO2OEN;

		// The device is being powered up in FM RX mode.
        cmd[1] |= POWER_UP_IN_FUNC_FMRX;

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
void si47xxFMRX_powerdown(void)
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
// initializing the RDS and STC interrupts.
//
// Note:
//     * RDS is only available on certain parts.  Please refer to the data
//       sheet for your part to determine if your part supports RDS.
//-----------------------------------------------------------------------------
static void si47xxFMRX_hardware_cfg(void)
{
	// Enable the RDS and STC interrupt here
    si47xx_set_property(GPO_IEN, GPO_IEN_STCIEN_MASK | GPO_IEN_RDSIEN_MASK);
}

//-----------------------------------------------------------------------------
// Set up general configuration properties:
//      Soft Mute Rate, Soft Mute Max Attenuation, Soft Mute SNR Threshold,
//      Blend Mono Threshold, Blend Stereo Threshold, Max Tune Error,
//      Seek Tune SNR Threshold, Seek Tune RSSI Threshold
//
// Note:
//     * RDS is only available on certain parts.  Please refer to the data
//       sheet for your part to determine if your part supports RDS.
//-----------------------------------------------------------------------------
static void si47xxFMRX_general_cfg(void)
{
    // Typically the settings used for stereo blend are determined by the 
    // designer and not exposed to the end user. They should be adjusted here.
    // If the user wishes to force mono set both of these values to 127.
    // si47xx_set_property(FM_BLEND_MONO_THRESHOLD, 30);
    // si47xx_set_property(FM_BLEND_STEREO_THRESHOLD, 49);

    // The softmute feature can be disabled, but it is normally left on.
    // The softmute feature is disabled by setting the attenuation property
    // to zero.
    //  si47xx_set_property(FM_SOFT_MUTE_RATE, 64);
    //  si47xx_set_property(FM_SOFT_MUTE_MAX_ATTENUATION, 16);
    //  si47xx_set_property(FM_SOFT_MUTE_SNR_THRESHOLD, 4);

    // The max tune error is normally left in its default state.  The designer
    // can change if desired.
    //  si47xx_set_property(FM_MAX_TUNE_ERROR, 30);
 
    // Typically the settings used for seek are determined by the designer
    // and not exposed to the end user. They should be adjusted here.
    si47xx_set_property(FM_SEEK_TUNE_SNR_THRESHOLD, 3);
    si47xx_set_property(FM_SEEK_TUNE_RSSI_THRESHOLD, 20);
}

//-----------------------------------------------------------------------------
// Set up regional configuration properties including:
//      Seek Band Bottom, Seek Band Top, Seek Freq Spacing, Deemphasis
//
// Inputs:
//     country
//
// Note:
//     * RDS is only available on certain parts.  Please see the part's
//       datasheet for more information.
//-----------------------------------------------------------------------------
static void si47xxFMRX_regional_cfg(country_enum country)
{
    // Typically the settings used for stereo blend are determined by the 
    // designer and not exposed to the end user. They should be adjusted here.
    // If the user wishes to force mono set both of these values to 127.
    // si47xx_set_property(FM_BLEND_MONO_THRESHOLD, 30);
    // si47xx_set_property(FM_BLEND_STEREO_THRESHOLD, 49);

    // Depending on the country, set the de-emphasis, band, and space settings
    // Also optionally enable RDS for countries that support it
    switch (country) {
    case USA :
        // This interrupt will be used to determine when RDS is available.
        si47xx_set_property(FM_RDS_INTERRUPT_SOURCE, 
					FM_RDS_INTERRUPT_SOURCE_SYNCFOUND_MASK); // RDS Interrupt

		// Enable the RDS and allow all blocks so we can compute the error
        // rate later.
        si47xx_set_property(FM_RDS_CONFIG, FM_RDS_CONFIG_RDSEN_MASK |
			(3 << FM_RDS_CONFIG_BLETHA_SHFT) |
			(3 << FM_RDS_CONFIG_BLETHB_SHFT) |
			(3 << FM_RDS_CONFIG_BLETHC_SHFT) |
			(3 << FM_RDS_CONFIG_BLETHD_SHFT));

        si47xx_set_property(FM_DEEMPHASIS, FM_DEEMPH_75US); // Deemphasis
        // Band is already set to 87.5-107.9MHz (US)
        // Space is already set to 200kHz (US)
        break;
    case JAPAN :
        si47xx_set_property(FM_RDS_CONFIG, 0);              // Disable RDS
        si47xx_set_property(FM_DEEMPHASIS, FM_DEEMPH_50US); // Deemphasis
        si47xx_set_property(FM_SEEK_BAND_BOTTOM, 7600);     // 76 MHz Bottom
        si47xx_set_property(FM_SEEK_BAND_TOP, 9000);        // 90 MHz Top
        si47xx_set_property(FM_SEEK_FREQ_SPACING, 10);      // 100 kHz Spacing
        break;
    case EUROPE :
    default:
        // This interrupt will be used to determine when RDS is available.
        si47xx_set_property(FM_RDS_INTERRUPT_SOURCE, 
			FM_RDS_INTERRUPT_SOURCE_SYNCFOUND_MASK); // RDS Interrupt

	    // Enable the RDS and allow all blocks so we can compute the error
        // rate later.
        si47xx_set_property(FM_RDS_CONFIG, FM_RDS_CONFIG_RDSEN_MASK |
		    (3 << FM_RDS_CONFIG_BLETHA_SHFT) |
			(3 << FM_RDS_CONFIG_BLETHB_SHFT) |
			(3 << FM_RDS_CONFIG_BLETHC_SHFT) |
			(3 << FM_RDS_CONFIG_BLETHD_SHFT));

        si47xx_set_property(FM_DEEMPHASIS, FM_DEEMPH_50US); // Deemphasis
        // Band is already set to 87.5-107.9MHz (Europe)
        si47xx_set_property(FM_SEEK_FREQ_SPACING, 10);      // 100 kHz Spacing
        break;
    }
}

//-----------------------------------------------------------------------------
// Configures the device for normal operation
//-----------------------------------------------------------------------------
void si47xxFMRX_configure(void)
{
    // Configure all other registers
    si47xxFMRX_hardware_cfg();
    si47xxFMRX_general_cfg();
    si47xxFMRX_regional_cfg(USA);

	// Turn on the Headphone Amp and analog out.
	M_INPUT_AD = 1;
	M_OUTPUT_AD = 0;
	GP1 = 1;
}

//-----------------------------------------------------------------------------
// Resets the part and initializes registers to the point of being ready for
// the first tune or seek.
//-----------------------------------------------------------------------------
void si47xxFMRX_initialize(void)
{
    // Zero status registers.
	PoweredUp = 0;

    // Perform a hardware reset, power up the device, and then perform the
    // initial configuration.
    si47xx_reset();
    si47xxFMRX_powerup();
    si47xxFMRX_configure();
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
void si47xxFMRX_set_volume(u8 volume)
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
void si47xxFMRX_mute(u8 mute)
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
//      frequency:  frequency in 10kHz steps
//
// Returns:
//      The RSSI level found during the tune.
//-----------------------------------------------------------------------------
u8 si47xxFMRX_tune(u16 frequency)
{
	// Enable the bit used for the interrupt of STC.
	SeekTuneInProc = 1;

	// Call the tune command to start the tune.
 	WaitSTCInterrupt = 1;
    fmTuneFreq(frequency);

    // wait for the interrupt before continuing
    // If you do not wish to use interrupts but wish to poll the part
    // then comment out this line.
    while (WaitSTCInterrupt); // Wait for interrupt to clear the bit

    // Wait for stc bit to be set
    while (!(getIntStatus() & STCINT));

	// Clear the STC bit and get the results of the tune.
    fmTuneStatus(0, 1);

	// Disable the bit used for the interrupt of STC.
	SeekTuneInProc = 0;

    // Return the RSSI level
    return RSSI;
}

//-----------------------------------------------------------------------------
// Inputs:
//      seekup:  0 = seek down
//               1 = seek up
//      seekmode: 0 = wrap at band limits
//                1 = stop at band limits
// Outputs:
//      zero = seek found a station
//      nonzero = seek did not find a station
//-----------------------------------------------------------------------------
u8 si47xxFMRX_seek(u8 seekup, u8 seekmode)
{
	// Enable the bit used for the interrupt of STC.
	SeekTuneInProc = 1;

	// Call the tune command to start the seek.
 	WaitSTCInterrupt = 1;
    fmSeekStart(seekup, !seekmode);

    // wait for the interrupt before continuing
    // If you do not wish to use interrupts but wish to poll the part
    // then comment out these two lines.
    while (WaitSTCInterrupt); // Wait for interrupt to clear the bit

    // Wait for stc bit to be set
    // If there is a display to update seek progress, then you could
    // call fmTuneStatus in this loop to get the current frequency.
    // When calling fmTuneStatus here make sure intack is zero.
    while (!(getIntStatus() & STCINT));

	// Clear the STC bit and get the results of the tune.
    fmTuneStatus(0, 1);

	// Disable the bit used for the interrupt of STC.
	SeekTuneInProc = 0;

    // The tuner is now set to the newly found channel if one was available
    // as indicated by the seek-fail bit.
    return BLTF; //return seek fail indicator
}

//-----------------------------------------------------------------------------
// Returns the current tuned frequency of the part
//
// Returns:
//      frequency in 10kHz steps
//-----------------------------------------------------------------------------
u16 si47xxFMRX_get_frequency()
{
	// Get the tune status which contains the current frequency
    fmTuneStatus(0, 0);

    // Return the frequency
    return Freq;
}

//-----------------------------------------------------------------------------
// Returns the current tuned frequency of the part
//
// Returns:
//      frequency in 10kHz steps
//-----------------------------------------------------------------------------
u8 si47xxFMRX_get_rssi()
{
	// Get the tune status which contains the current frequency
	fmRsqStatus(0);

    // Return the RSSI level
    return RSSI;
}

//-----------------------------------------------------------------------------
// Quickly tunes to the passed frequency, checks the power level and snr, 
// and returns
//
// Inputs:
//  Channel number in 10kHz steps
//
// Output:
//  The RSSI level after tune
//-----------------------------------------------------------------------------
u8 quickAFTune(u16 freq)
{
	u16 current_freq = 0;
	u8  current_rssi = 0;

	// Get the current frequency from the part
    fmTuneStatus(0, 0);
	current_freq = Freq;

    // Tune to the AF frequency, check the RSSI, tune back
    current_rssi = si47xxFMRX_tune(freq);

    // Return to the original channel
    si47xxFMRX_tune(current_freq);
    return current_rssi;
}


//-----------------------------------------------------------------------------
// Helper function that sends the FM_TUNE_FREQ command to the part
//
// Inputs:
// 	frequency in 10kHz steps
//-----------------------------------------------------------------------------
static void fmTuneFreq(u16 frequency)
{
    // Put the ID for the command in the first byte.
    cmd[0] = FM_TUNE_FREQ;

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
// Helper function that sends the FM_SEEK_START command to the part
//
// Inputs:
// 	seekUp: If non-zero seek will increment otherwise decrement
//  wrap:   If non-zero seek will wrap around band limits when hitting the end
//          of the band limit.
//-----------------------------------------------------------------------------
static void fmSeekStart(u8 seekUp, u8 wrap)
{
    // Put the ID for the command in the first byte.
    cmd[0] = FM_SEEK_START;

	// Put the flags if the bit was set for the input parameters.
	cmd[1] = 0;
    if(seekUp) cmd[1] |= FM_SEEK_START_IN_SEEKUP;
	if(wrap)   cmd[1] |= FM_SEEK_START_IN_WRAP;

    // Invoke the command
	si47xx_command(2, cmd, 1, rsp);
}

//-----------------------------------------------------------------------------
// Helper function that sends the FM_TUNE_STATUS command to the part
//
// Inputs:
// 	cancel: If non-zero the current seek will be cancelled.
//  intack: If non-zero the interrupt for STCINT will be cleared.
//
// Outputs:  // These are global variables and are set by this method
//  STC:    The seek/tune is complete
//  BLTF:   The seek reached the band limit or original start frequency
//  AFCRL:  The AFC is railed if this is non-zero
//  Valid:  The station is valid if this is non-zero
//  Freq:   The current frequency
//  RSSI:   The RSSI level read at tune.
//  ASNR:   The audio SNR level read at tune.
//  AntCap: The current level of the tuning capacitor.
//-----------------------------------------------------------------------------
static void fmTuneStatus(u8 cancel, u8 intack)
{
    // Put the ID for the command in the first byte.
    cmd[0] = FM_TUNE_STATUS;

	// Put the flags if the bit was set for the input parameters.
	cmd[1] = 0;
    if(cancel) cmd[1] |= FM_TUNE_STATUS_IN_CANCEL;
	if(intack) cmd[1] |= FM_TUNE_STATUS_IN_INTACK;

    // Invoke the command
	si47xx_command(2, cmd, 8, rsp);

    // Parse the results
    STC    = !!(rsp[0] & STCINT);
    BLTF   = !!(rsp[1] & FM_TUNE_STATUS_OUT_BTLF);
    AFCRL  = !!(rsp[1] & FM_TUNE_STATUS_OUT_AFCRL);
    Valid  = !!(rsp[1] & FM_TUNE_STATUS_OUT_VALID);
    Freq   = ((u16)rsp[2] << 8) | (u16)rsp[3];
    RSSI   = rsp[4];
    ASNR   = rsp[5];
    AntCap = rsp[7];   
}

//-----------------------------------------------------------------------------
// Helper function that sends the FM_RSQ_STATUS command to the part
//
// Inputs:
//  intack: If non-zero the interrupt for STCINT will be cleared.
//
// Outputs:
//  Status:  Contains bits about the status returned from the part.
//  RsqInts: Contains bits about the interrupts that have fired related to RSQ.
//  SMUTE:   The soft mute function is currently enabled
//  AFCRL:   The AFC is railed if this is non-zero
//  Valid:   The station is valid if this is non-zero
//  Pilot:   A pilot tone is currently present
//  Blend:   Percentage of blend for stereo. (100 = full stereo)
//  RSSI:    The RSSI level read at tune.
//  ASNR:    The audio SNR level read at tune.
//  FreqOff: The frequency offset in kHz of the current station from the tuned 
//           frequency.
//-----------------------------------------------------------------------------
static void fmRsqStatus(u8 intack)
{
    // Put the ID for the command in the first byte.
    cmd[0] = FM_RSQ_STATUS;

	// Put the flags if the bit was set for the input parameters.
	cmd[1] = 0;
	if(intack) cmd[1] |= FM_RSQ_STATUS_IN_INTACK;

    // Invoke the command
	si47xx_command(2, cmd, 8, rsp);

    // Parse the results
	Status  = rsp[0];
    RsqInts = rsp[1];
    SMUTE   = !!(rsp[2] & FM_RSQ_STATUS_OUT_SMUTE);
    AFCRL   = !!(rsp[2] & FM_RSQ_STATUS_OUT_AFCRL);
    Valid   = !!(rsp[2] & FM_RSQ_STATUS_OUT_VALID);
    Pilot   = !!(rsp[3] & FM_RSQ_STATUS_OUT_PILOT);
    Blend   = rsp[3] & FM_RSQ_STATUS_OUT_STBLEND;
    RSSI    = rsp[4];
    ASNR    = rsp[5];
    FreqOff = rsp[7];   
}


//-----------------------------------------------------------------------------
// Helper function that sends the FM_RDS_STATUS command to the part
//
// Inputs:
//  intack: If non-zero the interrupt for STCINT will be cleared.
//  mtfifo: If non-zero the fifo will be cleared.
//
// Outputs:
//  Status:      Contains bits about the status returned from the part.
//  RdsInts:     Contains bits about the interrupts that have fired related to RDS.
//  RdsSync:     If non-zero the RDS is currently synced.
//  GrpLost:     If non-zero some RDS groups were lost.
//  RdsFifoUsed: The amount of groups currently remaining in the RDS fifo.
//  BlockA:      Block A group data from the oldest FIFO entry.
//  BlockB:      Block B group data from the oldest FIFO entry.
//  BlockC:      Block C group data from the oldest FIFO entry.
//  BlockD:      Block D group data from the oldest FIFO entry.
//  BleA:        Block A corrected error information.
//  BleB:        Block B corrected error information.
//  BleC:        Block C corrected error information.
//  BleD:        Block D corrected error information.
//-----------------------------------------------------------------------------
void fmRdsStatus(u8 intack, u8 mtfifo)
{
    // Put the ID for the command in the first byte.
    cmd[0] = FM_RDS_STATUS;

	// Put the flags if the bit was set for the input parameters.
	cmd[1] = 0;
	if(intack) cmd[1] |= FM_RDS_STATUS_IN_INTACK;
    if(mtfifo) cmd[1] |= FM_RDS_STATUS_IN_MTFIFO;

    // Invoke the command
	si47xx_command(2, cmd, 13, rsp);

    // Parse the results
	Status      = rsp[0];
    RdsInts     = rsp[1];
	RdsSync     = !!(rsp[2] & FM_RDS_STATUS_OUT_SYNC);
    GrpLost     = !!(rsp[2] & FM_RDS_STATUS_OUT_GRPLOST);
    RdsFifoUsed = rsp[3];
    BlockA      = ((u16)rsp[4] << 8) | (u16)rsp[5];
    BlockB      = ((u16)rsp[6] << 8) | (u16)rsp[7];
    BlockC      = ((u16)rsp[8] << 8) | (u16)rsp[9];
    BlockD      = ((u16)rsp[10] << 8) | (u16)rsp[11];
    BleA        = (rsp[12] & FM_RDS_STATUS_OUT_BLEA) >> FM_RDS_STATUS_OUT_BLEA_SHFT;
    BleB        = (rsp[12] & FM_RDS_STATUS_OUT_BLEB) >> FM_RDS_STATUS_OUT_BLEB_SHFT;
    BleC        = (rsp[12] & FM_RDS_STATUS_OUT_BLEC) >> FM_RDS_STATUS_OUT_BLEC_SHFT;
    BleD        = (rsp[12] & FM_RDS_STATUS_OUT_BLED) >> FM_RDS_STATUS_OUT_BLED_SHFT;
}

