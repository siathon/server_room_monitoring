#ifndef MBED_AM2315_H
#define MBED_AM2315_H
 
#include "mbed.h"
 
#define AM2315_ADDR             0xB8
#define AM2315_REG_READ         0x03
 
 
class AM2315
{
    public:
        AM2315(PinName SDA , PinName SCL);
        bool read();
        
        double celsius;
        double humidity;   
    private:
        I2C i2c;
};
 
#endif