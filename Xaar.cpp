/*
 * This code controls a Xaar 128 printhead via SPI interface.
 * 
 * Target: Arduino Uno or better (Uno register used to generate 1 MHz)
 * Status: Very first steps
 * 
 * Hardware: Xaar 128 printhead
 * 
 * Power Control:
 * VPPH and VPPL are switched via BROADCOM LIMITED   ASSR-1611-301E  MOSFET-Relais, 60V, 2.5A, 0R1, SPST-NO
 * use resistor to drive LED. L is off, H is on. Use resistor as pulldown during Arduino reset
 * Pin 1 = Data Out, 2 = GND, 3 = nc, 4 = 6 = +35V, 5 = GND to 35V
 * Power -> Split here -> Diode -> Cap -> Relay -> Head
 */

typedef unsigned char ubyte;

// Pins on the Arduino and on the breakout board
// 9 outputs, 3 inputs (minimal 7 outputs, 1 input)
const int XaarSS2 = 4;    // Pin 1,  <- ~SPI chip 2 select 
const int XaarGNDH = -1;  // Pin 2      GNDH
const int XaarVPPH = 5;   // Pin 3,  <- switch high load 35V system, pull low externally
const int XaarVPPL = 6;   // Pin 4,  <- switch low noise 35V system, pull low externally
const int XaarGNDL = -1;  // Pin 5      GNDL
const int XaarFAULT = 7;  // Pin 6,  -> Arduino Input, overheating detected
const int XaarRESET = 8;  // Pin 7,  <- ~RESET, pull low externally
const int XaarMOSI = 9;   // Pin 8,  <- Xaar Data Input
const int XaarVDD = -1;   // Pin 9,     5V VDD
const int XaarGND = -1;   // Pin 10     GND
const int XaarSCK = 10;   // Pin 11, <- SPI Clock
const int XaarSS1 = 11;   // Pin 12, <- ~SPI chip 1 select
const int XaarMISO = 12;  // Pin 13, -> Arduino Input, data returned from Xaar
const int XaarREADY = 13; // Pin 14: -> Ready for next fire
const int XaarCLK = 3;    // Pin 15: <- 1MHz clock
const int XaarFIRE = 2;   // Pin 16: <- ~FIRE on all nozzles

const int EndStop = A0;
const int MotorDir = A5;
const int MotorStep = A4;
const int MotorEnable = A3;
const int MotorSpeed = 1600;

// 0 = RXD, 1 = TXD
// 10 = SS, 11 = MOSI, 12 = MISO, 13 = SCK
// A5 = SCL, A4 = SDA

void setup() {
  // put your setup code here, to run once:
  digitalWrite(EndStop, 0);   pinMode(EndStop, INPUT_PULLUP);
  digitalWrite(MotorDir, 0);  pinMode(MotorDir, OUTPUT);
  digitalWrite(MotorStep, 0); pinMode(MotorStep, OUTPUT);
  digitalWrite(MotorEnable, 1); pinMode(MotorEnable, OUTPUT);

  // setup all ports
  digitalWrite(XaarRESET, 0); pinMode(XaarRESET, OUTPUT); // keep in reset until all power is on
  digitalWrite(XaarVPPH, 0);  pinMode(XaarVPPH, OUTPUT);  // 
  digitalWrite(XaarVPPL, 0);  pinMode(XaarVPPL, OUTPUT);  
  digitalWrite(XaarSS1, 1);   pinMode(XaarSS1, OUTPUT);   
  digitalWrite(XaarSS2, 1);   pinMode(XaarSS2, OUTPUT);   
  digitalWrite(XaarMOSI, 0);  pinMode(XaarMOSI, OUTPUT);  
  digitalWrite(XaarSCK, 1);   pinMode(XaarSCK, OUTPUT);   
  digitalWrite(XaarFIRE, 1);  pinMode(XaarFIRE, OUTPUT);  

  digitalWrite(XaarREADY, 0); pinMode(XaarREADY, INPUT);
  digitalWrite(XaarFAULT, 0); pinMode(XaarFAULT, INPUT);
  digitalWrite(XaarMISO, 0);  pinMode(XaarMISO, INPUT);

  // Generate 1MHz for the ink cartridge in hardware
  // also: https://github.com/JChristensen/gpsFreq/blob/master/examples/t2_1MHz/t2_1MHz.ino
  // also: http://arduino.stackexchange.com/questions/16698/arduino-constant-clock-output
  TCCR2A = 0x23;
  TCCR2B = 0x09;
  OCR2A = 15;
  OCR2B = 7;
  pinMode(XaarCLK, OUTPUT);

  Serial.begin(9600);  
  Serial.println("--- Xaar 128 Control ---");
  Serial.println("u - power Up");
  Serial.println("d - power Down");  
  Serial.println("0..9 - print test pattern");  
  Serial.println("Motor: l=left, r=right, h=home, e=endstop, p, q");  
  Serial.println("Complex: p=print");  
  Serial.println("ALWAYS POWER DOWN BEFORE REPROGRAMMING!"); 
  Serial.println(); 

}

