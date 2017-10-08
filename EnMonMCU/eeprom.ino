/*
  *  Use the I2C bus with EEPROM 24LC64
  *  Sketch:    eeprom.ino
  *
  *  Author: hkhijhe
  *  Date: 01/10/2010
  *
  *
  */

struct eeprom{
  const int deviceaddress = 0x50;//eeprom i2c address
  unsigned int headPtr = 0; //pointer to the current memory location in eeprom for writing
  unsigned int tailPtr = 0; //pointer to the current memory location in eeprom for reading
  const int item_size = 16; //should be equal to 16
} ee;

void i2c_eeprom_write_byte(byte data ) {
    int rdata = data;
    Wire.beginTransmission(ee.deviceaddress);
    Wire.write((int)(ee.headPtr >> 8)); // MSB
    Wire.write((int)(ee.headPtr & 0xFF)); // LSB
    Wire.write(rdata);
    Wire.endTransmission();
    ee.headPtr++;
}

// WARNING: address is a page address, 6-bit end will wrap around
// also, data can be maximum of about 30 bytes, because the Wire library has a buffer of 32 bytes
void i2c_eeprom_write(byte* data, byte length ) {
  int l = min(length, ee.item_size);
  
    Wire.beginTransmission(ee.deviceaddress);
    Wire.write((int)(ee.headPtr >> 8)); // MSB
    Wire.write((int)(ee.headPtr & 0xFF)); // LSB
    byte c;
    for ( c = 0; c < l; c++)
        Wire.write(data[c]);
    Wire.endTransmission();
    ee.headPtr+=ee.item_size;
}

byte i2c_eeprom_read_byte( ) {
    byte rdata = 0xFF;
    Wire.beginTransmission(ee.deviceaddress);
    Wire.write((int)(ee.tailPtr >> 8)); // MSB
    Wire.write((int)(ee.tailPtr & 0xFF)); // LSB
    Wire.endTransmission();
    ee.tailPtr++;
    Wire.requestFrom(ee.deviceaddress,1);
    if (Wire.available()) rdata = Wire.read();
    return rdata;
}

// maybe let's not read more than 30 or 32 bytes at a time!
void i2c_eeprom_read( byte *buffer, int length ) {
   int l = min(length, ee.item_size);
   
    Wire.beginTransmission(ee.deviceaddress);
    Wire.write((int)(ee.tailPtr >> 8)); // MSB
    Wire.write((int)(ee.tailPtr & 0xFF)); // LSB
    Wire.endTransmission();
    ee.tailPtr+=ee.item_size;
    Wire.requestFrom(ee.deviceaddress,l);
    int c = 0;
    for ( c = 0; c < length; c++ )
        if (Wire.available()) buffer[c] = Wire.read();
}

void i2c_eeprom_read_start(){
  ee.tailPtr = ee.headPtr - 32*ee.item_size;
}

bool i2c_eeprom_read_end(){

  if ((0x7fff & ee.tailPtr) == (0x7fff & ee.headPtr))
    ee.tailPtr = ee.headPtr - ee.item_size;
    
  bool atEnd = ((ee.tailPtr + ee.item_size) & 0x7fff) == (ee.headPtr & 0x7fff);
  return atEnd;
}

