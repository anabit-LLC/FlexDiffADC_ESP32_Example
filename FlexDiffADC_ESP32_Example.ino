/********************************************************************************************************
This example Arduino sketch is meant to work with Anabit's Flex Differential ADC open source reference design

Product link: https://anabit.co/products/flex-differential-adc

This version is meant to work with an ESP32 based Arduino because it uses fast pin setting functions (faster than digitalwrite) to help get the max sample rate 
from the ADC. See the other Flex ADC example code sketch that is configured to work with any arduino board

The Flex Differential ADC design uses a pseudo differential ADC. Why is it pseudo differential? It is pseudo differential because it does not support negative 
voltages, refernced to the ADCs ground, and its common mode voltage is always vref/2. It can only support negative voltage below the common mode voltage. 
This ADC does provide 14 bits of resolution for both the +in range (0 to vref) and -in range (0 to vref). The ADC returns the difference between +in input and 
the -in input in the form of a 15 bit value. If +in is larger in voltage than -in, the 15th bit will be zero and the range is 0V --> 0 code to 2.048V --> 16383 code. 
If -in is larger in voltage than +in the 15th bit (MSB) will be one and the range is 0V --> 16383 code to -2.048 --> 0 code (with 15th bit set to 0). The counting 
of the code is reverse when -in is larger. That means if you are measuring a value where +in and -in are almost equal the codes can jumpr from slightly higher 
than 0 (+in is slightly larger) and slightly lower than 16383 (-in is larger). You can use this ADC as a single ended input by connecting one of the inputs to 
ground and using the other to measure the input voltage. It is more intutive to use +in as the input since there will be no 15th bit and the 14 bit code will 
count up with the voltage.

This sketch deomonstrats how to use the Flex ADC to make a single measurement or to make a group or burst of measurements as fast as possible. The single versus
burst mode is set by the "#define" MODE_SINGLE_MEASUREMENT or MODE_BURST_CAPTURE, comment out the mode you don't want to use

Please report any issue with the sketch to the Anagit forum: https://anabit.co/community/forum/analog-to-digital-converters-adcs
Example code developed by Your Anabit LLC © 2025
Licensed under the Apache License, Version 2.0.
*/

#include <SPI.h>
#include "soc/gpio_struct.h"   // Gives access to GPIO.out_w1ts
#include "soc/io_mux_reg.h"

// === CONFIGURATION === uncomment one of the modes
#define MODE_SINGLE_MEASUREMENT
//#define MODE_BURST_CAPTURE

//sets some constants including chip select
#define CS_PIN         5
#define VREF_VOLTAGE   4.096f //2.5f  //3.3f ADC reference voltage in volts
#define ADC_BITS       16383.0f //14 bit ADC value

//SPI Configuration: max SPI clock rate for this ADC is 40MHz, 
//you can set the clock faster (over clock ADC) but at a reduced ADC amplitude ouput 
SPISettings adsSettings(40000000, MSBFIRST, SPI_MODE0);  

//settings specific to burst mode, first variable sets the number of points measured in a burst
#if defined(MODE_BURST_CAPTURE)
const int NUM_SAMPLES = 128;
int16_t adcRaw[NUM_SAMPLES];
float adcVoltage[NUM_SAMPLES];
#endif

