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
#include "si47xxFMTX.h"

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


// The following global variables are populated by the txTuneStatus, txAsqStatus
// and txRdsBuff
extern u8  xdata Status;
extern u8  xdata STC;
extern u16 xdata Freq;
u8  xdata Voltage;
extern u16 xdata AntCap;
u8  xdata RNL;

u8 xdata OverMod;
u8 xdata IALH;
u8 xdata IALL;
u8 xdata InLevel;
u8 xdata RdsInt;
u8 xdata CbAvail;
u8 xdata CbUsed;
u8 xdata FifoAvail;
u8 xdata FifoUsed;

//-----------------------------------------------------------------------------
// Function prototypes
//-----------------------------------------------------------------------------
void wait_ms(u16 ms);
void si47xx_command(u8 cmd_size, u8 idata *cmd, u8 reply_size, u8 idata *reply);
void si47xx_reset(void);
u8 getIntStatus(void);
static void txTuneFreq(u16 frequency);
static void txTunePower(u8 voltage);
static void txTuneMeasure(u16 frequency);
static void txTuneStatus(u8 intack);
static void txRdsBuff(u8 inFlags, u16 rdsb, u16 rdsc, u16 rdsd);
static void txRdsPs(u8 psid, u8 pschar0, u8 pschar1, u8 pschar2, u8 pschar3);

//-----------------------------------------------------------------------------
// Take the Si47xx out of powerdown mode.
//-----------------------------------------------------------------------------
void si47xxFMTX_powerup(void)
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
        cmd[1] |= POWER_UP_IN_FUNC_FMTX;

		// The opmode needs to be set to analog mode
        cmd[2] = POWER_UP_IN_OPMODE_TX_ANALOG;

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
void si47xxFMTX_powerdown(void)
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
static void si47xxFMTX_hardware_cfg(void)
{
	// Enable the RDS and STC interrupt here
    si47xx_set_property(GPO_IEN, GPO_IEN_STCIEN_MASK);
}

//-----------------------------------------------------------------------------
// Set up general configuration properties:
//      Component Enable, Audio Deviation, Pilot Deviation, Rds Deviation,
//      Line Input Level, Preemphasis, Pilot Frequency, Acomp Properties,
//      and Limiter Properties.
//
// Note:
//     * RDS is only available on certain parts.  Please refer to the data
//       sheet for your part to determine if your part supports RDS.
//-----------------------------------------------------------------------------
static void si47xxFMTX_general_cfg(void)
{
    // Enable all signals on the part.  This defaults to 3 which is just
    // the pilot and the L-R signals.  If you have an RDS part then also
    // enable that as well.
    si47xx_set_property(TX_COMPONENT_ENABLE, TX_COMPONENT_ENABLE_PILOT_MASK |
											 TX_COMPONENT_ENABLE_LMR_MASK |
											 TX_COMPONENT_ENABLE_RDS_MASK);

	// The following properties setup the deviation of the different components
	si47xx_set_property(TX_AUDIO_DEVIATION, 6550);
	si47xx_set_property(TX_PILOT_DEVIATION,  750);
	si47xx_set_property(TX_RDS_DEVIATION,    200);

	// The following determines the level of the input audio signal.  Make
    // sure to set the attenuator according to the input level setting.
    // More information can be found in the programming guide.
    // si47xx_set_property(TX_LINE_INPUT_LEVEL, TX_LINE_INPUT_LEVEL_LIATTEN_60kOhm |
	//										 636);

	// Set the preemphasis to 75us
	si47xx_set_property(TX_PREEMPHASIS, TX_PREEMPHASIS_75US);

	// Change the pilot frequency if tone generation is desired.
	// si47xx_set_property(TX_PILOT_FREQUENCY, 19000);

	// Enable or disable the limit controls.  These can be changed by the designer
	// if necessary based on the audio that is being applied to the part.
	// si47xx_set_property(TX_ACOMP_ENABLE, TX_ACOMP_ENABLE_LIMITEN_MASK);
	// si47xx_set_property(TX_ACOMP_THRESHOLD, 0xFFD8);
	// si47xx_set_property(TX_ACOMP_ATTACK_TIME, TX_ACOMP_ATTACK_TIME_0_5MS);
	// si47xx_set_property(TX_ACOMP_RELEASE_TIME, TX_ACOMP_RELEASE_TIME_1000MS);
	// si47xx_set_property(TX_ACOMP_GAIN, 15);
	// si47xx_set_property(TX_LIMITER_RELEASE_TIME, 102);
}

