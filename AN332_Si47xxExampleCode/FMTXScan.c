//-----------------------------------------------------------------------------
// Includes
//-----------------------------------------------------------------------------
#include <c8051f320.h>
#include <stddef.h>
#include "typedefs.h"
#include "si47xxFMTX.h"

//-----------------------------------------------------------------------------
// Defines
//-----------------------------------------------------------------------------
#define NUM_SCAN_REGIONS 5
#define WEIGHT_MINUS2  1
#define WEIGHT_MINUS1  2
#define WEIGHT_CURRENT 6
#define WEIGHT_PLUS1   2
#define WEIGHT_PLUS2   1

#define SCAN_LOWER_LIMIT 8750
#define SCAN_UPPER_LIMIT 10790
#define SCAN_SPACING     20
#define TOTAL_STATIONS   103

//-----------------------------------------------------------------------------
// Global variables
//-----------------------------------------------------------------------------
u16 xdata scan_frequencies[NUM_SCAN_REGIONS];
u16 xdata scan_limits[NUM_SCAN_REGIONS];// = {9150, 9550, 9950, 10350, 10790};
u8 xdata scan_rnl[TOTAL_STATIONS];

//-----------------------------------------------------------------------------
// Function prototypes
//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
// This routine scans the band using the receive power scan functionality
// on the transmitter.  It uses a weighted average to choose the best station
// to transmit on in each band.
//
// There probably is a more efficient way of doing this but it is simplified
// for demonstration purposes.
//-----------------------------------------------------------------------------
u8 si47xxFMTX_scan(void) {
    u8 i;
	u8 current_region;
    u16 current_frequency;
	u16 weight;
	u16 average;
	u16 xdata scan_weighted[NUM_SCAN_REGIONS];  // Local array containing
                                                // the weighted averages.

	// Initialize the scan limits
	scan_limits[0] = 9150;
	scan_limits[1] = 9550;
	scan_limits[2] = 9950;
	scan_limits[3] = 10350;
	scan_limits[4] = 10790;

    // initialize scan arrays
    for ( i = 0; i < NUM_SCAN_REGIONS; i++ ) {
        scan_frequencies[i] = 0;
        scan_weighted[i] = 65535;
    }

	// Loop and measure all the stations in the band using txTuneMeasure
	i=0;
	current_frequency = SCAN_LOWER_LIMIT;
	for (; current_frequency <= SCAN_UPPER_LIMIT; current_frequency += SCAN_SPACING, i++)
	{
		// Call the measure functionality to put the part in measurement mode.
		si47xxFMTX_tuneMeasure(current_frequency);

		// Store the current rnl measurement into the array.
		scan_rnl[i] = si47xxFMTX_get_rnl();
	}

	// Loop through the results and pick the best stations to tune
	// to based on the weighted averages.
	i=0;
	current_region = 0;
	current_frequency = SCAN_LOWER_LIMIT;
	for (; current_frequency <= SCAN_UPPER_LIMIT; current_frequency += SCAN_SPACING, i++)
	{
		// If this frequency is equal to or greater than the scan region increment
        // the scan region
        if(current_frequency > scan_limits[current_region])
			current_region++;

		// Now calculate the weighted average for the current frequency
		average = scan_rnl[i] * WEIGHT_CURRENT;
		weight = WEIGHT_CURRENT;
		if(i >= 2) 
		{
			average += scan_rnl[i-2] * WEIGHT_MINUS2;
			weight += WEIGHT_MINUS2;
		}
		if(i >= 1)
		{
			average += scan_rnl[i-1] * WEIGHT_MINUS1;
			weight += WEIGHT_MINUS1;
		}
		if(i < (TOTAL_STATIONS - 1))
		{
			average += scan_rnl[i+1] * WEIGHT_PLUS1;
			weight += WEIGHT_PLUS1;
		}
		if(i < (TOTAL_STATIONS - 2))
		{
			average += scan_rnl[i+2] * WEIGHT_PLUS2;
			weight += WEIGHT_PLUS2;
		}
		average = average / weight;

		// Now if the weighted average is less than the one stored
		// for the current region then replace it with this average
		if(scan_weighted[current_region] >= average)
		{
			scan_weighted[current_region] = average;
			scan_frequencies[current_region] = current_frequency;
		}
    }	

	return NUM_SCAN_REGIONS;
	
}
