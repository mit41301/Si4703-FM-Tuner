//-----------------------------------------------------------------------------
// Includes
//-----------------------------------------------------------------------------
#include <stddef.h>
#include "typedefs.h"
#include "c8051F320.h"
#include "portdef.h"
#include "si47xxFMTX.h"
#include "FMTXtest.h"
#include "propertydefs.h"

//-----------------------------------------------------------------------------
// Defines
//-----------------------------------------------------------------------------
#define RDS_ARRAY_SIZE 22

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
extern u16 xdata scan_frequencies[];
extern u8  xdata InLevel;
extern u8  xdata FifoAvail;

//-----------------------------------------------------------------------------
// This exercises the minimum number of function calls to initiate a tune when
// the Si47xx device is first initialized.
//-----------------------------------------------------------------------------
void test_FMTXtune(void)
{
    si47xxFMTX_initialize();
    si47xxFMTX_tune(8750);        // tune to a station
	si47xxFMTX_tunePower(115);
    wait_ms(DEMO_DELAY);
}

//-----------------------------------------------------------------------------
// Test the power down function of the part.  Unlike the Si470x parts the Si47xx
// parts need to be fully initialized after power down.
//-----------------------------------------------------------------------------
void test_FMTXpowerCycle(void)
{
    si47xxFMTX_powerdown(); // Test powerdown with audio hi-z
    wait_ms(DEMO_DELAY);    // Wait 5s before starting next test
    si47xxFMTX_powerup();   // Powerup without reset, the part will need to be tuned again.
    si47xxFMTX_configure();
	si47xxFMTX_tune(8750);
	si47xxFMTX_tunePower(115);
	wait_ms(DEMO_DELAY);    // Wait 5s before starting next test
}


//-----------------------------------------------------------------------------
// Simple routine that tests the FMTX scan function..
//
// This particular test:
//  - Resets/Initializes the Si47xx
//  - Uses receive power scan function to determine best locations to transmit.
//  - Loops through the best stations tuning to each station.
//-----------------------------------------------------------------------------
void test_FMTXscan(void)
{
    u8  num_found;
	u8  scan_number;
	u16 scan_station;

    si47xxFMTX_initialize();
    si47xxFMTX_tune(8750);        // tune to a station
	si47xxFMTX_tunePower(115);

    num_found = si47xxFMTX_scan(); // populate the scan array with the best
								   // places to transmit.

    _nop_();           // break here to inspect scan array

	// Put the part back into transmit mode.
    si47xxFMTX_tune(8750);        // tune to any station
	si47xxFMTX_tunePower(115);

    scan_number = 0;

    while(scan_number < num_found)
    {
        // tune to the next station in the preset array
        scan_station = scan_frequencies[scan_number];

        if(scan_station != 0)
        {
            si47xxFMTX_tune(scan_station);
            wait_ms(DEMO_DELAY);
        }

        scan_number++;
    }
}

//-----------------------------------------------------------------------------
// Simple routine that demonstrates how to get the ASQ metrics from the part
// This routine will call the ASQ command on the part and keep the lower, upper
// and mean values for the signal level.
//-----------------------------------------------------------------------------
void test_FMTXasq(void)
{
	u8    maxLevel  = 0;
	u8    minLevel  = 255;
	float meanLevel = 0;
	u16   i;

	// Setup the part
	si47xxFMTX_initialize();
	si47xxFMTX_tune(8750);        // Tune to a station and power up the part.
	si47xxFMTX_tunePower(115);

	// Set the properties for the IALH and IALL indicators.
	si47xx_set_property(TX_ASQ_LEVEL_LOW, 0xCE);  // Set the low threshold to -50
	si47xx_set_property(TX_ASQ_DURATION_LOW, 1000); // Set the duration for 1 second.
	si47xx_set_property(TX_ASQ_LEVEL_HIGH, 0xFB);  // Set the high threshold to -10
	si47xx_set_property(TX_ASQ_DURATION_HIGH, 1000); // Set the duration for 1 second.

	// This will be done 100 times to average a decent signal level.
	for( i=0; i < 1000; i++)
	{
		// Get the ASQ status from the part.
		txAsqStatus(0);

		// Calculate the average
		meanLevel = (((meanLevel * i) + InLevel)/(i+1));
		
		// Replace the min or max level if necessary.
		if(InLevel < minLevel) minLevel = InLevel;
		if(InLevel > maxLevel) maxLevel = InLevel;

		// Wait for a while.
		wait_ms(1);
	}

	_nop_();   // Break here to inspect the three level values.
			   // Also determine if any of the interrupts are set,
			   // Overmod, IALH, or IALL.

	// Now reset the level indicators
	txAsqStatus(1);
}

