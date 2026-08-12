//-----------------------------------------------------------------------------
// Includes
//-----------------------------------------------------------------------------
#include <stddef.h>
#include "c8051f320.h"
#include "typedefs.h"
#include "portdef.h"
#include "FMRXrds.h"
#include "si47xxFMRX.h"

//-----------------------------------------------------------------------------
// local defines
//-----------------------------------------------------------------------------
#define RDS_TYPE_0A     ( 0 * 2 + 0)
#define RDS_TYPE_0B     ( 0 * 2 + 1)
#define RDS_TYPE_1A     ( 1 * 2 + 0)
#define RDS_TYPE_1B     ( 1 * 2 + 1)
#define RDS_TYPE_2A     ( 2 * 2 + 0)
#define RDS_TYPE_2B     ( 2 * 2 + 1)
#define RDS_TYPE_3A     ( 3 * 2 + 0)
#define RDS_TYPE_3B     ( 3 * 2 + 1)
#define RDS_TYPE_4A     ( 4 * 2 + 0)
#define RDS_TYPE_4B     ( 4 * 2 + 1)
#define RDS_TYPE_5A     ( 5 * 2 + 0)
#define RDS_TYPE_5B     ( 5 * 2 + 1)
#define RDS_TYPE_6A     ( 6 * 2 + 0)
#define RDS_TYPE_6B     ( 6 * 2 + 1)
#define RDS_TYPE_7A     ( 7 * 2 + 0)
#define RDS_TYPE_7B     ( 7 * 2 + 1)
#define RDS_TYPE_8A     ( 8 * 2 + 0)
#define RDS_TYPE_8B     ( 8 * 2 + 1)
#define RDS_TYPE_9A     ( 9 * 2 + 0)
#define RDS_TYPE_9B     ( 9 * 2 + 1)
#define RDS_TYPE_10A    (10 * 2 + 0)
#define RDS_TYPE_10B    (10 * 2 + 1)
#define RDS_TYPE_11A    (11 * 2 + 0)
#define RDS_TYPE_11B    (11 * 2 + 1)
#define RDS_TYPE_12A    (12 * 2 + 0)
#define RDS_TYPE_12B    (12 * 2 + 1)
#define RDS_TYPE_13A    (13 * 2 + 0)
#define RDS_TYPE_13B    (13 * 2 + 1)
#define RDS_TYPE_14A    (14 * 2 + 0)
#define RDS_TYPE_14B    (14 * 2 + 1)
#define RDS_TYPE_15A    (15 * 2 + 0)
#define RDS_TYPE_15B    (15 * 2 + 1)

#define CORRECTED_NONE          0
#define CORRECTED_ONE_TO_TWO    1
#define CORRECTED_THREE_TO_FIVE 2
#define UNCORRECTABLE           3
#define ERRORS_CORRECTED(data,block) ((data>>block)&0x03)

#define BLER_SCALE_MAX 200  // Block Errors are reported in .5% increments

//-----------------------------------------------------------------------------
// Globals
//-----------------------------------------------------------------------------
u8 xdata RdsReadPtr;
u8 xdata RdsWritePtr;
bit RdsFifoEmpty;

// Determines if the basic or advanced PS/RT RDS display functions are used
// The basic functions are closely tied to the recommendations in the RBDS
// specification and are faster to update but more error prone. The advanced
// functions attempt to detect errors and only display complete messages
static bit rdsBasic = 0;

bit Gpio2StickyBit = 0;

// RDS Program Identifier
u16 xdata piDisplay;              // Displayed Program Identifier

// RDS Program Type
u8  xdata ptyDisplay;             // Displayed Program Type

bit firstInitDone;
u8  xdata rdsBlerMax[4];

// RDS Radio Text
       u8  xdata rtDisplay[64];   // Displayed Radio Text
       u8  xdata rtSimple[64];    // Simple Displayed Radio Text
