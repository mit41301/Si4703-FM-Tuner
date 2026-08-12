//-----------------------------------------------------------------------------
// Includes
//-----------------------------------------------------------------------------
#include <c8051f320.h>
#include <stddef.h>
#include "typedefs.h"
#include "io.h" // To double check prototypes
#include "portdef.h" // Contains all port definitions

//-----------------------------------------------------------------------------
// Defines
//-----------------------------------------------------------------------------
#define READ    1
#define WRITE   0
#define IO3W_CTLWORD_WRITE  (0x00)
#define IO3W_CTLWORD_READ   (0x20)
#define IO3W_DELAY_US 1
#define IO3W_ADDR           (0xA0)

//-----------------------------------------------------------------------------
// Global variables
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
// Function prototypes
//-----------------------------------------------------------------------------

void wait_ns(u16 ns);
void wait_us(u16 us);
void _nop_(void);


//-----------------------------------------------------------------------------
// Externals
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
// This routine writes the address prior to writing any data to the part
//
// Inputs:
//       addr:      The address to write
//       operation: Specifies read or write operation
//-----------------------------------------------------------------------------
io3w_rawAddr(u8 addr, u8 operation)
{
    i8 i;
    u16 controlWord = 0x0000;

    SDIO_OUT |= SDIO_HEX;  // Configure SDIO as push-pull

    // Make room for READ/WRITE by shifting most sigificant 3 bits up.
    controlWord |= ((u16)(addr & 0xE0) << 1) | (addr & 0x1F);  
	
	// Add the READ/WRITE bit to the control word
    controlWord |= (operation == READ) ? IO3W_CTLWORD_READ : IO3W_CTLWORD_WRITE;
	controlWord |= (addr & 0x1F);

    SENB = 0;
    for (i=8; i>=0; i--)
    {
        SCLK = 0;
        SDIO = ((controlWord >> i) & 0x01);
        wait_ns(25);         // tLow
        SCLK = 1;
        wait_ns(25);         // tHigh
    }
}

//-----------------------------------------------------------------------------
// This routine writes one byte of data onto the 3 wire interface
//
// Inputs:
//       data_out: The data to write
//-----------------------------------------------------------------------------
void io3w_rawDataOut(u8 data_out)
{
    i8 i;
    for (i=7; i>=0; i--)
    {
        SCLK = 0;
        SDIO = ((data_out >> i) & 0x01);
        wait_ns(25);   // tLow
        SCLK = 1;
        wait_ns(25);   // tHigh
    }

    // Wait for the part to write the data
    wait_us(IO3W_DELAY_US);
}

//-----------------------------------------------------------------------------
// This routine reads one byte of data from the 3 wire interface
//
// Inputs:
//       data_in: The variable containing the data to read
//-----------------------------------------------------------------------------
void io3w_rawDataIn(u8 idata *data_in)
{
    i8 i;

    // Wait for control ready / time
	wait_us(IO3W_DELAY_US);

    SDIO_OUT &= ~(SDIO_HEX);  // Configure SDIO as open-drain
    SDIO = 1;                 // Configure SDIO as a digital input

    for (i=0; i<8; i++)
    {
        SCLK = 0;
        wait_ns(25);   // tLow
        SCLK = 1;
        wait_ns(25);   // tHigh
        *data_in = ((*data_in << 1) | SDIO);
    }
}

//-----------------------------------------------------------------------------
// This routine stops the 3 wire command
//-----------------------------------------------------------------------------
void io3w_rawStop(void) 
{
    // end the transaction
    SCLK = 0;
    wait_ns(5);        // tLOW - tS
    SENB = 1;
    wait_ns(20);       // tS
    SCLK = 1;
    wait_ns(25);       // tHIGH
    SCLK = 0;
    
    SDIO_OUT |= SDIO_HEX;  // Configure SDIO as push-pull
    SDIO = 1;                                                
}

//-----------------------------------------------------------------------------
// This routine transfers 2 bytes of data between the host and Si47xx.
//
// Inputs:
//		operation:  Specifies read or write operation
//		addr:		Address for register read or write
//		data_value:  The value of the data to write
//
// Pin drive status upon exit:
//      SDIO = high (push-pull output)
//      SCLK = low
//      SENB = high
//-----------------------------------------------------------------------------
void io3w_operation(u8 operation, u8 addr, u16 idata *data_value)
{
	io3w_rawAddr(addr, operation);  // Write the address to read or write

    // read or write data
    if (operation == WRITE)
    {
        io3w_rawDataOut((u8)(*data_value >> 8));  // Write the high byte
        io3w_rawDataOut((u8)(*data_value >> 0));  // Write the low byte
    }
    else  // READ
    {
        io3w_rawDataIn(((u8*)data_value) + 0);
        io3w_rawDataIn(((u8*)data_value) + 1);
    }

	io3w_rawStop();  // Stop the operation
}

//-----------------------------------------------------------------------------
// This routine reads an array of bytes from the si47xx part
//
// Inputs:
//		number_bytes: Number of bytes to read
//		data_in:	Destination array for data read
//
//-----------------------------------------------------------------------------
void io3w_read(u8 number_bytes, u8 *data_in)
{
	u16 idata tmp;
    u8 i;

	// Loop and read back the 16 bit data from the part and convert to 8 bit data.
    for (i=0; i < (number_bytes+1)/2; i++)
    {
        io3w_operation(READ, IO3W_ADDR | (i+8), &tmp);
		data_in[(i*2)+0] = (u8)(tmp >> 8);
        if (i < number_bytes)
        {
            data_in[(i*2)+1] = (u8)(tmp >> 0);
        }
    }

}

//-----------------------------------------------------------------------------
// This routine writes an array of bytes to the Si47xx.
//
// Inputs:
//		number_bytes: Number of bytes to write
//		data_out:	  Source array for data to be written
//
//-----------------------------------------------------------------------------
void io3w_write(u8 number_bytes, u8 *data_out)
{
	u16 idata regData[8] = {0,0,0,0,0,0,0,0};
	u16 tmp;
	u8 i;

	// Take up to 16 elements in the input array and convert those
    // values to a 8 element 16 bit array
    for (i=0; i<number_bytes; i++)
    {
        tmp = (u16)(data_out[i]);
        regData[i/2] |= tmp << (8 * (1 - (i%2)));
    }

    // Now write the elements via the 3w interface
    for (i=1; i < (number_bytes+1)/2; i++)
    {
        io3w_operation(WRITE, IO3W_ADDR | i, &(regData[i]));
    }
	io3w_operation(WRITE, IO3W_ADDR | 0, &(regData[0]));
}
