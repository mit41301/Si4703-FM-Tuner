//-----------------------------------------------------------------------------
//
// io.h
//
// Contains the function prototypes for the functions contained in io2w.c and
// io3w.c
//
//-----------------------------------------------------------------------------
#ifndef _IO_H_
#define _IO_H_

//-----------------------------------------------------------------------------
// Function prototypes
//-----------------------------------------------------------------------------
void io2w_write(u8 number_bytes, u8 *data_out);
void io2w_read(u8 number_bytes, u8 *data_in);
void io3w_write(u8 number_bytes, u8 *data_out);
void io3w_read(u8 number_bytes, u8 *data_in);

#endif