static u8  xdata rtTmp0[64];      // Temporary Radio Text (high probability)
static u8  xdata rtTmp1[64];      // Temporary radio text (low probability)
static u8  xdata rtCnt[64];       // Hit count of high probabiltiy radio text
static bit       rtFlag;          // Radio Text A/B flag
static bit       rtFlagValid;     // Radio Text A/B flag is valid
static bit       rtsFlag;         // Radio Text A/B flag
static bit       rtsFlagValid;    // Radio Text A/B flag is valid

// RDS Program Service
       u8  xdata psDisplay[8];    // Displayed Program Service text
static u8  xdata psTmp0[8];       // Temporary PS text (high probability)
static u8  xdata psTmp1[8];       // Temporary PS text (low probability)
static u8  xdata psCnt[8];        // Hit count of high probability PS text

// RDS Clock Time and Date
       bit       ctDayHigh;       // Modified Julian Day high bit
       u16 xdata ctDayLow;        // Modified Julian Day low 16 bits
       u8  xdata ctHour;          // Hour
       u8  xdata ctMinute;        // Minute
       i8  xdata ctOffset;        // Local Time Offset from UTC

// RDS Alternate Frequency List
static u8  xdata afCount;
static u16 xdata afList[25];

// RDS flags and counters
       u8  xdata RdsDataAvailable = 0;  // Count of unprocessed RDS Groups
static u8  xdata RdsIndicator     = 0;  // If true, RDS was recently detected
static u16 xdata RdsDataLost      = 0;  // Number of Groups lost
static u16 xdata RdsBlocksValid   = 0;  // Number of valid blocks received
static u16 xdata RdsBlocksTotal   = 0;  // Total number of blocks expected
static u16 xdata RdsBlockTimer    = 0;  // Total number of blocks expected
static u16 xdata RdsGroupCounters[32];  // Number of each kind of group received

       bit       RdsSynchTimer  = 0;
static u16 xdata RdsSynchValid  = 0;
static u16 xdata RdsSynchTotal  = 0;
static u16 xdata RdsGroupsTotal = 0;
static u16 xdata RdsGroups      = 0;
static u16 xdata RdsValid[4];

       bit       rdsIgnoreAB;

//-----------------------------------------------------------------------------
// Function prototypes
//-----------------------------------------------------------------------------

void wait_ms(u16 ms);
static void updateRdsFifo(u16 * group, u8 errorFlags);
static void init_alt_freq(void);

//-----------------------------------------------------------------------------
// Externals
//-----------------------------------------------------------------------------

extern u8  xdata RdsInts;
extern u8  xdata RdsSync;
extern u8  xdata GrpLost;
extern u8  xdata RdsFifoUsed;
extern u16 xdata BlockA;
extern u16 xdata BlockB;
extern u16 xdata BlockC;
extern u16 xdata BlockD;
extern u8  xdata BleA;
extern u8  xdata BleB;
extern u8  xdata BleC;
extern u8  xdata BleD;

