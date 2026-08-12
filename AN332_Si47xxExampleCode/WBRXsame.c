//-----------------------------------------------------------------------------
// Includes
//-----------------------------------------------------------------------------
#include <stddef.h>
#include "c8051f320.h"
#include "typedefs.h"
#include "portdef.h"
#include "WBRXsame.h"
#include "si47xxWBRX.h"

//-----------------------------------------------------------------------------
// local defines
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
// Globals
//-----------------------------------------------------------------------------
// SAME Buffer - This buffer when the message is available should be parsed
//               to notify the user when an alert has been sent.
u8  xdata sameBuffer[255];

// Confidence  - This array contains the confidence of each element in the 
//               array above.
u8  xdata sameConfidence[255];
u8  xdata endOfMessage;
u8  xdata startOfMessage;
u8  xdata headerReady;
u8  xdata lastMessageComplete;
u8  xdata preambleDetected;
u8  xdata headerCount;
u16  xdata timer;

//-----------------------------------------------------------------------------
// Function prototypes
//-----------------------------------------------------------------------------
void wait_ms(u16 ms);
void populateSameVariables(u8 resetInterrupts, u8 clearBuffer);

//-----------------------------------------------------------------------------
// Externals
//-----------------------------------------------------------------------------
extern u8 xdata SameInts;
extern u8 xdata SameMsgLen;
extern u8 xdata SameConf[8];
extern u8 xdata SameData[8];
extern u8 idata ProcessSame;

//-----------------------------------------------------------------------------
// After tuning to a new station, the SAME tracking variables need to be
// initialized.
//-----------------------------------------------------------------------------
void
initSameVars(void)
{
    u8 i;

	for(i=0; i < sizeof(sameBuffer); i++)
	{
		sameBuffer[i] = 0;
		sameConfidence[i] = 0;
	}

	lastMessageComplete = 1;
	headerCount = 0;
	timer = 0;
}

//-----------------------------------------------------------------------------
// After an SAME interrupt, read back the SAME buffer and process the data.
//-----------------------------------------------------------------------------
void
updateSame(void)
{
	u8 asqInts = 0;

	// Get the data from the part and clear the interrupts.
	populateSameVariables(1, 0);

	// Get the audio status to see if an alert tone was ever present
	si47xxWBRX_get_alert(1, &asqInts);

	// If this is a new message then log that info so it can
	// be used to clear the buffer later.
	if(lastMessageComplete && (startOfMessage || headerReady))
	{
		lastMessageComplete = 0;
		timer = 0;
	}

    //------------------------------------------------------------------
	// There are 4 conditions that should be used to clear out the
	// buffer on the part.  If the buffer is not cleared before the next
	// valid message is received by the part the buffer will become
	// corrupted and will most likely be unreadable.
	// The following are the 4 conditions for clearing the buffer:
	// 1. An end of message is received.
	// 2. Six seconds have elapsed between a headerReady and a preamble
	// 3. An alert tone was detected.
	// 4. Three headers were received.
	//------------------------------------------------------------------

	// If a preamble is detected then reset the timer.
	if(preambleDetected != 0)
	{
		timer = 0;
	}

	// The timer is reset whenever a header is received.
	if(headerReady != 0)
	{
		headerCount++;
		timer = 0;
	}

	// If one of the following conditions are met clear the buffer and mark
	// the message as complete.
	if(endOfMessage != 0 ||
	   (asqInts & WB_ASQ_STATUS_OUT_ALERTONINT) != 0 ||
	   (headerReady != 0 && headerCount >= 3) ||
	   (timer >= 600))
	{
		// Clear the buffer out whenever an endOfMessage is encountered.
		populateSameVariables(1, 1);
		lastMessageComplete = 1;
		headerCount = 0;
		timer = 0;
	}

	// TODO: When implementing a full SAME radio here is where you
	// would put code to interpret the SAME message and alert the user.
	// This can be done in real time as the message is being received or
	// can be delayed until the lastMessageComplete flag is set.
}

//-----------------------------------------------------------------------------
// Populate the SAME data variables with values from the part.  Also reset the
// buffer or interrupts if required.
//-----------------------------------------------------------------------------
void
populateSameVariables(u8 resetInterrupts, u8 clearBuffer)
{
	u8 length = 0;
	u8 i = 0;
	u8 j = 0;

	// Get the first address value from the device and all flags
	wbSameStatus(resetInterrupts, clearBuffer, 0);

	// Populate the variables that will be used to determine
	// when to clear the buffer.
	endOfMessage = !!(SameInts & WB_SAME_STATUS_OUT_EOMDET);
	headerReady = !!(SameInts & WB_SAME_STATUS_OUT_HDRRDY);
	startOfMessage = !!(SameInts & WB_SAME_STATUS_OUT_SOMDET);
	preambleDetected = !!(SameInts & WB_SAME_STATUS_OUT_PREDET);
	length = SameMsgLen;

	// Get the SAME message buffer only if the buffer is not being
    // cleared.
	if(!clearBuffer)
	{
		// Loop incrementing by 8 elements to retrieve the items
		// from the part and put the elements into the local buffers.
		for(i = 0; i < length && i < sizeof(sameBuffer); i += 8)
		{
			// Call the command on the part to get 8 elements
			// out of the buffer.
			wbSameStatus(0, 0, i);
			
			// Now only populate up to the length if i + 8 is greater
			// than the total length of the array.
			for(j = 0; j + i < length && j < 8; j++)
			{
				sameBuffer[j+i] = SameData[j];
				sameConfidence[j+i] = SameConf[j];
			}
		}

		// Pad the remaining elements in the array with zeros
		for(i = length; i < sizeof(sameBuffer); i++)
		{
			sameBuffer[i] = 0;
			sameConfidence[i] = 0;
		}
	}
}

//-----------------------------------------------------------------------------
// Interrupt service routine used to count seconds that are used to clear
// the SAME buffer.  If it has been longer than 6 seconds since the last
// SAME message has been received and the buffer has not been cleared then
// clear the buffer.
//-----------------------------------------------------------------------------
void SAME_TIMER_ISR(void) interrupt 14
{
	// Now this routine will increment the timer that is associated
	// with clearing the SAME buffer only if needed.
	if(!lastMessageComplete)
	{
		timer++;
	
		// If the timer is greater equal to 6 seconds then cause the main
		// routine to update the same data.
		if(timer == 600)
		{
			ProcessSame++;
		}
	}

    TMR3CN &= 0x3F;  // Clear overflow flag
}

