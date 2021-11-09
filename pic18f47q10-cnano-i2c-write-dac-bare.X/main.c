/*
    (c) 2020 Microchip Technology Inc. and its subsidiaries. 
    
    Subject to your compliance with these terms, you may use Microchip software and any 
    derivatives exclusively with Microchip products. It is your responsibility to comply with third party 
    license terms applicable to your use of third party software (including open source software) that 
    may accompany Microchip software.
    
    THIS SOFTWARE IS SUPPLIED BY MICROCHIP "AS IS". NO WARRANTIES, WHETHER 
    EXPRESS, IMPLIED OR STATUTORY, APPLY TO THIS SOFTWARE, INCLUDING ANY 
    IMPLIED WARRANTIES OF NON-INFRINGEMENT, MERCHANTABILITY, AND FITNESS 
    FOR A PARTICULAR PURPOSE.
    
    IN NO EVENT WILL MICROCHIP BE LIABLE FOR ANY INDIRECT, SPECIAL, PUNITIVE, 
    INCIDENTAL OR CONSEQUENTIAL LOSS, DAMAGE, COST OR EXPENSE OF ANY KIND 
    WHATSOEVER RELATED TO THE SOFTWARE, HOWEVER CAUSED, EVEN IF MICROCHIP 
    HAS BEEN ADVISED OF THE POSSIBILITY OR THE DAMAGES ARE FORESEEABLE. TO 
    THE FULLEST EXTENT ALLOWED BY LAW, MICROCHIP'S TOTAL LIABILITY ON ALL 
    CLAIMS IN ANY WAY RELATED TO THIS SOFTWARE WILL NOT EXCEED THE AMOUNT 
    OF FEES, IF ANY, THAT YOU HAVE PAID DIRECTLY TO MICROCHIP FOR THIS 
    SOFTWARE.
*/

#pragma config WDTE = OFF /* WDT operating mode->WDT Disabled */
#pragma config LVP = ON   /* Low-voltage programming enabled, RE3 pin is MCLR */

#define _XTAL_FREQ                      4000000UL

//#include <pic18.h>
#include <xc.h>
//#include <stdint.h>

#define I2C_CLIENT_ADDR     0x48
#define TC1321_REG_ADDR     0x00
#define I2C_RW_BIT          0x01
#define DATA_HIGH           0xFF
#define DATALENGTH          2


static void CLK_Initialize(void);
static void PPS_Initialize(void);
static void PORT_Initialize(void);
static void I2C1_Initialize(void);

static void I2C1_open(void);
static void I2C1_close(void);
static void I2C1_startCondition(void);
static void I2C1_stopCondition(void);
static void I2C1_sendData(uint8_t data);
static void I2C1_interruptFlagPolling(void);
static uint8_t I2C1_getAckstatBit(void);
static uint8_t I2C1_write1ByteRegister(uint8_t address, uint8_t reg, uint8_t data);
static uint8_t I2C1_writeNBytes(uint8_t address, uint8_t reg, uint8_t* data, size_t len);

static void CLK_Initialize(void)
{
    /* Set Oscillator Source: HFINTOSC and Set Clock Divider: 1 */
    OSCCON1bits.NOSC = 0x6;

    /* Set Nominal frequency: 4 MHz */
    OSCFRQbits.FRQ1 = 1;
}

static void PPS_Initialize(void)
{
    /* PPS setting for using RB1 as SCL */
    SSP1CLKPPS = 0x09;
    RB1PPS = 0x0F;

    /* PPS setting for using RB2 as SDA */
    SSP1DATPPS = 0x0A;
    RB2PPS = 0x10;
}

static void PORT_Initialize(void)
{
    /* Set pins RB1 and RB2 as Digital */
    ANSELBbits.ANSELB1 = 0;
    ANSELBbits.ANSELB2 = 0;
    
    /* Set pull-up resistors for RB1 and RB2 */
    WPUBbits.WPUB1 = 1;
    WPUBbits.WPUB2 = 1;
    
    /* Set open-drain mode for RB1 and RB2 */
    ODCONBbits.ODCB1 = 1;
    ODCONBbits.ODCB2 = 1;

}