//-----------------------------------------------------------------------------
// After tuning to a new station, the RDS tracking variables need to be
// initialized.
//-----------------------------------------------------------------------------
void
initRdsVars(void)
{
    u8 i;

    // Set the maximum allowable errors in a block considered acceptable
    // It's critical that block B be correct since it determines what's
    // contained in the latter blocks. For this reason, a stricter tolerance
    // is placed on block B
    if (!firstInitDone)
    {
        rdsBlerMax[0] = CORRECTED_THREE_TO_FIVE; // Block A
        rdsBlerMax[1] = CORRECTED_ONE_TO_TWO;    // Block B
        rdsBlerMax[2] = CORRECTED_THREE_TO_FIVE; // Block C
        rdsBlerMax[3] = CORRECTED_THREE_TO_FIVE; // Block D
        firstInitDone = 1;
    }

    // Reset RDS variables
    RdsDataAvailable = 0;
    RdsDataLost      = 0;
    RdsIndicator     = 0;
    RdsBlocksValid   = 0;
    RdsBlocksTotal   = 0;

    // Clear RDS Fifo
    RdsFifoEmpty = 1;
    RdsReadPtr = 0;
    RdsWritePtr = 0;

    // Clear Radio Text
    rtFlagValid     = 0;
    rtsFlagValid    = 0;
    for (i=0; i<sizeof(rtDisplay); i++)
    {
        rtSimple[i]  = 0;
        rtDisplay[i] = 0;
        rtTmp0[i]    = 0;
        rtTmp1[i]    = 0;
        rtCnt[i]     = 0;
    }

    // Clear Program Service
    for (i=0;i<sizeof(psDisplay);i++)
    {
        psDisplay[i] = 0;
        psTmp0[i]    = 0;
        psTmp1[i]    = 0;
        psCnt[i]     = 0;
    }

    // Reset Debug Group counters
    for (i=0; i<32; i++)
    {
        RdsGroupCounters[i] = 0;
    }

    // Reset alternate frequency tables
    init_alt_freq();

    RdsSynchValid  = 0;
    RdsSynchTotal  = 0;
    RdsValid[0]    = 0;
    RdsValid[1]    = 0;
    RdsValid[2]    = 0;
    RdsValid[3]    = 0;
    RdsGroups      = 0;
    RdsGroupsTotal = 0;
    ptyDisplay     = 0;
    piDisplay      = 0;
    ctDayHigh      = 0;
    ctDayLow       = 0;
    ctHour         = 0;
    ctMinute       = 0;
    ctOffset       = 0;
}

//-----------------------------------------------------------------------------
// This routine adds an additional level of error checking on the PI code.
// A PI code is only considered valid when it has been identical for several
// groups.
//-----------------------------------------------------------------------------
#define RDS_PI_VALIDATE_LIMIT  4

static void
update_pi(u16 current_pi)
{
    static u8 rds_pi_validate_count = 0;
    static u16 rds_pi_nonvalidated;

    // if the pi value is the same for a certain number of times, update
    // a validated pi variable
    if (rds_pi_nonvalidated != current_pi)
    {
        rds_pi_nonvalidated =  current_pi;
        rds_pi_validate_count = 1;
    }
    else
    {
        rds_pi_validate_count++;
    }

    if (rds_pi_validate_count > RDS_PI_VALIDATE_LIMIT)
    {
        piDisplay = rds_pi_nonvalidated;
    }
}


//-----------------------------------------------------------------------------
// This routine adds an additional level of error checking on the PTY code.
// A PTY code is only considered valid when it has been identical for several
// groups.
//-----------------------------------------------------------------------------
#define RDS_PTY_VALIDATE_LIMIT 4

static void
update_pty(u8 current_pty)
{
    static u8 xdata rds_pty_validate_count = 0;
    static u8 xdata rds_pty_nonvalidated;

    // if the pty value is the same for a certain number of times, update
    // a validated pty variable
    if (rds_pty_nonvalidated != current_pty)
    {
        rds_pty_nonvalidated =  current_pty;
        rds_pty_validate_count = 1;
    }
    else
    {
        rds_pty_validate_count++;
    }

    if (rds_pty_validate_count > RDS_PTY_VALIDATE_LIMIT)
    {
        ptyDisplay = rds_pty_nonvalidated;
    }
}


//-----------------------------------------------------------------------------
// The basic implementation of the Program Service update displays data
// immediately but does no additional error detection.
//-----------------------------------------------------------------------------
static void
update_ps_basic(u8 current_ps_addr, u8 current_ps_byte)
{
    psDisplay[current_ps_addr] = current_ps_byte;
}

