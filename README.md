# mood-lighting
Server for ESP32 that serves a webpage you can use to control RGB LEDs.  
I use this with LED strip lights, which require a lot more power than the ESP32 can provide, so I use PWM with power transistors that toggle a 12V DC power source.  

Written completely in C.  
Uses mongoose and the ESP IDF
