//-----------------------------------------------------------------------------
// This example code is targeted at the Si47xx parts that support AM RX, FM RX
// or FM TX functionality.  The following parts are supported currently by this
// example code:
//  - Si4704/05       Version 1.0
//  - Si4710/11/12/13 Version 1.0 and 2.0
//  - Si4720          Version 1.0
//  - Si4730/31       Version 1.0
// 
// Version:    0.1
// Target:     C8051F34x
// Tool chain: KEIL C51 6.03 / KEIL EVAL C51
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
// Includes
//-----------------------------------------------------------------------------
#include <c8051f320.h>
#include "typedefs.h"
#include "portdef.h"
#include "si47xxFMRX.h"
#include "FMRXtest.h"
#include "AMRXtest.h"
#include "FMTXtest.h"
#include "WBRXtest.h"

//-----------------------------------------------------------------------------
// Defines
//-----------------------------------------------------------------------------
// System clock selections (SFR CLKSEL)
#define SYS_INT_OSC              0x00        // Select to use internal oscillator
#define SYS_EXT_OSC              0x01        // Select to use an external oscillator
#define SYS_4X_DIV_2             0x02

//-----------------------------------------------------------------------------
// Function prototypes
//-----------------------------------------------------------------------------
void Init_Device(void);
void Port_IO_Init(void);
void Timer_Init(void);
void wait_us(u16 us);
void wait_ms(u16 ms);
void _nop_(void);
void test_FMRX(void);
void test_AMRX(void);
void test_AMSWLWRX(void);
void test_WBRX(void);
void test_FMTX(void);

//-----------------------------------------------------------------------------
// Externals
//-----------------------------------------------------------------------------


//-----------------------------------------------------------------------------
// Main Routine
//-----------------------------------------------------------------------------
void main(void)
{
    PCA0MD &= ~0x40; // Disable Watchdog timer
    VDM0CN = 0x80;   // Enable VDD Monitor Circuit
    IE = 0;          // Disable all interrupts for now.
    Init_Device();   // Initialize C8051F320
    wait_us(100);    // Wait for VDD Monitor output to settle
    RSTSRC = 0x02;   // Enable VDD Monitor as Reset Source
    EA = 1;          // Global Interrupt Enable

    // TODO:  Depending on what part you are using with this example code reorder
    // the tests so the function you will be using is first.  Then place a break
    // point to stop the tests on the first item you will not be testing to stop
    // execution before the tests you do not need to run.

    // FMRXrds or WBRXsame always needs to be the last test that is executed since 
	// the RDS portion of the test will never return.
	test_WBRX();
	test_WBRXsame();       // Tunes to a SAME frequency and waits for
						   // a SAME message to come in and then populates
                           // the variables with the SAME message.
	test_AMSWLWRX();
	test_AMRX();
	test_FMRX();
	test_FMTX();
	test_FMRXrds();        // Tunes to an RDS station and populates
                           // variables with RDS information.
}

//-----------------------------------------------------------------------------
// Test Group Subroutines
//-----------------------------------------------------------------------------
void test_FMRX(void)
{
    test_FMRXtune();             // Resets, inits, tunes to 1 station
    test_FMRXquickAFtune();      // Test a quick tune
    si47xx_getPartInformation(); // This debug command gets information 
								 // about the part.
    test_FMRXvolume();           // Steps through all volume settings
    test_FMRXautoseek();         // Seeks for the strongest stations and then
								 // tunes to each found station once.
    test_FMRXpowerCycle();       // Demonstrates how to power down and power 
								 // up the part
}

void test_AMRX(void)
{
    test_AMRXtune();             // Resets, inits, tunes to 1 station
    si47xx_getPartInformation(); // This debug command gets information 
								 // about the part.
    test_AMRXvolume();           // Steps through all volume settings
    test_AMRXautoseek();         // Seeks for the strongest stations and then
								 // tunes to each found station once.
    test_AMRXpowerCycle();       // Demonstrates how to power down and power 
								 // up the part
}

//AM (Medium Wave), SW (Short Wave), and LW (Long Wave) use the same AM_SW_LW 
//components, thus the commands and properties for these functions are the same.
//For simplicity reasons, the commands and properties only have a prefix AM 
//instead of AM_SW_LW. The main difference among AM, SW, and LW is only the
//frequency range.