//-----------------------------------------------------------------------------
// This implelentation of the Program Service update attempts to display
// only complete messages for stations who rotate text through the PS field
// in violation of the RBDS standard as well as providing enhanced error
// detection.
//-----------------------------------------------------------------------------
#define PS_VALIDATE_LIMIT 2
static void
update_ps(u8 addr, u8 byte)
{
    u8 i;
    bit textChange = 0; // indicates if the PS text is in transition
    bit psComplete = 1; // indicates that the PS text is ready to be displayed

    if(psTmp0[addr] == byte)
    {
        // The new byte matches the high probability byte
        if(psCnt[addr] < PS_VALIDATE_LIMIT)
        {
            psCnt[addr]++;
        }
        else
        {
            // we have recieved this byte enough to max out our counter
            // and push it into the low probability array as well
            psCnt[addr] = PS_VALIDATE_LIMIT;
            psTmp1[addr] = byte;
        }
    }
    else if(psTmp1[addr] == byte)
    {
        // The new byte is a match with the low probability byte. Swap
        // them, reset the counter and flag the text as in transition.
        // Note that the counter for this character goes higher than
        // the validation limit because it will get knocked down later
        if(psCnt[addr] >= PS_VALIDATE_LIMIT)
        {
            textChange = 1;
            psCnt[addr] = PS_VALIDATE_LIMIT + 1;
        }
        else
        {
            psCnt[addr] = PS_VALIDATE_LIMIT;
        }
        psTmp1[addr] = psTmp0[addr];
        psTmp0[addr] = byte;
    }
    else if(!psCnt[addr])
    {
        // The new byte is replacing an empty byte in the high
        // proability array
        psTmp0[addr] = byte;
        psCnt[addr] = 1;
    }
    else
    {
        // The new byte doesn't match anything, put it in the
        // low probablity array.
        psTmp1[addr] = byte;
    }

    if(textChange)
    {
        // When the text is changing, decrement the count for all
        // characters to prevent displaying part of a message
        // that is in transition.
        for(i=0;i<sizeof(psCnt);i++)
        {
            if(psCnt[i] > 1)
            {
                psCnt[i]--;
            }
        }
    }

    // The PS text is incomplete if any character in the high
    // probability array has been seen fewer times than the
    // validation limit.
    for (i=0;i<sizeof(psCnt);i++)
    {
        if(psCnt[i] < PS_VALIDATE_LIMIT)
        {
            psComplete = 0;
            break;
        }
    }

    // If the PS text in the high probability array is complete
    // copy it to the display array
    if (psComplete)
    {
        for (i=0;i<sizeof(psDisplay); i++)
        {
            psDisplay[i] = psTmp0[i];
        }
    }
}

//-----------------------------------------------------------------------------
// The basic implementation of the Radio Text update displays data
// immediately but does no additional error detection.
//-----------------------------------------------------------------------------
static void
update_rt_simple(bit abFlag, u8 count, u8 addr, u8 idata * chars)
{
    u8 errCount;
    u8 blerMax;
    u8 i,j;

    // If the A/B flag changes, wipe out the rest of the text
    if ((abFlag != rtsFlag) && rtsFlagValid)
    {
        for (i=addr;i<sizeof(rtDisplay);i++)
        {
            rtSimple[i] = 0;
        }
    }
    rtsFlag = abFlag;    // Save the A/B flag
    rtsFlagValid = 1;    // Now the A/B flag is valid

    for (i=0; i<count; i++)
    {
        // Choose the appropriate block. Count > 2 check is necessary for 2B groups
        if ((i < 2) && (count > 2))
        {
            errCount = BleC;
            blerMax = rdsBlerMax[2];
        }
        else
        {
            errCount = BleD;
            blerMax = rdsBlerMax[3];
        }

        if(errCount <= blerMax)
        {
            // Store the data in our temporary array
            rtSimple[addr+i] = chars[i];
            if(chars[i] == 0x0d)
            {
                // The end of message character has been received.
                // Wipe out the rest of the text.
                for (j=addr+i+1;j<sizeof(rtSimple);j++)
                {
                    rtSimple[j] = 0;
                }
                break;
            }
        }
    }

    // Any null character before this should become a space
    for (i=0;i<addr;i++)
    {
        if(!rtSimple[i])
        {
            rtSimple[i] = ' ';
        }
    }
}