//-----------------------------------------------------------------------------
// Configures the device for normal operation
//-----------------------------------------------------------------------------
void si47xxFMTX_configure(void)
{
    // Configure all other registers
    si47xxFMTX_hardware_cfg();
    si47xxFMTX_general_cfg();

	// Turn off the Headphone Amp and analog input.
	M_INPUT_AD = 1;
	M_OUTPUT_AD = 0;
	GP1 = 0;
}

//-----------------------------------------------------------------------------
// Resets the part and initializes registers to the point of being ready for
// the first tune or seek.
//-----------------------------------------------------------------------------
void si47xxFMTX_initialize(void)
{
    // Zero status registers.
	PoweredUp = 0;

    // Perform a hardware reset, power up the device, and then perform the
    // initial configuration.
    si47xx_reset();
    si47xxFMTX_powerup();
    si47xxFMTX_configure();
}

//-----------------------------------------------------------------------------
// Tunes to a station number using the current band and spacing settings.
//
// Inputs:
//      frequency:  frequency in 10kHz steps
//-----------------------------------------------------------------------------
void si47xxFMTX_tune(u16 frequency)
{
	// Enable the bit used for the interrupt of STC.
	SeekTuneInProc = 1;

	// Call the tune command to start the tune.
 	WaitSTCInterrupt = 1;
    txTuneFreq(frequency);

    // wait for the interrupt before continuing
    // If you do not wish to use interrupts but wish to poll the part
    // then comment out this line.
    while (WaitSTCInterrupt); // Wait for interrupt to clear the bit

    // Wait for stc bit to be set
    while (!(getIntStatus() & STCINT));

	// Clear the STC bit and get the results of the tune.
    txTuneStatus(1);

	// Disable the bit used for the interrupt of STC.
	SeekTuneInProc = 0;
}

//-----------------------------------------------------------------------------
// Tunes the power of the transmitter to the specified level
//
// Inputs:
//      voltage:  voltage in 1dBuV steps
//-----------------------------------------------------------------------------
void si47xxFMTX_tunePower(u8 voltage)
{
	// Enable the bit used for the interrupt of STC.
	SeekTuneInProc = 1;

	// Call the tune command to start the tune.
 	WaitSTCInterrupt = 1;
    txTunePower(voltage);

    // wait for the interrupt before continuing
    // If you do not wish to use interrupts but wish to poll the part
    // then comment out this line.
    while (WaitSTCInterrupt); // Wait for interrupt to clear the bit

    // Wait for stc bit to be set
    while (!(getIntStatus() & STCINT));

	// Clear the STC bit and get the results of the tune.
    txTuneStatus(1);

	// Disable the bit used for the interrupt of STC.
	SeekTuneInProc = 0;
}

//-----------------------------------------------------------------------------
// Tunes to the specified station in receive power scan mode for measuring
// the level of the station.
//
// Inputs:
//      frequency:  frequency in 10kHz steps
//-----------------------------------------------------------------------------
void si47xxFMTX_tuneMeasure(u16 frequency)
{
	// Enable the bit used for the interrupt of STC.
	SeekTuneInProc = 1;

	// Call the tune command to start the tune.
 	WaitSTCInterrupt = 1;
    txTuneMeasure(frequency);

    // wait for the interrupt before continuing
    // If you do not wish to use interrupts but wish to poll the part
    // then comment out this line.
    while (WaitSTCInterrupt); // Wait for interrupt to clear the bit

    // Wait for stc bit to be set
    while (!(getIntStatus() & STCINT));

	// Clear the STC bit and get the results of the tune.
    txTuneStatus(1);

	// Disable the bit used for the interrupt of STC.
	SeekTuneInProc = 0;
}

