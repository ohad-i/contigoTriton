#include <SCServo.h>
#include <SoftwareSerial.h>

#include <SPI.h>
#include <UIPEthernet.h>

#include <Servo.h>

SoftwareSerial mySerial(2, 3);  // RX, TX

#define DEBUG_SER_PORT mySerial

#define DBG_MSG
#ifdef DBG_MSG
#define WRITE_DEBUG_MSGLN(msg) \
  { DEBUG_SER_PORT.println(msg); }
#define WRITE_DEBUG_MSG(msg) \
  { DEBUG_SER_PORT.print(msg); }
#else
#define WRITE_DEBUG_MSGLN(msg) \
  {}
#define WRITE_DEBUG_MSG(msg) \
  {}
#endif

//#define DBG_METRY
#ifdef DBG_METRY
#define GET_METRY() \
{ getServoMetry_debug();}
#else
#define GET_METRY() \
{ getServoMetry();}

#endif

#define WRITE_CRITIC_MSGLN(msg) \
  { DEBUG_SER_PORT.println(msg); }
#define WRITE_CRITIC_MSG(msg) \
  { DEBUG_SER_PORT.print(msg); }

byte mac[] = {
  0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xED
};
IPAddress ip(10, 10, 0, 31);
unsigned int localPort = 1041;  // local port to listen on

// buffers for receiving and sending data
char packetBuffer[12];  // buffer to hold incoming packet,


// An EthernetUDP instance to let us send and receive packets over UDP
EthernetUDP Udp;


const int stopMovingOp   = 0x0801;
const int startMovingOp  = 0x0802;
const int doRotationsOp  = 0x0803;
const int gotoPositionOP = 0x0804;
const int setSpeedOp     = 0x0805;
const int setServoOp     = 0x0806;
const int burnServoIdOp  = 0x0807;
const int messengerOp    = 0x0808;

const int headSensorPin  = 9;
int sensorState          = 0;

int SERVO_ID = 22; //21, default is 1

int opId = 0;

//SCSCL sc;
SMS_STS sc;
int LEDpin = 13;


/*
byte ID[1];
s16 Position[2];
u16 Speed[2];
byte ACC[2];
*/

union upMsg_ {
  byte c[18];
  int value[9];
} upMsg;

union downMsg_ {
  byte c[16];
  int value[8];
} downMsg;

/*
union syncMsg_{
    byte b[2];
    short sync;
  } synMsg;

synMsg.b[0] = 0xac;
synMsg.b[1] = 0xad;
*/

int Pos;
int Speed;
int Load;
int Voltage;
int Temper;
int Move;
int Current;
int id = SERVO_ID;
int metryCycles = -1;


unsigned long lsatMetrySent = millis();
unsigned long startRotations = millis();


void getServoMetry_debug() {
  Pos = sc.ReadPos(SERVO_ID);
  if (Pos == -1) {
    WRITE_DEBUG_MSGLN(F("read position err"));
  }
  WRITE_DEBUG_MSG(F("Servo position:"));
  WRITE_DEBUG_MSGLN(Pos);

  Voltage = sc.ReadVoltage(SERVO_ID);
  if (Voltage == -1) {
    WRITE_DEBUG_MSGLN(F("read Voltage err"));
  }
  WRITE_DEBUG_MSG(F("Servo Voltage:"));
  WRITE_DEBUG_MSGLN(Voltage);

  Temper = sc.ReadTemper(SERVO_ID);
  if (Temper == -1) {
    WRITE_DEBUG_MSGLN(F("read temperature err"));
  }
  WRITE_DEBUG_MSG(F("Servo temperature:") );
  WRITE_DEBUG_MSGLN(Temper);

  Speed = sc.ReadSpeed(SERVO_ID);
  if (Speed == -1) {
    WRITE_DEBUG_MSGLN(F("read Speed err"));
  }
  WRITE_DEBUG_MSG(F("Servo Speed:"));
  WRITE_DEBUG_MSGLN(Speed);

  Load = sc.ReadLoad(SERVO_ID);
  if (Load == -1) {
    WRITE_DEBUG_MSGLN(F("read Load err"));
  }
  WRITE_DEBUG_MSG(F("Servo Load:"));
  WRITE_DEBUG_MSGLN(Load);

  Current = sc.ReadCurrent(SERVO_ID);
  if (Current == -1) {
    WRITE_DEBUG_MSGLN(F("read Current err"));
  }
  WRITE_DEBUG_MSG(F("Servo Current:"));
  WRITE_DEBUG_MSGLN(Current);

  Move = sc.ReadMove(SERVO_ID);
  if (Move == -1) {
    WRITE_DEBUG_MSGLN(F("read Move err"));
  }
  WRITE_DEBUG_MSG(F("Servo Move:"));
  WRITE_DEBUG_MSGLN(Move);

  id = sc.Ping(SERVO_ID);
  WRITE_DEBUG_MSG(F("Servo ID:"));
  WRITE_DEBUG_MSGLN(id);
}