// power up sequence

// power down sequence


void waitForReadyL()
{
  int i;
  for (i=100; i>0; --i) {
    if (digitalRead(XaarREADY)==0) return;
    delayMicroseconds(100);
  }
  Serial.println("No READY L");
}


void waitForReadyH()
{
  int i;
  for (i=100; i>0; --i) {
    if (digitalRead(XaarREADY)==1) return;
    delayMicroseconds(100);
  }
  Serial.println("No READY H");
}


void motorAdvance(int, int);

// test pattern
void firePattern(unsigned char *p, int advance=0)
{
  ubyte i, j;
  waitForReadyH();
  digitalWrite(XaarSS2, 0);
  for (i=0;i<8;i++) {
    ubyte d = p[i];
    for (j=0;j<8;j++) {
      digitalWrite(XaarSCK, 0);
      digitalWrite(XaarMOSI, (d&128)==128);
      //digitalWrite(XaarMOSI, 0);
      d = (d<<1);
      digitalWrite(XaarSCK, 1);
    }
  }
  digitalWrite(XaarSS2, 1);
  digitalWrite(XaarSS1, 0);
  for (;i<16;i++) {
    ubyte d = p[i], v;
    for (j=0;j<8;j++) {
      digitalWrite(XaarSCK, 0);
      digitalWrite(XaarMOSI, (d&128)==128);
      //digitalWrite(XaarMOSI, 1);
      d = (d<<1);
      // delay 100ns
      digitalWrite(XaarSCK, 1);
    }
  }
  digitalWrite(XaarSS1, 1);
  digitalWrite(XaarSCK, 1);
  digitalWrite(XaarFIRE, 0);
  delayMicroseconds(1);
  digitalWrite(XaarFIRE, 1);
  //waitForReadyL();
  if (advance) {
    motorAdvance(advance, 1);
  } else {
    if (digitalRead(XaarREADY)) Serial.write("R"); else Serial.write("r");
    if (digitalRead(XaarFAULT)) Serial.write("F"); else Serial.write("f");
    Serial.write(" ");
  }
}

void fireCoarsePattern(unsigned char pp, int adv)
{
  unsigned char p[16];
  int i;
  for (i=0; i<8; i++) {
    if (pp&1) p[i*2] = 0xff; else p[i*2] = 0x00; 
    if (pp&1) p[i*2+1] = 0xff; else p[i*2+1] = 0x00; 
    pp = (pp>>1);
  }
  for (i=0; i<20; i++)
    firePattern(p, adv);
}

void testPattern00(int rpt)
{
  // nothing
  static unsigned char p[1][16] = {
    { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 }
  };
  Serial.println("Pattern 00: no ink");
  for ( ; rpt>0; rpt--) {
    firePattern(p[0]);
    delay(50);
  }
  Serial.println("Pattern 00 done!");
}


void testPattern01(int rpt)
{
  // random
  static unsigned char p[2][16] = {
    { 0xfe, 0x23, 0x8e, 0x28, 0xff, 0xa2, 0x79, 0x54, 0xfe, 0x23, 0x8e, 0x28, 0xff, 0xa2, 0x79, 0x54 },
    { 0xfe, 0x23, 0x8e, 0x28, 0xff, 0xa2, 0x79, 0x54, 0xfe, 0x23, 0x8e, 0x28, 0xff, 0xa2, 0x79, 0x54 }
  };
  Serial.println("Pattern 01: some random pattern");
  for ( ; rpt>0; rpt--) {
    firePattern(p[0]);
    delay(50);
    firePattern(p[1]);
    delay(50);
  }
  Serial.println("Pattern 01 done!");
}


void testPattern02(int rpt)
{
  // lines
  unsigned char p[16];
  Serial.println("Pattern 02: every nozzle once");
  for ( ; rpt>0; rpt--) {
    int i, j;
    for (j=0;j<16;j++) p[j] = 0;
    for (i=0;i<128;i++) {
      int x = (i>>3), y = i&7;
      p[x] = 128>>y;
      firePattern(p);
      p[x] = 0;
    }
  }
  Serial.println("Pattern 02 done!");
}