//-----------------------------------------------------------------------------
// Returns the current received noise level of the part if measured with
// txTuneMeasure.
//
// Returns:
//      frequency in 10kHz steps
//-----------------------------------------------------------------------------
u8 si47xxFMTX_get_rnl()
{
	// Get the tune status which contains the current frequency
	txTuneStatus(0);

    // Return the RSSI level
    return RNL;
}

//-----------------------------------------------------------------------------
// Helper function that sends the TX_TUNE_FREQ command to the part
//
// Inputs:
// 	frequency in 10kHz steps
//-----------------------------------------------------------------------------
static void txTuneFreq(u16 frequency)
{
    // Put the ID for the command in the first byte.
    cmd[0] = TX_TUNE_FREQ;

	// Initialize the reserved section to 0
    cmd[1] = 0;

	// Put the frequency in the second and third bytes.
    cmd[2] = (u8)(frequency >> 8);
	cmd[3] = (u8)(frequency & 0x00FF);

    // Invoke the command
	si47xx_command(4, cmd, 1, rsp);
}

//-----------------------------------------------------------------------------
// Helper function that sends the TX_TUNE_POWER command to the part
//
// Inputs:
// 	Voltage in 1dBuV steps
//-----------------------------------------------------------------------------
static void txTunePower(u8 voltage)
{
    // Put the ID for the command in the first byte.
    cmd[0] = TX_TUNE_POWER;

	// Initialize the reserved section to 0
    cmd[1] = 0;
	cmd[2] = 0;

	// The voltage
	cmd[3] = voltage;

	// Set the chip to automatically calculate the antenna cap.
	cmd[4] = 0;

    // Invoke the command
	si47xx_command(5, cmd, 1, rsp);
}

//-----------------------------------------------------------------------------
// Helper function that sends the TX_TUNE_MEASURE command to the part
//
// Inputs:
//      frequency in 10kHz steps
//-----------------------------------------------------------------------------
static void txTuneMeasure(u16 frequency)
{
    // Put the ID for the command in the first byte.
    cmd[0] = TX_TUNE_MEASURE;

	// Initialize the reserved section to 0
    cmd[1] = 0;

	// Put the frequency in the second and third bytes.
    cmd[2] = (u8)(frequency >> 8);
	cmd[3] = (u8)(frequency & 0x00FF);

	// Set the chip to automatically calculate the antenna cap.
	cmd[4] = 0;

    // Invoke the command
	si47xx_command(5, cmd, 1, rsp);
}


//-----------------------------------------------------------------------------
// Helper function that sends the TX_TUNE_STATUS command to the part
//
// Inputs:
//  intack: If non-zero the interrupt for STCINT will be cleared.
//
// Outputs:  // These are global variables and are set by this method
//  STC:    The seek/tune is complete
//  Freq:   The current frequency
//  Voltage:The current tuned voltage level.
//  AntCap: The antenna capacitor value.
//  RNL:    The measured signal level if called after TX_TUNE_MEASURE
//-----------------------------------------------------------------------------
static void txTuneStatus(u8 intack)
{
    // Put the ID for the command in the first byte.
    cmd[0] = TX_TUNE_STATUS;

	// Put the flags if the bit was set for the input parameters.
	cmd[1] = 0;
	if(intack) cmd[1] |= TX_TUNE_STATUS_IN_INTACK;

    // Invoke the command
	si47xx_command(2, cmd, 8, rsp);

    // Parse the results
    STC    = !!(rsp[0] & STCINT);
    Freq   = ((u16)rsp[2] << 8) | (u16)rsp[3];
    Voltage= rsp[5];
    AntCap = rsp[6];   
	RNL    = rsp[7];

}