void getServoMetry() {
  Pos = sc.ReadPos(SERVO_ID);
  Voltage = sc.ReadVoltage(SERVO_ID);
  Temper = sc.ReadTemper(SERVO_ID);
  Speed = sc.ReadSpeed(SERVO_ID);
  Load = sc.ReadLoad(SERVO_ID);
  Current = sc.ReadCurrent(SERVO_ID);
  Move = sc.ReadMove(SERVO_ID);
  id = sc.Ping(SERVO_ID);
 }

void sendServoMetry() {
  upMsg.value[0] = Pos;
  upMsg.value[1] = Speed;
  upMsg.value[2] = Load;
  upMsg.value[3] = Voltage;
  upMsg.value[4] = Temper;
  upMsg.value[5] = Move;
  upMsg.value[6] = Current;
  upMsg.value[7] = id;
  upMsg.value[8] = metryCycles;

  Udp.beginPacket("10.10.0.25", 1040);
  Udp.write(upMsg.c, 18);
  Udp.endPacket();

  
}
int cyclesCount;

Servo messengerServo; 

void setup() {
  pinMode(LEDpin, OUTPUT);
  digitalWrite(LEDpin, HIGH);
  DEBUG_SER_PORT.begin(115200);
  Serial.begin(1000000);
  sc.pSerial = &Serial;

  WRITE_CRITIC_MSGLN(F("setup init..."));

  // start the Ethernet
  Ethernet.begin(mac, ip);

  //cyclesCount = 0;

  // write servo ID
  //sc.unLockEprom(1);
  //sc.writeByte(1, SCSCL_ID, SERVO_ID);//ID
  //int ret = sc.LockEprom(SERVO_ID);

  // Check for Ethernet hardware present
  if (Ethernet.hardwareStatus() == EthernetNoHardware) {
    WRITE_CRITIC_MSGLN(F("Ethernet shield was not found.  Sorry, can't run without hardware. :("));
    while (true) {

      if (Ethernet.hardwareStatus() != EthernetNoHardware) {
        WRITE_CRITIC_MSGLN(F("Ethernet shield was not found"));
      }
      else {
        WRITE_CRITIC_MSGLN(F("Ethernet shield was found!"));
        break;
      }
      delay(1);  // do nothing, no point running without Ethernet hardware
    }
  }
  if (Ethernet.linkStatus() == LinkOFF) {
    WRITE_DEBUG_MSGLN(F("Ethernet cable is not connected."));
  }
  // start UDP
  Udp.begin(localPort);

  int ID = sc.Ping(SERVO_ID);
  
  int tries = 0;
  while (ID != SERVO_ID){
    ID = sc.Ping(SERVO_ID);
    WRITE_CRITIC_MSG(SERVO_ID);
    WRITE_CRITIC_MSG(F(" "));
    WRITE_CRITIC_MSG(tries);
    WRITE_CRITIC_MSGLN(F("--motor not connected..."));
    tries += 1;
    delay(500);
  }
  
  WRITE_CRITIC_MSG(F("--motor connected... "));
  WRITE_CRITIC_MSGLN(SERVO_ID);
  
  sc.PositionMode(SERVO_ID);
  sc.WriteSpe(SERVO_ID, 0);

  sc.unLockEprom(SERVO_ID);
  // set servo not angle limits
  sc.writeByte(SERVO_ID, 9, 0);//ID
  sc.writeByte(SERVO_ID, 10, 0);//ID
  sc.writeByte(SERVO_ID, 11, 0);//ID
  sc.writeByte(SERVO_ID, 12, 0);//ID
  int ret = sc.LockEprom(SERVO_ID);

  // start servo on stall mode
  sc.WheelMode(SERVO_ID);
  sc.WriteSpe(SERVO_ID, 0);
  
  delay(10);

  messengerServo.attach(6);
  pinMode(headSensorPin, INPUT_PULLUP);
 
  WRITE_CRITIC_MSGLN(F("end init...") );
  
}


void stopServo(int servoId, int pos = -1) {
  // use rtPos for stop with power
  
  WRITE_CRITIC_MSGLN(F("Stop servo, stopping...") ) ;
  sc.PositionMode(servoId);
  if(pos == -1)
  {
    sc.WriteSpe(servoId, 0, 200);
  }
  else
  {
    sc.WritePosEx (servoId, pos, 0, 1000);
  }
  return;
}