//-----------------------------------------------------------------------------
// This routine tests RDS.  
// -  First it demonstrates the RDS functionality that is present in the part 
//    through properties.  
// -  Second it demonstrates using the circular buffer to transmit RDS data 
//    along with the 0A messages from the RDS properties.  
// -  Third use the fifo to transmit some radio text data instead of the
//    circular buffer.
//-----------------------------------------------------------------------------
void test_FMTXrds(void)
{  
	u8  i;
	u16 count;
	u16 BlockB;
	u16 xdata RDSDataArray[RDS_ARRAY_SIZE];

	// Setup the part
	si47xxFMTX_initialize();
	si47xxFMTX_tune(8750);        // Tune to a station and power up the part.
	si47xxFMTX_tunePower(115);

	//------------------------------------------------------------------------
	// 1. Setup the 0A messages that will be transmitted automatically
    //    by the firmware on the Si47xx part.
	//------------------------------------------------------------------------

	// Send the PS messages to the part:
	txRdsPs(0, 'S', 'i', 'l', 'i'); // This sets up 5 PS messages that state
	txRdsPs(1, 'c', 'o', 'n', ' '); // Silicon, Labs, Example, Code RDS, Demo
    txRdsPs(2, 'L', 'a', 'b', 's'); // If characters are not used it is
	txRdsPs(3, ' ', ' ', ' ', ' '); // recommended that they be padded with
	txRdsPs(4, 'E', 'x', 'a', 'm'); // the space character.
    txRdsPs(5, 'p', 'l', 'e', ' ');
    txRdsPs(6, 'C', 'o', 'd', 'e');
	txRdsPs(7, ' ', ' ', ' ', ' ');
	txRdsPs(8, 'R', 'D', 'S', ' ');
	txRdsPs(9, 'D', 'e', 'm', 'o');

	// Setup the Mix that will determine how many PS (0A) messages will be
    // sent by the part relative to the buffers.  The first example is all 0A
    // messages so set it to 100%
	si47xx_set_property(TX_RDS_PS_MIX, TX_RDS_PS_MIX_100_PCT);

	// Setup the PI code that will be transmitted
	si47xx_set_property(TX_RDS_PI, 16551);

	// Setup the PS misc bits that will be sent with the 0A messages
	si47xx_set_property(TX_RDS_PS_MISC, TX_RDS_PS_MISC_RDSMS_MASK |  // Music
									    TX_RDS_PS_MISC_RDSD0_MASK |  // Stereo
										TX_RDS_PS_MISC_FORCEB_MASK |
				(TX_RDS_PS_MISC_PTY_INFO << TX_RDS_PS_MISC_RDSPTY_SHFT));

	// Set the number of times a PS group will be repeated before sending
    // the next message
	si47xx_set_property(TX_RDS_PS_REPEAT_COUNT, 3);

	// Set the number of PS messages that are to be sent.
	si47xx_set_property(TX_RDS_PS_MESSAGE_COUNT, 5);

	// Do not sent an AF code.
	si47xx_set_property(TX_RDS_PS_AF, 0xE0E0);    

	// Now wait for awhile and view the RDS on a receiver to verify.
	wait_ms(RDS_DELAY);


	//------------------------------------------------------------------------
	// The following array will be used by the next two steps so initialize it
	// here.
	//------------------------------------------------------------------------
	RDSDataArray[0]  = 0x4669; // Fi
	RDSDataArray[1]  = 0x666F; // fo
	RDSDataArray[2]  = 0x2042; //  B
	RDSDataArray[3]  = 0x7566; // uf
	RDSDataArray[4]  = 0x6665; // fe
	RDSDataArray[5]  = 0x7220; // r 
	RDSDataArray[6]  = 0x4973; // Is
	RDSDataArray[7]  = 0x2042; //  B
	RDSDataArray[8]  = 0x6569; // ei
	RDSDataArray[9]  = 0x6E67; // ng
	RDSDataArray[10] = 0x2055; //  U
	RDSDataArray[11] = 0x7365; // se
	RDSDataArray[12] = 0x6420; // d 
	RDSDataArray[13] = 0x466F; // Fo
	RDSDataArray[14] = 0x7220; // r 
	RDSDataArray[15] = 0x5261; // Ra
	RDSDataArray[16] = 0x6469; // di
	RDSDataArray[17] = 0x6F20; // o 
	RDSDataArray[18] = 0x5465; // te
	RDSDataArray[19] = 0x7874; // xt
	RDSDataArray[20] = 0x0D00; // <CR>
	RDSDataArray[21] = 0x0000; 

	//------------------------------------------------------------------------
	// 2. Load radio text into the circular buffer.
	//------------------------------------------------------------------------

	// Set the FIFO size to zero.  The circular buffer is equal to the total
	// buffer size minus the size of the fifo.
	si47xx_set_property(TX_RDS_FIFO_SIZE, 0);

	// Clear out the circular buffer.
	txRdsBuff(0, 1, 0, 0, 0, 0, 0);

	// Load the Radio Text message into the circular buffer.
	// The array has FIFO in it so do not use the first elements
	// override it here.
	BlockB = 0x2000;
	txRdsBuff(0, 0, 1, 0, BlockB++, 0x4369, 0x7263);  // Circ
	txRdsBuff(0, 0, 1, 0, BlockB++, 0x756C, 0x6172);  // ular
	for( i=2; i < RDS_ARRAY_SIZE; i+=2 ) 
	{
		txRdsBuff(0, 0, 1, 0, BlockB++, RDSDataArray[i], RDSDataArray[i+1]);
	}

	// Setup the Mix that will determine how many PS (0A) messages will
	// be sent along with the circular buffer.  Try 75%.
	si47xx_set_property(TX_RDS_PS_MIX, TX_RDS_PS_MIX_75_PCT);

	// Now wait for awhile and view the RDS on a receiver to verify.
	wait_ms(RDS_DELAY);

	
	//------------------------------------------------------------------------
	// 3. Load radio text into fifo and keep fifo filled with data.
	//------------------------------------------------------------------------

	// First clear the circular buffer.
	txRdsBuff(0, 1, 0, 0, 0, 0, 0);

	// Allocate enough memory for the fifo.  The fifo requires 3 blocks for
	// a type A message and 2 blocks for a type B message.  Since this example
	// is using type A messages allocate in increments of 3.
	si47xx_set_property(TX_RDS_FIFO_SIZE, 36);

	// Now loop loading data into the fifo as long as there is some room
	// available:
	// Note this will only be done for 100 times for this demo.
	BlockB = 0x2000;
	for( count=0, i=0; count < 100; count++, i+=2 )
	{
		// If i is bigger than the RDS array size then set it back
		// to zero and then also reset the BlockB element.
		if(i >= RDS_ARRAY_SIZE)
		{
			i = 0;
			BlockB = 0x2000;
		}

		// Call the txRdsBuff command to find out how much fifo is available
		// make sure there is at least three available for each group
		// and delay until those three are available.
		txRdsBuff(0, 0, 0, 0, 0, 0, 0);

		while(FifoAvail <= 3)
		{
			wait_ms(10);
			txRdsBuff(0, 0, 0, 0, 0, 0, 0);
		}

		// Now the fifo has enough space available to put a group in
		txRdsBuff(0, 0, 1, 1, BlockB++, RDSDataArray[i], RDSDataArray[i+1]);		
	}

	// Wait for the fifo to clear
	wait_ms(RDS_DELAY);

	// NOTE:  This example does not demonstrate how to use the fifo and
	// circular buffer at the same time.  This can be done by filling both
	// buffers and the circular buffer will be sent whenever the fifo is empty.
	// If the fifo buffer is never empty the circular buffer will not be sent.

}

