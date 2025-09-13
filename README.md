# FlexDiffADC_ESP32_Example
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