//-----------------------------------------------------------------------------
// This implelentation of the Radio Text update attempts to display
// only complete messages even if the A/B flag does not toggle as well
// as provide additional error detection. Note that many radio stations
// do not implement the A/B flag properly. In some cases, it is best left
// ignored.
//-----------------------------------------------------------------------------
#define RT_VALIDATE_LIMIT 2
static void
display_rt(void)
{
    bit rtComplete = 1;
    u8 i;

    // The Radio Text is incomplete if any character in the high
    // probability array has been seen fewer times than the
    // validation limit.
    for (i=0; i<sizeof(rtTmp0);i++)
    {
        if(rtCnt[i] < RT_VALIDATE_LIMIT)
        {
            rtComplete = 0;
            break;
        }
        if(rtTmp0[i] == 0x0d)
        {
            // The array is shorter than the maximum allowed
            break;
        }
    }

    // If the Radio Text in the high probability array is complete
    // copy it to the display array
    if (rtComplete)
    {
        for (i=0;i<sizeof(rtDisplay); i++)
        {
            rtDisplay[i] = rtTmp0[i];
            if(rtTmp0[i] == 0x0d)
            {
                break;
            }
        }
        // Wipe out everything after the end-of-message marker
        for (i++;i<sizeof(rtDisplay);i++)
        {
            rtDisplay[i] = 0;
            rtCnt[i]     = 0;
            rtTmp0[i]    = 0;
            rtTmp1[i]    = 0;
        }
    }
}

//-----------------------------------------------------------------------------
// This implementation of the Radio Text update attempts to further error
// correct the data by making sure that the data has been identical for
// multiple receptions of each byte.
//-----------------------------------------------------------------------------
static void
update_rt_advance(bit abFlag, u8 count, u8 addr, u8 idata * byte)
{
    u8 i;
    bit textChange = 0; // indicates if the Radio Text is changing

    if (abFlag != rtFlag && rtFlagValid && !rdsIgnoreAB)
    {
        // If the A/B message flag changes, try to force a display
        // by increasing the validation count of each byte
        // and stuffing a space in place of every NUL char
        for (i=0;i<sizeof(rtCnt);i++)
        {
            if (!rtTmp0[i])
            {
                rtTmp0[i] = ' ';
                rtCnt[i]++;
            }
        }
        for (i=0;i<sizeof(rtCnt);i++)
        {
            rtCnt[i]++;
        }
        display_rt();

        // Wipe out the cached text
        for (i=0;i<sizeof(rtCnt);i++)
        {
            rtCnt[i]  = 0;
            rtTmp0[i] = 0;
            rtTmp1[i] = 0;
        }
    }

    rtFlag = abFlag;    // Save the A/B flag
    rtFlagValid = 1;    // Our copy of the A/B flag is now valid

    for(i=0;i<count;i++)
    {
        u8 errCount;
        u8 blerMax;
        // Choose the appropriate block. Count > 2 check is necessary for 2B groups
        if ((i < 2) && (count > 2))
        {
            errCount = BleC;
            blerMax = rdsBlerMax[2];
        }
        else
        {
            errCount = BleD;
            blerMax = rdsBlerMax[3];
        }
        if (errCount <= blerMax)
        {
            if(!byte[i])
            {
                byte[i] = ' '; // translate nulls to spaces
            }

            // The new byte matches the high probability byte
            if(rtTmp0[addr+i] == byte[i])
            {
                if(rtCnt[addr+i] < RT_VALIDATE_LIMIT)
                {
                    rtCnt[addr+i]++;
                }
                else
                {
                    // we have recieved this byte enough to max out our counter
                    // and push it into the low probability array as well
                    rtCnt[addr+i] = RT_VALIDATE_LIMIT;
                    rtTmp1[addr+i] = byte[i];
                }
            }
            else if(rtTmp1[addr+i] == byte[i])
            {
                // The new byte is a match with the low probability byte. Swap
                // them, reset the counter and flag the text as in transition.
                // Note that the counter for this character goes higher than
                // the validation limit because it will get knocked down later
                if(rtCnt[addr+i] >= RT_VALIDATE_LIMIT)
                {
                    textChange = 1;
                    rtCnt[addr+i] = RT_VALIDATE_LIMIT + 1;
                }
                else
                {
                    rtCnt[addr+i] = RT_VALIDATE_LIMIT;
                }
                rtTmp1[addr+i] = rtTmp0[addr+i];
                rtTmp0[addr+i] = byte[i];
            }
            else if(!rtCnt[addr+i])
            {
                // The new byte is replacing an empty byte in the high
                // proability array
                rtTmp0[addr+i] = byte[i];
                rtCnt[addr+i] = 1;
            }
            else
            {
                // The new byte doesn't match anything, put it in the
                // low probablity array.
                rtTmp1[addr+i] = byte[i];
            }
        }
    }

    if(textChange)
    {
        // When the text is changing, decrement the count for all
        // characters to prevent displaying part of a message
        // that is in transition.
        for(i=0;i<sizeof(rtCnt);i++)
        {
            if(rtCnt[i] > 1)
            {
                rtCnt[i]--;
            }
        }
    }

    // Display the Radio Text
    display_rt();
}


