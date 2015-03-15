#Bongo-boy the Guitar Hero drum to MIDI converter.

Introduction

BongoBoy is a simple Arduino project that translates from the Wii Guitar Hero World Tour drum set to MIDI without using any external hardware or modifying the drum set in any way.

Details
Using just two cables and a resistor we can read out the data provided by the drum pads, including velocity data, convert it to MIDI and send it on to your keyboard or other MIDI device. This is a simple way to convert your Guitar Hero drums to something that aproximates a real drum set reasonably well.

In addition you can communicate with the wii drum kit over midi to adjust its sensitivity, and now I added support for adding additional inputs via the analog pins to support aditional pedals or drum pads.