void setup() {
  Serial.begin(115200);
  delay(2500); //delay to give time to get serial monitor or plotter setup

  // Setup chip select pin and start SPI communication
  pinMode(CS_PIN, OUTPUT);
  digitalWrite(CS_PIN, HIGH);
  SPI.begin();

//code for single measurement mode, loops and prints out measured value 
//along with raw ADC code
#if defined(MODE_SINGLE_MEASUREMENT)
  Serial.println("Single Measurement Mode (CH0 via GPIO)");
  readADS7945(0xC000);  // Dummy read clear buffer
  delayMicroseconds(5);
  while(1) {
    uint16_t usRaw = readADS7945(0xC000);
    Serial.print("ADC Value unsigned hex: "); Serial.println(usRaw,HEX);
    Serial.print("ADC Value unsigned decimal: "); Serial.println(usRaw);
    float voltage = convertToVoltage(usRaw);
    Serial.print("Voltage: "); Serial.print(voltage, 4); Serial.println(" V");
    Serial.println();
    delay(2500);
  }

//code for burst mode. Grabs a burst of measurements and prints them to serial plotter
//loops and repeats after delay of a couple seconds
#elif defined(MODE_BURST_CAPTURE)
  Serial.println("Burst Capture Mode (CH0 via GPIO)");
  while(1) {
    captureBurstFast();
    for (int i = 0; i < NUM_SAMPLES; i++) {
      Serial.println(adcVoltage[i], 4);  // Print Voltage For Serial Plotter
    }
    delay(2000);
  }
#else
  #error "Please define one of the modes."
#endif
}

void loop() {
  // Empty
}

// Function to read data from ADC
// Input arument is command, for ADC reading can set command to all zeros
// Returns result from ADC
uint16_t readADS7945(uint16_t cmd) {
  uint16_t value;
  SPI.beginTransaction(adsSettings);
  GPIO.out_w1tc = (1 << CS_PIN);  // Set pin LOW
  //value = SPI.transfer16(cmd);
  uint8_t hi = SPI.transfer(0x00);         // send cmd if needed instead of 0x00
  uint8_t lo = SPI.transfer(0x00);
  GPIO.out_w1ts = (1 << CS_PIN);  // Set pin HIGH
  SPI.endTransaction();
  uint16_t w = (uint16_t(hi) << 8) | lo;   // 16-bit frame from ADC
  value = (w >> 2) & 0x3FFF;     // left-justified -> right-justify to 14 bits
  return value;
}


//Takes code form the ADC and converts it to a voltage value
//this function uses various constants defined earlier in the sketch
//input argument is the ADC code and returns voltage as float
float convertToVoltage(uint16_t raw_code) {
   if(raw_code > 0x3FFF) { //if true negative input was larger
    raw_code = raw_code & 0x3FFF; //get rid of 15th bit telling us its negative
    raw_code = 0x3FFF - raw_code;  //reverse the counting direction of the bits
    return (((float)raw_code /ADC_BITS) * VREF_VOLTAGE) * -1.0f;
  }
  else {
    return ((float)raw_code /ADC_BITS) * VREF_VOLTAGE;  //return positive voltage value
  }
}

//function for running burst mode
//optimized for speed to run the ADC as fast as possible
//This function uses ESP32 specific GPIO commands for faster 
//spedds compared to traditional digitalwrite() functions
#if defined(MODE_BURST_CAPTURE)
void captureBurstFast() {
  // Dummy read to start pipeline
  readADS7945(0xC000);
  delayMicroseconds(5);
  noInterrupts();  // Max speed, no preemption

  SPI.beginTransaction(adsSettings);
  uint32_t startTime = micros();  // Start timer, timer tells you how long it took to capture samples
  for (int i = 0; i < NUM_SAMPLES; i++) {
    GPIO.out_w1tc = (1 << CS_PIN);  // Set pin LOW
    adcRaw[i] = SPI.transfer16(0xC000); //14 bit so shift result in 16 bit variable
    GPIO.out_w1ts = (1 << CS_PIN);  // Set pin HIGH
  }

  uint32_t endTime = micros();  // End timer
  uint32_t duration = endTime - startTime; //get time it took to run burst mode
 // Serial.print("Operation took ");
 // Serial.print(duration);
 // Serial.println(" microseconds.");

  SPI.endTransaction();

  interrupts();  // Restore interrupts

  //convert ADC codes to voltage values and save in global array
  for (int i = 0; i < NUM_SAMPLES; i++) {
    adcVoltage[i] = convertToVoltage(adcRaw[i]); //convert raw ADC readings to voltages but first shift 1
  }
}
#endif