void startServo(int servoId, int speed) {
  WRITE_CRITIC_MSG(F("Start servo motor running... speed:"));
  WRITE_CRITIC_MSGLN(speed);
  sc.WheelMode(servoId);
  sc.WriteSpe(servoId, speed);
  return;
}

int desPosition = -1;

int cycleInitPos = -1;
int curCycle     = 0;

int rtPos = -1;

int desSpeed = 0;

bool slowDown = false;
int cmdServoId = -1;

int LoopDet = 30;
bool hasStopped = true;

void loop() {
  
  
  rtPos = sc.ReadPos(SERVO_ID);
  if(rtPos == -1)
  {
    rtPos= Pos;
  }

  /*
  if(cyclesCount > 0)
  {
    WRITE_CRITIC_MSG(abs(cycleInitPos-rtPos));
    WRITE_CRITIC_MSG(" ");
    WRITE_CRITIC_MSG(rtPos);
    WRITE_CRITIC_MSG(" ");
    WRITE_CRITIC_MSGLN(cycleInitPos);
  }
  */
  unsigned long loopTic = millis();

  

  
  if ((loopTic - lsatMetrySent) >= 100) {
    // ~10 Hz metry
    //getServoMetry();
    GET_METRY();
    sendServoMetry();
    lsatMetrySent = loopTic;
    sensorState = digitalRead(headSensorPin);  // read the magnetic sensor 
    
    if (sensorState == HIGH && not hasStopped ) // && Move == 1)
    {
      //WRITE_CRITIC_MSG(Move);
      //WRITE_CRITIC_MSGLN(F("stop by sensor..."));
      sc.PositionMode(SERVO_ID);
      sc.WritePosEx(SERVO_ID, rtPos, 0, 1000);
      cycleInitPos = -1;
      cyclesCount  = 0;
      curCycle     = 0;
      metryCycles  = 0;
      hasStopped = true;
    }
    /*
    WRITE_CRITIC_MSG(sensorState);
    
    if(sensorState == LOW) {
      WRITE_CRITIC_MSGLN(F(" sensor head not detected..."));
    }
    else {
      WRITE_CRITIC_MSGLN(F(" sensor head detected!"));
    }
    */
  }

  
  if( slowDown and (curCycle == cyclesCount-1) )
  { 
    // slow down before stopping
    int newSpd = (int)(desSpeed);
    if(newSpd > 0)
    {
      sc.WriteSpe(SERVO_ID, int(min(300, desSpeed)) );
    }
    else
    {
      sc.WriteSpe(SERVO_ID, int(max(-300, desSpeed)));
    }
    WRITE_CRITIC_MSG(F("slowing down... "));
    WRITE_CRITIC_MSGLN(newSpd);
    LoopDet = 10;
    slowDown = false;
  }

  if( (cyclesCount > 0) and (abs(cycleInitPos-rtPos) < LoopDet) and
                       (loopTic - startRotations > 130) )
  {
    curCycle++;
    metryCycles  = curCycle;
    WRITE_CRITIC_MSGLN(curCycle);
    startRotations = loopTic;
    
    if(curCycle==cyclesCount)
    {
      /*
      sc.WriteSpe(SERVO_ID, 0);
      sc.PositionMode(SERVO_ID);
      */
      sc.WheelMode(SERVO_ID);
      sc.WriteSpe(SERVO_ID, 0);
      WRITE_CRITIC_MSG(rtPos);
      WRITE_CRITIC_MSG(F(" finish cycles... "));
      WRITE_CRITIC_MSGLN(sc.ReadPos(SERVO_ID) );
      cycleInitPos = -1;
      cyclesCount  = 0;
      curCycle     = 0;
    }
  }

  if(desPosition != -1 && abs(desPosition - rtPos)< 10)
  {
    WRITE_CRITIC_MSGLN("inPosition");
    desPosition = -1;
  }

  int packetSize = Udp.parsePacket();

  if (packetSize) {
    WRITE_DEBUG_MSG(F("Received packet of size ") );
    WRITE_DEBUG_MSGLN(packetSize);
    WRITE_DEBUG_MSG(F("From "));
    IPAddress remoteIp = Udp.remoteIP();
    WRITE_DEBUG_MSG(remoteIp);
    WRITE_DEBUG_MSG(F(", port "));
    WRITE_DEBUG_MSGLN(Udp.remotePort());
    // read the packet into packetBufffer

    int len = Udp.read(packetBuffer, 12);
    if (len > 0) {
      WRITE_DEBUG_MSG(F("got msg..."));
      WRITE_DEBUG_MSGLN(len);
      memcpy(downMsg.c, packetBuffer, len);

      int curOpId = downMsg.value[opId];

      WRITE_CRITIC_MSG(F("got command: "));
      WRITE_CRITIC_MSGLN(curOpId);
      

      if( curOpId == setServoOp)
      {
        SERVO_ID = downMsg.value[1];
      }

      if(curOpId == burnServoIdOp)
      {
        int newServoId = downMsg.value[1];
        sc.unLockEprom(SERVO_ID);
        sc.writeByte(SERVO_ID, SCSCL_ID, newServoId);//ID
        int ret = sc.LockEprom(newServoId);
        SERVO_ID = newServoId;
      }

      if(curOpId == messengerOp)
      {
        int pwm = downMsg.value[1];
        WRITE_CRITIC_MSG(F("got messnger msg pwm: "));
        WRITE_CRITIC_MSGLN(pwm);
        messengerServo.write(pwm);
      }

      if( curOpId == stopMovingOp)
      {
        hasStopped = true;
        desPosition != -1;
        WRITE_CRITIC_MSGLN(F("Servo111"));
        cmdServoId = downMsg.value[1];
        if (cmdServoId < 1)
        {
          cmdServoId = SERVO_ID;
        }
        stopServo(cmdServoId);

        cycleInitPos = -1;
        cyclesCount  = 0;
        curCycle     = 0;
        metryCycles  = 0;
      }
      else if(curOpId == startMovingOp)
      {
        hasStopped = false;
        desPosition != -1;
        WRITE_CRITIC_MSGLN(F("Start servo"));
        desSpeed = downMsg.value[2];
        cmdServoId = downMsg.value[1];
        if (cmdServoId == -1)
        {
          cmdServoId = SERVO_ID;
        }
        startServo(cmdServoId, desSpeed);
        cycleInitPos = -1;
        cyclesCount  = 0;
        curCycle     = 0;
        metryCycles  = 0;
      }
      else if(curOpId == doRotationsOp)
      {
        hasStopped = false;
        //sc.WritePosEx (cmdServoId, desPosition, desSpeed, 50);
        WRITE_CRITIC_MSG(F("do Rotations - "));
        
        desPosition != -1;
        cyclesCount = downMsg.value[2];
        desSpeed    = downMsg.value[3];
        WRITE_CRITIC_MSGLN(cyclesCount);
        float partial =  (cyclesCount%10) *0.1;
        WRITE_CRITIC_MSG(F("partial ... "));
        WRITE_CRITIC_MSGLN(partial);
        cyclesCount = (int)(cyclesCount/10);
        if(partial>0)
        {
          cyclesCount++;
        }
        
        if (cyclesCount > 0 )
        {
          WRITE_CRITIC_MSG(F("start cycles... "))
          WRITE_CRITIC_MSGLN(cyclesCount);
          
          cycleInitPos = ( rtPos+int(4096*(partial)) )%4096;
          WRITE_CRITIC_MSG(F("start position: "))
          WRITE_CRITIC_MSG(rtPos);
          WRITE_CRITIC_MSG(F("end position: "))
          WRITE_CRITIC_MSG(cycleInitPos);
          curCycle     = 0;
          slowDown     = true;
          LoopDet      = 30;
          metryCycles  = curCycle;
          cmdServoId   = downMsg.value[1];
          if (cmdServoId != SERVO_ID)
          {
            WRITE_CRITIC_MSGLN(F("Bad servo id..."));
          }
          else
          {
            startServo(SERVO_ID, desSpeed);
          }
        }
        startRotations = millis();
      }
      
      else if(curOpId == setSpeedOp)
      {
        WRITE_CRITIC_MSGLN(F("set speed"));
        desSpeed = downMsg.value[2];
        cmdServoId = downMsg.value[1];
        if (cmdServoId < 1)
        {
          cmdServoId = SERVO_ID;
        }
        sc.WriteSpe(cmdServoId, desSpeed);
      }
      else if(curOpId == gotoPositionOP)
      {
        hasStopped = false;
        desPosition = downMsg.value[2];//%4095;
        desSpeed    = downMsg.value[3];
        WRITE_CRITIC_MSG(F("Servo go to command, position "));
        WRITE_CRITIC_MSG(desPosition);
        WRITE_CRITIC_MSG(F(" speed "));
        WRITE_CRITIC_MSGLN(desSpeed);

        cycleInitPos = rtPos;
        curCycle     = 0;
        slowDown     = true;

        cmdServoId = downMsg.value[1];
        if (cmdServoId < 1)
        {
          cmdServoId = SERVO_ID;
        }
        sc.PositionMode(cmdServoId);
        sc.WritePosEx (cmdServoId, desPosition, desSpeed, 50);
      }
    }
  }
}
