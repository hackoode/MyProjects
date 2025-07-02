/************************************************************************/
/*								  			 							*/
/*	FILE:  		hsmain2.c				       	  						*/
/*	CREATED:	05/28/02	     by	WFR			   						*/
/*	COMMENTS:	Firmware for High Speed Digital Transducer		     	*/
/*				Uses Cygnal C8051F005       							*/
/*				Supports digital and analog outputs                   	*/
/*                                                                      */
/*	Part Number 084-1406-00	   Ver. 1.13			                	*/
/*  Copyright (c) 2001, 2002 All Rights Reserved		              	*/
/*  Sensotec, Inc. Columbus, Ohio  USA                                  */
/*																		*/
/************************************************************************
REVISION HISTORY:
v1.13   07/11/03    Changed default value ov vmon_scale to 0.00146
v1.12   04/24/03    Added ? option to II cmd;'BZ' cmd default to 100
v1.11   04/07/03    Mod. MFG part # stuff for double dash
v1.10   08/20/02    Watchdog timer kicked after a FLASH erase fixing reset
                    problem with the W1 cmd, (it did two writes to FLASH
                    and Watchdog timed out.)
v1.09   07/23/02    Using watchdog timer, set to max. interval of 47.4ms 
                    with 22 MHz xtal. changed 'D0' command: if 
                    checksum failure is present, 'D0' responds with 
                    error message instead of value. 'SV' cmd also used for
                    analog output when checksum failure is detected.  
                    Checksum test is now ongoing, one byte per main loop.
                    Data valid flag for FLASH parameters is now a #define, 
                    change value to be mix of ones and zeros.

v1.08   07/19/02    Added user-forced analog output:'SU' selects between
                    xdcr and user for analog output. 'SV' sets default 
                    value for user mode, used at power up and when first
                    selected, 'SA' sets value for analog output in user
                    mode (no write enable required);
                    'DC' cmd displays temp in DegC            
v1.07   07/08/02    Changed Ix cmd to IIx to be consistent with 2 byte
                    command structure & to work with utility software, 
                    parameter saved each time it's changed now; removed
                    'V' from display analog response and from PN version;
                    removed 'SN' from serial number response; universal
                    address changed from '00' to 'ff', default is still
                    '00'; defaults for DACs is 100 now; temp_bounds 
                    defaults based on one dozen units now
v1.06   06/06/02    Add baudrate dependent time delay before transmit
v1.05   05/28/02    Command format changes: remove trailing 'F' from
                    temperature reading ('DT'), 
v1.02   05/16/02    Added 'F0' cmd for digital reading to work with 
                    utility software
V1.01   05/08/02    Fixed FACT_SPAN_ADDR overwrite of MFG_PART_ADDR
v1.00   04/16/02    Added software reset with factory or write enable
v0.95   04/09/02    Added error for over/under range on DH cmd; Set both
                    DACs to same value at startup, before main loop
v0.94   03/29/03    Added test of floats for Cx, Lx, Hx commmands; updated
                    defaults for temp boundaries, max/min pressure & temp
                    from first prototype test
v0.93   03/21/02    Changed hardware for temp signal ~= 0 volts @ room 
                    temp, allows gain = 4, increases resolution, add 2048 
                    to ADC result for temp, average 4 temp rdgs in 
                    ADC_isr() for result to main
v0.92   03/18/02    Change default for DAC0 from 100 to 250, same as
                    factory_zero; Added factory_cal parameter to 
                    calibrate analog output gain, allows span counts
                    to remain constant
v0.91   03/14/02    Gain for temp signal reduced to 1 for present
                    hardware
v0.90   02/25/02    Readback function for voltage out, First build of
                    target version;
v0.80   02/13/02    Conditional compile for transducer hardware.
                    Hardware change: DAC0 is zero offset of voltage
                    output. Calculate temperature in DegF and output, 
                    requires 4 coefficients. Digital output gives error
                    message when raw pressure signal is out of range.
v0.70   01/21/02    Checksum on FLASH data, backup copy of FLASH data,
                    test raw ADC readings for out of range, implement
                    status byte for error conditions
v0.60   01/14/02    Add user defined averaging, fixed pointer problem
                    in write_char_FLASH()
v0.50   01/08/02    Mod. for 3 temp ranges with 3 sets of coefficients,
                    Mod. FLASH routines for access to all 512 bytes
                    of one page
v0.40   01/07/02    Add support for 22.1184 MHz xtal
v0.30   10/17/01    Add temp rdg. controlled by timer, baud rate stuff
v0.20   10/11/01    Add nonvolatile storage for floats, chars in FLASH, 
v0.10	10/10/01	Start with ADC routine, interrupt driven by timer3 
                    overflow; serial interrupt state machine

*****************************************************************************/

                                                    /* compiler directives	*/
/****************************************************************************/
#pragma DEBUG OBJECTEXTEND CODE   

                                                    /* control defines		*/
/****************************************************************************/

#define _22MHZ_       // define for use with 22.1184 MHz xtal
                        // if not defined, defaults to 11.0592 MHz xtal

#define _XDCR_        // define for transducer hardware
                          // if not defined, defaults to evaluation board

                                                    /* includes		        */
/****************************************************************************/

#include <c8051f005.h>
//#include <stdio.h>
#include <intrins.h>
//#include <math.h>
#include <ctype.h>
#include <string.h>
#include <stdlib.h>
#include <float.h>
                                                    /* defines		        */
/****************************************************************************/


                                // errors associated with commmands
#define NOT_A_COMMAND       "NaC"
#define ACCESS_DENIED       "AcD"
#define NOT_A_NUMBER        "NaN"
#define OUT_OF_RANGE        "OoR"       // not used yet 2/25/02
#define INVALID_FORMAT      "InF"       // ??need this
#define CHECKSUM_FAIL       "CsF"
#define OVER_RANGE          "OvR"
#define UNDER_RANGE         "UnR"


#define PNVERSION       "084-1406-00 1.13"

#ifdef _22MHZ_
#define TIMER3_RELOAD       74
#else
#define TIMER3_RELOAD       37      // 46 for 50 us interrupt (55 for 60 us.)
#endif                              // 37 for 40 us
                                    // 28 for 30 us: too fast, temp rdg corrupted

         	  			            // states for serial recv. state machine     
#define STATE_SYNC          0
#define STATE_ADDR_1	    1
#define STATE_ADDR_2	    2
#define STATE_CMD_1		    3
#define STATE_CMD_2		    4
#define STATE_DATA		    5                                  				 
#define SYNC		    	0x23    // sync character is # 

                                    // timer0 reload values: system tick		
#define RELOAD_HIGH		    0x4b    // for 50ms w/ 11MHz xtal
#define RELOAD_LOW		    0xfd    // for 25ms w/ 22MHz xtal

/////software timers for timer0
#ifdef _22MHZ_                      // values for 22 MHZ xtal, 25ms system tick
#define RECV_TIMEOUT        201     // 5 seconds 
#define TEMP_TIMER_RELOAD    40     // 1 second 
#else                               // values for 11 MHZ xtal, 50ms system tick
#define RECV_TIMEOUT	    101     // serial receiver timeout: 5 seconds
#define TEMP_TIMER_RELOAD    20     // temperature taken: 20*50ms=1 sec
#endif
                                    // baudrate timer2 low byte reload values
#ifdef _22MHZ_                      // high byte is handle by code
#define BAUD12              0xC0
#define BAUD24              0xE0
#define BAUD48			    0x70
#define BAUD96			    0xB8
#define BAUD192			    0xDC
#define BAUD384			    0xEE
#define BAUD576			    0xF4
#define BAUD1152		    0xFA
#define BAUD2304            0xFD    // not used

#else                               // values for 11.0592 MHz xtal
#define BAUD12              0xE0
#define BAUD24              0x70
#define BAUD48			    0xB8
#define BAUD96			    0xDC
#define BAUD192			    0xEE
#define BAUD384			    0xF7
#define BAUD576			    0xFA
#define BAUD1152		    0xFD
                                    // baudrates not yet supported;
                                    // 14.4K    0xFFE8
                                    // 28.8K    0xFFF4
#endif

/*
    use timer1 to time a baudrate dependent delay
     before sending first char of response
*/
#ifdef _22MHZ_
#define DELAY12     0xBF32
#define DELAY24     0xDF99
#define DELAY48     0xF028
#define DELAY96     0xF813
#define DELAY192    0xFBAD
#define DELAY384    0xFDD6
#define DELAY576    0xFE8E
#define DELAY1152   0xFF47
#else
#define DELAY12     0xDF98
#define DELAY24     0xEFCC
#define DELAY48     0xFB13
#define DELAY96     0xFC09
#define DELAY192    0xFDD6
#define DELAY384    0xFEEB
#define DELAY576    0xFF47
#define DELAY1152   0xFFA3
#endif


#define BUFF_MAX_SIZE	    20
#define COEFF_MAX_NUM       7       // space reserved for max. of 10
                                    // 1-9-02:3 sets of coeffs used now, 7 each
#define ASCII_OFFSET        0x30

#define DATA_FLASH_ADDR     0x7A00  // last 512 bytes of FLASH available:
                                    // page @ 0x7C00 contains security 
                                    //   lock bytes, can NOT be erased by 
                                    //   software
#define DEFAULT_FLASH_ADDR  0x7800

#define DATA_XRAM_ADDR      0x0600  // last 512 bytes of XRAM available
#define DATA_LENGTH         0x01FF   // 0x1FF is maximum length (one FLASH page)
                                    
#define DATA_FLAG       0x00
                                    // address offset for data storage

#define DAC0_ADDR           0x98
#define USER_CAL_DATE_ADDR  0x93
#define USER_SCALE_ADDR     0x8E
#define USER_OFFSET_ADDR    0x89
#define USER_STR_ADDR       0x78
//#define MFG_PART_ADDR       0x6C  // need more space for double dash part #s
#define USER_DIG_GAIN_ADDR  0x67
#define USER_DIG_ZERO_ADDR  0x62
#define FACT_SPAN_ADDR      0x5D    //v1.01 fix; was 0x6D
#define FACT_ZERO_ADDR      0x58
#define EU_STR_ADDR         0x53
#define EU_CONV_ADDR        0x4E
#define RANGE_ADDR          0x49
#define SER_NUM_ADDR        0x40
#define CAL_DATE_ADDR       0x37
#define UNIT_ADDR           0x34
#define BAUD_ADDR           0x32
// address offset 0x00 - 0x31 reserved for coefficients, mid range

#define TEMP_COEFF_ADDR_OFFSET  0x9D
#define LOW_COEFF_ADDR_OFFSET   0xBA    // 7 floats w/flag byte: 35 bytes
#define HIGH_COEFF_ADDR_OFFSET  0xDD

#define MIDTOLOW_ADDR   0x0100
#define LOWTOMID_ADDR   0x0105
#define HIGHTOMID_ADDR  0x010A
#define MIDTOHIGH_ADDR  0x010F
#define USER_RATE_ADDR  0x0114

#define MAX_TEMP_ADDR   0x0116
#define MIN_TEMP_ADDR   0x011B
#define MAX_PRESS_ADDR  0x0120
#define MIN_PRESS_ADDR  0x0125

#define V_MON_OFFSET_ADDR   0x012A
#define V_MON_SCALE_ADDR    0x012F
#define FACT_ANA_CAL_ADDR   0x0134
#define XMIT_DELAY_ADDR     0x0139
// next available location 0x013C
#define FORCED_OUT_FLAG_ADDR        0x013C     // char, flag:  0=xdcr, 1=user control
#define FORCED_OUT_DEFAULT_ADDR     0x013E     // float, default value 
//next location  0x0143 
#define MFG_PART_ADDR       0x0143  // new location for double dash part #
//next location 0x0152


#define LOW         1
#define MID         2
#define HIGH        3

#define MAX_NUM_TEMP_COEFF          4


                                              	    /* function prototypes	*/
/****************************************************************************/

void sysclk_init (void);
void xbar_init(void);

void timer3_init(void);

void ADC_init(void);
void ADC_enable(void);

void int_to_ascii (char *ptr, long n);
char flt_to_sci(char *str, float f);

void hextoascii(unsigned char num, unsigned char *ptr); 

void ExecuteCommand (void);
void xmit_OK(void);
void xmit_ERR(unsigned char *error_code);

void copy_FLASH_to_XRAM(void);
void erase_FLASH(void);
void copy_XRAM_to_FLASH(void);
void write_float_FLASH(unsigned int data_addr, float *fltptr);
void read_float_FLASH(unsigned int data_addr, float *fltptr);
void write_char_FLASH(unsigned int data_addr, unsigned char length,
                      unsigned char *charptr);
void read_char_FLASH(unsigned int data_addr, unsigned char length,
                     unsigned char *charptr);

void init_addr(void);                      
void init_baudrate(void);

void Wait5(void);

void init_coeff(void);
void compensate (void);
void init_digital_constants(void);

void check_range(void);
void init_high_coeff(void);
void init_low_coeff(void);
void init_temp_bounds(void);
void init_average(void);
void analog_out(void);
void init_analog_constants(void);

void set_checksum_XRAM(void);
bit verify_checksum_FLASH (void);
void erase_DEFAULT(void);
void copy_XRAM_to_DEFAULT(void);
void copy_DEFAULT_to_XRAM(void);

void init_DAC0(void);

void sint_to_ascii (char *ptr, int n);

void init_temp_coeff(void);
void init_vmon_coeff(void);

void soft_reset(void);

void forced_output(void);
void init_forced_out(void);



                                      	   	        /* globals          	*/
/****************************************************************************/


unsigned int result_p;          // ADC_isr updates these
unsigned int result_t;

unsigned int inter_p;           // while(1) copies above to these for processing
unsigned int inter_t;

float pressure;
float temperature;
float presscal;                 // calculated pressure, output of comp. equation

float xdata dig_m;
float xdata dig_b;

float xdata ana_m;       // check impact of putting these in xdata
float xdata ana_b;  

float xdata vmon_scale;     // use to cal Vout readback function
float xdata vmon_offset;  

sfr16 ADC0 = 0xBE;              // define ADC output as 16 bit register

sbit TIMINGBIT = P2^0;          // bit to test timing 
sbit driver485 = P1^1;          // RS485 driver enable
                     
bit CommandWaiting;
bit WriteEnable;
bit FactoryEnable;
bit newdata;
  
unsigned char unit_addr_hi;  
unsigned char unit_addr_lo;
                             // try the following in idata
unsigned char ser_xmit_ctr = 0;
unsigned char ser_xmit_length = 0;
unsigned char ser_recv_ctr = 0;
unsigned char ser_current_state;
unsigned char recv_timer;        // max timeout ~12.5 sec w/ 8 bit ctr.(11MHz)
   
unsigned char idata ser_xmit_buffer[BUFF_MAX_SIZE] = {
    // Flush UART RX buffer to avoid residual data
    while (RI) {
        volatile char junk = SBUF;
        RI = 0;
    }
0};
unsigned char idata ser_recv_buffer[BUFF_MAX_SIZE] = {0};
             
unsigned char idata command_buffer[BUFF_MAX_SIZE] = {0};

float idata coeff[COEFF_MAX_NUM] = {0}; // can these be in data, not idata???

unsigned char take_temp_timer;  // init in main() before forever loop
                                // value of 20 for 1 second intervals

unsigned char check_range_timer;
unsigned char previous_range;

unsigned int xdata mid_to_low;
unsigned int xdata low_to_mid;
unsigned int xdata high_to_mid;
unsigned int xdata mid_to_high;

unsigned int xdata max_temp;
unsigned int xdata min_temp;

unsigned int xdata max_press;
unsigned int xdata min_press;

unsigned char user_defined_rate;

unsigned char bdata status;
sbit hightemp = status^0;
sbit lowtemp = status^1;
sbit highpress = status^2;
sbit lowpress = status^3;
sbit chksumfail = status^6;

bit overrange;
bit underrange;

float xdata temp_coeff[4];

bit  DelayDone;

unsigned char xdata timer1reload_high;
unsigned char xdata timer1reload_low;

bit forced_out_flag;
float xdata forced_out_value;

unsigned char code *ptr_FLASH;
unsigned char sum;
bit recentupdate;

float xdata analog_default;


                                              	    /* functions        	*/
/****************************************************************************/

/////////////////////////////////////////////////////////////////////////////
//	Function name:	serial_isr ()
//	
//	Function handles all serial communication. A state machine processes
//		incoming characters after a SYNC character is recognized,  
// 		constructs a command string, and then sets a flag to notify
//  		main loop that a command needs processing.
// 	Function handles outgoing character strings after main program starts
// 		a transmisson by setting SBUF to first character of string.
//
/////////////////////////////////////////////////////////////////////////////