//-----------------------------------------------------------------------------
// When tracking alternate frequencies, it's important to clear the list
// after tuning to a new station. (unless you are tuning to check an alternate
// frequency.
//-----------------------------------------------------------------------------
static void
init_alt_freq(void)
{
    u8 i;
    afCount = 0;
    for (i = 0; i < sizeof(afList)/sizeof(afList[0]); i++)
    {
        afList[i] = 0;
    }
}

//-----------------------------------------------------------------------------
// Decode the RDS AF data into an array of AF frequencies.
//-----------------------------------------------------------------------------
#define AF_COUNT_MIN 225
#define AF_COUNT_MAX (AF_COUNT_MIN + 25)
#define AF_FREQ_MIN 0
#define AF_FREQ_MAX 204
#define AF_FREQ_TO_U16F(freq) (8750+((freq-AF_FREQ_MIN)*10))
static void
update_alt_freq(u16 current_alt_freq)
{
    // Currently this only works well for AF method A, though AF method B
    // data will still be captured.
    u8 dat;
    u8 i;
    u16 freq;

    // the top 8 bits is either the AF Count or AF Data
    dat = (u8)(current_alt_freq >> 8);
    // look for the AF Count indicator
    if ((dat >= AF_COUNT_MIN) && (dat <= AF_COUNT_MAX) && ((dat - AF_COUNT_MIN) != afCount))
    {
        init_alt_freq();  // clear the alternalte frequency list
        afCount = (dat - AF_COUNT_MIN); // set the count
        dat = (u8)current_alt_freq;
        if (dat >= AF_FREQ_MIN && dat <= AF_FREQ_MAX)
        {
            freq = AF_FREQ_TO_U16F(dat);
            afList[0]= freq;
        }
    }
    // look for the AF Data
    else if (afCount && dat >= AF_FREQ_MIN && dat <= AF_FREQ_MAX)
    {
        bit foundSlot = 0;
        static xdata u8 clobber=1;  // index to clobber if no empty slot is found
        freq = AF_FREQ_TO_U16F(dat);
        for (i=1; i < afCount; i+=2)
        {
            // look for either an empty slot or a match
            if ((!afList[i]) || (afList[i] = freq))
            {
                afList[i] = freq;
                dat = (u8)current_alt_freq;
                freq = AF_FREQ_TO_U16F(dat);
                afList[i+1] = freq;
                foundSlot = 1;
                break;
            }
        }
        // If no empty slot or match was found, overwrite a 'random' slot.
        if (!foundSlot)
        {
            clobber += (clobber&1) + 1; // this ensures that an odd slot is always chosen.
            clobber %= (afCount);       // keep from overshooting the array
            afList[clobber] = freq;
            dat = (u8)current_alt_freq;
            freq = AF_FREQ_TO_U16F(dat);
            afList[clobber+1] = freq;
        }
    }

}

