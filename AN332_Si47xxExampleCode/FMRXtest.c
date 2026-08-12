//-----------------------------------------------------------------------------
// Includes
//-----------------------------------------------------------------------------
#include <stddef.h>
#include "typedefs.h"
#include "c8051F320.h"
#include "portdef.h"
#include "FMRXrds.h"
#include "si47xxFMRX.h"
#include "FMRXtest.h"
#include "propertydefs.h"

//-----------------------------------------------------------------------------
// Defines
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
// Global variables
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
// Function prototypes
//-----------------------------------------------------------------------------
void wait_ms(u16 ms);
void _nop_(void);

//-----------------------------------------------------------------------------
// Externals
//-----------------------------------------------------------------------------
extern u16 xdata seek_preset[];
extern u8 idata RdsAvailable;
extern bit RdsTestInProc;

//-----------------------------------------------------------------------------
// This exercises the minimum number of function calls to initiate a tune when
// the Si47xx device is first initialized.
//-----------------------------------------------------------------------------
void test_FMRXtune(void)
{
    si47xxFMRX_initialize();
    si47xxFMRX_set_volume(63);     // full volume, turn off mute
    si47xxFMRX_tune(10230);        // tune to a station
    wait_ms(DEMO_DELAY);
}

//-----------------------------------------------------------------------------
// Simple routine that tests the si47xx driver autoseek function.
//
// This particular test:
//  - Resets/Initializes the Si47xx
//  - Fills the preset array with the strongest stations found
//  - Loops through each preset pausing on each one
//-----------------------------------------------------------------------------
void test_FMRXautoseek(void)
{
    u16 preset_freq;
    u8 preset_number;
    u8 num_found;

    si47xxFMRX_initialize();
    si47xxFMRX_set_volume(63); // full volume, turn off mute

    num_found = si47xxFMRX_autoseek(); // populate the preset array with strongest stations

    _nop_();           // break here to inspect preset array

    preset_number = 0;

    while(preset_number < num_found)
    {
        // tune to the next station in the preset array
        preset_freq = seek_preset[preset_number];

        if(preset_freq != 0)
        {
            si47xxFMRX_tune(preset_freq);
            wait_ms(DEMO_DELAY);
        }

        preset_number++;
    }
}


//-----------------------------------------------------------------------------
// Tune to a station known to transmit RDS data. Wait a few seconds and
// then stop execution so you can examine the various RDS buffers.  If you
// place a break point on the si47_rdsGetBler, you'll have to wait for the
// second time the code stops there.
//-----------------------------------------------------------------------------
void test_FMRXrds(void)
{
    u16 bler;
    u8 updatecntr=0;
    u16 rdsConfig=0;
    si47xxFMRX_initialize();
    si47xxFMRX_set_volume(63); // full volume, turn off mute

	// Setup the RDS Interrupt to interrupt when RDS data is available.
	si47xx_set_property(FM_RDS_INTERRUPT_SOURCE, FM_RDS_INTERRUPT_SOURCE_RECV_MASK);
	si47xx_set_property(FM_RDS_INTERRUPT_FIFO_COUNT, 1);

	// Setup the RDS configuration to enable RDS and allow any error condition.
	rdsConfig |= FM_RDS_CONFIG_RDSEN_MASK;
	rdsConfig |= FM_RDS_BLETH_UNCORRECTABLE << FM_RDS_CONFIG_BLETHA_SHFT;
	rdsConfig |= FM_RDS_BLETH_UNCORRECTABLE << FM_RDS_CONFIG_BLETHB_SHFT;
	rdsConfig |= FM_RDS_BLETH_UNCORRECTABLE << FM_RDS_CONFIG_BLETHC_SHFT;
	rdsConfig |= FM_RDS_BLETH_UNCORRECTABLE << FM_RDS_CONFIG_BLETHD_SHFT;
    si47xx_set_property(FM_RDS_CONFIG, rdsConfig);

	RdsTestInProc = 1;
    si47xxFMRX_tune(10230);    // tune to a station that transmits RDS

    initRdsVars();             // Initialize RDS variables

    // Allow this loop to run for several seconds before stopping execution.
    // The RDS data can be examined in the following variables:
    // piDisplay, ptyDisplay, rtDisplay, rtSimple, psDisplay,
    // ctDayHigh, ctDayLow, ctHour, ctMinute, ctOffset
    while(1) {
		if(RdsAvailable)
		{
	        updateRds();
	        if (!updatecntr++) {
	            // Update bler every time counter rolls over
	            si47_rdsGetBler(&bler);
	        }
		}
    }
	RdsTestInProc = 0;
}

//-----------------------------------------------------------------------------
// Test the volume.  Starts at mute, moves up through all 63 steps.
//-----------------------------------------------------------------------------
void test_FMRXvolume(void)
{
    u8 vol;
    for (vol=0; vol<63; vol++) {
        si47xxFMRX_set_volume(vol);
        wait_ms(500);
    }
}

//-----------------------------------------------------------------------------
// Test the alternate frequency quick tune.
// To truly test this, you need either a radio station that broadcasts RDS
// alternate frequency content or a signal generator that does. The idea is to
// quickly tune to the frequencies in the alternate frequency list to check
// the signal power at those frequencies. If an alternate frequency has a
// higher power level than the current frequency you can choose to switch over.
// Alternate frequency as described in the RDS spec is more complicated than
// this. The RDS spec indicates that the PI code is to be checked prior to
// jumping to the other frequency. Since the amount of time it takes to recieve
// RDS depends on many factors, including signal strength, it is better to
// first do a quick check of the power level and then check the PI code on a
// subsequent tune. There are numerous special cases where the PI codes will
// not be identical. See the RDS specification for more information.
//
// This routine merely checks the ability to quickly tune.
//-----------------------------------------------------------------------------
void test_FMRXquickAFtune(void)
{
    u16 rssi;
    rssi = quickAFTune(9670); // You'll hear a brief audio drop out
    wait_ms(1000);            // Break here and check rssi
    si47xxFMRX_tune(9670);
    wait_ms(1000);
    rssi = si47xxFMRX_get_rssi();
    wait_ms(1000);            // Break here and check rssi. The two should be close
}

//-----------------------------------------------------------------------------
// Test the power down function of the part.  Unlike the Si470x parts the Si47xx
// parts need to be fully initialized after power down.
//-----------------------------------------------------------------------------
void test_FMRXpowerCycle(void)
{
    si47xxFMRX_powerdown(); // Test powerdown with audio hi-z
    wait_ms(DEMO_DELAY);    // Wait 5s before starting next test
    si47xxFMRX_powerup();   // Powerup without reset, the part will need to be tuned again.
    si47xxFMRX_configure();
    si47xxFMRX_set_volume(63);     // full volume, turn off mute
	si47xxFMRX_tune(10230);
	wait_ms(DEMO_DELAY);    // Wait 5s before starting next test
}

