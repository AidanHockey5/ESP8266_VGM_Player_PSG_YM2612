//YM2612 & SN76489
#include "music.h"

//Control bits (on control shift register)
// SN_WE = QA (bit 0, pin 15)
// YM_IC = QB (bit 1, pin 1)
// YM_CS = QC (bit 2, pin 2)
// YM_WR = QD (bit 3, pin 3)
// YM_RD = QE (bit 4, pin 4)
// YM_A0 = QF (bit 5, pin 5)
// YM_A1 = QG (bit 6, pin 6)
// NO CONNECT = QH (bit 7, pin 7)

//PSG DATA - Shift Register 
const int psgLatch = D0;
const int psgClock = D1;
const int psgData = D2;

//Control Shift Register
const int controlLatch = D3; 
const int controlClock = D4; 
const int controlData = D5;


//Timing Variables
float singleSampleWait = 0;
const float WAIT60TH = 1000 / (44100/735);
const float WAIT50TH = 1000 / (44100/882);

void setup() 
{
  Serial.begin(115200);
  //Setup SN DATA 595
  pinMode(psgLatch, OUTPUT);
  pinMode(psgClock, OUTPUT);
  pinMode(psgData, OUTPUT);

  //Setup CONTROL 595
  pinMode(controlLatch, OUTPUT);
  pinMode(controlClock, OUTPUT);
  pinMode(controlData, OUTPUT);

  ResetRegisters();


  SN_WE(HIGH);

  delay(500);
  SilenceAllChannels();
  delay(500);
}

void SilenceAllChannels()
{
  SendSNByte(0x9f);
  SendSNByte(0xbf);
  SendSNByte(0xdf);
  SendSNByte(0xff);
}

void SN_WE(bool state)
{
  SendControlReg(0, state);
}

void ResetRegisters()
{
   digitalWrite(controlLatch, LOW);
   shiftOut(controlData, controlClock, MSBFIRST, 0x00);
   digitalWrite(controlLatch, LOW); 
   digitalWrite(psgLatch, LOW);
   shiftOut(psgData, psgClock, MSBFIRST, 0x00);   
   digitalWrite(psgLatch, HIGH);
}

uint8_t controlRegister = 0x00;
void SendControlReg(byte bitLocation, bool state)
{
  state == HIGH ? controlRegister |= 1 << bitLocation : controlRegister &= ~(1 << bitLocation);
  //controlRegister = ~(controlRegister & b);
  digitalWrite(controlLatch, LOW);
  shiftOut(controlData, controlClock, MSBFIRST, controlRegister);
  digitalWrite(controlLatch, HIGH);
}

void SendSNByte(byte b) //Send 1-byte of data to PSG
{
  SN_WE(HIGH);
  digitalWrite(psgLatch, LOW);
  shiftOut(psgData, psgClock, MSBFIRST, b);   
  digitalWrite(psgLatch, HIGH);
  SN_WE(LOW);
  delay(1);
  SN_WE(HIGH);
}

//Read music data from flash memory
uint8 ICACHE_FLASH_ATTR read_rom_uint8(const uint8* addr)
{
    uint32 bytes;
    bytes = *(uint32*)((uint32)addr & ~3);
    return ((uint8*)&bytes)[(uint32)addr & 3];
}

unsigned long parseLocation = 64; //Where we're currently looking in the music_data array. (64 = 0x40 = start of VGM music data)
uint32_t lastWaitData = 0;
float cachedWaitTime = 0;
void ICACHE_FLASH_ATTR loop(void) 
{
  switch(read_rom_uint8(&music_data[parseLocation])) //Use this switch statement to parse VGM commands
  {
    case 0x50:
    parseLocation++;
    SendSNByte(read_rom_uint8(&music_data[parseLocation]));
    break;
    case 0x61: 
    {
    uint32_t wait = 0;
    parseLocation++;
    for ( int i = 0; i < 2; i++ ) 
    {
      wait += ( uint32_t( read_rom_uint8(&music_data[parseLocation]) ) << ( 8 * i ));
      parseLocation++;
    }
    if(lastWaitData != wait) //Avoid doing lots of unnecessary division.
    {
      lastWaitData = wait;
      cachedWaitTime = 1000/(44100/wait);
    }
    delay(cachedWaitTime);
    break;
    }
    case 0x62:
    delay(WAIT60TH); //Actual time is 16.67ms (1/60 of a second)
    break;
    case 0x63:
    delay(WAIT50TH); //Actual time is 20ms (1/50 of a second)
    break;
    case 0x70:
    case 0x71:
    case 0x72:
    case 0x73:
    case 0x74:
    case 0x75:
    case 0x76:
    case 0x77:
    case 0x78:
    case 0x79:
    case 0x7A:
    case 0x7B:
    case 0x7C:
    case 0x7D:
    case 0x7E:
    case 0x7F: 
    {
      //There seems to be an issue with this wait function
      uint32_t wait = read_rom_uint8(&music_data[parseLocation]) & 0x0F;
      delay(1000/(44100/wait));
    break;
    }
    case 0x66:
    parseLocation = 64;
    break;
  }
  parseLocation++;
  if (parseLocation == music_length)
  {
    parseLocation = 64;
  }
}