static void I2C1_Initialize(void)
{
    /* I2C Host Mode: Clock = F_OSC / (4 * (SSP1ADD + 1)) */
    SSP1CON1bits.SSPM3 = 1;
    
    /* Set the baud rate divider to obtain the I2C clock at 100000 Hz*/
    SSP1ADD  = 0x9F;
}

static void I2C1_interruptFlagPolling(void)
{
    /* Polling Interrupt Flag */
    
    while (!PIR3bits.SSP1IF)
    {
        ;
    }

    /* Clear Interrupt Flag */
    PIR3bits.SSP1IF = 0;
}

static void I2C1_open(void)
{
    /* Clear IRQ */
    PIR3bits.SSP1IF = 0;

    /* I2C Host Open */
    SSP1CON1bits.SSPEN = 1;
}

static void I2C1_close(void)
{
    /* Disable I2C1 */
    SSP1CON1bits.SSPEN = 0;
}

static void I2C1_startCondition(void)
{
    /* START Condition*/
    SSP1CON2bits.SEN = 1;
    
    I2C1_interruptFlagPolling();
}

static void I2C1_stopCondition(void)
{
    /* STOP Condition */
    SSP1CON2bits.PEN = 1;
    
    I2C1_interruptFlagPolling();
}

static void I2C1_sendData(uint8_t byte)
{
    SSP1BUF  = byte;
    I2C1_interruptFlagPolling();
}


static uint8_t I2C1_getAckstatBit(void)
{
    /* Return ACKSTAT bit:
     * returns 1 if a NACK was received */
    return SSP1CON2bits.ACKSTAT;
}

/* Returns a dataSentSuccessful variable, which is 1 if no errors occurred */
static uint8_t I2C1_write1ByteRegister(uint8_t address, uint8_t reg, uint8_t data)
{
    /* Shift the 7 bit address and add a 0 bit to indicate write operation */
    uint8_t writeAddress = (address << 1) & ~I2C_RW_BIT;
    uint8_t dataSentSuccessful = 1;
    
    I2C1_open();
    I2C1_startCondition();
    
    I2C1_sendData(writeAddress);
    if (I2C1_getAckstatBit())
    {
        /* Handle error */
        dataSentSuccessful = 0;
    }
    
    I2C1_sendData(reg);
    if (I2C1_getAckstatBit())
    {
        /* Handle error */
        dataSentSuccessful = 0;
    }
    
    I2C1_sendData(data);
    if (I2C1_getAckstatBit())
    {
        /* Handle error */
        dataSentSuccessful = 0;
    }
    
    I2C1_stopCondition();
    I2C1_close();
    return dataSentSuccessful;
}

static uint8_t I2C1_writeNBytes(uint8_t address, uint8_t reg, uint8_t* data, size_t len)
{
    /* Shift the 7 bit address and add a 0 bit to indicate write operation */
    uint8_t writeAddress = ((address << 1) & (~I2C_RW_BIT));
    uint8_t dataSentSuccessful = 1;
    
    I2C1_open();
    I2C1_startCondition();
    
    I2C1_sendData(writeAddress);
    if (I2C1_getAckstatBit())
    {
        /* Handle error */
        dataSentSuccessful = 0;
    }
    
    I2C1_sendData(reg);
    if (I2C1_getAckstatBit())
    {
        /* Handle error */
        dataSentSuccessful = 0;
    }

    for(uint8_t i = 0; i < len; i++)
    {
        I2C1_sendData(*data++);
        if (I2C1_getAckstatBit())
        {
            /* Handle error */
            dataSentSuccessful = 0;
        } 
    }
    
    I2C1_stopCondition();
    I2C1_close();
    return dataSentSuccessful;
}

int main(void)
{   
    /* Initialize the device */
    CLK_Initialize();
    PPS_Initialize();
    PORT_Initialize();
    I2C1_Initialize();

    uint8_t data[DATALENGTH];
    data[0] = DATA_HIGH;
    data[1] = DATA_HIGH;
    while(1)
    {   
        
        /* Write to DATA REGISTER in TC1321 */
        if (I2C1_writeNBytes(I2C_CLIENT_ADDR, TC1321_REG_ADDR, data, DATALENGTH))
        {
            /* Overwrite data with its inverse*/
            data[0] = ~data[0];
            data[1] = ~data[1];
            /*Delay 1 second*/
            __delay_ms(1000);
        }
        else
        {    
            /* Handle error */
        }
    }
}