void serial_isr (void) interrupt 4  using 1     // "using 0": comp.routine
{ unsigned char c;                              // crashed serial comm
   
       	if (_testbit_(RI))
 	    {	c = SBUF;
      					// implement state machine for receiving characters	
 	        switch (ser_current_state)
               {
                 case STATE_SYNC:           //looking for sync character
                 	if (c == SYNC)
                        {
                           ser_current_state = STATE_ADDR_1;
                           recv_timer = RECV_TIMEOUT;
                         }  
                 break;
                        
                 case STATE_ADDR_1:         // high byte of address   
                        if (isalnum(c))
                          {
                           if ((c==unit_addr_hi) || (c== 'f'))
                             {
                              ser_current_state = STATE_ADDR_2;
                              recv_timer = RECV_TIMEOUT;
                              break;
                              }
                           }
                        if (c==SYNC)       //if SYNC again, stay here
                          {
                              ser_current_state = STATE_ADDR_1;
                              recv_timer = RECV_TIMEOUT;
                              break;
                           }                          
                        ser_current_state = STATE_SYNC;    //else start over
                        recv_timer = 0;
                 break;
                
                 case STATE_ADDR_2:         // low byte of address
                        if (isalnum(c))
                          {
                           if ((c==unit_addr_lo) || (c == 'f'))
                             {
                              ser_current_state = STATE_CMD_1;
                              recv_timer = RECV_TIMEOUT;
                              break;
                              } 
                           }
                        ser_current_state = STATE_SYNC;
                        recv_timer = 0;
                 break;
         
                 case STATE_CMD_1:          // high byte of command
                        if (isalpha(c))
                          {
                            ser_recv_buffer[ser_recv_ctr] = c;
                            ser_recv_ctr++;
                            ser_current_state = STATE_CMD_2;
                            recv_timer = RECV_TIMEOUT;
                            break;
                           }
                         ser_current_state = STATE_SYNC;
                         recv_timer = 0;
                 break;
                 
                 case STATE_CMD_2:          // low byte of command
                        if (isalnum(c))
                          {
                            ser_recv_buffer[ser_recv_ctr] = c;
                            ser_recv_ctr++;
                            ser_current_state = STATE_DATA;
                            recv_timer = RECV_TIMEOUT;
                            break;
                           }
                        ser_current_state = STATE_SYNC;
                        ser_recv_ctr = 0;
                        recv_timer = 0;
                 break;
                 
                 case STATE_DATA:            // data for command if necessary
                    if (c == 0x0d)           //  <CR>, i.e. end of command
                      {
                        ser_recv_buffer[ser_recv_ctr] = 0x00;
                        memcpy (command_buffer, ser_recv_buffer, ser_recv_ctr +1);
                        CommandWaiting = 1;
                        TR1 = 1; 
                        ser_recv_ctr = 0;
                        ser_current_state = STATE_SYNC;
                        recv_timer = 0;
  		         		break;               
                       }
                    if ((isalnum(c)) || (ispunct(c)))
                      {
                        ser_recv_buffer[ser_recv_ctr] = c;
                        ser_recv_ctr++;
                        recv_timer = RECV_TIMEOUT; 
                        break;
                        }
                        ser_current_state = STATE_SYNC;
                        ser_recv_ctr = 0;
                        recv_timer = 0;
                 break;     
	
               }			// end switch ()	
             } 			  // end if			      		       
    
// serial transmit routine; to start, set SBUF equal to first element of
//   string in main code, the interrupt routine will send out the rest	
     
 	if (_testbit_(TI))
 	  	 {
             ser_xmit_ctr++;
             if (ser_xmit_ctr < ser_xmit_length)
                {
                    if (ser_xmit_ctr == (ser_xmit_length - 1))
                        SBUF = 0x0d;  
                    else
                        SBUF = ser_xmit_buffer[ser_xmit_ctr];
    while (!TI); TI = 0; delay_ms(2);
                        ser_xmit_buffer[ser_xmit_ctr] = 0;      //try this to clear it
                  } // end if 

        // for 485 only
			 if(ser_xmit_ctr == ser_xmit_length) 
				{
					driver485 = 0;
				  }

           }	// end if		       
   
  }	// end serial_int ()	


//////////////////////////////////////////////////////////////////////////////
//	Function name:	timer0_isr ()
//	                25 ms ticks at 22.1184 MHz xtal
//                  50 ms ticks at 11.0592 MHz xtal
//  software timer reload values vary with xtal.
//  timer0 reload values remain the same for both xtals
// 	system timer -- use to time out serial receiver state machine
//               -- times out take_temp_timer, ADC_isr resets timer
//////////////////////////////////////////////////////////////////////////////
 
void timer0_isr (void) interrupt 1 using 0
{ 

//static unsigned char bytesum = 0;


	TR0 = 0;                      		// stop timer
	TH0 = RELOAD_HIGH;                	// reload count   	
	TL0 = RELOAD_LOW;
	TR0 = 1;                            // start timer

       // service serial receiver timer
 //    TIMINGBIT = ~TIMINGBIT;   // toggle test bit      	     				
    if (recv_timer > 1) recv_timer -- ;  	   	// countdown
		else if (recv_timer == 1)         	   	// reset
				{
                 ser_current_state = STATE_SYNC;
                 recv_timer = 0;
                 }
                                           		 // ==0, do nothing
// service temperature interval timer
    take_temp_timer--;   
    
//used to check which temperature range coefficients to use
    check_range_timer--;    

  }       				   				// end void timer0_isr (void) 	


////////////////////////////////////////////////////////////////////////
//
//  timer1_isr()  delay at least one character time before transmit
//
/////////////////////////////////////////////////////////////////////////

void timer1_isr(void) interrupt 3 using 3
{

    TR1 = 0;    // turn off timer

    TH1 = timer1reload_high;
    TL1 = timer1reload_low;

    DelayDone = 1;

  }             // end timer1_isr()

/////////////////////////////////////////////////////////////////////////////
//	Function    ADC_isr()
//	Parameters  none
//	Returns     none
//	Comments    this version uses accumulate/dump to increase res. by 2 bits
//              Globals used:
//                      sfr16  ADC0  = 0xBE;
//                      unsigned int result_p; 
//                      unsigned int result_t;                        
/////////////////////////////////////////////////////////////////////////////
void ADC_isr (void) interrupt 15 using 2
{
    static unsigned char cntr = 8;
    static long accumulator = 0L;       // need long : conversion result
                                        //   is left justified!

    static unsigned int temp_sum = 0;   // for averaging temp value
    static unsigned char temp_cntr = 4;

// TIMINGBIT = 1;   // toggle test bit

    ADCINT = 0;                     // clear interrupt flag
    accumulator += ADC0;            // add result to running total
    cntr--;

// TIMINGBIT = 0;   // toggle test bit

    if (cntr == 0)
     {
// TIMINGBIT = 1;   // toggle test bit

       cntr = 8;

       result_p = accumulator >> 5;     // divide by 8(3), reduce to 14 bits(2)

       accumulator = 0L;

       newdata = 1;    // indicate to main() that new data is ready

// TIMINGBIT = 0;   // toggle test bit

       if (take_temp_timer == 0)    
        {
// TIMINGBIT = 1;   // toggle test bit

          take_temp_timer = TEMP_TIMER_RELOAD;      // reset timer
            
          EIE2 &= ~0x02;        // turn off ADC interrupt
          ADCEN = 0;            // MUST turn off ADC before reconfiguring!

// with AMX0CF=0x02, AMX0SL=0x02 selects AIN2 & AIN3 as differential
          AMX0SL = 0x02;        // select temperature channnel

#ifdef _XDCR_
      //   ADC0CF |= 0x01; // this sets gain to 2, ADC clk divider untouched
         ADC0CF |= 0x02;    //gain  = 4
#endif

//???is settling time requirement of ADC met here -- OK    
          ADC0CN = 0x40;        // conversion started by write to ADBUSY
  
          ADCEN = 1;            // ADC enabled, 
          ADBUSY = 1;           // start the conversion

          _nop_();              // NEED delay, 3 SAR CLK of tracking first
          _nop_();              //   ADBUSY=1 during conversion
          _nop_();
          _nop_();
          _nop_();
          _nop_();
          _nop_();
          _nop_();              // CygFAE:M. Duerr: 1 SAR Clock required before
                                //    ADBUSY is read as a 1.
          while(ADBUSY)         // wait for end of conversion,
             ;                  //   ADBUSY=0 when finished
        // result_t = ADC0;         // update global temp. variable
// set voltage divider so room temp is ~ 0 volts, cold is negative, hot positive

    //      result_t = ADC0 + 2048;      // update global temp. variable
          temp_sum += (ADC0 + 2048);
          temp_cntr--;
          if (temp_cntr == 0)
            { result_t = temp_sum >> 2;
              temp_cntr = 4;
              temp_sum = 0;
              }

          ADCEN = 0;            // ADC off

#ifdef _XDCR_
        ADC0CF &= ~0x02;  // returns gain to 1
#endif

          AMX0SL = 0x00;        // select pressure channel

          ADC0CN = 0xC5;        // configured as before with ADC enabled
          EIE2 |= 0x02;         // turn on ADC interrupt

// TIMINGBIT = 0;   // toggle test bit         
          }                  
       }
  }     // end ADC_isr()

//---------------------------------------------------------------------------
//	Function    sysclk_init()
//	Parameters  none
//	Returns     none
//	Comments    Disables watchdog timer, sets up ext oscillator
//              waits until it's stable then selects it with
//              missing clock detector enabled
//---------------------------------------------------------------------------

void sysclk_init (void)
{
    unsigned int i;                      // delay counter

    WDTCN = 0xDE;               // disable watchdog timer
    WDTCN = 0xAD;

#ifdef _22MHZ_
    OSCXCN = 0x67;              // PF for 22.1184 MHz
#else
    OSCXCN = 0x66;              // set external xtal, no divide
                                // PF calculated from xtal values
                                // PF good for leaded and SMD xtal
                                //  at 11.0592 MHz
#endif
                                
    for (i=0; i < 2560; i++)  ;  // XTLVLD blanking interval (>1 ms)
                                // DS rev1.3 p101

    while (!(OSCXCN & 0x80)) ;  // wait for xtal osc. to stablize
                                // XTLVLD is MSbit of OSCXCN

    OSCICN = 0x88;              // select ext. osc. for sysclk
                                // enable missing clock detector

  } //end sysclk_init()

//---------------------------------------------------------------------------
//	Function    xbar_init()
//	Parameters  none
//	Returns     none
//	Comments    Configures ports for SPI, UART, 1 PCA, /INT0, CNVSTR
//---------------------------------------------------------------------------
void xbar_init(void)
{
#ifdef _XDCR_
    XBR0 = 0x05;        // SMBus and UART
    XBR1 = 0x00;        // nothing else
    XBR2 = 0x40;        // enable crossbar, weak pullups,
                        // pinout as follows:
                        //   P0.0  :  SDA
                        //   P0.1  :  SCL
                        //   P0.2  :  TX
                        //   P0.3  :  RX
                        // all other pins GPIO

#else               // eval board configuration:
    XBR0 = 0x06;            // enable SPI, UART, 
    XBR1 = 0x04;            // enable INT0, external interrupt
    XBR2 = 0x40;            // enable crossbar. weak pull-ups, 
                            // pinout as follows:
                            //    P0.0  :  SCK
                            //    P0.1  :  MISO
                            //    P0.2  :  MOSI
                            //    P0.3  :  NSS
                            //    P0.4  :  TX
                            //    P0.5  :  RX
                            //    P0.6  :  /INT0
                            // all other pins GPIO
#endif

    PRT0CF |= 0xFF;         // enable all P0 outputs as push-pull
                            //  let xbar configure pins as inputs
                            //  as necessary   (AN03 example)
    PRT1CF = 0x00;
//  PRT0CF = 0x57;          //?? config. wizard gives these values
//  PRT1CF = 0x00;          //?? for above peripheral selection
                            //  check these values before using directly

  } //end xbar_init

//---------------------------------------------------------------------------
//	Function    timer3_init()
//	Parameters  none
//	Returns     none
//	Comments    must define reload value,
//               e.g. TIMER3_RELOAD = 18432 for 20 ms with 11.0592MHz xtal
//---------------------------------------------------------------------------
void timer3_init(void)
{
    TMR3CN = 0;             // stop timer3, clear TF3, use SYSCLK/12

    TMR3RLH = 0xFF & ((-TIMER3_RELOAD) >> 8);
    TMR3RLL = 0xFF & (-TIMER3_RELOAD);
    TMR3H   = 0xFF;
    TMR3L   = 0xFF;
    EIE2   &= ~0x01;        // disable timer3 interrupts
    TMR3CN |= 0x04;         // TR3 = 1, timer3 enabled

  } // end timer3_init()

//---------------------------------------------------------------------------
//	Function    ADC_init()
//	Parameters  none
//	Returns     none
//	Comments    
//---------------------------------------------------------------------------
void ADC_init(void)
{

    ADCEN = 0;          // make sure ADC is disabled first    
                        // ADCEN = ADC0CN.7 bit addressable
#ifdef _XDCR_
///////////////////////////////////////////////////////////////////////
  //  REF0CN = 0x03;  // test internal reference, 
///////////////////////////////////////////////////////////////////////
    REF0CN = 0x02;      // use ext. reference, enabled for ADC & DAC
                        // int. temp sensor disabled (0x06 to enable it)

////////////////////////////////////////////////////////////////////
    AMX0CF = 0x02;      // AIN2 & AIN3 differential, all others single ended
  //  AMX0CF = 0x00;  // test 2nd inamp on 2nd board, all single ended
/////////////////////////////////////////////////////////////////////

#else
                    // see DS p.61
    REF0CN = 0x03;      // use int. reference for ADC & DAC
                        // int. temp sensor disabled (0x07 to enable it)
    AMX0CF = 0x00;      // default setting, all inputs single ended
#endif

/////////////////////////////////////////////////////////////////
// test 2nd inamp on 2nd board, uses ADC3
 //   AMX0SL = 0x03;  //test
    AMX0SL = 0x00;      // select AIN0 for input
///////////////////////////////////////////////////////////////////////


    
#ifdef _22MHZ_
    ADC0CF = 0x80;      // ADC conv. clk = sysclk / 16 for 22.1184MHz
                        // Gain = 1
#else
    ADC0CF = 0x60;      // ADC conv. clk = sysclk / 8 for 11.0592MHz
                        // Gain = 1
#endif

    ADC0CN = 0x45;      // ADC disabled,Timer 3 overflow starts tracking 
                        //   for 3 ADC clocks and then starts a conversion,
                        //   data is left justified

  }     //  end ADC_init()

//---------------------------------------------------------------------------
//	Function    ADC_enable()
//	Parameters  none
//	Returns     none
//	Comments    
//---------------------------------------------------------------------------
void ADC_enable(void)
{
    ADCEN = 1;          // enable ADC
    EIE2 |= 0x02;       // enable ADC interrupts (normal priority)    


  }     // end ADC_enable()


//---------------------------------------------------------------------------
//	Function name:	sint_to_ascii
//  Parameters:		*ptr -  pointer to resultant string
//        		    n    -  interger to be converted to string
//  Return values:	none
//	Comments:       Function converts a signed integer to a string of ascii 
//                  characters. Leading zeros are included, & sign if negative
//                  Tested for positive values from 0 to 32767.
//---------------------------------------------------------------------------
void sint_to_ascii (char *ptr, int n)
{
 	int i;
 	static int code decade[] = {10000, 1000, 100, 10, 1};
   	char c;
   	
   	if (n < 0)					
      {
        n *= -1;
   		*ptr++ = '-';
   	    }
   	n++;
  	for (i = 0; i < 5; i++)
	  {
        c = '0';
   	    do  { *ptr = c++; }
   	    while ((n -= decade[i]) > 0);
   	    n += decade[i];
   	    ptr++;
   	    }
    *ptr = '\0';
  }			// end sint_to_ascii()