//-----------------------------------------------------------------------------
// Helper function that sends the TX_ASQ_STATUS command to the part
//
// Inputs:
//  intack: If non-zero the interrupt for ASQINT will be cleared.
//
// Outputs:  // These are global variables and are set by this method
//  OverMod: The audio signal is overmodulating the device.
//  IALH:    The audio signal was above the audio threshold for the duration
//           specified.
//  IALL:    The audio signal was below the audio threshold for the duration
//           specified.
//  InLevel: The current measured audio input level.
//-----------------------------------------------------------------------------
void txAsqStatus(u8 intack)
{
    // Put the ID for the command in the first byte.
    cmd[0] = TX_ASQ_STATUS;

	// Put the flags if the bit was set for the input parameters.
	cmd[1] = 0;
	if(intack) cmd[1] |= TX_ASQ_STATUS_IN_INTACK;

    // Invoke the command
	si47xx_command(2, cmd, 5, rsp);

    // Parse the results
	OverMod = !!(rsp[1] & TX_ASQ_STATUS_OUT_OVERMOD);
	IALH    = !!(rsp[1] & TX_ASQ_STATUS_OUT_IALH);
	IALL    = !!(rsp[1] & TX_ASQ_STATUS_OUT_IALL);
	InLevel = rsp[4];
}

//-----------------------------------------------------------------------------
// Helper function that sends the TX_RDS_PS command to the part
//
// Inputs:
//  id, char0-3: See the programming manual for more information.
//-----------------------------------------------------------------------------
void txRdsPs(u8 id, u8 char0, u8 char1, u8 char2, u8 char3)
{
    // Put the ID for the command in the first byte.
    cmd[0] = TX_RDS_PS;

	// Store the id and characters into the input parameters.
	cmd[1] = id;
	cmd[2] = char0;
	cmd[3] = char1;
	cmd[4] = char2;
	cmd[5] = char3;

    // Invoke the command
	si47xx_command(6, cmd, 1, rsp);
}


//-----------------------------------------------------------------------------
// Helper function that sends the TX_RDS_BUFF command to the part
//
// Inputs:
//  intack: If non-zero the interrupt for RDSINT will be cleared.
//  mtbuff: Empties the specified buffer.
//  ldbuff: Loads the RDS data passed into the specified buffer.
//  fifo:   If 0-this command will interact with the circular buffer otherwise
//          the fifo.
//  rdsb:   RDS group b data placed into the buffer if ldbuff is 1.
//  rdsc:   RDS group c data placed into the buffer if ldbuff is 1.
//  rdsd:   RDS group d data placed into the buffer if ldbuff is 1.
//
// Outputs:  // These are global variables and are set by this method
//  RdsInt:    Contains any RDS interrupt information
//  CbAvail:   The amount of circular buffer currently available.
//  CbUsed:    The amount of circular buffer currently being used.
//  FifoAvail: The amount of the fifo currently available.
//  FifoUsed:  The amount of the fifo currently being used.
//-----------------------------------------------------------------------------
void txRdsBuff(bit intack, bit mtbuff, bit ldbuff, bit fifo,
			   u16 rdsb, u16 rdsc, u16 rdsd)
{
    // Put the ID for the command in the first byte.
    cmd[0] = TX_RDS_BUFF;

	// Put the flags if the bit was set for the input parameters.
	cmd[1] = 0;
	if(intack) cmd[1] |= TX_RDS_BUFF_IN_INTACK;
	if(mtbuff) cmd[1] |= TX_RDS_BUFF_IN_MTBUFF;
	if(ldbuff) cmd[1] |= TX_RDS_BUFF_IN_LDBUFF;
	if(fifo)   cmd[1] |= TX_RDS_BUFF_IN_FIFO;
    
	// Load the RDS values into the command array.
	cmd[2] = (u8)(rdsb >> 8);
	cmd[3] = (u8)(rdsb & 0x00FF);
	cmd[4] = (u8)(rdsc >> 8);
	cmd[5] = (u8)(rdsc & 0x00FF);
	cmd[6] = (u8)(rdsd >> 8);
	cmd[7] = (u8)(rdsd & 0x00FF);

    // Invoke the command
	si47xx_command(8, cmd, 6, rsp);

    // Parse the results
	RdsInt	  = rsp[1];
	CbAvail   = rsp[2];
	CbUsed    = rsp[3];
	FifoAvail = rsp[4];
	FifoUsed  = rsp[5];

}