void test_AMSWLWRX(void)
{
    test_AMSWLWRXtune();             // Resets, inits, tunes to 1 station
    si47xx_getPartInformation(); // This debug command gets information 
								 // about the part.
    test_AMRXvolume();           // Steps through all volume settings
    test_AMRXpowerCycle();       // Demonstrates how to power down and power 
								 // up the part
}


void test_WBRX(void)
{
    test_WBRXtune();             // Resets, inits, tunes to 1 station
    si47xx_getPartInformation(); // This debug command gets information 
								 // about the part.
    test_WBRXvolume();           // Steps through all volume settings
	test_WBRX_scan();			 // Scans for strongest station and tune to it
	test_WBRXpowerCycle();       // Demonstrates how to power down and power 
								 // up the part
}

void test_FMTX(void)
{
	test_FMTXtune();			// Resets, inits, tunes to 1 station and power
								// level.
	test_FMTXscan();			// Determines five best stations to transmit on
								// and tunes to each station one time.
	test_FMTXasq();				// Demonstrates how to determine the signal level
								// metrics of the input audio.
	test_FMTXrds();				// Demonstrates RDS in TX mode.
	test_FMTXpowerCycle();		// Demonstrates how to power down and power up
								// the part.
}

//-----------------------------------------------------------------------------
// Init_Device
//-----------------------------------------------------------------------------
// - Enables all peripherals needed for the application
void Init_Device(void)
{
    Port_IO_Init();
    Timer_Init();
}

//-----------------------------------------------------------------------------
// Port_IO_Init
//-------------------------
// - C8051F320 Port Initialization
// - Configure the Crossbar and GPIO ports.
//-----------------------------------------------------------------------------
void Port_IO_Init(void)
{
    //P0.0  -  Skipped, Push-Pull,  Digital, SCLK,   default configuration: high
    //P0.1  -  Skipped, Open-Drain, Digital, GPIO1,  default configuration: input
    //P0.2  -  Skipped, Open-Drain, Digital, SDIO,   default configuration: input
    //P0.3  -  Skipped, Push-Pull,  Digital, SENB,
    //P0.4  -  Skipped, Open-Drain, Digital, MC_SDA, default configuration: input
    //P0.5  -  Skipped, Push-Pull,  Digital, MC_SCL, default configuration: high
    //P0.6  -  Skipped, Open-Drain, Digital, GPIO2,  default configuration: input
    //P0.7  -  Skipped, Open-Drain, Digital, GPIO3,  default configuration: input

    //P1.0  -  Skipped, Push-Pull,  Digital, GP1
    //P1.1  -  Skipped, Open-Drain, Digital, GP2
    //P1.2  -  Skipped, Open-Drain, Digital, GP3
    //P1.3  -  Skipped, Push-Pull,  Digital, RSTB
    //P1.4  -  Skipped, Push-Pull,  Digital, MT_RST
    //P1.5  -  Skipped, Push-Pull,  Digital, LED
    //P1.6  -  Skipped, Push-Pull,  Digital, MT_MOSI
    //P1.7  -  Skipped, Push-Pull,  Digital, MT_CCLK, default configuration: high

    //P2.0  -  Skipped, Open-Drain, Digital, MT_MISO, default configuration: input
    //P2.1  -  Skipped, Push-Pull,  Digital, MT_SS,   default configuration: high
    //P2.2  -  Skipped, Push-Pull,  Digital, M_INPUT_SEL
    //P2.3  -  Skipped, Push-Pull,  Digital, M_INPUT_AD
    //P2.4  -  Skipped, Push-Pull,  Digital, M_OUTPUT_AD
    //P2.5  -  Skipped, Open-Drain, Digital, GP4,     default configuration: input
    //P2.6  -  Skipped, Open-Drain, Digital, GP5,     default configuration: input
    //P2.7  -  Skipped, Open-Drain, Digital, GP6,     default configuration: input

    P0MDIN    = 0xFF; // All Port 0 pins are NOT analog
    P0MDOUT   = 0x29;
    P0SKIP    = 0xFF; // All pins should be skipped
    SMB0CF    = 0xC1; // Enable SMBus
    XBR0      = 0x00; // SYSCLK output NOT routed to port pin

    P1MDIN    = 0xFF; // All Port 1 pins are NOT analog
    P1MDOUT   = 0xF9;
    P1SKIP    = 0xFF; // All pins should be skipped
    XBR1      = 0xC0; // Crossbar enabled and pull-ups disabled

    P2MDIN    = 0xFF; // All Port 2 pins are NOT analog
    P2MDOUT   = 0xDE;
    P2SKIP    = 0xFF;

    GPIO1 = 1;        // GPIO1, GPIO2, GPIO3, SDIO
    GPIO2 = 1;        // configured as digital inputs
    GPIO3 = 1;
    SDIO  = 1;
    SCLK  = 1;
    LED   = 1;
    // LED is on.

    MC_SDA = 1;
    MC_SCL = 1;
    MT_MISO = 1;
    MT_SS   = 1;
    MT_CCLK = 1;
    GP4 = 1;
    GP5 = 0;
    GP6 = 1;

    IT01CF = GPIO2_PIN; // /INT0 interrupt is active low; /INT0 Port Pin is GPIO2
    TCON   = 0x01;      // /INT0 is edge-triggered
    EX0    = 1;

}

