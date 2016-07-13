#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <ctype.h>
#include <unistd.h>
#include <fcntl.h>
#include "cpu.h"
#include <termios.h>

#define MAX_INSTR 0x100000
#define SERIAL_HEADER "\rSDMP"

void printBits(size_t const size, void const * const ptr)
{
    unsigned char *b = (unsigned char*) ptr;
    unsigned char bit;
    unsigned char lastbit;
    int i, j;

    for (i=size-1;i>=0;i--)
    {
        for (j=7;j>=0;j--)
        {
            bit = (b[i] >> j) & 1;

            if(bit != lastbit){
              if(bit==1){
                printf("\x1b[35m%u", ' ');
              }
              else
              {
                printf("\x1b[34m%u", bit);
              }
            }
            else
            {
                printf("%u", bit);
            }
            lastbit = bit;
        }
    }
    puts("\x1b[0m");
}

typedef struct
{
  unsigned short freq;
  unsigned short pulse;
  unsigned short adsr;
  unsigned char wave;
  int note;
} CHANNEL;

typedef struct
{
  unsigned short cutoff;
  unsigned char ctrl;
  unsigned char type;
} FILTER;

int main(int argc, char **argv);
unsigned char readbyte(FILE *f);
unsigned short readword(FILE *f);

CHANNEL chn[3];
CHANNEL prevchn[3];
CHANNEL prevchn2[3];
FILTER filt;
FILTER prevfilt;

extern unsigned short pc;

int main(int argc, char **argv)
{
  int subtune = 0;
  int seconds = 300;
  int fps = 50;
  int instr = 0;
  int frames = 0;
  int firstframe = 0;
  int usage = 0;
  unsigned loadend;
  unsigned loadpos;
  unsigned loadsize;
  unsigned loadaddress;
  unsigned initaddress;
  unsigned playaddress;
  unsigned dataoffset;
  unsigned delay = 1000000/50;

  FILE *in;
  char *sidname = 0;
  int c;


  // Scan arguments
  for (c = 1; c < argc; c++)
  {
    if (argv[c][0] == '-')
    {
      switch(toupper(argv[c][1]))
      {
        case '?':
        usage = 1;
        break;

        case 'S':
        sscanf(&argv[c][2], "%u", &fps);
        delay = 1000000 / fps;
        break;

        case 'A':
        sscanf(&argv[c][2], "%u", &subtune);
        break;

        case 'F':
        sscanf(&argv[c][2], "%u", &firstframe);
        break;

        case 'T':
        sscanf(&argv[c][2], "%u", &seconds);
        break;
      }
    }
    else 
    {
      if (!sidname) sidname = argv[c];
    }
  }

  // Usage display
  if ((argc < 2) || (usage))
  {
    printf("Usage: SIDDUMP <sidfile> [options]\n"
           "Warning: CPU emulation may be buggy/inaccurate, illegals support very limited\n\n"
           "Options:\n"
           "-f<value> First frame to play, default = 0\n"
           "-a<value> Subtune number, default = 0\n"
           "-s<value> Playback speed in updates-per-second, default = 50\n"
           "-t<value> Playback time in seconds, default 300\n");
    return 1;
  }

  // Open SID file
  if (!sidname)
  {
    printf("Error: no SID file specified.\n");
    return 1;
  }

  in = fopen(sidname, "rb");
  if (!in)
  {
    printf("Error: couldn't open SID file.\n");
    return 1;
  }

  // Read interesting parts of the SID header
  fseek(in, 6, SEEK_SET);
  dataoffset = readword(in);
  loadaddress = readword(in);
  initaddress = readword(in);
  playaddress = readword(in);
  fseek(in, dataoffset, SEEK_SET);
  if (loadaddress == 0)
    loadaddress = readbyte(in) | (readbyte(in) << 8);

  // Load the C64 data
  loadpos = ftell(in);
  fseek(in, 0, SEEK_END);
  loadend = ftell(in);
  fseek(in, loadpos, SEEK_SET);
  loadsize = loadend - loadpos;
  if (loadsize + loadaddress >= 0x10000)
  {
    printf("Error: SID data continues past end of C64 memory.\n");
    fclose(in);
    return 1;
  }
  fread(&mem[loadaddress], loadsize, 1, in);
  fclose(in);


  // Open the target serial port
  char *portname = "/dev/ttyAMA0";

  int serial = open(portname, O_RDWR | O_NOCTTY | O_NDELAY);

  if(serial < 0)
  {
    printf("Unable to open serial port!\n");
    return 1;
  }

  struct termios theTermios;

  memset(&theTermios, 0, sizeof(struct termios));
  cfmakeraw(&theTermios);
  cfsetspeed(&theTermios, 115200);

  tcsetattr(serial, TCSANOW, &theTermios);


  // Print info & run initroutine
  mem[0x01] = 0x37;
  initcpu(initaddress, subtune, 0, 0);
  instr = 0;
  while (runcpu())
  {
    instr++;
    if (instr > MAX_INSTR)
    {
      printf("Warning: CPU executed a high number of instructions in init, breaking\n");
      break;
    }
  }

  if (playaddress == 0)
  {
    printf("Warning: SID has play address 0, reading from interrupt vector instead\n");
    if ((mem[0x01] & 0x07) == 0x5)
      playaddress = mem[0xfffe] | (mem[0xffff] << 8);
    else
      playaddress = mem[0x314] | (mem[0x315] << 8);
    printf("New play address is $%04X\n", playaddress);
  }

  // Data collection & display loop
  while (frames < firstframe + seconds*50)
  {
    int c;

    // Run the playroutine
    instr = 0;
    initcpu(playaddress, 0, 0, 0);
    while (runcpu())
    {
      instr++;
      if (instr > MAX_INSTR)
      {
        printf("Error: CPU executed abnormally high amount of instructions in playroutine, exiting\n");
        return 1;
      }
      // Test for jump into Kernal interrupt handler exit
      if ((mem[0x01] & 0x07) != 0x5 && (pc == 0xea31 || pc == 0xea81))
        break;
    }

    // Frame display
    if (frames >= firstframe)
    {
      unsigned char buf[25];
      memcpy(buf, &mem[0xd400], 25);

      for(c = 0; c < 25; c++){
        printf("%02x ",buf[c]);
      }
      printf("\n");
      //printBits(25, buf);

      write(serial, SERIAL_HEADER, 5);
      write(serial, buf, 25);

      usleep(delay);
    }
    // Advance to next frame
    frames++;
  }
  
  close(serial);
  return 0;
}

unsigned char readbyte(FILE *f)
{
  unsigned char res;

  fread(&res, 1, 1, f);
  return res;
}

unsigned short readword(FILE *f)
{
  unsigned char res[2];

  fread(&res, 2, 1, f);
  return (res[0] << 8) | res[1];
}