//---------------------------------------------------------------------------
//	Function name:	int_to_ascii
//  Parameters:		*ptr -  pointer to resultant string
//        		    n    -  interger to be converted to string
//  Return values:	none
//	Comments:       Function converts a signed integer to a string of ascii 
//                  characters. Leading zeros are included.
//                  Tested for positive values from 0 to 32767.
//	REVISED:  For positive int only now
//	          Changed: varible 'n' from int to long
//		      comment out 4 lines: the less than 0 test
//---------------------------------------------------------------------------
void int_to_ascii (char *ptr, long n)
{
 	int i;
 	static int code decade[] = {10000, 1000, 100, 10, 1};
   	char c;
   	
//   	if (n < 0)					
//		{n *= -1;
//   		*ptr++ = '-';
//   	 	 }
 
   	n++;
  	for (i = 0; i < 5; i++)
		{c = '0';
   	     	do  { *ptr = c++; }
   	        while ((n -= decade[i]) > 0);
   	        n += decade[i];
   	        ptr++;
   	    }
        *ptr = '\0';
  }			// end int_to_ascii


//---------------------------------------------------------------------------
//   Function:	    hextoascii()  	
//   Parameters:	 num   - hex byte to be converted
//    				*ptr  -	pointer to resulting ASCII values
//   Returns:	    none      
// 	 Comments:      Accepts an unsigned byte converts it to its ASCII codes			
//---------------------------------------------------------------------------

void hextoascii(unsigned char num, unsigned char *ptr)

 {unsigned char x;
	
    	x = _cror_((num & 0xf0), 4);
    	if ((x>=0) && (x<=9)) x += 0x30; else x += 0x37;
    	*ptr = x;
    	x = num & 0x0f;
       	if ((x>=0) && (x<=9)) x += 0x30; else x += 0x37;
       	*(ptr + 1) = x;

  }     // end hextoascii()


//---------------------------------------------------------------------------
//	Function:   flt_to_sci()
//	Parameters: f - value to convert to ascii string
//				*str - pointer to string for result
//	Returns:    0 - result good
//  Comments:	Original by K.Rastatter 5/26/98 for 3 byte floats for PIC
//	            Modified by W.Rase      3/30/99 for 4 byte floats
//---------------------------------------------------------------------------
char flt_to_sci(char *str, float f)   
{
	signed char expcnt = 0;      //	exponent counter
	char dpflag = 0;             // decimal point written yet
	unsigned long fl;            // 4
	unsigned long decdigit;      // 4
	char qty;                    // character of string created
	signed char decchar;         // 1
	
    if(f<0) 
    	{
    	 *str++ = '-';
    	 f *= -1;  	
      	 }
    else
    	*str++ = '+';    	
   	if (f != 0.0) 
   		{
   		  while(f>9.999985)      // 41 1f ff f0
   	   	 	{
   	   	   	  f /= 10;
   	   	      expcnt++;
   	   	     }
   	   	  while (f<.9999990)     // 3f 7f ff f0
   	   	    {
   	   	  	  f *= 10;
   	   	      expcnt--;
   	   	     }  	   	    
         }
    f += .000005;
    fl = (long)(f * 1E6L);
    decdigit = (long)1E6;
    while (decdigit > 1)
    	{
    	qty =0;
    	while(fl >=decdigit)
    		{
     		qty++;
      		fl -= decdigit;
       		}     
	    *str++ = qty+'0';
     	if (!dpflag)
     		{
     		*str++ = '.';
      		dpflag = 0xff;
       		}
       	decdigit /= 10;
       	}
    *str++ = 'E';
    if (expcnt<0)
    	{
    		*str++ = '-';
     		expcnt = -expcnt;
      	}
    else
    	*str++ = '+';
    
	decchar = 10;
	while (decchar)
		{
 			qty = 0;
   			while(expcnt >= decchar)
   	 		{
   	  			qty++;
   	   			expcnt -= decchar;
   	   		 }
   	   	  	 *str++ = qty+'0';
   	   	  	 decchar /= 10;
   	   	}
   	*str = '\0';
   	
   	return(0);
   	
    }   // end flt_to_sci()   


//-------------------------------------------------------------------------
//  Function:   set_checksum_XRAM()
//  Comments:   Calculates checksum on XRAM image of stored data
//              Writes checksum value to last byte of the page
//  
//-------------------------------------------------------------------------
void set_checksum_XRAM(void)
{   
  unsigned char xdata *pread_XRAM;  // pointer to XRAM
  unsigned char bytesum;

  unsigned int i;                   // loop counter

    pread_XRAM = DATA_XRAM_ADDR;    // initialize pointer

    bytesum = 0x00;

    for (i=0; i <= (DATA_LENGTH - 1); i++)
      {
        bytesum += *pread_XRAM;
        pread_XRAM++;

        }

  *pread_XRAM = (0xFF - bytesum);

  } // end set_checksum_XRAM()


//-------------------------------------------------------------------------
//  Function:   verify_checksum_FLASH()
//  Returns:    0 - checksum BAD
//              1 - checksum GOOD
//  Comments:   verifies checksum by summing all bytes, sum = 0xFF to pass
//              run at startup, also available from command
//
//-------------------------------------------------------------------------

bit verify_checksum_FLASH (void)
{
    unsigned char code *pread_FLASH;

    unsigned char bytesum;
    unsigned int i;

    bytesum = 0;
    pread_FLASH = DATA_FLASH_ADDR;

    for (i=0; i <= DATA_LENGTH; i++)
      {
        bytesum += *pread_FLASH;
        pread_FLASH++;
       }
    if (bytesum == 0xFF) return (1);
    else return (0);

  } // end verify_checksum_FLASH()


//---------------------------------------------------------------------------
//  Function:   copy_FLASH_to_XRAM()
//
//  Comments:   copies one page of FLASH used for data storage to XRAM            
//---------------------------------------------------------------------------
void copy_FLASH_to_XRAM(void)
{

  unsigned char code *pread_FLASH;    // pointer to read from FLASH
  unsigned char xdata *pwrite_XRAM;   // pointer to write to XRAM
                                      // MUST set PSCTL = 0x00
  unsigned int i;                     // loop index


 // unsigned char temp;

    //initialize pointers
  pread_FLASH = DATA_FLASH_ADDR;      // last page of FLASH
  pwrite_XRAM = DATA_XRAM_ADDR;       // last page of XRAM available

  PSCTL = 0x00;                       // ensure that writes are to XRAM

    //copy block
  for (i=0; i<=DATA_LENGTH; i++)
   {
     *pwrite_XRAM++ = *pread_FLASH++;        //??verify this works -- OK
        
      //  temp = *pread_FLASH;             // or this
      //  *pwrite_XRAM = temp;
      //  pread_FLASH++;
      //  pwrite_XRAM++;

    }

 }    //end copy_FLASH_to_XRAM()

//---------------------------------------------------------------------------
//  Function:   erase_FLASH()
//
//  Comments:   Erases one page of FLASH used for data storage
//              uses #define DATA_FLASH_ADDR
//---------------------------------------------------------------------------
void erase_FLASH(void)
{

  unsigned char xdata *pwrite_FLASH;  // pointer in CODE space(FLASH) to write to
                                    // PSCTL must NOT be 0x00 
//      TIMINGBIT = 1;

#ifdef _22MHZ_
  FLSCL = 0x89;
#else
  FLSCL = 0x88;                       // FLASH scale SFR for 11.0592 MHz clock
#endif

  PSCTL = 0x03;                       // sets PSWE, PSEE
                                    // MOVX will erase FLASH page

  pwrite_FLASH = DATA_FLASH_ADDR;     // pointer to page to erase

  *pwrite_FLASH =  0;                 // erase the page by doing a write to it

  PSCTL = 0x00;           // disables FLASH writes, writes are to XRAM now
  FLSCL = 0x8F;           // disables FLASH writes also

  WDTCN = 0xA5;   // reload watchdog timer

//    TIMINGBIT = 0;

 }    // end erase_FLASH()

//---------------------------------------------------------------------------
//  Function:   copy_XRAM_to_FLASH
//
//  Comments:   Copies one page of XRAM to FLASH used for data storage
//              Uses #defines DATA_XRAM_ADDR and DATA_FLASH_ADDR
//---------------------------------------------------------------------------
void copy_XRAM_to_FLASH(void)
{

  unsigned char xdata *pread_XRAM;    // pointer to XRAM
  unsigned char xdata *pwrite_FLASH;  // pointer in CODE space (FLASH) to write to
                                      // must set PSCTL = 0x01
  unsigned char temp;                 // temporary storage 
  unsigned int i;

        //initialize pointers
  pread_XRAM = DATA_XRAM_ADDR;
  pwrite_FLASH = DATA_FLASH_ADDR;

#ifdef _22MHZ_
  FLSCL = 0x89;             // for 22.1184 MHz       
#else
  FLSCL = 0x88;             // for 11.0592 MHz
#endif

  for (i=0; i<=DATA_LENGTH; i++)
   {
    PSCTL = 0x00;
    temp = *pread_XRAM;         
    PSCTL = 0x01;
    *pwrite_FLASH = temp;
    pread_XRAM++;
    pwrite_FLASH++;
     }

  FLSCL = 0x8F;                   // disable FLASH writes
  PSCTL = 0x00;

 } // end copy_XRAM_to_FLASH()


//---------------------------------------------------------------------------
//  Function:   write_float_FLASH()
//
//  Comments:   addresses of variables range from 0 to 255 for now
//   1/14/02:   Change data_addr to uint, allows writes to all 512 bytes 
//                  of one FLASH page
//---------------------------------------------------------------------------

void write_float_FLASH(unsigned int data_addr, float *fltptr)
{

  unsigned char xdata *pwrite_XRAM;
  unsigned char *chptr;

//    TIMINGBIT = 1;

  pwrite_XRAM = DATA_XRAM_ADDR + data_addr;

  chptr = (char *)fltptr;

  copy_FLASH_to_XRAM();
  erase_FLASH();

        //update XRAM image of data
  *pwrite_XRAM = *chptr;
  pwrite_XRAM++;
  *pwrite_XRAM = *(chptr + 1);
  pwrite_XRAM++;
  *pwrite_XRAM = *(chptr + 2);
  pwrite_XRAM++;
  *pwrite_XRAM = *(chptr + 3);
  pwrite_XRAM++;
  *pwrite_XRAM = DATA_FLAG;        // write flag indicating valid value exist

  set_checksum_XRAM();  

  copy_XRAM_to_FLASH();

  recentupdate = 1;

//   TIMINGBIT = 0;


  }     //end write_float_FLASH()


//---------------------------------------------------------------------------
//  Function:   read_float_FLASH()
//
//  Comments:
//   1/14/02:   Change data_addr to uint, allows writes to all 512 bytes 
//                  of one FLASH page
//---------------------------------------------------------------------------
void read_float_FLASH(unsigned int data_addr, float *fltptr)
{
unsigned char code *pread_FLASH;
unsigned char *chptr;

pread_FLASH = DATA_FLASH_ADDR + data_addr;

chptr = (char *)fltptr;

*chptr = *pread_FLASH;
*(chptr + 1) = *(pread_FLASH + 1);
*(chptr + 2) = *(pread_FLASH + 2);
*(chptr + 3) = *(pread_FLASH + 3);

  } // end read_float_FLASH()


//---------------------------------------------------------------------------
//  Function:   write_char_FLASH()
//
//  Comments:   address of variables range from 0 to 255 for now
//   1/14/02:   Change data_addr to uint, allows writes to all 512 bytes 
//                  of one FLASH page
//
//---------------------------------------------------------------------------
void write_char_FLASH(unsigned int data_addr, unsigned char length,
                      unsigned char *charptr)
{
  unsigned char xdata *pwrite_XRAM;
  unsigned char i;        

  pwrite_XRAM = DATA_XRAM_ADDR + data_addr;

  copy_FLASH_to_XRAM();
  erase_FLASH();

  for (i=0; i<length; i++)
    {
        *pwrite_XRAM = *(charptr + i);
        pwrite_XRAM++;
      }
  *pwrite_XRAM = DATA_FLAG;    // set validity flag after last char written

  set_checksum_XRAM();

  copy_XRAM_to_FLASH();

  recentupdate = 1;

   }    // end write_char_FLASH()

//---------------------------------------------------------------------------
//  Function:   read_char_FLASH()
//
//  Comments:   address of variables range from 0 to 255 for now
//   1/14/02:   Change data_addr to uint, allows writes to all 512 bytes 
//                  of one FLASH page
//
//---------------------------------------------------------------------------
void read_char_FLASH(unsigned int data_addr, unsigned char length,
                     unsigned char *charptr)
{
  unsigned char code *pread_FLASH;
  unsigned char i;

  pread_FLASH = DATA_FLASH_ADDR + data_addr;

  for (i=0; i<length; i++)
    {
        *(charptr + i) = *(pread_FLASH + i);
      }

  }     // end read_char_FLASH()


//---------------------------------------------------------------------------
//  Function:   erase_DEFAULT()
//
//  Comments:   Erases one page of FLASH used for backup copy of data
//              uses #define DEFAULT_FLASH_ADDR
//---------------------------------------------------------------------------
void erase_DEFAULT(void)
{

  unsigned char xdata *pwrite_FLASH;  // pointer in CODE space(FLASH) to write to
                                    // PSCTL must NOT be 0x00 
#ifdef _22MHZ_
  FLSCL = 0x89;
#else
  FLSCL = 0x88;                       // FLASH scale SFR for 11.0592 MHz clock
#endif

  PSCTL = 0x03;                       // sets PSWE, PSEE
                                    // MOVX will erase FLASH page

  pwrite_FLASH = DEFAULT_FLASH_ADDR;     // pointer to page to erase

  *pwrite_FLASH =  0;                 // erase the page by doing a write to it

  PSCTL = 0x00;           // disables FLASH writes, writes are to XRAM now
  FLSCL = 0x8F;           // disables FLASH writes also

  WDTCN = 0xA5;   // reload watchdog timer

 }    // end erase_DEFAULT()


//---------------------------------------------------------------------------
//  Function:   copy_XRAM_to_DEFAULT()
//
//  Comments:   Copies one page of XRAM to DEFAULT FLASH, backup storage
//              Uses #defines DATA_XRAM_ADDR and DEFAULT_FLASH_ADDR
//---------------------------------------------------------------------------
void copy_XRAM_to_DEFAULT(void)
{
  unsigned char xdata *pread_XRAM;    // pointer to XRAM
  unsigned char xdata *pwrite_FLASH;  // pointer in CODE space (FLASH) to write to
                                      // must set PSCTL = 0x01
  unsigned char temp;                 // temporary storage 
  unsigned int i;
        //initialize pointers
  pread_XRAM = DATA_XRAM_ADDR;
  pwrite_FLASH = DEFAULT_FLASH_ADDR;

#ifdef _22MHZ_
  FLSCL = 0x89;             // for 22.1184 MHz       
#else
  FLSCL = 0x88;             // for 11.0592 MHz
#endif

  for (i=0; i<=DATA_LENGTH; i++)
   {
    PSCTL = 0x00;
    temp = *pread_XRAM;         
    PSCTL = 0x01;
    *pwrite_FLASH = temp;
    pread_XRAM++;
    pwrite_FLASH++;
     }

  FLSCL = 0x8F;                   // disable FLASH writes
  PSCTL = 0x00;

 } // end copy_XRAM_to_DEFAULT()


//---------------------------------------------------------------------------
//  Function:   copy_DEFAULT_to_XRAM()
//
//  Comments:   copies default page of FLASH used for data backup to XRAM            
//---------------------------------------------------------------------------
void copy_DEFAULT_to_XRAM(void)
{

  unsigned char code *pread_FLASH;    // pointer to read from FLASH
  unsigned char xdata *pwrite_XRAM;   // pointer to write to XRAM
                                      // MUST set PSCTL = 0x00
  unsigned int i;                     // loop index


 // unsigned char temp;

    //initialize pointers
  pread_FLASH = DEFAULT_FLASH_ADDR;      // default page of FLASH
  pwrite_XRAM = DATA_XRAM_ADDR;       // last page of XRAM available

  PSCTL = 0x00;                       // ensure that writes are to XRAM

    //copy block
  for (i=0; i<=DATA_LENGTH; i++)
   {
     *pwrite_XRAM++ = *pread_FLASH++;        //??verify this works
        
      //  temp = *pread_FLASH;             //   1st change
      //  *pwrite_XRAM = temp;
      //  pread_FLASH++;
      //  pwrite_XRAM++;

    }

 }    //end copy_DEFAULT_to_XRAM()


//---------------------------------------------------------------------------
//	Function:	xmit_OK()
//---------------------------------------------------------------------------
void xmit_OK(void)
{
   	strcpy(ser_xmit_buffer, "OK");
  //	ser_xmit_buffer[2] =  0x0a;
    ser_xmit_length = 3;
    ser_xmit_ctr = 0;
    SBUF = ser_xmit_buffer[ser_xmit_ctr];

 }      // end xmit_OK()


