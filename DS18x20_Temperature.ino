#include <OneWire.h>

// OneWire DS18S20, DS18B20, DS1822, MAX31888, MAX30207 Temperature Example
//
// http://www.pjrc.com/teensy/td_libs_OneWire.html
//
// The DallasTemperature library can do all this work for you!
// https://github.com/milesburton/Arduino-Temperature-Control-Library

#define OWPin 15

OneWire  ds(OWPin);  // on OWPin (a 4.7K resistor is necessary)

void setup(void) {
  Serial.begin(115200);
}

void loop(void) {
  byte i;
  byte present = 0;
  byte type_s;
  byte data[100]; // 3B command, 32x2B FIFO, 2B CRC + margin
  byte addr[8];
  float celsius, fahrenheit;
  int16_t raw;
  
  if ( !ds.search(addr)) {
    Serial.println("No more addresses.");
    Serial.println();
    ds.reset_search();
    delay(250);
    return;
  }
  
  Serial.print("ROM =");
  for( i = 0; i < 8; i++) {
    Serial.write(' ');
    Serial.print(addr[i], HEX);
  }

  if (OneWire::crc8(addr, 7) != addr[7]) {
      Serial.println("CRC is not valid!");
      return;
  }
  Serial.println();
 
  // the first ROM byte indicates which chip
  switch (addr[0]) {
    case 0x10:
      Serial.println("  Chip = DS18S20");  // or old DS1820
      type_s = 1;
      break;
    case 0x28:
      Serial.println("  Chip = DS18B20");
      type_s = 0;
      break;
    case 0x22:
      Serial.println("  Chip = DS1822");
      type_s = 0;
      break;
    case 0x54:
      Serial.println("  Chip = MAX31888 or MAX30207");
      type_s = 2;
      break;
    default:
      Serial.println("Device is not a DS18x20 or MAX3xxxx temperature sensor.");
      return;
  } 
  ds.reset();
  ds.select(addr);
  ds.write(0x44, 1);        // start conversion, with parasite power on at the end
  switch(type_s)
  {
    case 0:
    case 1:
      delay(1000);     // maybe 750ms is enough, maybe not
      // we might do a ds.depower() here, but the reset will take care of it.

      present = ds.reset();
      ds.select(addr);    
      ds.write(0xBE);         // Read Scratchpad

      Serial.print("  Data = ");
      Serial.print(present, HEX);
      Serial.print(" ");
      for ( i = 0; i < 9; i++) {           // we need 9 bytes
        data[i] = ds.read();
        Serial.print(data[i], HEX);
        Serial.print(" ");
      }
      Serial.print(" CRC=");
      Serial.print(OneWire::crc8(data, 8), HEX);
      Serial.println();
      // Convert the data to actual temperature
      // because the result is a 16 bit signed integer, it should
      // be stored to an "int16_t" type, which is always 16 bits
      // even when compiled on a 32 bit processor.
      raw = (data[1] << 8) | data[0];
      if (type_s) {
        raw = raw << 3; // 9 bit resolution default
        if (data[7] == 0x10) {
          // "count remain" gives full 12 bit resolution
          raw = (raw & 0xFFF0) + 12 - data[6];
        }
      } else {
        byte cfg = (data[4] & 0x60);
        // at lower res, the low bits are undefined, so let's zero them
        if (cfg == 0x00) raw = raw & ~7;  // 9 bit resolution, 93.75 ms
        else if (cfg == 0x20) raw = raw & ~3; // 10 bit res, 187.5 ms
        else if (cfg == 0x40) raw = raw & ~1; // 11 bit res, 375 ms
        //// default is 12 bit resolution, 750 ms conversion time
      }
      celsius = (float)raw / 16.0;
      break;
    case 2:
      data[0]=0x44;
      ds.read_bytes(&data[1], 2); // 2 bytes of CRC 16
      pinMode(OWPin, OUTPUT);    // pin to output (was input after reading)
      digitalWrite(OWPin, HIGH); // sets strong pullup on DQ line
      delay(20);  // in case of parasite supply, 17.5ms is maximum conversion time for MAX31888
      digitalWrite(OWPin, LOW); // sets DQ low
      pinMode(OWPin, INPUT);    // and input direction (jus after reading)
      Serial.print("  Command = ");
      for ( i = 0; i < 3; i++) {           
        Serial.print(data[i], HEX);
        Serial.print(" ");
      }
      if(!ds.check_crc16(data,1,&data[1])) Serial.println("CRC Error");
      else {
        Serial.println("CRC OK");
        present = ds.reset();
        ds.select(addr);
        data[0]=0x33;         // Read Register
        data[1]=0x08;         // Starting Adddress -> FIFO Data Register   
        data[2]=0x01;         // Length (Bytes -1) -> 2 Bytes
        ds.write_bytes(data, 3);
        ds.read_bytes(&data[3], 4); // we need 2 bytes of data and 2 bytes of CRC 16
        Serial.print("  Data = ");
        for ( i = 0; i < 7; i++) { 
          Serial.print(data[i], HEX);
          Serial.print(" ");
        }
        if(!ds.check_crc16(data,5,&data[5])) Serial.println("CRC Error");
        else Serial.println("CRC OK");
        
      }
      raw = (data[3] << 8) | data[4];
      celsius = float(raw)*0.005;
      break;
    default:
      celsius = 0;   
  }        
  fahrenheit = celsius * 1.8 + 32.0;
  Serial.print("  Temperature = ");
  Serial.print(celsius);
  Serial.print(" Celsius, ");
  Serial.print(fahrenheit);
  Serial.println(" Fahrenheit");
}
