# SIDStream

Stream raw SID register data directly from SID files to a serial port.

Designed for use with SIDcog on the Parallax Propeller.

----

SIDStream is based upon SIDDump V1.05 by Cadaver (loorni@gmail.com)

### SIDDump Version history:

* V1.0    - Original
* V1.01   - Fixed BIT instruction
* V1.02   - Added incomplete illegal opcode support, enough for John Player
* V1.03   - Some CPU bugs fixed
* V1.04   - Improved command line handling, added illegal NOP instructions, fixed 
          illegal LAX to work again
* V1.05   - Partial support for multispeed tunes