//---------------------------------------------------------------------------
//  Function:   xmit_ERR()
//---------------------------------------------------------------------------
void xmit_ERR(unsigned char *error_code)
{
    strcpy (ser_xmit_buffer, "Err_");
    strcpy (&ser_xmit_buffer[4], error_code);
    ser_xmit_length = 8;
    ser_xmit_ctr = 0;
    SBUF = ser_xmit_buffer[ser_xmit_ctr];

  } // end xmit_ERR()


//---------------------------------------------------------------------------
//	Function:	Wait5()
//        
//  Function waits for approximately 5 microseconds.
//  With Cygnal & 11.0592 MHz xtal: i = 10 -> 5.5 us 
//                                  i =  9 -> 4.6 us
//----------------------------------------------------------------------------
void Wait5(void)
{        	
  unsigned char i;

// TIMINGBIT = 1;   // toggle test bit

#ifdef _22MHZ_
    for (i=0; i<=19; i++) {;}  //verify value
#else
    for (i=0; i<=10; i++) {;}
#endif 

// TIMINGBIT = 0;   // toggle test bit
	
 }		// end Wait5()
 
 
//----------------------------------------------------------------------------
//  soft_reset()
//
//----------------------------------------------------------------------------

void soft_reset(void)
{
    ((void (code *) (void)) 0x0000) ();
  }			
				      	