void testPattern09(int rpt)
{
  // all nozzles fire
  static unsigned char p[1][16] = {
    { 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff }
  };
  Serial.println("Pattern 09: all black");
  for ( ; rpt>0; rpt--) {
    firePattern(p[0]);
    delay(50);
  }
  Serial.println("Pattern 09 done!");
}


void powerUpSequence()
{
  // wait 100ms after 5V power is available
  delay(100);
  // apply high voltage
  digitalWrite(XaarVPPL, 1);
  digitalWrite(XaarVPPH, 1);
  // ideally, we chek if power is actually present
  delay(100);
  // release the RESET line
  digitalWrite(XaarRESET, 1);
  delay(100);
  Serial.println("Power is up!");
}


void powerDownSequence()
{
  delay(1);
  digitalWrite(XaarRESET, 0);
  delay(5);
  digitalWrite(XaarVPPH, 0);
  digitalWrite(XaarVPPL, 0);
  delay(100);
  // save to switch VDD power off
  Serial.println("Power is down!");
}


void testEndStop()
{
  while (Serial.available()) Serial.read();
  while (!Serial.available()) {
    if (digitalRead(EndStop)==0) 
      Serial.print("e");
    else
      Serial.print("E");
    delay(250);
  }
  Serial.println("");
}


void motorAdvance(int n, int keepEnabled=0)
{
  const int spd = 1;
  int i;
  digitalWrite(MotorEnable, 0);
  if (n<0) {
    n = -n;
    digitalWrite(MotorDir, 0);
  } else {
    digitalWrite(MotorDir, 1);
  }
  delay(1);
  for (i=0; i<n; i++) {
    digitalWrite(MotorStep, 1);
    delayMicroseconds(MotorSpeed);
    digitalWrite(MotorStep, 0);
    delayMicroseconds(MotorSpeed);
    
  }
  if (keepEnabled==0)
    digitalWrite(MotorEnable, 1);
}


void motorHome(int keepEnabled=0)
{
  int i, n = 3500;
  digitalWrite(MotorEnable, 0);
  digitalWrite(MotorDir, 1);
  delay(1);
  for (i=0; i<n; i++) {
    if (digitalRead(EndStop)) break; 
    digitalWrite(MotorStep, 1);
    delayMicroseconds(MotorSpeed);
    digitalWrite(MotorStep, 0);
    delayMicroseconds(MotorSpeed);
  }
  if (keepEnabled==0)
    digitalWrite(MotorEnable, 1);
}


void testPrint()
{ 
  int i;
  motorHome(1);
  motorAdvance(-1400, 1);
  
  fireCoarsePattern(0xff, -1);
  fireCoarsePattern(0xaa, -1);
  fireCoarsePattern(0x55, -1);
  fireCoarsePattern(0xff, -1);
  fireCoarsePattern(0xaa, -1);
  fireCoarsePattern(0x55, -1);
  fireCoarsePattern(0xff, -1);
  fireCoarsePattern(0xaa, -1);
  fireCoarsePattern(0x55, -1);
  fireCoarsePattern(0xff, -1);
  fireCoarsePattern(0xaa, -1);
  fireCoarsePattern(0x55, -1);
  fireCoarsePattern(0xff, -1);
  fireCoarsePattern(0xaa, -1);
  fireCoarsePattern(0x55, -1);
  fireCoarsePattern(0xff, -1);
  
  motorAdvance(0);
}

void testPrint2()
{ 
  int i;
  motorHome(1);
  motorAdvance(-1000, 1);

  for (i=0; i<32; i++) {
    fireCoarsePattern(0xff, -1);
  }
  
  motorAdvance(0);
}

void loop() {
  // put your main code here, to run repeatedly:
  if (Serial.available())
  {
    int cmd = Serial.read();
    switch (cmd) {
      case 'u': powerUpSequence(); break;
      case 'd': powerDownSequence(); break;
      case '0': testPattern00(10); break;
      case '1': testPattern01(10); break;
      case '2': testPattern02( 3); break;
      case '9': testPattern09(10); break;
      case 'l': motorAdvance(100); break;  // 5.7mm
      case 'r': motorAdvance(-100); break;
      case 'h': motorHome(); break;
      case 'p': testPrint(); break;
      case 'q': testPrint2(); break;
      case 'e': testEndStop(); break;
    }
  }
}
