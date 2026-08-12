//-----------------------------------------------------------------------------
// Pin/Port definitions for Si47xx BB Rev 1.2, Rev 1.3
//
// Most pin names are self explanitory and match the name of the Si47xx pin
// On most daughtercards, the less obvious are commented below.
//
//-----------------------------------------------------------------------------

#ifndef _PORTDEF_H_
#define _PORTDEF_H_

sbit SCLK        = P0^0;  // Routed to SCLK on the Si47xx Part
sbit GPIO1       = P0^1;  // Routed to GPIO1 on the Si47xx Part
sbit SDIO        = P0^2;  // Routed to SDIO on the Si47xx Part
sbit SENB        = P0^3;  // Routed to SENB on the Si47xx Part
sbit MC_SDA      = P0^4;  // Routed to SDIN on the WM8731 Codec
sbit MC_SCL      = P0^5;  // Routed to SLCK on the WM8731 Codec
sbit GPIO2       = P0^6;  // Routed to GPIO2 on the Si47xx Part
sbit GPIO3       = P0^7;  // Routed to GPIO3 on the Si47xx Part

sbit GP1         = P1^0;  // Used to switch the headphone amp on/off
sbit GP2         = P1^1;  // Not currently in use
sbit GP3         = P1^2;  // Not currently in use
sbit RSTB        = P1^3;  // Routed to RSTB on the baseboard
sbit MT_RST      = P1^4;  // Routed to RST_B on the CS8427 SPDIF transceiver
sbit LED         = P1^5;  // Controls the LED on the baseboard
sbit MT_MOSI     = P1^6;  // Routed to CDIN on the CS8427 SPDIF transceiver
sbit MT_CCLK     = P1^7;  // Routed to CCLK on the CS8427 SPDIF transceiver

sbit MT_MISO     = P2^0;  // Routed to CDOUT on the CS8427 SPDIF transceiver
sbit MT_SS       = P2^1;  // Routed to CS_B on the CS8427 SPDIF transceiver
sbit M_INPUT_SEL = P2^2;  // Configures codec/SPDIF selection for LIN/RIN
sbit M_INPUT_AD  = P2^3;  // Configures analog/digital on LIN/RIN on Si47xx
sbit M_OUTPUT_AD = P2^4;  // Configures analog/digital on LOUT/ROUT on Si47xx
sbit GP4         = P2^5;  // Not currently in use
sbit GP5         = P2^6;  // Configures switch for jumper J44 on baseboard
sbit GP6         = P2^7;  // Configures switch for jumper J45 on baseboard

// The following macros are used to help change port direction so the code
// can be in a common place.  

// SDIO
#define SDIO_HEX 0x04
#define SDIO_OUT P0MDOUT

// GPIO1
#define GPIO1_HEX 0x02
#define GPIO1_OUT P0MDOUT

// GPIO2
#define GPIO2_HEX 0x40
#define GPIO2_OUT P0MDOUT
#define GPIO2_PIN 0x06  // GPIO2 must be on port 0 to support interrupt capability.

// GPIO3
#define GPIO3_HEX 0x80
#define GPIO3_OUT P0MDOUT

// GP1
#define GP1_HEX 0x01
#define GP1_OUT P1MDOUT

// GP2
#define GP2_HEX 0x02
#define GP2_OUT P1MDOUT

// GP3
#define GP3_HEX 0x04
#define GP3_OUT P1MDOUT

#endif