//---------------------------------------------------------------------------
// Function name:	ExecuteCommmand ()
// Parameters:		none
// Return values:	none
// Comments:        Function runs in main loop, if detects a CommandWaiting=1,
//                  interprets the command.
//---------------------------------------------------------------------------
void ExecuteCommand (void)
{ 
  unsigned char addr;
  unsigned char index;
  unsigned int x;     // loop index to time out xmit_OK() after baudrate change
  
  float tmpfloat;
  
  unsigned int xdata tmpint;
  unsigned char xdata highbyte;

//test format temp output:  
  int xdata testint;
  unsigned char xdata test_buffer[14] = {0};
  float xdata tempsquared;

  union float_or_bytes {unsigned char b[3];
                                float flt;};
  union float_or_bytes output;
  		


  switch (command_buffer[0])
    {
        // MOVE COMMAND THAT NEEDS TO EXECUTE THE QUICKEST TO THE TOP

//D-      
      case 'd':
      case 'D':
         switch ( command_buffer[1] )
          {
            case '0':               // display digital output

              if (overrange == 1) xmit_ERR(OVER_RANGE);
              else if (underrange == 1) xmit_ERR(UNDER_RANGE);
              else if (chksumfail == 1) xmit_ERR(CHECKSUM_FAIL);
              else
                {
                  tmpfloat = dig_m*presscal + dig_b;
                  flt_to_sci(ser_xmit_buffer, tmpfloat);
                  ser_xmit_length = 13;
                  ser_xmit_ctr = 0;
                  SBUF = ser_xmit_buffer[ser_xmit_ctr];

 //   TIMINGBIT = 0;

                 }
            break;

            case 'b':       // display user's digital zero adjustment
            case 'B':
                read_float_FLASH(USER_DIG_ZERO_ADDR, &tmpfloat);
                flt_to_sci(ser_xmit_buffer, tmpfloat);
                ser_xmit_length = 13;
                ser_xmit_ctr = 0;
                SBUF = ser_xmit_buffer[ser_xmit_ctr];
            break;

            case 'e':       // display engineering units conversion factor
            case 'E':
                read_float_FLASH(EU_CONV_ADDR, &tmpfloat);
                flt_to_sci(ser_xmit_buffer, tmpfloat);
                ser_xmit_length = 13;
                ser_xmit_ctr = 0;
                SBUF = ser_xmit_buffer[ser_xmit_ctr];
            break;

            case 'h':
            case 'H':
                if (overrange == 1) output.flt = FLT_MAX;
                else if (underrange == 1) output.flt = FLT_MIN;
                else output.flt = dig_m*presscal + dig_b;
                hextoascii(output.b[0], &ser_xmit_buffer[0]);
                hextoascii(output.b[1], &ser_xmit_buffer[2]);
                hextoascii(output.b[2], &ser_xmit_buffer[4]);
                hextoascii(output.b[3], &ser_xmit_buffer[6]);
                ser_xmit_length = 9;
                ser_xmit_ctr = 0;
                SBUF = ser_xmit_buffer[ser_xmit_ctr];
 //   TIMINGBIT = 0;

            break;
//D-
            case 'm':       // display user's digital gain adjustment
            case 'M':
                read_float_FLASH(USER_DIG_GAIN_ADDR, &tmpfloat);
                flt_to_sci(ser_xmit_buffer, tmpfloat);
                ser_xmit_length = 13;
                ser_xmit_ctr = 0;
                SBUF = ser_xmit_buffer[ser_xmit_ctr];
            break;

            case 'p':       // display user defined string
            case 'P':
                read_char_FLASH(USER_STR_ADDR, 16, ser_xmit_buffer);
                ser_xmit_length = 17;
                ser_xmit_ctr = 0;
                SBUF = ser_xmit_buffer[ser_xmit_ctr]; 
            break;

            case 'r':       // display error status
            case 'R':       
                ser_xmit_buffer[0] = 'E';
                ser_xmit_buffer[1] = 'r';
                ser_xmit_buffer[2] = 'r';
                ser_xmit_buffer[3] = '_';
                ser_xmit_buffer[4] = status;
                ser_xmit_length = 6;
                ser_xmit_ctr = 0;
                SBUF = ser_xmit_buffer[ser_xmit_ctr];
                status &= 0x30;

            break;
//D-
            case 't':       // display temperature in DegF
            case 'T':       // uses Eqn 2040

                tempsquared = temperature*temperature;
                tmpfloat = temp_coeff[0] + temp_coeff[1]*temperature +
                           temp_coeff[2]*tempsquared +
                           temp_coeff[3]*tempsquared*temperature;
                testint = (int)tmpfloat;
                sint_to_ascii(test_buffer, testint);

            // format : up to 3 positive digits, 2 negative digits
            //          leading zeros stripped, '-' if negative,
            //          no sign if positive

            //rev1.1: remove trailing 'F'
                if (test_buffer[0] == '-')      //if negative, 6 byte string
                 { 
                    ser_xmit_buffer[0] = '-';
                    if (test_buffer[4] != '0')
                    {
                        ser_xmit_buffer[1] = test_buffer[4];
                        ser_xmit_buffer[2] = test_buffer[5];
                     //   ser_xmit_buffer[3] = 'F';
                        ser_xmit_length = 4;
                     }
                    else
                      {
                        ser_xmit_buffer[1] = test_buffer[5];
                      //  ser_xmit_buffer[2] = 'F';
                        ser_xmit_length = 3;
                        }
                    }
                else                //if positive, only 5 byte string
                  {
                    if (test_buffer[2] != '0')
                     {
                      ser_xmit_buffer[0] = test_buffer[2];
                      ser_xmit_buffer[1] = test_buffer[3];
                      ser_xmit_buffer[2] = test_buffer[4];
                    //  ser_xmit_buffer[3] = 'F';
                      ser_xmit_length = 4;
                      }
                     else if (test_buffer[3] != '0')
                      {
                        ser_xmit_buffer[0] = test_buffer[3];
                        ser_xmit_buffer[1] = test_buffer[4];
                     //   ser_xmit_buffer[2] = 'F';
                        ser_xmit_length = 3;
                       }
                     else
                       {
                        ser_xmit_buffer[0] = test_buffer[4];
                     //   ser_xmit_buffer[1] = 'F';
                        ser_xmit_length = 2;
                        }                                  
                    }

              ser_xmit_ctr = 0;         
              SBUF = ser_xmit_buffer[ser_xmit_ctr];  

            break;

//D-           
            case 'C':       // display temperature in DegC
            case 'c':       // uses EQN2040 then converts to C

                tempsquared = temperature*temperature;
                tmpfloat = (5.0/9)*((temp_coeff[0] + temp_coeff[1]*temperature +
                           temp_coeff[2]*tempsquared +
                           temp_coeff[3]*tempsquared*temperature)-32);
              //  tmpfloat = (5/9)*(tmpfloat - 32);

                testint = (int)tmpfloat;
                sint_to_ascii(test_buffer, testint);

            // format : up to 3 positive digits, 2 negative digits
            //          leading zeros stripped, '-' if negative,
            //          no sign if positive

                if (test_buffer[0] == '-')      //if negative, 6 byte string
                 { 
                    ser_xmit_buffer[0] = '-';
                    if (test_buffer[4] != '0')
                    {
                        ser_xmit_buffer[1] = test_buffer[4];
                        ser_xmit_buffer[2] = test_buffer[5];
                        ser_xmit_length = 4;
                     }
                    else
                      {
                        ser_xmit_buffer[1] = test_buffer[5];
                        ser_xmit_length = 3;
                        }
                    }
                else                //if positive, only 5 byte string
                  {
                    if (test_buffer[2] != '0')
                     {
                      ser_xmit_buffer[0] = test_buffer[2];
                      ser_xmit_buffer[1] = test_buffer[3];
                      ser_xmit_buffer[2] = test_buffer[4];
                      ser_xmit_length = 4;
                      }
                     else if (test_buffer[3] != '0')
                      {
                        ser_xmit_buffer[0] = test_buffer[3];
                        ser_xmit_buffer[1] = test_buffer[4];
                        ser_xmit_length = 3;
                       }
                     else
                       {
                        ser_xmit_buffer[0] = test_buffer[4];
                        ser_xmit_length = 2;
                        }                                  
                    }

              ser_xmit_ctr = 0;         
              SBUF = ser_xmit_buffer[ser_xmit_ctr];  

            break;

//D-
//1/31/02  DAC0 is now zero adjust of voltage output
            case '1':       // display bias for pressure INamp
                read_float_FLASH(DAC0_ADDR, &tmpfloat);
		        ser_xmit_buffer[0] = 'D';      
         	    ser_xmit_buffer[1] = '0';
		        ser_xmit_buffer[2] = '=';      
                int_to_ascii(&ser_xmit_buffer[3], (unsigned int)(tmpfloat));
                ser_xmit_length = 9;
                ser_xmit_ctr = 0;
                SBUF = ser_xmit_buffer[ser_xmit_ctr];
            break;
// D-
            case 'a':       // readback analog voltage output
            case 'A':
                            
               TMR3CN &= ~0x04;    // turn off timer3
               EIE2 &= ~0x02;      // turn off ADC interrupt

               ADCEN = 0;          // turn off ADC before reconfiguring

#ifdef _XDCR_
               AMX0SL = 0x04;      // select AIN4 as input, (single ended)
                            // ADC0CF unchanged, gain=1 as in ISR
#else  
               AMX0SL = 0x02;       // for testing on eval board
  
#endif
        //       ADC0CF |= 0x01; // this sets gain to 2

               ADC0CN = 0x40;       // conversion started by write to ADBUSY
        
               ADCEN = 1;           // ADC enabled
                    // wait for settling time
                //  for (index = 0; index <= 15; index++) _nop_() ; // settling > 1.5 us

               ADBUSY = 1;          // start conversion

               _nop_();             // delay before polling ADBUSY
               _nop_();
               _nop_();
               _nop_();
               _nop_();
               _nop_();
               _nop_();
               _nop_();

               while (ADBUSY)       // wait for end of conversion
                 ;                  //  ADBUSY = 0 when finished

               tmpint = ADC0;       //  get reading
                                    // average a couple readings??

               ADCEN = 0;          // ADC off
               AMX0SL = 0x00;      // select pressure channel
     //        ADC0CF &= ~0x01;  // returns gain to 1
               ADC0CN = 0xC5;      // configured as before with ADC enabled
               EIE2 |= 0x02;       // enable ADC interrupt
               TMR3CN |= 0x04;     // timer3 running again
// calculate corresonding voltage output

               tmpfloat = vmon_scale*(tmpint + vmon_offset);
// format for serial output

               flt_to_sci(test_buffer, tmpfloat);
             // this simplifies the output of flt_to_sci()
             // valid for +9.999 to -9.999, only 3 decimal digits displayed
               ser_xmit_buffer[0] = test_buffer[0];
           
               if (test_buffer[9] == '+')
                {
                  for (index=1; index<6; index++) ser_xmit_buffer[index] = test_buffer[index];
                //  ser_xmit_length = 7;      // is this the same for all??
                 }
               else if (test_buffer[11] == '1')
                 {
                   ser_xmit_buffer[1] = '0';
                   ser_xmit_buffer[2] = '.';
                   ser_xmit_buffer[3] = test_buffer[1];
                   ser_xmit_buffer[4] = test_buffer[3];
                   ser_xmit_buffer[5] = test_buffer[4];
                //   ser_xmit_length = 7;   
                  }
               else if (test_buffer[11] == '2')
                 {
                   ser_xmit_buffer[1] = '0';
                   ser_xmit_buffer[2] = '.';
                   ser_xmit_buffer[3] = '0';
                   ser_xmit_buffer[4] = test_buffer[1];
                   ser_xmit_buffer[5] = test_buffer[3];
               //    ser_xmit_length = 7;
                  }
               else if (test_buffer[11] == '3')
                 {
                   ser_xmit_buffer[1] = '0';
                   ser_xmit_buffer[2] = '.';
                   ser_xmit_buffer[3] = '0';
                   ser_xmit_buffer[4] = '0';
                   ser_xmit_buffer[5] = test_buffer[1];
              //     ser_xmit_length =7;
                  }
               else 
                 {
                   ser_xmit_buffer[1] = '0';
                   ser_xmit_buffer[2] = '.';
                   ser_xmit_buffer[3] = '0';
                   ser_xmit_buffer[4] = '0';
                   ser_xmit_buffer[5] = '0';
               //    ser_xmit_length =7;
                  }

           //    ser_xmit_buffer[6] = 'V';
               ser_xmit_length = 7;                 
               ser_xmit_ctr = 0;
               SBUF = ser_xmit_buffer[ser_xmit_ctr];  

            break;

            default:
               xmit_ERR(NOT_A_COMMAND);          
            break;

            }  // end (command_buffer[1])  
               
      break;   // for outer case 'D'

////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////


//C-
      case 'C':             // write coefficients to FLASH
      case 'c':
         if (isdigit(command_buffer[1]))
         {
            if (_testbit_(FactoryEnable))
             { 
                index = command_buffer[1] - ASCII_OFFSET;
                coeff[index] = atof(&command_buffer[2]);
         // check if valid float is returned, if not transmit error
                if ( (_chkfloat_(coeff[index])==0) || (_chkfloat_(coeff[index])==1) )
                {
                  addr = (index * 5);
                  write_float_FLASH(addr, &coeff[index]);
                  xmit_OK();
                  }
                 else xmit_ERR(NOT_A_NUMBER);        
               }  // end test for FactoryEnable
              else xmit_ERR(ACCESS_DENIED);         
           }                                                           
      break; // for outer case 'C'
      
//F-
      case 'f':
      case 'F':
         switch ( command_buffer[1] )
          {
            case 'c':       // display factory calibration date
            case 'C':
               read_char_FLASH(CAL_DATE_ADDR, 8, ser_xmit_buffer);
               ser_xmit_length = 9;
               ser_xmit_ctr = 0;
               SBUF = ser_xmit_buffer[ser_xmit_ctr];
            break;

            case 'e':       // display factory serial number
            case 'E':
               read_char_FLASH(SER_NUM_ADDR, 6, ser_xmit_buffer);
               ser_xmit_length = 7;
               ser_xmit_ctr = 0;
               SBUF = ser_xmit_buffer[ser_xmit_ctr]; 
            break;

            case 'r':       // restore factory defaults
            case 'R':
               if(_testbit_(WriteEnable))
                {
                 xmit_OK();
                 copy_DEFAULT_to_XRAM();
                 erase_FLASH();
                 copy_XRAM_to_FLASH();
// cygnal's software reset?
                 RSTSRC |= 0x02; //set PORSF bit to 1 to force reset

// delay required before reset?? allow transmission of 'OK'
                 }
               else xmit_ERR(ACCESS_DENIED);   
            break;


            case 't':       // test FLASH checksum
            case 'T':
               if ( !verify_checksum_FLASH() ) 
                { 
                  chksumfail = 1;
                  xmit_ERR(CHECKSUM_FAIL);
                 }
               else xmit_OK();
            break;

            case '0':               // display digital output
                  tmpfloat = dig_m*presscal + dig_b;
                  flt_to_sci(ser_xmit_buffer, tmpfloat);
                  ser_xmit_length = 13;
                  ser_xmit_ctr = 0;
                  SBUF = ser_xmit_buffer[ser_xmit_ctr];
            break;

            default:
               xmit_ERR(NOT_A_COMMAND);
            break;

            }   // end (command_buffer[1])
      break;    // for outer case 'F'

//P-             
      case 'p':
      case 'P':
         switch ( command_buffer[1] )
          {	   		
            case 'p':         // send out raw pressure reading
            case 'P':           // get last value used by calculations 
              ser_xmit_buffer[0] = unit_addr_hi;      
         	  ser_xmit_buffer[1] = unit_addr_lo;        
		      ser_xmit_buffer[2] = 'P';      
         	  ser_xmit_buffer[3] = '=';
              int_to_ascii(&ser_xmit_buffer[4], (unsigned int)(pressure));
              ser_xmit_length = 10;        
              ser_xmit_ctr = 0;         
              SBUF = ser_xmit_buffer[ser_xmit_ctr];  
            break;

            case 'T':           // send out raw temperatrue reading
            case 't':
              ser_xmit_buffer[0] = unit_addr_hi;
              ser_xmit_buffer[1] = unit_addr_lo;
              ser_xmit_buffer[2] = 'T';
              ser_xmit_buffer[3] = '=';
              int_to_ascii(&ser_xmit_buffer[4], (unsigned int)(temperature));
              ser_xmit_length = 10;
              ser_xmit_ctr = 0;
              SBUF = ser_xmit_buffer[ser_xmit_ctr];
            break;

            default:
              xmit_ERR(NOT_A_COMMAND);    
            break;

             }  // end switch(commmand_buffer[1])
      break;    // for outer case 'P'
//R-
      case 'r':
      case 'R':
         switch ( command_buffer[1] )
          {
            case '4':
              ser_xmit_buffer[0] = unit_addr_hi;
              ser_xmit_buffer[1] = unit_addr_lo;
              ser_xmit_length = 3;
              ser_xmit_ctr = 0;
              SBUF = ser_xmit_buffer[ser_xmit_ctr];
            break;

            case '5':           // write full scale range value in PSI
              read_float_FLASH(RANGE_ADDR, &tmpfloat);
              flt_to_sci(ser_xmit_buffer, tmpfloat);
              ser_xmit_length = 13;
              ser_xmit_ctr = 0;
              SBUF = ser_xmit_buffer[ser_xmit_ctr];
            break;
        
            case '6':           //read engineering units label
              read_char_FLASH(EU_STR_ADDR, 4, ser_xmit_buffer);
              ser_xmit_length = 5;
              ser_xmit_ctr = 0;
              SBUF = ser_xmit_buffer[ser_xmit_ctr]; 
            break;

            case 'm':       // display mfg part number
            case 'M':
        /*      read_char_FLASH(MFG_PART_ADDR, 11, ser_xmit_buffer);
              ser_xmit_length = 12;
              ser_xmit_ctr = 0;
              SBUF = ser_xmit_buffer[ser_xmit_ctr]; */

              read_char_FLASH(MFG_PART_ADDR, 15, ser_xmit_buffer);
              if (ser_xmit_buffer[11] == DATA_FLAG)
                  ser_xmit_length = 12;
              else ser_xmit_length = 15;
              ser_xmit_ctr = 0;
              SBUF = ser_xmit_buffer[ser_xmit_ctr];
            break;
           
            case 'r':           //read software revision 
            case 'R':
              strcpy (ser_xmit_buffer, PNVERSION);
              ser_xmit_length = 17;
              ser_xmit_ctr = 0;
              SBUF = ser_xmit_buffer[ser_xmit_ctr];  
            break;

            case 'o':       // display scale factor
            case 'O':
              read_float_FLASH(USER_SCALE_ADDR, &tmpfloat);   
              flt_to_sci(ser_xmit_buffer, tmpfloat);
              ser_xmit_length = 13;
              ser_xmit_ctr = 0;
              SBUF = ser_xmit_buffer[ser_xmit_ctr];
            break;

            case 'n':       // display offset factor
            case 'N':
              read_float_FLASH(USER_OFFSET_ADDR, &tmpfloat);  
              flt_to_sci(ser_xmit_buffer, tmpfloat);
              ser_xmit_length = 13;
              ser_xmit_ctr = 0;
              SBUF = ser_xmit_buffer[ser_xmit_ctr];
            break;


            default:
              xmit_ERR(NOT_A_COMMAND);       
            break;

            }   // end switch(command_buffer[1])
      break;    // for outer case 'R'
//S-
      case 's':
      case 'S':               
         switch ( command_buffer[1] )
          {
            case 'b':       // write user's digital zero adjustment
            case 'B':
              if(_testbit_(WriteEnable))
                {
                  tmpfloat = atof(&command_buffer[2]);
                    if((_chkfloat_(tmpfloat)==0) || (_chkfloat_(tmpfloat) == 1) )    
                      {
                        write_float_FLASH(USER_DIG_ZERO_ADDR, &tmpfloat);
                        xmit_OK();  
                        init_digital_constants();  // update dig_m, dig_b
                        }
                  //      else if(_chkfloat_(tmpfloat)==1) xmit_ERR(INVALID_FORMAT);
                        else xmit_ERR(NOT_A_NUMBER);// this for NaN or OoR, could separate
                  }
                else
                  xmit_ERR(ACCESS_DENIED);   

            break;

            case 'c':       // write factory calibration date
            case 'C':
              if (_testbit_(FactoryEnable))
                {
                  write_char_FLASH(CAL_DATE_ADDR, 8, &command_buffer[2]);
                // set the default copy of FLASH now
              //     WDTCN = 0xA5;   // reload watchdog timer 
              //     copy_FLASH_to_XRAM();
              //     erase_DEFAULT();
              //     copy_XRAM_to_DEFAULT();

                  xmit_OK(); 
                 }
               else
                  xmit_ERR(ACCESS_DENIED);
            break;

            case 'd':       // set DEFAULT values--FACTORY
            case 'D':
              if (_testbit_(FactoryEnable))
               { 
                 copy_FLASH_to_XRAM();
                 erase_DEFAULT();
                 copy_XRAM_to_DEFAULT();
                 xmit_OK();         
                 }  // end test for FactoryEnable
              else xmit_ERR(ACCESS_DENIED);         
            break;
//S-
            case 'e':       // write engineering units conversion factor  
            case 'E':
               if(_testbit_(WriteEnable))
  //with this error checking negative numbers get through
                {
                  tmpfloat = atof(&command_buffer[2]);
                    if(_chkfloat_(tmpfloat)==0)    
                      {
                        write_float_FLASH(EU_CONV_ADDR, &tmpfloat);
                        xmit_OK();  
                        init_digital_constants();  // update dig_m, dig_b
                        }
                      else if(_chkfloat_(tmpfloat)==1) xmit_ERR(INVALID_FORMAT);
                        else xmit_ERR(NOT_A_NUMBER);// this for NaN or OoR, could separate
                  }
                else
                  xmit_ERR(ACCESS_DENIED);   
            break;

            case 'm':       // write user's digital gain adjustment
            case 'M':
              if(_testbit_(WriteEnable))
                {
                  tmpfloat = atof(&command_buffer[2]);
                  if(_chkfloat_(tmpfloat)==0)    
                   {
                    write_float_FLASH(USER_DIG_GAIN_ADDR, &tmpfloat);
                    xmit_OK();  
                    init_digital_constants();  // update dig_m, dig_b
                      }
                   else if(_chkfloat_(tmpfloat)==1) xmit_ERR(INVALID_FORMAT);
                   else xmit_ERR(NOT_A_NUMBER);// this for NaN or OoR
                  }
               else
                 xmit_ERR(ACCESS_DENIED);                           
            break;

            case 'p':       // write user defined string
            case 'P':
              if (_testbit_(WriteEnable))
                {
                  write_char_FLASH(USER_STR_ADDR, 16, &command_buffer[2]);
                  xmit_OK();
                  }
                else 
                  xmit_ERR(ACCESS_DENIED);    
            break;

//S-
            case 'r':           // software reset
            case 'R':
              if( (_testbit_(FactoryEnable)) || (_testbit_(WriteEnable))  )
               { 
                 xmit_OK(); 
                 for (x=0;x<=3000;x++) Wait5();  //allow OK before reset
                 soft_reset();              
                 }  // end test for FactoryEnable
               else 
                 xmit_ERR(ACCESS_DENIED); 
            break;          


            case '1':   // set bias for pressure INamp    
              if(_testbit_(FactoryEnable))
                {
                  tmpfloat = atof(&command_buffer[2]);
                  write_float_FLASH(DAC0_ADDR, &tmpfloat);

                  tmpint = (unsigned int)(tmpfloat);
                  highbyte = (tmpint & 0xFF00) / 256;
                  DAC0L = (tmpint - (highbyte*256) & 0x00FF);
                  DAC0H = highbyte;
                  xmit_OK();  
                  }
                else
                  xmit_ERR(ACCESS_DENIED);
            break;
//S-
            case 'w':       // write factory serial number
            case 'W':
              if (_testbit_(FactoryEnable))
                {
               //   command_buffer[0] = 'S';
               //   command_buffer[1] = 'N';
                  write_char_FLASH(SER_NUM_ADDR, 6, &command_buffer[2]);
                  xmit_OK();
                  }
                else
                  xmit_ERR(ACCESS_DENIED);
            break;
/////////////////////////////////////////////

            case 'A':   // sets value for forced output
            case 'a':

              tmpfloat = atof(&command_buffer[2]);
              if( (_chkfloat_(tmpfloat)==0) || (_chkfloat_(tmpfloat) == 1) ) 
               //  if it's standard fp number or it's a fp zero, use it     
               {
                if ( (tmpfloat >= -100.0) && (tmpfloat <=100.0 ) )     // test 0<=x<=100
                 {
                  forced_out_value = tmpfloat;
                  xmit_OK(); 
                  }
                 else xmit_ERR(INVALID_FORMAT);

                }
                 else xmit_ERR(NOT_A_NUMBER);// this for NaN or OoR

            break;

            case 'v':       // set default value for forced output
            case 'V':
              if(_testbit_(WriteEnable))
                {    
                 tmpfloat = atof(&command_buffer[2]);
                 if( (_chkfloat_(tmpfloat)==0) || (_chkfloat_(tmpfloat) == 1) ) 
              //   if it's standard fp number or it's a fp zero, use it     
                  {
                   if ( (tmpfloat >= 0.0) && (tmpfloat <=100.0 ) )     // test 0<=x<=100
                    {
                     write_float_FLASH(FORCED_OUT_DEFAULT_ADDR, &tmpfloat);
                     xmit_OK(); 
                     }
                     else xmit_ERR(INVALID_FORMAT);
                  }
                   else xmit_ERR(NOT_A_NUMBER);// this for NaN or OoR
                 }                
                else
                  xmit_ERR(ACCESS_DENIED);
            break;



            case 'S':           // turns forced output off/on
            case 's':           // sets source of analog output
                  
              if(_testbit_(WriteEnable))
                {    
                  if ( (command_buffer[2] =='0') || (command_buffer[2] == '1') )
                   {
                    if (command_buffer[2] == '0')
                     {
                      forced_out_flag = 0;
                      index = 0;
                      }
                     else
                      {
                       forced_out_flag = 1;
                       index = 1;

                       read_char_FLASH(FORCED_OUT_DEFAULT_ADDR + 4, 1, &addr);    
                       if (addr == DATA_FLAG)                                            
                        read_float_FLASH(FORCED_OUT_DEFAULT_ADDR, &forced_out_value);  
                        else                                                        
                        forced_out_value = 0.0;   

                       }
                    write_char_FLASH(FORCED_OUT_FLAG_ADDR, 1, &index);
                    xmit_OK();
                    }
                    else xmit_ERR(INVALID_FORMAT);
                 }
                  else xmit_ERR(ACCESS_DENIED);         
            break;


            case 'Y':
            case 'y':
                read_float_FLASH(FORCED_OUT_DEFAULT_ADDR, &tmpfloat);   
                flt_to_sci(ser_xmit_buffer, tmpfloat);
                ser_xmit_length = 13;
                ser_xmit_ctr = 0;
                SBUF = ser_xmit_buffer[ser_xmit_ctr];
            break;


            case 'X':   // read back forced_out_value
            case 'x':           
            // read_float_FLASH(V_MON_OFFSET_ADDR, &tmpfloat);   
                flt_to_sci(ser_xmit_buffer, forced_out_value);
                ser_xmit_length = 13;
                ser_xmit_ctr = 0;
                SBUF = ser_xmit_buffer[ser_xmit_ctr];
            break;



////////////////////////////////////////

            case 'z':       // enable factory writes
            case 'Z':
              FactoryEnable = 1;
              xmit_OK();
            break;

            default:
              xmit_ERR(NOT_A_COMMAND);       
            break;

            }   // end switch(command_buffer[1])
      break;    // for outer case 'S'
//W-
      case 'w':
      case 'W':
         switch ( command_buffer[1] )
          {
	        case '1':          // set baud rate
							   //  baud rate changed at this time
              if (_testbit_(WriteEnable))  
               {
                if (command_buffer[2] > ASCII_OFFSET && command_buffer[2] <= (ASCII_OFFSET + 8)) 
                 {
                 switch ( command_buffer[2] )
                   {
                    case '1':
                      command_buffer[0] = BAUD12;
                      tmpint = DELAY12;
                    break;
                    case '2':
                      command_buffer[0] = BAUD24;
                      tmpint = DELAY24;
                    break;                            
                    case '3':
                      command_buffer[0] = BAUD48;
                      tmpint = DELAY48;
                    break; 
                    case '4':
                      command_buffer[0] = BAUD96;
                      tmpint = DELAY96;
                    break;
                    case '5':
                      command_buffer[0] = BAUD192;
                      tmpint = DELAY192;
                    break;
                    case '6':
                      command_buffer[0] = BAUD384;
                      tmpint = DELAY384;
                    break;
                    case '7':
                      command_buffer[0] = BAUD576;
                      tmpint = DELAY576;
                    break;    
                    case '8':
                      command_buffer[0] = BAUD1152;
                      tmpint = DELAY1152;
                    break;
                     }
                 xmit_OK();      
                 for (x=0;x<=3000;x++) Wait5();  //allow OK before changing 

                 write_char_FLASH(BAUD_ADDR, 1, command_buffer);
                                  
                                                                
     		     TR2 = 0;                   // timer 2 off, then set it up  
#ifdef _22MHZ_
                 if(command_buffer[0] == 0xC0) RCAP2H = 0xFD;
                 else if(command_buffer[0] == 0xE0) RCAP2H = 0xFE;
                 else RCAP2H = 0xFF;
#else
                 if(command_buffer[0] == 0xE0) RCAP2H = 0xFE;
                 else RCAP2H = 0xFF;
#endif                
                 TH2 = RCAP2H;
        		 RCAP2L = command_buffer[0];
        		 TL2 = command_buffer[0];
        		 TR2 = 1; 
                         
              //tmpint now has timer1 reload value for transmit delay
              //save it and use it now                 
                 highbyte = (tmpint & 0xFF00) / 256;
                 command_buffer[0] = (tmpint - (highbyte*256) & 0x00FF);
                 command_buffer[1] = highbyte;
                 timer1reload_low = command_buffer[0];
                 timer1reload_high = command_buffer[1];

                 TL1 = command_buffer[0];
                 TH1 = command_buffer[1]; 

                 write_char_FLASH(XMIT_DELAY_ADDR, 2, command_buffer);                  
                                                      					
                  }       // end if(command_buffer[2] > ASCII...
                else
                   xmit_ERR(INVALID_FORMAT);   
               }        // end if (_testbit_(WriteEnable))
             else   
               xmit_ERR(ACCESS_DENIED);
            break;


            case '4':       // write user assigned address
              if(_testbit_(WriteEnable))
                {
                    write_char_FLASH(UNIT_ADDR, 2, &command_buffer[2]);
                    unit_addr_hi = command_buffer[2];
                    unit_addr_lo = command_buffer[3];
                    xmit_OK();
                  }
                else 
                  xmit_ERR(ACCESS_DENIED); 
            break;

            case '5':       // write F.S. pressure range (PSI), factory
              if(_testbit_(FactoryEnable))
                {
                  tmpfloat = atof(&command_buffer[2]);
                  // check for valid float, 
                  write_float_FLASH(RANGE_ADDR, &tmpfloat);
                  xmit_OK();  
                  init_digital_constants();  // update dig_m, dig_b
                  }
                else
                  xmit_ERR(ACCESS_DENIED);
            break;
            
            case '6':       // write engineering units label
              if(_testbit_(WriteEnable))
                {
                    write_char_FLASH(EU_STR_ADDR, 4, &command_buffer[2]);
                    xmit_OK();
                  }
                else
                 xmit_ERR(ACCESS_DENIED);
            break;


            case 'm':       // write mgf. part number string
            case 'M':
              if (_testbit_(FactoryEnable))
                {
                 // write_char_FLASH(MFG_PART_ADDR, 11, &command_buffer[2]);

                  if ( command_buffer[13] == 0x0D)  // single dash
                  write_char_FLASH(MFG_PART_ADDR, 11, &command_buffer[2]);
                
                  else                              // double dash
                  write_char_FLASH(MFG_PART_ADDR, 14, &command_buffer[2]);

                  xmit_OK();
                  }
                else 
                  xmit_ERR(ACCESS_DENIED);    
            break;

            case 'o':       // set analog scale factor
            case 'O':
              if(_testbit_(WriteEnable))
                {
                  tmpfloat = atof(&command_buffer[2]);
                  if(_chkfloat_(tmpfloat)==0)       // check for valid float 
                   {                 
                    write_float_FLASH(USER_SCALE_ADDR, &tmpfloat);
                    xmit_OK();  
                    init_analog_constants();  // update ana_m, ana_b
                    }
                  else if(_chkfloat_(tmpfloat)==1) xmit_ERR(INVALID_FORMAT);
                  else xmit_ERR(NOT_A_NUMBER);// this for NaN or OoR
                  }
                else
                  xmit_ERR(ACCESS_DENIED);
            break;

            case 'n':       // set analog offset factor
            case 'N':
              if(_testbit_(WriteEnable))
                {
                  tmpfloat = atof(&command_buffer[2]);
                  if( (_chkfloat_(tmpfloat)==0) || (_chkfloat_(tmpfloat) == 1) )  
                   {
                    write_float_FLASH(USER_OFFSET_ADDR, &tmpfloat);
                    xmit_OK();  
                    init_analog_constants();  // update ana_m, ana_b
                    }
      //            else if(_chkfloat_(tmpfloat)==1) xmit_ERR(INVALID_FORMAT);
                  else xmit_ERR(NOT_A_NUMBER);// this for NaN or OoR
                  }
                else
                  xmit_ERR(ACCESS_DENIED);
            break;

            case 'e':       // write enable
            case 'E':
              WriteEnable = 1;
              xmit_OK();
            break;
  
            default:
              xmit_ERR(NOT_A_COMMAND);  
            break;

            }   // end switch(command_buffer[1])
      break;    // for outer case 'W'


      case 'k':
      case 'K':
           switch(command_buffer[1])
             {
              case '1':   // set vmon_scale
                if (_testbit_(FactoryEnable))
                  { 
                    vmon_scale = atof(&command_buffer[2]);
                    // check for valid float
                    write_float_FLASH(V_MON_SCALE_ADDR, &vmon_scale);
                    xmit_OK(); 
                   }
                else
                  xmit_ERR(ACCESS_DENIED);            
              break;

              case '2':   // set vmon_offset
                if (_testbit_(FactoryEnable))
                  { 
                    vmon_offset = atof(&command_buffer[2]);
                    // check for valid float
                    write_float_FLASH(V_MON_OFFSET_ADDR, &vmon_offset);
                    xmit_OK();
                   }
                else
                  xmit_ERR(ACCESS_DENIED);                     
              break;

              case '3':   // read vmon_scale
                read_float_FLASH(V_MON_SCALE_ADDR, &tmpfloat);   
                flt_to_sci(ser_xmit_buffer, tmpfloat);
                ser_xmit_length = 13;
                ser_xmit_ctr = 0;
                SBUF = ser_xmit_buffer[ser_xmit_ctr];
              break;
                
              case '4':   // read vmon_offset
                read_float_FLASH(V_MON_OFFSET_ADDR, &tmpfloat);   
                flt_to_sci(ser_xmit_buffer, tmpfloat);
                ser_xmit_length = 13;
                ser_xmit_ctr = 0;
                SBUF = ser_xmit_buffer[ser_xmit_ctr];
              break;

              default:
              xmit_ERR(NOT_A_COMMAND);       
              break;

               }    //end switch(command_buffer[1])
          
      break;    // for outer case 'K'


// I-
      case 'I':
      case 'i':

        switch(command_buffer[1])
         {
          case 'I':
          case 'i':

            if (command_buffer[2] == '?')
              {
                int_to_ascii(ser_xmit_buffer, (unsigned int)user_defined_rate);
                ser_xmit_length = 6;
                ser_xmit_ctr = 0;
                SBUF = ser_xmit_buffer[ser_xmit_ctr];
               }
               else

            if (_testbit_(WriteEnable))  
           {
             switch (command_buffer[2])
             {
              case '0':
              user_defined_rate = 1;
              write_char_FLASH(USER_RATE_ADDR, 1, &user_defined_rate);
              xmit_OK();
              break;

              case '1':
              user_defined_rate = 2;
              write_char_FLASH(USER_RATE_ADDR, 1, &user_defined_rate);
              xmit_OK();
              break;

              case '2':
              user_defined_rate = 4;
              write_char_FLASH(USER_RATE_ADDR, 1, &user_defined_rate);
              xmit_OK();
              break;
  
              case '3':
              user_defined_rate = 8;
              write_char_FLASH(USER_RATE_ADDR, 1, &user_defined_rate);
              xmit_OK();
              break;

              case '4':
              user_defined_rate = 16;
              write_char_FLASH(USER_RATE_ADDR, 1, &user_defined_rate);
              xmit_OK();
              break;

              case '5':
              user_defined_rate = 32;
              write_char_FLASH(USER_RATE_ADDR, 1, &user_defined_rate);
              xmit_OK();
              break;

              case '6':
              user_defined_rate = 64;
              write_char_FLASH(USER_RATE_ADDR, 1, &user_defined_rate);
              xmit_OK();
              break;

              case '7':
              user_defined_rate = 128;
              write_char_FLASH(USER_RATE_ADDR, 1, &user_defined_rate);
              xmit_OK();
              break;

              case '8':
              user_defined_rate = 255;
              write_char_FLASH(USER_RATE_ADDR, 1, &user_defined_rate);
              xmit_OK();  
              break;

              default:
              xmit_ERR(INVALID_FORMAT);
              break;

            }   //end switch(command_buffer[2])

          }        // end if (_testbit_(WriteEnable))
          else   
           xmit_ERR(ACCESS_DENIED);
          break;

          default:
            xmit_ERR(NOT_A_COMMAND);       
          break;
    
            }
 
      break;  // end outer case 'I'


 //H-
      case 'H':             // write high range coefficients to FLASH
      case 'h':
         if (isdigit(command_buffer[1]))
         {
            if (_testbit_(FactoryEnable))
             { 
                index = command_buffer[1] - ASCII_OFFSET;
                coeff[index] = atof(&command_buffer[2]);
                 // check if valid float is returned, if not transmit error
                if ( (_chkfloat_(coeff[index])==0) || (_chkfloat_(coeff[index])==1) )
                  {
                    addr = (index * 5) + HIGH_COEFF_ADDR_OFFSET;
                    write_float_FLASH(addr, &coeff[index]);
                    xmit_OK();
                    }
                  else xmit_ERR(NOT_A_NUMBER);         
               }  // end test for FactoryEnable
              else xmit_ERR(ACCESS_DENIED);         
           }                                                           
      break;    // end outer case 'H'
    
//L-
      case 'L':             // write low range coefficients to FLASH
      case 'l':
         if (isdigit(command_buffer[1]))
         {
            if (_testbit_(FactoryEnable))
             { 
                index = command_buffer[1] - ASCII_OFFSET;
                coeff[index] = atof(&command_buffer[2]);
                // check if valid float is returned, if not transmit error
                if ( (_chkfloat_(coeff[index])==0) || (_chkfloat_(coeff[index])==1) )
                  {
                    addr = (index * 5) + LOW_COEFF_ADDR_OFFSET;
                    write_float_FLASH(addr, &coeff[index]);
                    xmit_OK(); 
                    }
                  else xmit_ERR(NOT_A_NUMBER);        
               }  // end test for FactoryEnable
              else xmit_ERR(ACCESS_DENIED);         
           }                                                           
      break;    // end outer case 'L'
 
//B-     
    case 'b':
    case 'B':
             switch ( command_buffer[1] )
          {

            case'1':
              if(_testbit_(FactoryEnable))
                {
                  tmpfloat = atof(&command_buffer[2]);
                  write_float_FLASH(MIDTOLOW_ADDR, &tmpfloat);
                  mid_to_low = (unsigned int) (tmpfloat);
                  xmit_OK();  
                  }
                else
                  xmit_ERR(ACCESS_DENIED);
            break;

            case'2':
              if(_testbit_(FactoryEnable))
                {
                  tmpfloat = atof(&command_buffer[2]);
                  write_float_FLASH(LOWTOMID_ADDR, &tmpfloat);
                  low_to_mid = (unsigned int) (tmpfloat);
                  xmit_OK();  
                  }
                else
                  xmit_ERR(ACCESS_DENIED);
            break;

            case'3':
              if(_testbit_(FactoryEnable))
                {
                  tmpfloat = atof(&command_buffer[2]);
                  write_float_FLASH(HIGHTOMID_ADDR, &tmpfloat);
                  high_to_mid = (unsigned int) (tmpfloat);
                  xmit_OK();  
                  }
                else
                  xmit_ERR(ACCESS_DENIED);
            break;

            case'4':
              if(_testbit_(FactoryEnable))
                {
                  tmpfloat = atof(&command_buffer[2]);
                  write_float_FLASH(MIDTOHIGH_ADDR, &tmpfloat);
                  mid_to_high = (unsigned int) (tmpfloat);
                  xmit_OK();  
                  }
                else
                  xmit_ERR(ACCESS_DENIED);
            break;

            case'5':
              if(_testbit_(FactoryEnable))
                {
                  tmpfloat = atof(&command_buffer[2]);
                  write_float_FLASH(MAX_TEMP_ADDR, &tmpfloat);
                  max_temp = (unsigned int) (tmpfloat);
                  xmit_OK();  
                  }
                else
                  xmit_ERR(ACCESS_DENIED);
            break;

            case'6':
              if(_testbit_(FactoryEnable))
                {
                  tmpfloat = atof(&command_buffer[2]);
                  write_float_FLASH(MIN_TEMP_ADDR, &tmpfloat);
                  min_temp = (unsigned int) (tmpfloat);
                  xmit_OK();  
                  }
                else
                  xmit_ERR(ACCESS_DENIED);
            break;

            case'7':
              if(_testbit_(FactoryEnable))
                {
                  tmpfloat = atof(&command_buffer[2]);
                  write_float_FLASH(MAX_PRESS_ADDR, &tmpfloat);
                  max_press = (unsigned int) (tmpfloat);
                  xmit_OK();  
                  }
                else
                  xmit_ERR(ACCESS_DENIED);
            break;

            case'8':
              if(_testbit_(FactoryEnable))
                {
                  tmpfloat = atof(&command_buffer[2]);
                  write_float_FLASH(MIN_PRESS_ADDR, &tmpfloat);
                  min_press = (unsigned int) (tmpfloat);
                  xmit_OK();  
                  }
                else
                  xmit_ERR(ACCESS_DENIED);
            break;

            case 'c':
            case 'C':
              if(_testbit_(FactoryEnable))
                {
                  tmpfloat = atof(&command_buffer[2]);
                  // check for valid float
                  write_float_FLASH(FACT_ANA_CAL_ADDR, &tmpfloat);
                  xmit_OK();  
                  init_analog_constants();  // update ana_m, ana_b
                  }
                else
                  xmit_ERR(ACCESS_DENIED);

            break;

            case 's':               // set factory_span
            case 'S':
              if(_testbit_(FactoryEnable))
                {
                  tmpfloat = atof(&command_buffer[2]);
                  // check for valid float
                  write_float_FLASH(FACT_SPAN_ADDR, &tmpfloat);
                  xmit_OK();  
                  init_analog_constants();  // update ana_m, ana_b
                  }
                else
                  xmit_ERR(ACCESS_DENIED);
            break;

            case 'z':               // set factory_zero
            case 'Z':
              if(_testbit_(FactoryEnable))
                {
                  tmpfloat = atof(&command_buffer[2]);
                  // check for valid float
                  write_float_FLASH(FACT_ZERO_ADDR, &tmpfloat);
                  xmit_OK();  
                  init_analog_constants();  // update ana_m, ana_b
                  }
                else
                  xmit_ERR(ACCESS_DENIED);
            break;

            default:
              xmit_ERR(NOT_A_COMMAND);  
            break;

            }
    break;  // end outer case 'B'

//J-
    case 'j':
    case 'J':
             switch ( command_buffer[1] )
          {

            case'1':
              ser_xmit_buffer[0] = 'B';      
         	  ser_xmit_buffer[1] = '1';        
		      ser_xmit_buffer[2] = '=';      
              int_to_ascii(&ser_xmit_buffer[3], mid_to_low);
              ser_xmit_length = 9;        
              ser_xmit_ctr = 0;         
              SBUF = ser_xmit_buffer[ser_xmit_ctr];  
            break;

            case'2':
              ser_xmit_buffer[0] = 'B';      
         	  ser_xmit_buffer[1] = '2';        
		      ser_xmit_buffer[2] = '=';      
              int_to_ascii(&ser_xmit_buffer[3], low_to_mid);
              ser_xmit_length = 9;        
              ser_xmit_ctr = 0;         
              SBUF = ser_xmit_buffer[ser_xmit_ctr];  
            break;

            case'3':
              ser_xmit_buffer[0] = 'B';      
         	  ser_xmit_buffer[1] = '3';        
		      ser_xmit_buffer[2] = '=';      
              int_to_ascii(&ser_xmit_buffer[3], high_to_mid);
              ser_xmit_length = 9;        
              ser_xmit_ctr = 0;         
              SBUF = ser_xmit_buffer[ser_xmit_ctr];  
            break;

            case'4':
              ser_xmit_buffer[0] = 'B';      
         	  ser_xmit_buffer[1] = '4';        
		      ser_xmit_buffer[2] = '=';      
              int_to_ascii(&ser_xmit_buffer[3], mid_to_high);
              ser_xmit_length = 9;        
              ser_xmit_ctr = 0;         
              SBUF = ser_xmit_buffer[ser_xmit_ctr];  
            break;

            case'5':
              ser_xmit_buffer[0] = 'B';      
         	  ser_xmit_buffer[1] = '5';        
		      ser_xmit_buffer[2] = '=';      
              int_to_ascii(&ser_xmit_buffer[3], max_temp);
              ser_xmit_length = 9;        
              ser_xmit_ctr = 0;         
              SBUF = ser_xmit_buffer[ser_xmit_ctr];  
            break;

            case'6':
              ser_xmit_buffer[0] = 'B';      
         	  ser_xmit_buffer[1] = '6';        
		      ser_xmit_buffer[2] = '=';      
              int_to_ascii(&ser_xmit_buffer[3], min_temp);
              ser_xmit_length = 9;        
              ser_xmit_ctr = 0;         
              SBUF = ser_xmit_buffer[ser_xmit_ctr];  
            break;

            case'7':
              ser_xmit_buffer[0] = 'B';      
         	  ser_xmit_buffer[1] = '7';        
		      ser_xmit_buffer[2] = '=';      
              int_to_ascii(&ser_xmit_buffer[3], max_press);
              ser_xmit_length = 9;        
              ser_xmit_ctr = 0;         
              SBUF = ser_xmit_buffer[ser_xmit_ctr];  
            break;

            case'8':
              ser_xmit_buffer[0] = 'B';      
         	  ser_xmit_buffer[1] = '8';        
		      ser_xmit_buffer[2] = '=';      
              int_to_ascii(&ser_xmit_buffer[3], min_press);
              ser_xmit_length = 9;        
              ser_xmit_ctr = 0;         
              SBUF = ser_xmit_buffer[ser_xmit_ctr];  
            break;

            case 'c':
            case 'C':
              read_float_FLASH(FACT_ANA_CAL_ADDR, &tmpfloat);
              flt_to_sci(ser_xmit_buffer, tmpfloat);
              ser_xmit_length = 13;
              ser_xmit_ctr = 0;
              SBUF = ser_xmit_buffer[ser_xmit_ctr];

            break;


            case 's':
            case 'S':
                read_float_FLASH(FACT_SPAN_ADDR, &tmpfloat);
                ser_xmit_buffer[0] = 'S';
                ser_xmit_buffer[1] = '=';
                int_to_ascii(&ser_xmit_buffer[2], (unsigned int)(tmpfloat));
             //   flt_to_sci(ser_xmit_buffer, tmpfloat);
                ser_xmit_length = 8;
                ser_xmit_ctr = 0;
                SBUF = ser_xmit_buffer[ser_xmit_ctr];
            break;

            case 'z':
            case 'Z':
                read_float_FLASH(FACT_ZERO_ADDR, &tmpfloat);
                ser_xmit_buffer[0] = 'O';
                ser_xmit_buffer[1] = '=';
                int_to_ascii(&ser_xmit_buffer[2], (unsigned int)(tmpfloat));
             //   flt_to_sci(ser_xmit_buffer, tmpfloat);
                ser_xmit_length = 8;
                ser_xmit_ctr = 0;
                SBUF = ser_xmit_buffer[ser_xmit_ctr];
            break;

            default:
              xmit_ERR(NOT_A_COMMAND);  
            break;

            }
      break;    // end outer case 'J'

//T-
      case 't':           //write temperature coefficients to FLASH
      case 'T':
         if (isdigit(command_buffer[1]))
         {
            if (_testbit_(FactoryEnable))
             { 
                index = command_buffer[1] - ASCII_OFFSET;
                temp_coeff[index] = atof(&command_buffer[2]);
         // check if valid float is returned, if not transmit error
                addr = (index * 5) + TEMP_COEFF_ADDR_OFFSET;
                write_float_FLASH(addr, &temp_coeff[index]);

                xmit_OK();         
               }  // end test for FactoryEnable
              else xmit_ERR(ACCESS_DENIED);         
           }                                                           
          else xmit_ERR(NOT_A_COMMAND);
      break;

//U-
      case 'u':
      case 'U':
         index = command_buffer[1] - ASCII_OFFSET;
      //   addr = (index * 5);
      //   read_float_FLASH(addr, &coeff[index]);
         flt_to_sci(ser_xmit_buffer, temp_coeff[index]);
         ser_xmit_length = 13;
         ser_xmit_ctr = 0;
         SBUF = ser_xmit_buffer[ser_xmit_ctr];
      break;

//V-
      case 'v':     // Read back the low temp pressure equation coefficients
      case 'V':     // Read from FLASH memory, 'addr' is uchar
         index = command_buffer[1] - ASCII_OFFSET;
         addr = (index * 5) + LOW_COEFF_ADDR_OFFSET;
         read_float_FLASH(addr, &tmpfloat);
         flt_to_sci(ser_xmit_buffer, tmpfloat);
         ser_xmit_length = 13;
         ser_xmit_ctr = 0;
         SBUF = ser_xmit_buffer[ser_xmit_ctr];
      break;

//X-
      case 'x':     // final version will NOT read from FLASH
      case 'X':     // coefficients will have been initialized already
         index = command_buffer[1] - ASCII_OFFSET;
      //   addr = (index * 5);
      //   read_float_FLASH(addr, &coeff[index]);
         flt_to_sci(ser_xmit_buffer, coeff[index]);
         ser_xmit_length = 13;
         ser_xmit_ctr = 0;
         SBUF = ser_xmit_buffer[ser_xmit_ctr];
      break;

//Y-
      case 'y':     // Read back the high temp pressure equation coefficients
      case 'Y':     // Read from FLASH memory, 'addr' is uchar, only 0-6 valid
         index = command_buffer[1] - ASCII_OFFSET;
         addr = (index * 5) + HIGH_COEFF_ADDR_OFFSET;
         read_float_FLASH(addr, &tmpfloat);
         flt_to_sci(ser_xmit_buffer, tmpfloat);
         ser_xmit_length = 13;
         ser_xmit_ctr = 0;
         SBUF = ser_xmit_buffer[ser_xmit_ctr];
      break;

//Z-
      case 'z':                     //   manifest command
      case 'Z':                     // to work with utility software
         if((command_buffer[1]=='y')||(command_buffer[1]=='Y'))
          {
           strcpy(ser_xmit_buffer, "E08432");
           ser_xmit_length = 7;
           ser_xmit_ctr = 0;
           SBUF = ser_xmit_buffer[ser_xmit_ctr];
           }
         else xmit_ERR(NOT_A_COMMAND); 
      break;


      default:
         xmit_ERR(NOT_A_COMMAND);   
      break;    //    
             
      }   // end switch(command_buffer[0])

  } // end ExecuteCommand()



