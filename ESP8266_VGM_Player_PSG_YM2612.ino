//YM2612 & SN76489
//PCM Buffering, playback, and seeking.
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

//YM DATA - Shift Register
const int ymLatch = D6;
const int ymClock = D7;
const int ymData = D8; 

//Timing Variables
float singleSampleWait = 0;
const int sampleRate = 44100; //44100
const float WAIT60TH = 1000 / (sampleRate/735);
const float WAIT50TH = 1000 / (sampleRate/882);
#define MAX_PCM_BUFFER_SIZE 30000 //In bytes
uint8_t pcmBuffer[MAX_PCM_BUFFER_SIZE];
uint32_t pcmBufferPosition = 0;

uint32_t loopOffset = 0;

//Read music data from flash memory
uint8 ICACHE_FLASH_ATTR read_rom_uint8(const uint8* addr)
{
    uint32 bytes;
    bytes = *(uint32*)((uint32)addr & ~3);
    return ((uint8*)&bytes)[(uint32)addr & 3];
}

void setup() 
{
  for ( int i = 0x1C; i < 0x1F; i++ )
  {
    loopOffset += uint32_t(read_rom_uint8(&music_data[i])) << ( 8 * i );
  }
  singleSampleWait = 1000/(sampleRate/1);
  Serial.begin(115200);
  //Serial.print("Offset: ");
  //Serial.println(loopOffset);
  //Setup SN DATA 595
  pinMode(psgLatch, OUTPUT);
  pinMode(psgClock, OUTPUT);
  pinMode(psgData, OUTPUT);

  //Setup CONTROL 595
  pinMode(controlLatch, OUTPUT);
  pinMode(controlClock, OUTPUT);
  pinMode(controlData, OUTPUT);

  //Setup YM DATA 595
  pinMode(ymLatch, OUTPUT);
  pinMode(ymClock, OUTPUT);
  pinMode(ymData, OUTPUT);

  ResetRegisters();

  SN_WE(HIGH);

  delay(500);
  SilenceAllChannels();
  YM_A0(LOW);
  YM_A1(LOW);
  YM_CS(HIGH);
  YM_WR(HIGH);
  YM_RD(HIGH);
  YM_IC(HIGH);
  delay(10);
  YM_IC(LOW);
  delay(10);
  YM_IC(HIGH);
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

void YM_IC(bool state)
{
  SendControlReg(1, state);
}

void YM_CS(bool state)
{
  SendControlReg(2, state);
}

void YM_WR(bool state)
{
  SendControlReg(3, state);
}

void YM_RD(bool state)
{
  SendControlReg(4, state);
}

void YM_A0(bool state)
{
  SendControlReg(5, state);
}

void YM_A1(bool state)
{
  SendControlReg(6, state);
}

void ResetRegisters()
{
   digitalWrite(controlLatch, LOW);
   shiftOut(controlData, controlClock, MSBFIRST, 0x00);
   digitalWrite(controlLatch, LOW); 
   
   digitalWrite(psgLatch, LOW);
   shiftOut(psgData, psgClock, MSBFIRST, 0x00);   
   digitalWrite(psgLatch, HIGH);

   digitalWrite(ymLatch, LOW);
   shiftOut(ymData, ymClock, MSBFIRST, 0x00);
   digitalWrite(ymLatch, HIGH);   
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
  delayMicroseconds(5);
  SN_WE(HIGH);
}

void SendYMByte(byte b)
{
  digitalWrite(ymLatch, LOW);
  shiftOut(ymData, ymClock, MSBFIRST, b);
  digitalWrite(ymLatch, HIGH);
}

void ShiftControlFast(byte b)
{
  digitalWrite(controlLatch, LOW);
  shiftOut(controlData, controlClock, MSBFIRST, b);
  digitalWrite(controlLatch, HIGH);
}



unsigned long parseLocation = 64; //Where we're currently looking in the music_data array. (64 = 0x40 = start of VGM music data)
uint32_t lastWaitData61 = 0;
uint32_t lastWaitData7n = 0;
uint32_t lastWaitData8n = 0;

float cachedWaitTime61 = 0;
float cachedWaitTime7n = 0;
float cachedWaitTime8n = 0;
void ICACHE_FLASH_ATTR loop(void) 
{
  switch(read_rom_uint8(&music_data[parseLocation])) //Use this switch statement to parse VGM commands
  {
    case 0x50:
    parseLocation++;
    SendSNByte(read_rom_uint8(&music_data[parseLocation]));
    delay(singleSampleWait);
    break;
    
    case 0x52:
    {
    parseLocation++;
    uint8 address = read_rom_uint8(&music_data[parseLocation]);
    parseLocation++;
    uint8 data = read_rom_uint8(&music_data[parseLocation]);
    YM_A1(LOW);
    YM_A0(LOW);
    YM_CS(LOW);
    //ShiftControlFast(B00011010);
    SendYMByte(address);
    YM_WR(LOW);
    //delayMicroseconds(1);
    YM_WR(HIGH);
    YM_CS(HIGH);
    //delayMicroseconds(1);
    YM_A0(HIGH);
    YM_CS(LOW);
    SendYMByte(data);
    YM_WR(LOW);
    //delayMicroseconds(1);
    YM_WR(HIGH);
    YM_CS(HIGH);
    }
    delay(singleSampleWait);
    break;
    
    case 0x53:
    {
    parseLocation++;
    uint8 address = read_rom_uint8(&music_data[parseLocation]);
    parseLocation++;
    uint8 data = read_rom_uint8(&music_data[parseLocation]);
    YM_A1(HIGH);
    YM_A0(LOW);
    YM_CS(LOW);
    SendYMByte(address);
    YM_WR(LOW);
    //delayMicroseconds(1);
    YM_WR(HIGH);
    YM_CS(HIGH);
    //delayMicroseconds(1);
    YM_A0(HIGH);
    YM_CS(LOW);
    SendYMByte(data);
    YM_WR(LOW);
    //delayMicroseconds(1);
    YM_WR(HIGH);
    YM_CS(HIGH);
    }
    delay(singleSampleWait);
    break;

    
    case 0x61: 
    {
      //Serial.print("0x61 WAIT: at location: ");
      //Serial.print(parseLocation);
      //Serial.print("  -- WAIT TIME: ");
    uint32_t wait = 0;
    for ( int i = 0; i < 2; i++ ) 
    {
      parseLocation++;
      wait += ( uint32_t( read_rom_uint8(&music_data[parseLocation]) ) << ( 8 * i ));
    }
    if(lastWaitData61 != wait) //Avoid doing lots of unnecessary division.
    {
      lastWaitData61 = wait;
      if(wait == 0)
        break;
      cachedWaitTime61 = 1000/(sampleRate/wait);
    }
    //Serial.println(cachedWaitTime61);
    delay(cachedWaitTime61);
    break;
    }
    case 0x62:
    delay(WAIT60TH); //Actual time is 16.67ms (1/60 of a second)
    break;
    case 0x63:
    delay(WAIT50TH); //Actual time is 20ms (1/50 of a second)
    break;

    case 0x67:
    {
      //Serial.print("DATA BLOCK 0x67.  PCM Data Size: ");
      parseLocation+=2; //Skip 0x66 and data type
      pcmBufferPosition = parseLocation;
      uint32_t PCMdataSize = 0;
      for ( int i = 0; i < 4; i++ ) 
      {
      parseLocation++;
      PCMdataSize += ( uint32_t( read_rom_uint8(&music_data[parseLocation]) ) << ( 8 * i ));
      }
      //Serial.println(PCMdataSize);
      //parseLocation++;
      
      for ( int i = 0; i < PCMdataSize; i++ ) 
      {
         parseLocation++;
         if(PCMdataSize <= MAX_PCM_BUFFER_SIZE)
            pcmBuffer[ i ] = (uint8_t)read_rom_uint8(&music_data[parseLocation]); 
      }
      //Serial.println("Finished buffering PCM");
      
      break;
      
    }
    
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
      //Serial.println("0x7n WAIT");
      //There seems to be an issue with this wait function
      uint32_t wait = read_rom_uint8(&music_data[parseLocation]) & 0x0F;
      //Serial.print("Wait value: ");
      //Serial.println(wait);
      //Serial.print("Wait Location: ");
      //Serial.println(read_rom_uint8(&music_data[parseLocation]), HEX);
    if(lastWaitData7n != wait) //Avoid doing lots of unnecessary division.
    {
      lastWaitData7n = wait;
      if(wait == 0)
        break;
      cachedWaitTime7n = 1000/(sampleRate/wait+1);
    }
      delay(cachedWaitTime7n);
    break;
    }
    case 0x80:
    case 0x81:
    case 0x82:
    case 0x83:
    case 0x84:
    case 0x85:
    case 0x86:
    case 0x87:
    case 0x88:
    case 0x89:
    case 0x8A:
    case 0x8B:
    case 0x8C:
    case 0x8D:
    case 0x8E:
    case 0x8F:
      {
      //Serial.print("PLAY DATA BLOCK: ");
      //Serial.print(read_rom_uint8(&music_data[parseLocation]), HEX);
      //Serial.print("LOCATION: ");
      //Serial.println(parseLocation, HEX);

      uint32_t wait = read_rom_uint8(&music_data[parseLocation]) & 0x0F;
      
      uint8 address = 0x2A;
      uint8 data = pcmBuffer[pcmBufferPosition++];
      //pcmBufferPosition++;
      YM_A1(LOW);
      YM_A0(LOW);
      YM_CS(LOW);
      //ShiftControlFast(B00011010);
      SendYMByte(address);
      YM_WR(LOW);
      //delayMicroseconds(1);
      YM_WR(HIGH);
      YM_CS(HIGH);
      //delayMicroseconds(1);
      YM_A0(HIGH);
      YM_CS(LOW);
      SendYMByte(data);
      YM_WR(LOW);
      //delayMicroseconds(1);
      YM_WR(HIGH);
      YM_CS(HIGH);

      if(wait == 0)
        break;
      if(lastWaitData8n != wait) //Avoid doing lots of unnecessary division.
      {
        lastWaitData8n = wait;
        if(wait == 0)
          break;
        cachedWaitTime8n = 1000/(sampleRate/wait);
      }
      delay(cachedWaitTime8n);
      //delayMicroseconds(23*wait); //This is a temporary solution for a bigger delay problem.
      }      
      break;
    case 0xE0:
      //Serial.print("LOCATION: ");
      //Serial.print(parseLocation, HEX);
      //Serial.print(" - PCM SEEK 0xE0. NEW POSITION: ");
      pcmBufferPosition = 0;
      //parseLocation++;
      for ( int i = 0; i < 4; i++ ) 
      {      
        parseLocation++;
        pcmBufferPosition += ( uint32_t( read_rom_uint8(&music_data[parseLocation]) ) << ( 8 * i ));
      }
      //Serial.println(pcmBufferPosition);    
    break;
    case 0x66:
    parseLocation = loopOffset;
    break;
  }
  parseLocation++;
  if (parseLocation == music_length)
  {
    parseLocation = loopOffset;
  }
}
