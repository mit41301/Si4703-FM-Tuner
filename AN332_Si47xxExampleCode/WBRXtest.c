//-----------------------------------------------------------------------------
// Includes
//-----------------------------------------------------------------------------
#include <stddef.h>
#include "typedefs.h"
#include "c8051F320.h"
#include "portdef.h"
#include "si47xxWBRX.h"
#include "WBRXtest.h"
#include "WBRXsame.h"
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
extern bit SameTestInProc;
extern u8 idata ProcessSame;

//-----------------------------------------------------------------------------
// This exercises the minimum number of function calls to initiate a tune when
// the Si47xx device is first initialized.
//-----------------------------------------------------------------------------
void test_WBRXtune(void)
{
    si47xxWBRX_initialize();
    si47xxWBRX_set_volume(63);     // full volume, turn off mute
    si47xxWBRX_tune(1624000/25);   // tune to Station 1 (162.4MHz)
    wait_ms(DEMO_DELAY);
}

//-----------------------------------------------------------------------------
// Test the volume.  Starts at mute, moves up through all 63 steps.
//-----------------------------------------------------------------------------
void test_WBRXvolume(void)
{
    u8 vol;
    for (vol=0; vol<63; vol++) {
        si47xxWBRX_set_volume(vol);
        wait_ms(500);
    }
}

//-----------------------------------------------------------------------------
// Test the power down function of the part.  Unlike the Si470x parts the Si47xx
// parts need to be fully initialized after power down.
//-----------------------------------------------------------------------------
void test_WBRXpowerCycle(void)
{
    si47xxWBRX_powerdown(); // Test powerdown with audio hi-z
    wait_ms(DEMO_DELAY);    // Wait 5s before starting next test
    si47xxWBRX_powerup();   // Powerup without reset, the part will need to be tuned again.
    si47xxWBRX_configure();
    si47xxWBRX_set_volume(63);     // full volume, turn off mute
	si47xxWBRX_tune(64960);
	wait_ms(DEMO_DELAY);    // Wait 5s before starting next test
}


//-----------------------------------------------------------------------------
// This routine scans all seven weather band frequencies and tunes to channel
// with the highest RSSI.
//-----------------------------------------------------------------------------
void test_WBRX_scan(void) {
    u16 i;
    u16 best_frequency;
    u8 best_rssi = 0;

    // Mute the audio
    si47xxWBRX_mute(1);

	for(i=64960;i<=65020;i+=10)
	{
		si47xxWBRX_tune(i);
		if( si47xxWBRX_get_rssi() > best_rssi)
		{
			best_rssi = si47xxWBRX_get_rssi();
			best_frequency = si47xxWBRX_get_frequency();
		}
	}

    // Tune to station with highest RSSI. 
    si47xxWBRX_tune(best_frequency);

	// Get the 1050kHz alert tone status
	si47xxWBRX_get_alert(0, 0);		 

    // Unmute
    si47xxWBRX_mute(0);

}

//-----------------------------------------------------------------------------
// Tune to a WB frequency that you will generate a SAME message on.  When
// the message has been received place a breakpoint below and inspect the
// message buffer and confidence buffer to inspect the SAME message.
//-----------------------------------------------------------------------------
void test_WBRXsame(void)
{
	// Initialize the part.
	si47xxWBRX_initialize();
	si47xxWBRX_set_volume(63);

	// Setup the SAME Interrupt to interrupt when a SAME header is available
    // or an end of message has been received.
	si47xx_set_property(WB_SAME_INTERRUPT_SOURCE, WB_SAME_INTERRUPT_SOURCE_HDRRDY_MASK | 
												  WB_SAME_INTERRUPT_SOURCE_EOMDET_MASK);

    // Setup the Alert tone interrupt to fire when an alert tone is present.  This
    // will be one of the items that is used to clear the alert tone buffer.
    si47xx_set_property(WB_ASQ_INTERRUPT_SOURCE, WB_ASQ_INTERRUPT_SOURCE_ALERTON_MASK);

	// Set the bit that will allow the interrupt handler to know that the incoming
    // interrupts are SAME related.
	SameTestInProc = 1;

	si47xxWBRX_tune(1624750/25); // Tune to a station that will broadcast
								 // a SAME message.

	initSameVars();              // Initialize the SAME variables

	// This loop will run and if same data is available or another
	// item like a timer fires the SAME data will be updated.
	// After this loop runs awhile place a breakpoint on the if statement
    // in order to inspect the SAME buffers: sameBuffer and sameConfidence
	while(1)
	{
		if(ProcessSame)
		{
			ProcessSame--;
			updateSame();
		}
	}

	// Reset the Same Test
	SameTestInProc = 0;
}