//---------------------------------------------------------------------------
//  Function:   init_DAC0()
//
//
//---------------------------------------------------------------------------

void init_DAC0(void)
{

  float xdata tmpfloat;
  unsigned char xdata test;
  unsigned int xdata tmpint;

  read_char_FLASH(DAC0_ADDR + 4, 1, &test);    // get the flag
  if (test == DATA_FLAG)
  {                             // if valid value
    read_float_FLASH(DAC0_ADDR, &tmpfloat);      // use it


      tmpint = (unsigned int)(tmpfloat);
      test = (tmpint & 0xFF00) / 256;
      DAC0L = (tmpint - (test*256) & 0x00FF);
      DAC0H = test;

      DAC1L = (tmpint - (test*256) & 0x00FF);
      DAC1H = test;
    }
    else                                        // otherwise
    {
            // default is 100 counts for zero
      DAC0L = 0x64;
      DAC0H = 0x00;                            // use the default
      DAC1L = 0x64;
      DAC1H = 0x00;
      
      }
}   // end init_DAC0()


//---------------------------------------------------------------------------
//  Function:   init_addr()
//
//
//---------------------------------------------------------------------------
void init_addr(void)
{
  read_char_FLASH(UNIT_ADDR, 3, command_buffer);
  if (command_buffer[2] == DATA_FLAG)
    {
      unit_addr_hi = command_buffer[0];
      unit_addr_lo = command_buffer[1];  
      }
  else
    {
      unit_addr_hi = 0x30;      // default to 00, 
      unit_addr_lo = 0x30;
      } 

  } // end init_addr()