//-----------------------------------------------------------------------------
// Timer_Init
//-----------------------------------------------------------------------------
void Timer_Init(void)
{
    volatile int x;

    // Select internal oscillator (12MHz) as input to clock multiplier
	CLKMUL  = 0x00;

	CLKMUL |= 0x80;                           // Enable clock multiplier
	CLKMUL |= 0xC0;                           // Initialize the clock multiplier
    for ( x = 0; x < 500; x ) // Wait 5us for initialization
        x++;

	while(!(CLKMUL & 0x20));                  // Wait for multiplier to lock

    // Select USBCLK as 4x clk multiplier and system clock as 4x clk multiplier/2
    CLKSEL  = SYS_INT_OSC;                    // Select USB clock (48MHz)
    CLKSEL |= SYS_4X_DIV_2;                   // Select system clock (24MHz)

    // Enable internal oscillator and set IFCN=11b
	OSCICN |= 0x83;

    // Timer 0 operates in mode 1 (16-bit counter/timer).
    // Note: Timer 0 is used in the function wait_us.
    TMOD = 0x21;

    // Counter/Timer 0 uses the system clock.
    CKCON = 0x0C;

    TCON = 0x01; // Disable Timer 0.

	// Timer2 - RDS block counter
    // Timer is operating in 16-bit timer with autoreload mode, sysclk/12
	TMR2L = TMR2RLL = 0xF3;
	TMR2H = TMR2RLH = 0x54;
	TMR2CN = 0x04;

    // Enable Timer 2 interrupt
	ET2 = 1;

	// Timer3 - SAME timer used to reset the SAME buffer
	// Timer is operating in 16-bit timer with autoreload mode, sysclk/12
	// Setup this timer for every 10mS
	TMR3L = TMR3RLL = 0xDF;
	TMR3H = TMR3RLH = 0xB1;
	TMR3CN = 0x04;

	// Enable Timer 3 interrupt
	EIE1 |= 0x80;
}

//-----------------------------------------------------------------------------
// delay routine for long delays
//
// Inputs:
//      Delay in milliseconds
//-----------------------------------------------------------------------------
void wait_ms(u16 ms)
{
    int i;

    for ( i = 0; i < ms; i++ ) {
        wait_us(1000);
    }
}

//-----------------------------------------------------------------------------
// Delay routine based on hardware timer
//
// Inputs:
//      Delay in microseconds
//-----------------------------------------------------------------------------
void wait_us(u16 us)
{
    u16 j;

    j = 65535u - 24 * us;
    TL0 = j;         // Load Timer 0 low byte
    TH0 = j >> 8;    // Load Timer 0 high byte
    TR0 = 1;         // Enable Timer 0
    TF0 = 0;         // Clear Timer 0 Overflow flag

    while(TF0 != 1); // Wait for Timer 0 to overflow

    TR0 = 0;         // Disable Timer 0
    TF0 = 0;         // Clear Timer 0 Overflow Flag
}

//-----------------------------------------------------------------------------
// delay routine for small delays.  Assume each for loop takes
// at least 64nS, and divide parameter by just doing a shift.
// This routine primarily exists for documentation purposes.
// On this 8051, all waits will be much longer than they need to be.
//
// Inputs:
//		Delay in nanoseconds
//-----------------------------------------------------------------------------
void wait_ns(u16 ns)
{
    u8 i;

    for ( i = 1; i <= ns / 32; i++ )
    ;
}

