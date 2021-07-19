#include "AM2315.h"
#include "mbed.h"
 
 
AM2315::AM2315(PinName SDA, PinName SCL):i2c(SDA, SCL)
{
}
 
// I2C Temperature and Humidity Sensor AM2315
//
bool AM2315::read()
{
 
  char data_write[5];
  char data_read[10];
  int i = 0;
  for(i=0; i<10; i++)
    data_read[i]=0;
 
  // Wake up the sensor
  // write single byte twice to wake up
  // single write is not enough
  data_write[0] = 0x00;
  i2c.write(AM2315_ADDR,data_write,1,0); 
  i2c.write(AM2315_ADDR,data_write,1,0);
  
  wait_us(2000);
  // Read temperature and humidity register
  // send request to AM2315
  data_write[0] = AM2315_REG_READ;
  data_write[1] = 0x00;  // read from adr 0x00
  data_write[2] = 0x04;  // read 4 bytes
  i2c.write(AM2315_ADDR, data_write, 3, 0); // with stop
  
  // wait 2ms before we start to read reg
  wait_us(2000); 
  
  i2c.read(AM2315_ADDR, data_read, 8, 1);
 
  if (data_read[0] != AM2315_REG_READ) {
    // printf("no address\r\n");
    humidity=data_read[0];
    return false;
  }
  // check numbers of bytes read
  if (data_read[1] != 4) {
   printf("few bytes\r\n");
   humidity=-3;
    return false; 
  }
  humidity = data_read[2];
  humidity *= 256;
  humidity += data_read[3];
  humidity /= 10;
  
  celsius = data_read[4] & 0x7F;
  celsius *= 256;
  celsius += data_read[5];
  celsius /= 10;
 
  if (data_read[4] >> 7) 
    celsius = -(celsius);
 
  return true;
}
 
 
       