//---------------------------------------------------------------------------
//	Function:	init_baudrate()
//                                                                       		
//  Comments:   Sets baudrate: uses values in FLASH if flag is valid
//                  else default is 9600 sey in main() at startup  
//    
//---------------------------------------------------------------------------
void init_baudrate (void)
{                                       	
   read_char_FLASH(BAUD_ADDR, 2, command_buffer);
   if (command_buffer[1] == DATA_FLAG)              	// if valid flag
     {
       TR2 = 0;

#ifdef _22MHZ_
       if(command_buffer[0] == 0xC0) RCAP2H = 0xFD;
       else if(command_buffer[0] == 0xE0) RCAP2H = 0xFE;
       else RCAP2H = 0xFF;
#else
       if(command_buffer[0] == 0xE0) RCAP2H = 0xFE;
       else RCAP2H = 0xFF;
#endif

        TH2 = RCAP2H;              // 
        RCAP2L = command_buffer[0];
        TL2 = command_buffer[0];
        TR2 = 1;
        
 //now get transmit delay value       
       read_char_FLASH(XMIT_DELAY_ADDR, 2, command_buffer);  
       timer1reload_low = command_buffer[0];
       timer1reload_high = command_buffer[1];
       
       TL1 = command_buffer[0];
       TH1 = command_buffer[1]; 
          
        }   
          								// no valid flag, defaults to 9600
    								    //    set in main()
 }   // end init_baudrate()			


//---------------------------------------------------------------------------
// 	Function:	init_coeff ()	
//  Parameters:	none
//  Returns:	none    
//  Comments:   Loads compensation coefficients from FLASH at power up
//
//---------------------------------------------------------------------------
void init_coeff(void)
{
  unsigned char i;

//first check flag for last coefficient
    read_char_FLASH(((COEFF_MAX_NUM*5)-1), 1, command_buffer);
    if (command_buffer[0] == DATA_FLAG)
        {
           for (i=0; i<=COEFF_MAX_NUM-1; i++)
             read_float_FLASH(i*5, &coeff[i]);
           }
      else      // re-evaluate defaults for this implementation!!!!
        {
            for (i=0; i<=COEFF_MAX_NUM-1; i++) coeff[i] = 0;
            coeff[0] = -7.95;
            coeff[1] = 0.007476;
           }

  }	// end init_coeff()      


//----------------------------------------------------------------------------
//  Function:   init_digital_constants()
//
//  Comments:   Initializes dig_m and dig_b for digital output equation:
//                  output = dig_m*presscomp + dig_b
//                      where presscomp is output of temp. comp. equation
//
//              Output will default to % F.S. if range and Eu-conv are not set
//              If range is set, output defaults to PSI if eu_conv is not set
//                  (set range = 100, eu_conv = 1 for %F.S. output)
//              Defaults to no user adjustments
//              All four should be set at factory. User cannot change range.
//----------------------------------------------------------------------------

void init_digital_constants(void)
{
  float range;              // where should these be stored??
  float eu_conv;            // could use xdata 
  float user_dig_zero;
  float user_dig_gain;

  unsigned char test;

  read_char_FLASH(RANGE_ADDR + 4, 1, &test);    // get the flag
  if (test == DATA_FLAG)                             // if valid value
    read_float_FLASH(RANGE_ADDR, &range);       // use it
    else                                        // otherwise
      range = 100;                              // use the default

  read_char_FLASH(EU_CONV_ADDR + 4, 1, &test);
  if (test == DATA_FLAG)
    read_float_FLASH(EU_CONV_ADDR, &eu_conv);
    else
      eu_conv = 1;

  read_char_FLASH(USER_DIG_ZERO_ADDR + 4, 1, &test); 
  if (test == DATA_FLAG)
    read_float_FLASH(USER_DIG_ZERO_ADDR, &user_dig_zero);
    else
      user_dig_zero = 0;

  read_char_FLASH(USER_DIG_GAIN_ADDR + 4, 1, &test);
  if (test == DATA_FLAG)
    read_float_FLASH(USER_DIG_GAIN_ADDR, &user_dig_gain);
    else
      user_dig_gain = 100;

  dig_m = ((range*eu_conv)/100)*(user_dig_gain/100);
  dig_b = ((range*eu_conv)/100)*user_dig_zero;

  } // end init_digital_constants()



//---------------------------------------------------------------------------
//  Function name:	Compensate()
//  Parameters:		none
//  Returns:	    none
//  Comments:       Function compensates pressure reading.
//                   Uses globals "pressure" and "temperature"
//                   Modifies global "presscal"
//---------------------------------------------------------------------------
void compensate (void)
{
 float  numer, denom; 
 float  temp2;
         	
// FULL COMPENSATION ROUTINE HERE  -  EQN 1010 implemented:
// out = (a + bx + cy + dy^2)/(1 + ex + fy + gy^2)

//  numer = coeff[0] + coeff[1]*pressure + coeff[2]*temperature + coeff[3]*temperature*temperature;
//  denom = 1 + coeff[4]*pressure + coeff[5]*temperature + coeff[6]*temperature*temperature;
//  presscal = (numer /denom);

///FASTER:

 // temp2 = (float)temperature * (float)temperature;
    temp2 = temperature * temperature;      //cast when copying from isr variables

  numer = coeff[0] + coeff[1]*pressure + coeff[2]*temperature + coeff[3]*temp2;
  	
  denom = 1 + coeff[4]*pressure + coeff[5]*temperature + coeff[6]*temp2;
    
  presscal = (numer / denom);  
       
  }	// end compensate ()
                    

//---------------------------------------------------------------------------
//  Function:   init_temp_bounds()
//              also max / min for raw temp and press
//---------------------------------------------------------------------------

void init_temp_bounds(void)
{

  float tmpfloat;
  unsigned char test;


  read_char_FLASH(MIDTOLOW_ADDR + 4, 1, &test);    // get the flag
  if (test == DATA_FLAG)                                  // if valid value
    {                                   
    read_float_FLASH(MIDTOLOW_ADDR, &tmpfloat); 
    mid_to_low = (unsigned int)(tmpfloat);      // use it
    }
    else 
      mid_to_low = 1700;                              // use the default

  read_char_FLASH(LOWTOMID_ADDR + 4, 1, &test);    // get the flag
  if (test == DATA_FLAG)                                  // if valid value
    {                                   
    read_float_FLASH(LOWTOMID_ADDR, &tmpfloat); 
    low_to_mid = (unsigned int)(tmpfloat);      // use it
    }
    else 
      low_to_mid = 1780;                              // use the default

  read_char_FLASH(HIGHTOMID_ADDR + 4, 1, &test);    // get the flag
  if (test == DATA_FLAG)                                  // if valid value
    {                                   
    read_float_FLASH(HIGHTOMID_ADDR, &tmpfloat); 
    high_to_mid = (unsigned int)(tmpfloat);      // use it
    }
    else 
      high_to_mid = 2400;                              // use the default

  read_char_FLASH(MIDTOHIGH_ADDR + 4, 1, &test);    // get the flag
  if (test == DATA_FLAG)                                  // if valid value
    {                                   
    read_float_FLASH(MIDTOHIGH_ADDR, &tmpfloat); 
    mid_to_high = (unsigned int)(tmpfloat);      // use it
    }
    else 
      mid_to_high = 2480;                              // use the default

  read_char_FLASH(MAX_TEMP_ADDR + 4, 1, &test);    // get the flag
  if (test == DATA_FLAG)                                  // if valid value
    {                                   
    read_float_FLASH(MAX_TEMP_ADDR, &tmpfloat); 
    max_temp = (unsigned int)(tmpfloat);      // use it
    }
    else 
      max_temp = 4000;                              // use the default

  read_char_FLASH(MIN_TEMP_ADDR + 4, 1, &test);    // get the flag
  if (test == DATA_FLAG)                                  // if valid value
    {                                   
    read_float_FLASH(MIN_TEMP_ADDR, &tmpfloat); 
    min_temp = (unsigned int)(tmpfloat);      // use it
    }
    else 
      min_temp = 1000;                              // use the default

  read_char_FLASH(MAX_PRESS_ADDR + 4, 1, &test);    // get the flag
  if (test == DATA_FLAG)                                  // if valid value
    {                                   
    read_float_FLASH(MAX_PRESS_ADDR, &tmpfloat); 
    max_press = (unsigned int)(tmpfloat);      // use it
    }
    else 
      max_press = 15500;                              // use the default


  read_char_FLASH(MIN_PRESS_ADDR + 4, 1, &test);    // get the flag
  if (test == DATA_FLAG)                                  // if valid value
    {                                   
    read_float_FLASH(MIN_PRESS_ADDR, &tmpfloat); 
    min_press = (unsigned int)(tmpfloat);      // use it
    }
    else 
      min_press = 300;                              // use the default


  }     // end of init_temp_bounds() 


//---------------------------------------------------------------------------
//
//  Function:   check_range()
//  Parameters: none
//  Returns:    none
//  Comments:   Checks which of 3 temperature ranges the present temp is in,
//              loads new coefficients if different than previous range.
//              Hysteresis at boundaries prevents toggling between two ranges 
//---------------------------------------------------------------------------

void check_range(void)
{
    if (( inter_t <= mid_to_low) && (previous_range == MID))
        {
          init_low_coeff();
          previous_range = LOW;
          return;
         }
    if (( inter_t >= mid_to_high) && (previous_range == MID))
        {
          init_high_coeff();
          previous_range = HIGH;
          return;
         }
     if(  (  (inter_t >= low_to_mid) && (inter_t <= high_to_mid)  )
        &&
          (  (previous_range == LOW) || (previous_range == HIGH)  )   )
         {
            init_coeff();
            previous_range = MID;
            return;
           }                  
        
 }  // end void check_range()

//---------------------------------------------------------------------------
// 	Function:	init_low_coeff ()	
//  Parameters:	none
//  Returns:	none    
//  Comments:   Loads compensation coefficients from FLASH at power up
//                  for LOW temp range
//---------------------------------------------------------------------------
void init_low_coeff(void)
{
  unsigned char i;
//  TIMINGBIT = 1;

//first check flag for last coefficient
    read_char_FLASH((((COEFF_MAX_NUM*5)-1)+LOW_COEFF_ADDR_OFFSET), 1, command_buffer);
    if (command_buffer[0] == DATA_FLAG)
        {
           for (i=0; i<=COEFF_MAX_NUM-1; i++)
             read_float_FLASH((i*5)+LOW_COEFF_ADDR_OFFSET, &coeff[i]);
           }
      else      // re-evaluate defaults for this implementation!!!!
        {
            for (i=0; i<=COEFF_MAX_NUM-1; i++) coeff[i] = 0;
            coeff[0] = -7.95;
            coeff[1] = 0.007476;
           }
//  TIMINGBIT = 0;

  }	// end init_low_coeff()  
  
  