//-----------------------------------------------------------------------------
// Decode the RDS time data into its individual parts.
//-----------------------------------------------------------------------------
static void
update_clock(u16 b, u16 c, u16 d)
{

    if (BleB <= rdsBlerMax[1] &&
        BleC <= rdsBlerMax[2] &&
        BleD <= rdsBlerMax[3] &&
        (BleB + BleC + BleD) <= rdsBlerMax[1]) {

        ctDayHigh = (b >> 1) & 1;
        ctDayLow  = (b << 15) | (c >> 1);
        ctHour    = ((c&1) << 4) | (d >> 12);
        ctMinute  = (d>>6) & 0x3F;
        ctOffset  = d & 0x1F;
        if (d & (1<<5))
        {
            ctOffset = -ctOffset;
        }
    }
}

//-----------------------------------------------------------------------------
// After an RDS interrupt, read back the RDS registers and process the data.
//-----------------------------------------------------------------------------
void
updateRds(void)
{
	u8 idata rtblocks[4];
    u8 group_type;      // bits 4:1 = type,  bit 0 = version
    u8 addr;
    u8 errorCount;
    bit abflag;

    RdsDataAvailable = 0;

    // Get the RDS status from the part.
	fmRdsStatus(1, 0);

	// Loop until all the RDS information has been read from the part.
	while(RdsFifoUsed)
    {
   		u8 bler[4];
        u8 field ;
 
		if(GrpLost)
		{
			RdsDataLost++;
		}

        // Gather the latest BLER info
        bler[0] = BleA;
        bler[1] = BleB;
        bler[2] = BleC;
        bler[3] = BleD;

        errorCount = 0;
        RdsGroups++;
        for (field = 0; field <= 3; field++)
        {
            if (bler[field] == UNCORRECTABLE)
            {
                errorCount++;
            }
            else
            {
                RdsValid[field]++;
            }
        }

	    if (errorCount < 4)
	    {
	        RdsBlocksValid += (4 - errorCount);
	    }

	    if (RdsIndicator)
	    {
	        LED = !LED;
	    }

	    RdsIndicator = 100; // Reset RdsIndicator

	    // Update pi code.
	    if (BleA < rdsBlerMax[0])
	    {
	        update_pi(BlockA);
	    }

	    if (BleB <= rdsBlerMax[1])
	    {
	        group_type = BlockB >> 11;  // upper five bits are the group type and version
	        // Check for group counter overflow and divide all by 2
	        if((RdsGroupCounters[group_type] + 1) == 0)
	        {
	            u8 i;
	            for (i=0;i<32;i++)
	            {
	                RdsGroupCounters[i] >>= 1;
	            }
	        }
	        RdsGroupCounters[group_type] += 1;
	    }
	    else
	    {
	        // Drop the data if more than two errors were corrected in block B
	        return;
	    }


	    // Update pi code.  Version B formats always have the pi code in words A and C
	    if (group_type & 0x01)
	    {
	        update_pi(BlockC);
	    }


	    // update pty code.
	    update_pty((BlockB >> 5) & 0x1f);

	    switch (group_type) {
	        case RDS_TYPE_0A:
	            if (BleC <= rdsBlerMax[2])
	            {
	                update_alt_freq(BlockC);
	            }
	            // fallthrough
	        case RDS_TYPE_0B:
	            addr = (BlockB & 0x3) * 2;
	            if (BleD <= rdsBlerMax[3])
	            {
	                if(rdsBasic)
	                {
	                    update_ps_basic(addr+0, BlockD >> 8  );
	                    update_ps_basic(addr+1, BlockD & 0xff);
	                }
	                else
	                {
	                    update_ps(addr+0, BlockD >> 8  );
	                    update_ps(addr+1, BlockD & 0xff);
	                }
	            }
	            break;

	        case RDS_TYPE_2A:
	        {
				rtblocks[0] = (u8)(BlockC >> 8);
				rtblocks[1] = (u8)(BlockC & 0xFF);
				rtblocks[2] = (u8)(BlockD >> 8);
				rtblocks[3] = (u8)(BlockD & 0xFF);
	            addr = (BlockB & 0xf) * 4;
	            abflag = (BlockB & 0x0010) >> 4;
	            update_rt_simple(abflag, 4, addr, rtblocks);
	            update_rt_advance(abflag, 4, addr, rtblocks);
	            break;
	        }

	        case RDS_TYPE_2B:
	        {
	           	rtblocks[0] = (u8)(BlockD >> 8);
				rtblocks[1] = (u8)(BlockD & 0xFF);
				rtblocks[2] = 0;
				rtblocks[3] = 0;
				addr = (BlockB & 0xf) * 2;
	            abflag = (BlockB & 0x0010) >> 4;
	            // The last 32 bytes are unused in this format
	            rtSimple[32]  = 0x0d;
	            rtTmp0[32]    = 0x0d;
	            rtTmp1[32]    = 0x0d;
	            rtCnt[32]     = RT_VALIDATE_LIMIT;
	            update_rt_simple (abflag, 2, addr, rtblocks);
	            update_rt_advance(abflag, 2, addr, rtblocks);
	            break;
	        }
	        case RDS_TYPE_4A:
	            // Clock Time and Date
	            update_clock(BlockB, BlockC, BlockD);
	            break;
	        default:
	            break;
	    }
    
	    // Get the RDS status from the part.
		fmRdsStatus(1, 0);
    }
}