//---------------------------------------------------------------------------
// 	Function:	init_high_coeff ()	
//  Parameters:	none
//  Returns:	none    
//  Comments:   Loads compensation coefficients from FLASH at power up
//                  for HIGH temp range
//---------------------------------------------------------------------------
void init_high_coeff(void)
{
  unsigned char i;

//first check flag for last coefficient
    read_char_FLASH((((COEFF_MAX_NUM*5)-1)+HIGH_COEFF_ADDR_OFFSET), 1, command_buffer);
    if (command_buffer[0] == DATA_FLAG)
        {
           for (i=0; i<=COEFF_MAX_NUM-1; i++)
             read_float_FLASH((i*5)+HIGH_COEFF_ADDR_OFFSET, &coeff[i]);
           }
      else      // re-evaluate defaults for this implementation!!!!
        {
            for (i=0; i<=COEFF_MAX_NUM-1; i++) coeff[i] = 0;
            coeff[0] = -7.95;
            coeff[1] = 0.007476;
           }

  }	// end init_high_coeff()  
  
  
//---------------------------------------------------------------------------
//  Function:   init_temp_coeff()
//
//---------------------------------------------------------------------------

void init_temp_coeff(void)
{

  unsigned char i;

//first check flag for last coefficient

    read_char_FLASH( (TEMP_COEFF_ADDR_OFFSET + (MAX_NUM_TEMP_COEFF*5)-1 ), 1, command_buffer);

    if (command_buffer[0] == DATA_FLAG)
        {
           for (i=0; i<=MAX_NUM_TEMP_COEFF-1; i++)
             read_float_FLASH(TEMP_COEFF_ADDR_OFFSET + (i*5), &temp_coeff[i]);
           }
      else      // determine defaults for temperature !!!!
        {
            for (i=0; i<=MAX_NUM_TEMP_COEFF-1; i++) temp_coeff[i] = 0;
            temp_coeff[0] = -525;
            temp_coeff[1] = 0.526;
            temp_coeff[2] = -.00017;    //use these for now 2/1/02
           }

 }  //  end init_temp_coeff()
   
 
//---------------------------------------------------------------------------
//  Function: init_average()
//
//--------------------------------------------------------------------------

void init_average(void)
{
  read_char_FLASH(USER_RATE_ADDR, 2, command_buffer);
  if (command_buffer[1] == DATA_FLAG)
    {
        user_defined_rate = command_buffer[0];
      }
  else
    {
      user_defined_rate = 1;      // default to 1
      } 

  } // end init_average()


//---------------------------------------------------------------------------
//  Function:   init_analog_constants()
//
//---------------------------------------------------------------------------

void init_analog_constants(void)
{

// need to read all values used from flash to ensure valid values

  float xdata factory_span;             
  float xdata factory_zero;            
  float xdata user_scale;
  float xdata user_offset;
  float xdata factory_cal;

  unsigned char xdata test;       // test for valid stored value

// determine default values if nothing is stored yet

  read_char_FLASH(FACT_ANA_CAL_ADDR + 4, 1, &test);
  if (test == DATA_FLAG)
    read_float_FLASH(FACT_ANA_CAL_ADDR, &factory_cal);
    else
      factory_cal = 1.0000;


  read_char_FLASH(FACT_SPAN_ADDR + 4, 1, &test);    // get the flag
  if (test == DATA_FLAG)                                 // if valid value
    read_float_FLASH(FACT_SPAN_ADDR, &factory_span);    // use it
    else                                            // otherwise
      factory_span = 3850;                          // use the default

  read_char_FLASH(FACT_ZERO_ADDR + 4, 1, &test);    
  if (test == DATA_FLAG)                                
    read_float_FLASH(FACT_ZERO_ADDR, &factory_zero);    
    else                                            
      factory_zero = 100;                           

  read_char_FLASH(USER_SCALE_ADDR + 4, 1, &test);   
  if (test == DATA_FLAG)                                 
    read_float_FLASH(USER_SCALE_ADDR, &user_scale);      
    else                                           
      user_scale = 100;                             

  read_char_FLASH(USER_OFFSET_ADDR + 4, 1, &test);  
  if (test == DATA_FLAG)                                
    read_float_FLASH(USER_OFFSET_ADDR, &user_offset);    
    else                                            
      user_offset = 0;                              

//calculate
    ana_m = (factory_span / user_scale)*factory_cal;

    ana_b = factory_zero - (user_offset / user_scale) * factory_span;

// default value for error condition

  read_char_FLASH(FORCED_OUT_DEFAULT_ADDR + 4, 1, &test);    
  if (test == DATA_FLAG)                                            
    read_float_FLASH(FORCED_OUT_DEFAULT_ADDR, &analog_default);  
   else                                                        
     analog_default = 0.0;   

  } // end init_analog_constants()

//---------------------------------------------------------------------------
//  Function:   init_vmon_coeff()
//
//---------------------------------------------------------------------------
void init_vmon_coeff(void)
{
  unsigned char xdata test;       // test for valid stored value

  read_char_FLASH(V_MON_SCALE_ADDR + 4, 1, &test);  
  if (test == DATA_FLAG)                                
    read_float_FLASH(V_MON_SCALE_ADDR, &vmon_scale);    
    else                                            
      vmon_scale = 0.00146; //  determine correct default value                              

  read_char_FLASH(V_MON_OFFSET_ADDR + 4, 1, &test);  
  if (test == DATA_FLAG)                                
    read_float_FLASH(V_MON_OFFSET_ADDR, &vmon_offset);    
    else                                            
      vmon_offset = 0; //  determine correct default value                              



  } // end init_vmon_coeff()

//---------------------------------------------------------------------------
//  Function:   analog_out()
//
//---------------------------------------------------------------------------

void analog_out(void)
{
float tmpfloat;

unsigned int tmpint;
unsigned char highbyte;

// presscal maybe negative when near zero, test for this

//    TIMINGBIT = 1;

//    tmpfloat = ana_m * presscal + ana_b;

    if (chksumfail == 1) tmpfloat = ana_m*analog_default + ana_b;
      else tmpfloat = ana_m * presscal + ana_b;

    if(tmpfloat <= 0.5) tmpfloat = 0;      // !! need to define limits !!
    if(tmpfloat >= 4095) tmpfloat = 4095;   // !! need to define defaults !!
 
    tmpint = (unsigned int)(tmpfloat);
    highbyte = (tmpint & 0xFF00) / 256;
    DAC1L = (tmpint - (highbyte*256) & 0x00FF);
    DAC1H = highbyte;

//    TIMINGBIT = 0;

  } // end analog_out()

///////////////////////////////////////////////////////////
//
//  forced_output()
//
///////////////////////////////////////////////////////////
void forced_output(void)
{
float tmpfloat;

unsigned int tmpint;
unsigned char highbyte;

    tmpfloat = ana_m * forced_out_value + ana_b;

    if(tmpfloat <= 0.5) tmpfloat = 0;      // ??
    if(tmpfloat >= 4095) tmpfloat = 4095;   // ??
 
    tmpint = (unsigned int)(tmpfloat);
    highbyte = (tmpint & 0xFF00) / 256;
    DAC1L = (tmpint - (highbyte*256) & 0x00FF);
    DAC1H = highbyte;

  } // end forced_output()

////////////////////////////////////////////////////////
//
//  init_forced_out()
//
///////////////////////////////////////////////////////
void init_forced_out(void)
{
  unsigned char test;
//  float tmpfloat;

  read_char_FLASH(FORCED_OUT_FLAG_ADDR, 2, command_buffer);
  if (command_buffer[1] == DATA_FLAG)              	// if valid flag
    {
      if (command_buffer[0] == 1)     //forced output enabled
        {
          forced_out_flag = 1;
          read_char_FLASH(FORCED_OUT_DEFAULT_ADDR + 4, 1, &test);    
          if (test == DATA_FLAG)                                            
           read_float_FLASH(FORCED_OUT_DEFAULT_ADDR, &forced_out_value);  
           else                                                        
            forced_out_value = 0.0;   
            }
        else  
          {
           forced_out_flag = 0;
           forced_out_value = 0.0;
           }
    }

  } // end init_forced_out()



/////////////////////////////////////////////////////////////////////////////
//
//                              main()
//
/////////////////////////////////////////////////////////////////////////////


#include <intrins.h>  // For __nop()

void delay_ms(unsigned int ms)
{
    unsigned int i, j;
    for (i = 0; i < ms; i++) {
        for (j = 0; j < 112; j++) {
            __nop(); __nop(); __nop(); __nop();
            __nop(); __nop(); __nop(); __nop();
        }
    }
}


void main (void)
{
// varibles to test averaging
static unsigned int integrate_factor;   
static unsigned long integrate_acc;
unsigned char j;    // test loop counter

    sysclk_init();

    xbar_init();

    CKCON = 0x00;       // default value, timers 0, 1, 2 use clk/12, std 8051
                        // value ignored for timer2 when used as baudrate gen.

                        // init serial port using timer2
                        // default 9600 baud, 1 stop bit, no parity
#ifdef _22MHZ_
    RCAP2H = 0xFF;      // set up for 22.1184 MHz
    RCAP2L = 0xB8;
    TH2    = 0xFF;
    TL2    = 0xB8;
#else
    RCAP2H = 0xFF;      // set up for 11.0592 MHz
    RCAP2L = 0xDC;          
    TH2    = 0xFF;
    TL2    = 0xDC;
#endif

    SCON   = 0x52;
    T2CON  = 0x34;          // 00110100b:-,-,RCLK,TCLK,-,TR2,-,-; 
                            // timer2 used for both xmit and recv
					        // TR2 turns timer2 ON

  	TH0 = RELOAD_HIGH;              // setup timer0 for system tick
    TL0 = RELOAD_LOW;
 //	TMOD |= 0x01;                   // 16 bit timer controlled by TR0
    TMOD |= 0x11;           // timer1 as 16 bit also

    ser_current_state = STATE_SYNC; // initialize the serial state machine
    recv_timer = 0;

//    IE = 0x12;          // enable serial and timer0 interrupts 
    IE = 0x1A;          // enables serial, timer0 and timer1 interrupts

    IP = 0x10;          // serial interrupt to high priority

// transmit delay stuff
    TR1 = 0;            // turn off timer

    //use values for 9600
    timer1reload_low = 0x13;
    timer1reload_high = 0xF8;
    TL1 = 0x13;
    TH1 = 0xF8;

    DelayDone = 0;

    timer3_init();
    ADC_init();
    ADC_enable();       // this enables ADC interrupt in EIE2 

    status = 0x30;

    if ( !verify_checksum_FLASH() )  chksumfail = 1;

    init_addr();
    init_baudrate();
    init_coeff();
    init_digital_constants();
    init_analog_constants();
    init_temp_bounds();
    init_average();
    init_DAC0();
    DAC0CN = 0x80;      // enable DAC0 for right justified data
    DAC1CN = 0x80;      // enable DAC1 for right justified data

    init_temp_coeff();
    init_vmon_coeff();

    forced_out_flag = 0;
    forced_out_value = 0.0;
    init_forced_out();


    CommandWaiting = 0;
    FactoryEnable = 0;

    take_temp_timer = 1;    // allow temp reading right away
                            // using 0 gives over range error at startup
    check_range_timer = 250;
    previous_range = MID;

/////////////////test///////////
    PRT2CF = 0x01;        // configure P2.0 as push-pull output for test bit
    TIMINGBIT = 0;
///////////////to here//////////


    newdata = 0;    // initialize new data ready indicator

    integrate_acc = 0;
    integrate_factor = user_defined_rate;

                        // configure 485driver and initialize
    PRT1CF = 0x02;
    driver485 = 0;

//for background checksum test
    ptr_FLASH = DATA_FLASH_ADDR;
    sum = 0;
    recentupdate = 0;


    TR0 = 1;            // turn on timer0

  //  EA = 1;           // enable interrupts

// Are these necessary??
    inter_p = 4000;
    result_p = 4000;
    // without the following, low temp error recorded at startup

  //  inter_t = 1200;         // differential now, signed, only 11 bits
 //   result_t = 1200;

 //   inter_t = 1900;         // test
 //   result_t = 1900;

    inter_t = 2048;     //differential temp signal with 2048 offset
    result_t = 2048;    // room = 0(from signal) + 2048

    EA = 1;         // enable interrupts

//////////////////
// adjust this loop for reference stablization time
// with eval. board. bypass caps on VREF caused 2 ms before stable reference
// symptom was unusually high readings at start up
//      over range flags were set

    for (j=0; j<= 250; j++) Wait5();


    newdata = 0;    // initialize new data ready indicator


    WDTCN = 0x07;   // set max. interval time
    WDTCN = 0xFF;   // lock out disable feature
    WDTCN = 0xA5;   // enable and reload


//////////////the forever loop//////////////////////////

    while(1)
    {
 //   TIMINGBIT = ~TIMINGBIT;
//  TIMINGBIT = 1;

                // test routine for averaging
    if (_testbit_(newdata))
      {
//    TIMINGBIT = ~TIMINGBIT;

        EA = 0;                 // get results for processing
        inter_p = result_p;
        inter_t = result_t;
        EA = 1;

        if (inter_p > max_press)
         { highpress = 1;
           overrange = 1;}
        else overrange = 0;
            
        if (inter_p < min_press)
         { lowpress = 1;
           underrange = 1;}
        else underrange = 0;

        if (inter_t > max_temp) hightemp = 1;
        if (inter_t < min_temp) lowtemp = 1;

 //  TIMINGBIT = ~TIMINGBIT;
       
        if (user_defined_rate != 1)     //if = 1, go compensate
          {
            integrate_acc += inter_p;
            integrate_factor--;
        
            if (integrate_factor == 0)
             { 
                integrate_factor = user_defined_rate; 
                pressure = (float)(integrate_acc / integrate_factor);
                temperature = (float)inter_t;
                integrate_acc = 0;
    
//   TIMINGBIT = ~TIMINGBIT;      // use to time freq. response while averaging

                compensate();
               }
            }
         else 
         {
//   TIMINGBIT = ~TIMINGBIT;      // use to time freq. response when not averaging

            pressure = (float)inter_p;
            temperature = (float)inter_t;
            compensate();
           }
        }   //  end if (_testbit_(newdata))

//    TIMINGBIT = 0;
// then update DAC(s) for analog output

//    analog_out();

    if (forced_out_flag == 1) forced_output();
    else analog_out();


    if (check_range_timer == 0)
      { 
                         //  TIMINGBIT = 1;
         check_range_timer = 250;
         check_range();
                         //  TIMINGBIT = 0;
        }

    if(CommandWaiting == 1)
      {
        if (DelayDone == 1)
        {
          CommandWaiting = 0;
          DelayDone = 0;

          driver485 = 1;
          Wait5();

          WDTCN = 0xA5;   // reload watchdog timer

          ExecuteCommand();
          }
       }


    WDTCN = 0xA5;   // reload watchdog timer


////////////test ongoing checksum test
if (!recentupdate)
  {
    sum += *ptr_FLASH;
    ptr_FLASH++;

    if (ptr_FLASH == (DATA_FLASH_ADDR + DATA_LENGTH + 1))
      {
        if (sum ==0xFF) chksumfail = 0;
          else chksumfail = 1;
        sum = 0;
        ptr_FLASH = DATA_FLASH_ADDR;
        }
    }
  else
   {
     recentupdate = 0;
     sum = 0;
     ptr_FLASH = DATA_FLASH_ADDR;
     }





/*
    if(_testbit_(CommandWaiting))
       {

//   TIMINGBIT = 1;

            // enable RS485 driver - check timing to enable 485 chip
            // DriverEnable to valid output for MAX1357 is 3.5 us
					driver485 = 1;
                    Wait5();    // Wait5(); Wait5();
         ExecuteCommand();
         }
*/


// tune following loop for fastest response but with new data each loop
  //  for (j=0; j<= 5; j++) Wait5();
     //   j=0;
      } // end while(1)

 }   // end main()

//////////////////////////////////////EOF////////////////////////////////////