//-----------------------------------------------------------------------------
// Compute the block error rate.
// Returns a value between 0 and BLER_SCALE_MAX (200) which can be used
// to determine the % errors in 0.5% resolution.
//-----------------------------------------------------------------------------
void
si47_rdsGetBler(u16 *bler)
{
    if (RdsBlocksTotal < RdsBlocksValid)
    {
        // The timer which generates the total block count is not synchronized
        // to the RDS signal, so occasionally it looks like we received more
        // valid blocks than we should have. In this case, just assume no
        // errors.
        *bler = 0;
    }
    else if (!RdsBlocksTotal)
    {
        // If the timer has not fired, we will assume 100% errors
        *bler = BLER_SCALE_MAX;
    }
    else
    {
        // Calculate the block error rate
        *bler = BLER_SCALE_MAX - ((RdsBlocksValid * BLER_SCALE_MAX) / RdsBlocksTotal);
    }
}

//-----------------------------------------------------------------------------
// Interrupt service routine to estimate the number of RDS blocks that should
// have been received in a given amount of time.  This routine fires
// approximately every 21.9ms
//-----------------------------------------------------------------------------
void RDS_TIMER_ISR(void) interrupt 5
{
    static u8 ledTimer = 0;

    // If RDS hasn't been detected recently, cause the LED to blink at a slow
    // rate to indicate the board is alive.
    if (!RdsIndicator)
    {
        ledTimer++;
        LED = !!((ledTimer>>2) % 16); // Normally on
    }

    RdsBlocksTotal++;

    // The Si47xx triggers 1 RDS interrupt for 4 RDS blocks. This conditional
    // helps to track the theoretical expected number of groups.
    if (!(RdsBlocksTotal%4))
    {
        RdsGroupsTotal++;
        RdsSynchTotal++;
    }

    // To avoid the counters rolling over, adjust them occasionally
    if (RdsBlocksTotal > (0xFFFF / BLER_SCALE_MAX))
    {
        // Drop about 6% blocks from totals
        RdsBlocksTotal = (RdsBlocksTotal * 15) / 16;
        RdsBlocksValid = (RdsBlocksValid * 15) / 16;
    }

    if (RdsIndicator)
    {
        // Make the RdsIndicator decay if no valid data has been seen for a
        // while.  This get reset to 100 every time RDS is retrieved successfully.
        RdsIndicator--;
    }
    else
    {
        // If it drops all the way to zero, reset the counters to indicate
        // RDS is gone entirely.
        RdsBlocksValid = 0;
        RdsBlocksTotal = 1;
    }

    TF2H = 0;  // Clear overflow flag
    TF2L = 0;  // Clear overflow flag
}

