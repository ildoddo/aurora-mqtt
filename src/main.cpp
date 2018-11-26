//#include <Thread.h>
#include <Arduino.h>
//#include <Wire.h>
#include <TimeLib.h>
#include <SoftwareSerial.h>
#include <Tasker.h>
#include <ESP8266WiFi.h>
#include <ESP8266mDNS.h>
#include <WiFiUdp.h>
#include <ArduinoOTA.h>
#include <PubSubClient.h>
#include "RemoteDebug.h"

//--------- Configuration
// WiFi
const char* ssid = "FRITZ!Box 7490";
const char* password = "00aa11bb22cc33dd44ee55";

const char* mqttServer = "m23.cloudmqtt.com";
const char* mqttUser = "sgpjlnfw";
const char* mqttPass = "UwGiZrpG9uBS";
const char* mqttClientName = "ABBAurora"; //will also be used hostname and OTA name
const char* mqttTopicPrefix = "sensEnergy/ABBAurora/";

unsigned long measureDelay = 10000; //read every 60s
// internal vars
WiFiClient espClient;
PubSubClient mqttClient(espClient);
Tasker tasker;
//SoftwareSerial serDebug(10, 11); // RX, TX
SoftwareSerial serAurora(13, 12); //Conn RS485

char mqttTopicStatus[64];
char mqttTopicIp[64];

char mqttTopicPower[64];
char mqttTopicEnergy[64];

long lastReconnectAttempt = 0; //For the non blocking mqtt reconnect (in millis)
RemoteDebug Debug;

class clsAurora {
private:
  int MaxAttempt = 1;
  byte Address = 0;

  void clearData(byte *data, byte len) {
    for (int i = 0; i < len; i++) {
      data[i] = 0;
    }
  }

  int Crc16(byte *data, int offset, int count)
  {
    byte BccLo = 0xFF;
    byte BccHi = 0xFF;

    for (int i = offset; i < (offset + count); i++)
    {
      byte New = data[offset + i] ^ BccLo;
      byte Tmp = New << 4;
      New = Tmp ^ New;
      Tmp = New >> 5;
      BccLo = BccHi;
      BccHi = New ^ Tmp;
      Tmp = New << 3;
      BccLo = BccLo ^ Tmp;
      Tmp = New >> 4;
      BccLo = BccLo ^ Tmp;

    }

    return (int)word(~BccHi, ~BccLo);
  }

  bool Send(byte address, byte param0, byte param1, byte param2, byte param3, byte param4, byte param5, byte param6)
  {

    SendStatus = false;
    ReceiveStatus = false;

    byte SendData[10];
    SendData[0] = address;
    SendData[1] = param0;
    SendData[2] = param1;
    SendData[3] = param2;
    SendData[4] = param3;
    SendData[5] = param4;
    SendData[6] = param5;
    SendData[7] = param6;

    int crc = Crc16(SendData, 0, 8);
    SendData[8] = lowByte(crc);
    SendData[9] = highByte(crc);

    clearReceiveData();

    for (int i = 0; i < MaxAttempt; i++)
    {
      if (serAurora.write(SendData, sizeof(SendData)) != 0) {
        serAurora.flush();
        SendStatus = true;
        if (serAurora.readBytes(ReceiveData, sizeof(ReceiveData)) != 0) {
          if ((int)word(ReceiveData[7], ReceiveData[6]) == Crc16(ReceiveData, 0, 6)) {
            ReceiveStatus = true;
            break;
          }
        }
      }
    }
    return ReceiveStatus;
  }

  union {
    byte asBytes[4];
    float asFloat;
  } foo;

  union {
    byte asBytes[4];
    unsigned long asUlong;
  } ulo;

public:
  bool SendStatus = false;
  bool ReceiveStatus = false;
  byte ReceiveData[8];

  clsAurora(byte address) {
    Address = address;

    SendStatus = false;
    ReceiveStatus = false;

    clearReceiveData();
  }

  void clearReceiveData() {
    clearData(ReceiveData, 8);
  }

  String TransmissionState(byte id) {
    switch (id)
    {
    case 0:
      return ("Everything is OK.");
      break;
    case 51:
      return ("Command is not implemented");
      break;
    case 52:
      return ("Variable does not exist");
      break;
    case 53:
      return ("Variable value is out of range");
      break;
    case 54:
      return ("EEprom not accessible");
      break;
    case 55:
      return ("Not Toggled Service Mode");
      break;
    case 56:
      return ("Can not send the command to internal micro");
      break;
    case 57:
      return ("Command not Executed");
      break;
    case 58:
      return ("The variable is not available, retry");
      break;
    default:
      return ("Sconosciuto");
      break;
    }
  }

  String GlobalState(byte id) {
    switch (id)
    {
    case 0:
      return ("Sending Parameters");
      break;
    case 1:
      return ("Wait Sun / Grid");
      break;
    case 2:
      return ("Checking Grid");
      break;
    case 3:
      return ("Measuring Riso");
      break;
    case 4:
      return ("DcDc Start");
      break;
    case 5:
      return ("Inverter Start");
      break;
    case 6:
      return ("Run");
      break;
    case 7:
      return ("Recovery");
      break;
    case 8:
      return ("Pausev");
      break;
    case 9:
      return ("Ground Fault");
      break;
    case 10:
      return ("OTH Fault");
      break;
    case 11:
      return ("Address Setting");
      break;
    case 12:
      return ("Self Test");
      break;
    case 13:
      return ("Self Test Fail");
      break;
    case 14:
      return ("Sensor Test + Meas.Riso");
      break;
    case 15:
      return ("Leak Fault");
      break;
    case 16:
      return ("Waiting for manual reset");
      break;
    case 17:
      return ("Internal Error E026");
      break;
    case 18:
      return ("Internal Error E027");
      break;
    case 19:
      return ("Internal Error E028");
      break;
    case 20:
      return ("Internal Error E029");
      break;
    case 21:
      return ("Internal Error E030");
      break;
    case 22:
      return ("Sending Wind Table");
      break;
    case 23:
      return ("Failed Sending table");
      break;
    case 24:
      return ("UTH Fault");
      break;
    case 25:
      return ("Remote OFF");
      break;
    case 26:
      return ("Interlock Fail");
      break;
    case 27:
      return ("Executing Autotest");
      break;
    case 30:
      return ("Waiting Sun");
      break;
    case 31:
      return ("Temperature Fault");
      break;
    case 32:
      return ("Fan Staucked");
      break;
    case 33:
      return ("Int.Com.Fault");
      break;
    case 34:
      return ("Slave Insertion");
      break;
    case 35:
      return ("DC Switch Open");
      break;
    case 36:
      return ("TRAS Switch Open");
      break;
    case 37:
      return ("MASTER Exclusion");
      break;
    case 38:
      return ("Auto Exclusion");
      break;
    case 98:
      return ("Erasing Internal EEprom");
      break;
    case 99:
      return ("Erasing External EEprom");
      break;
    case 100:
      return ("Counting EEprom");
      break;
    case 101:
      return ("Freeze");
    default:
      return ("Sconosciuto");
      break;
    }
  }

  String DcDcState(byte id) {
    switch (id)
    {
    case 0:
      return ("DcDc OFF");
      break;
    case 1:
      return ("Ramp Start");
      break;
    case 2:
      return ("MPPT");
      break;
    case 3:
      return ("Not Used");
      break;
    case 4:
      return ("Input OC");
      break;
    case 5:
      return ("Input UV");
      break;
    case 6:
      return ("Input OV");
      break;
    case 7:
      return ("Input Low");
      break;
    case 8:
      return ("No Parameters");
      break;
    case 9:
      return ("Bulk OV");
      break;
    case 10:
      return ("Communication Error");
      break;
    case 11:
      return ("Ramp Fail");
      break;
    case 12:
      return ("Internal Error");
      break;
    case 13:
      return ("Input mode Error");
      break;
    case 14:
      return ("Ground Fault");
      break;
    case 15:
      return ("Inverter Fail");
      break;
    case 16:
      return ("DcDc IGBT Sat");
      break;
    case 17:
      return ("DcDc ILEAK Fail");
      break;
    case 18:
      return ("DcDc Grid Fail");
      break;
    case 19:
      return ("DcDc Comm.Error");
      break;
    default:
      return ("Sconosciuto");
      break;
    }
  }

  String InverterState(byte id) {
    switch (id)
    {
    case 0:
      return ("Stand By");
      break;
    case 1:
      return ("Checking Grid");
      break;
    case 2:
      return ("Run");
      break;
    case 3:
      return ("Bulk OV");
      break;
    case 4:
      return ("Out OC");
      break;
    case 5:
      return ("IGBT Sat");
      break;
    case 6:
      return ("Bulk UV");
      break;
    case 7:
      return ("Degauss Error");
      break;
    case 8:
      return ("No Parameters");
      break;
    case 9:
      return ("Bulk Low");
      break;
    case 10:
      return ("Grid OV");
      break;
    case 11:
      return ("Communication Error");
      break;
    case 12:
      return ("Degaussing");
      break;
    case 13:
      return ("Starting");
      break;
    case 14:
      return ("Bulk Cap Fail");
      break;
    case 15:
      return ("Leak Fail");
      break;
    case 16:
      return ("DcDc Fail");
      break;
    case 17:
      return ("Ileak Sensor Fail");
      break;
    case 18:
      return ("SelfTest: relay inverter");
      break;
    case 19:
      return ("SelfTest : wait for sensor test");
      break;
    case 20:
      return ("SelfTest : test relay DcDc + sensor");
      break;
    case 21:
      return ("SelfTest : relay inverter fail");
      break;
    case 22:
      return ("SelfTest timeout fail");
      break;
    case 23:
      return ("SelfTest : relay DcDc fail");
      break;
    case 24:
      return ("Self Test 1");
      break;
    case 25:
      return ("Waiting self test start");
      break;
    case 26:
      return ("Dc Injection");
      break;
    case 27:
      return ("Self Test 2");
      break;
    case 28:
      return ("Self Test 3");
      break;
    case 29:
      return ("Self Test 4");
      break;
    case 30:
      return ("Internal Error");
      break;
    case 31:
      return ("Internal Error");
      break;
    case 40:
      return ("Forbidden State");
      break;
    case 41:
      return ("Input UC");
      break;
    case 42:
      return ("Zero Power");
      break;
    case 43:
      return ("Grid Not Present");
      break;
    case 44:
      return ("Waiting Start");
      break;
    case 45:
      return ("MPPT");
      break;
    case 46:
      return ("Grid Fail");
      break;
    case 47:
      return ("Input OC");
      break;
    default:
      return ("Sconosciuto");
      break;
    }
  }

  String AlarmState(byte id) {
    switch (id)
    {
    case 0:
      return ("No Alarm");
      break;
    case 1:
      return ("Sun Low");
      break;
    case 2:
      return ("Input OC");
      break;
    case 3:
      return ("Input UV");
      break;
    case 4:
      return ("Input OV");
      break;
    case 5:
      return ("Sun Low");
      break;
    case 6:
      return ("No Parameters");
      break;
    case 7:
      return ("Bulk OV");
      break;
    case 8:
      return ("Comm.Error");
      break;
    case 9:
      return ("Output OC");
      break;
    case 10:
      return ("IGBT Sat");
      break;
    case 11:
      return ("Bulk UV");
      break;
    case 12:
      return ("Internal error");
      break;
    case 13:
      return ("Grid Fail");
      break;
    case 14:
      return ("Bulk Low");
      break;
    case 15:
      return ("Ramp Fail");
      break;
    case 16:
      return ("Dc / Dc Fail");
      break;
    case 17:
      return ("Wrong Mode");
      break;
    case 18:
      return ("Ground Fault");
      break;
    case 19:
      return ("Over Temp.");
      break;
    case 20:
      return ("Bulk Cap Fail");
      break;
    case 21:
      return ("Inverter Fail");
      break;
    case 22:
      return ("Start Timeout");
      break;
    case 23:
      return ("Ground Fault");
      break;
    case 24:
      return ("Degauss error");
      break;
    case 25:
      return ("Ileak sens.fail");
      break;
    case 26:
      return ("DcDc Fail");
      break;
    case 27:
      return ("Self Test Error 1");
      break;
    case 28:
      return ("Self Test Error 2");
      break;
    case 29:
      return ("Self Test Error 3");
      break;
    case 30:
      return ("Self Test Error 4");
      break;
    case 31:
      return ("DC inj error");
      break;
    case 32:
      return ("Grid OV");
      break;
    case 33:
      return ("Grid UV");
      break;
    case 34:
      return ("Grid OF");
      break;
    case 35:
      return ("Grid UF");
      break;
    case 36:
      return ("Z grid Hi");
      break;
    case 37:
      return ("Internal error");
      break;
    case 38:
      return ("Riso Low");
      break;
    case 39:
      return ("Vref Error");
      break;
    case 40:
      return ("Error Meas V");
      break;
    case 41:
      return ("Error Meas F");
      break;
    case 42:
      return ("Error Meas Z");
      break;
    case 43:
      return ("Error Meas Ileak");
      break;
    case 44:
      return ("Error Read V");
      break;
    case 45:
      return ("Error Read I");
      break;
    case 46:
      return ("Table fail");
      break;
    case 47:
      return ("Fan Fail");
      break;
    case 48:
      return ("UTH");
      break;
    case 49:
      return ("Interlock fail");
      break;
    case 50:
      return ("Remote Off");
      break;
    case 51:
      return ("Vout Avg errror");
      break;
    case 52:
      return ("Battery low");
      break;
    case 53:
      return ("Clk fail");
      break;
    case 54:
      return ("Input UC");
      break;
    case 55:
      return ("Zero Power");
      break;
    case 56:
      return ("Fan Stucked");
      break;
    case 57:
      return ("DC Switch Open");
      break;
    case 58:
      return ("Tras Switch Open");
      break;
    case 59:
      return ("AC Switch Open");
      break;
    case 60:
      return ("Bulk UV");
      break;
    case 61:
      return ("Autoexclusion");
      break;
    case 62:
      return ("Grid df / dt");
      break;
    case 63:
      return ("Den switch Open");
      break;
    case 64:
      return ("Jbox fail");
      break;
    default:
      return ("Sconosciuto");
      break;
    }
  }

  typedef struct {
    byte TransmissionState;
    byte GlobalState;
    byte InverterState;
    byte Channel1State;
    byte Channel2State;
    byte AlarmState;
    bool ReadState;
  } DataState;

  DataState State;

  bool ReadState() {
    State.ReadState = Send(Address, (byte)50, (byte)0, (byte)0, (byte)0, (byte)0, (byte)0, (byte)0);

    if (State.ReadState == false) {
      ReceiveData[0] = 255;
      ReceiveData[1] = 255;
      ReceiveData[2] = 255;
      ReceiveData[3] = 255;
      ReceiveData[4] = 255;
      ReceiveData[5] = 255;
    }

    State.TransmissionState = ReceiveData[0];
    State.GlobalState = ReceiveData[1];
    State.InverterState = ReceiveData[2];
    State.Channel1State = ReceiveData[3];
    State.Channel2State = ReceiveData[4];
    State.AlarmState = ReceiveData[5];

    return State.ReadState;
  }

  typedef struct {
    byte TransmissionState;
    byte GlobalState;
    String Par1;
    String Par2;
    String Par3;
    String Par4;
    bool ReadState;
  } DataVersion;

  DataVersion Version;

  bool ReadVersion() {
    Version.ReadState = Send(Address, (byte)58, (byte)0, (byte)0, (byte)0, (byte)0, (byte)0, (byte)0);

    if (Version.ReadState == false) {
      ReceiveData[0] = 255;
      ReceiveData[1] = 255;
    }

    Version.TransmissionState = ReceiveData[0];
    Version.GlobalState = ReceiveData[1];

    switch ((char)ReceiveData[2])
    {
    case 'i':
      Version.Par1 = ("Aurora 2 kW indoor");
      break;
    case 'o':
      Version.Par1 = ("Aurora 2 kW outdoor");
      break;
    case 'I':
      Version.Par1 = ("Aurora 3.6 kW indoor");
      break;
    case 'O':
      Version.Par1 = ("Aurora 3.0 - 3.6 kW outdoor");
      break;
    case '5':
      Version.Par1 = ("Aurora 5.0 kW outdoor");
      break;
    case '6':
      Version.Par1 = ("Aurora 6 kW outdoor");
      break;
    case 'P':
      Version.Par1 = ("3 - phase interface (3G74)");
      break;
    case 'C':
      Version.Par1 = ("Aurora 50kW module");
      break;
    case '4':
      Version.Par1 = ("Aurora 4.2kW new");
      break;
    case '3':
      Version.Par1 = ("Aurora 3.6kW new");
      break;
    case '2':
      Version.Par1 = ("Aurora 3.3kW new");
      break;
    case '1':
      Version.Par1 = ("Aurora 3.0kW new");
      break;
    case 'D':
      Version.Par1 = ("Aurora 12.0kW");
      break;
    case 'X':
      Version.Par1 = ("Aurora 10.0kW");
      break;
    default:
      Version.Par1 = ("Sconosciuto");
      break;
    }

    switch ((char)ReceiveData[3])
    {
    case 'A':
      Version.Par2 = ("UL1741");
      break;
    case 'E':
      Version.Par2 = ("VDE0126");
      break;
    case 'S':
      Version.Par2 = ("DR 1663 / 2000");
      break;
    case 'I':
      Version.Par2 = ("ENEL DK 5950");
      break;
    case 'U':
      Version.Par2 = ("UK G83");
      break;
    case 'K':
      Version.Par2 = ("AS 4777");
      break;
    default:
      Version.Par2 = ("Sconosciuto");
      break;
    }

    switch ((char)ReceiveData[4])
    {
    case 'N':
      Version.Par3 = ("Transformerless Version");
      break;
    case 'T':
      Version.Par3 = ("Transformer Version");
      break;
    default:
      Version.Par3 = ("Sconosciuto");
      break;
    }

    switch ((char)ReceiveData[5])
    {
    case 'W':
      Version.Par4 = ("Wind version");
      break;
    case 'N':
      Version.Par4 = ("PV version");
      break;
    default:
      Version.Par4 = ("Sconosciuto");
      break;
    }

    return Version.ReadState;
  }

  typedef struct {
    byte TransmissionState;
    byte GlobalState;
    float Valore;
    bool ReadState;
  } DataDSP;

  DataDSP DSP;

  bool ReadDSP(byte type, byte global) {
    if ((((int)type >= 1 && (int)type <= 9) || ((int)type >= 21 && (int)type <= 63)) && ((int)global >= 0 && (int)global <= 1)) {
      DSP.ReadState = Send(Address, (byte)59, type, global, (byte)0, (byte)0, (byte)0, (byte)0);

      if (DSP.ReadState == false) {
        ReceiveData[0] = 255;
        ReceiveData[1] = 255;
      }

    }
    else {
      DSP.ReadState = false;
      clearReceiveData();
      ReceiveData[0] = 255;
      ReceiveData[1] = 255;
    }

    DSP.TransmissionState = ReceiveData[0];
    DSP.GlobalState = ReceiveData[1];

    foo.asBytes[0] = ReceiveData[5];
    foo.asBytes[1] = ReceiveData[4];
    foo.asBytes[2] = ReceiveData[3];
    foo.asBytes[3] = ReceiveData[2];

    DSP.Valore = foo.asFloat;

    return DSP.ReadState;
  }

  typedef struct {
    byte TransmissionState;
    byte GlobalState;
    unsigned long Secondi;
    bool ReadState;
  } DataTimeDate;

  DataTimeDate TimeDate;

  bool ReadTimeDate() {
    TimeDate.ReadState = Send(Address, (byte)70, (byte)0, (byte)0, (byte)0, (byte)0, (byte)0, (byte)0);

    if (TimeDate.ReadState == false) {
      ReceiveData[0] = 255;
      ReceiveData[1] = 255;
    }

    TimeDate.TransmissionState = ReceiveData[0];
    TimeDate.GlobalState = ReceiveData[1];
    TimeDate.Secondi = ((unsigned long)ReceiveData[2] << 24) + ((unsigned long)ReceiveData[3] << 16) + ((unsigned long)ReceiveData[4] << 8) + (unsigned long)ReceiveData[5];
    return TimeDate.ReadState;
  }

  typedef struct {
    byte TransmissionState;
    byte GlobalState;
    byte Alarms1;
    byte Alarms2;
    byte Alarms3;
    byte Alarms4;
    bool ReadState;
  } DataLastFourAlarms;

  DataLastFourAlarms LastFourAlarms;

  bool ReadLastFourAlarms() {
    LastFourAlarms.ReadState = Send(Address, (byte)86, (byte)0, (byte)0, (byte)0, (byte)0, (byte)0, (byte)0);

    if (LastFourAlarms.ReadState == false) {
      ReceiveData[0] = 255;
      ReceiveData[1] = 255;
      ReceiveData[2] = 255;
      ReceiveData[3] = 255;
      ReceiveData[4] = 255;
      ReceiveData[5] = 255;
    }

    LastFourAlarms.TransmissionState = ReceiveData[0];
    LastFourAlarms.GlobalState = ReceiveData[1];
    LastFourAlarms.Alarms1 = ReceiveData[2];
    LastFourAlarms.Alarms2 = ReceiveData[3];
    LastFourAlarms.Alarms3 = ReceiveData[4];
    LastFourAlarms.Alarms4 = ReceiveData[5];

    return LastFourAlarms.ReadState;
  }

  bool ReadJunctionBoxState(byte nj) {
    return Send(Address, (byte)200, nj, (byte)0, (byte)0, (byte)0, (byte)0, (byte)0);
  }

  bool ReadJunctionBoxVal(byte nj, byte par) {
    return Send(Address, (byte)201, nj, par, (byte)0, (byte)0, (byte)0, (byte)0);
  }

  // Inverters
  typedef struct {
    String PN;
    bool ReadState;
  } DataSystemPN;

  DataSystemPN SystemPN;

  bool ReadSystemPN() {
    SystemPN.ReadState = Send(Address, (byte)52, (byte)0, (byte)0, (byte)0, (byte)0, (byte)0, (byte)0);

    SystemPN.PN = String(String((char)ReceiveData[0]) + String((char)ReceiveData[1]) + String((char)ReceiveData[2]) + String((char)ReceiveData[3]) + String((char)ReceiveData[4]) + String((char)ReceiveData[5]));

    return SystemPN.ReadState;
  }

  typedef struct {
    String SerialNumber;
    bool ReadState;
  } DataSystemSerialNumber;

  DataSystemSerialNumber SystemSerialNumber;

  bool ReadSystemSerialNumber() {
    SystemSerialNumber.ReadState = Send(Address, (byte)63, (byte)0, (byte)0, (byte)0, (byte)0, (byte)0, (byte)0);

    SystemSerialNumber.SerialNumber = String(String((char)ReceiveData[0]) + String((char)ReceiveData[1]) + String((char)ReceiveData[2]) + String((char)ReceiveData[3]) + String((char)ReceiveData[4]) + String((char)ReceiveData[5]));

    return SystemSerialNumber.ReadState;
  }

  typedef struct {
    byte TransmissionState;
    byte GlobalState;
    String Week;
    String Year;
    bool ReadState;
  } DataManufacturingWeekYear;

  DataManufacturingWeekYear ManufacturingWeekYear;

  bool ReadManufacturingWeekYear() {
    ManufacturingWeekYear.ReadState = Send(Address, (byte)65, (byte)0, (byte)0, (byte)0, (byte)0, (byte)0, (byte)0);

    if (ManufacturingWeekYear.ReadState == false) {
      ReceiveData[0] = 255;
      ReceiveData[1] = 255;
    }

    ManufacturingWeekYear.TransmissionState = ReceiveData[0];
    ManufacturingWeekYear.GlobalState = ReceiveData[1];
    ManufacturingWeekYear.Week = String(String((char)ReceiveData[2]) + String((char)ReceiveData[3]));
    ManufacturingWeekYear.Year = String(String((char)ReceiveData[4]) + String((char)ReceiveData[5]));

    return ManufacturingWeekYear.ReadState;
  }

  typedef struct {
    byte TransmissionState;
    byte GlobalState;
    String Release;
    bool ReadState;
  } DataFirmwareRelease;

  DataFirmwareRelease FirmwareRelease;

  bool ReadFirmwareRelease() {
    FirmwareRelease.ReadState = Send(Address, (byte)72, (byte)0, (byte)0, (byte)0, (byte)0, (byte)0, (byte)0);

    if (FirmwareRelease.ReadState == false) {
      ReceiveData[0] = 255;
      ReceiveData[1] = 255;
    }

    FirmwareRelease.TransmissionState = ReceiveData[0];
    FirmwareRelease.GlobalState = ReceiveData[1];
    FirmwareRelease.Release = String(String((char)ReceiveData[2]) + "." + String((char)ReceiveData[3]) + "." + String((char)ReceiveData[4]) + "." + String((char)ReceiveData[5]));

    return FirmwareRelease.ReadState;
  }

  typedef struct {
    byte TransmissionState;
    byte GlobalState;
    unsigned long Energia;
    bool ReadState;
  } DataCumulatedEnergy;

  DataCumulatedEnergy CumulatedEnergy;

  bool ReadCumulatedEnergy(byte par) {
    if ((int)par >= 0 && (int)par <= 6) {
      CumulatedEnergy.ReadState = Send(Address, (byte)78, par, (byte)0, (byte)0, (byte)0, (byte)0, (byte)0);

      if (CumulatedEnergy.ReadState == false) {
        ReceiveData[0] = 255;
        ReceiveData[1] = 255;
      }

    }
    else {
      CumulatedEnergy.ReadState = false;
      clearReceiveData();
      ReceiveData[0] = 255;
      ReceiveData[1] = 255;
    }

    CumulatedEnergy.TransmissionState = ReceiveData[0];
    CumulatedEnergy.GlobalState = ReceiveData[1];
    if (CumulatedEnergy.ReadState == true) {
            ulo.asBytes[0] = ReceiveData[5];
           ulo.asBytes[1] = ReceiveData[4];
            ulo.asBytes[2] = ReceiveData[3];
            ulo.asBytes[3] = ReceiveData[2];

           CumulatedEnergy.Energia = ulo.asUlong;
    }
    return CumulatedEnergy.ReadState;
  }

  bool WriteBaudRateSetting(byte baudcode) {
    if ((int)baudcode >= 0 && (int)baudcode <= 3) {
      return Send(Address, (byte)85, baudcode, (byte)0, (byte)0, (byte)0, (byte)0, (byte)0);
    }
    else {
      clearReceiveData();
      return false;
    }
  }

  // Central
  bool ReadFlagsSwitchCentral() {
    return Send(Address, (byte)67, (byte)0, (byte)0, (byte)0, (byte)0, (byte)0, (byte)0);
  }

  bool ReadCumulatedEnergyCentral(byte var, byte ndays_h, byte ndays_l, byte global) {
    return Send(Address, (byte)68, var, ndays_h, ndays_l, global, (byte)0, (byte)0);
  }

  bool ReadFirmwareReleaseCentral(byte var) {
    return Send(Address, (byte)72, var, (byte)0, (byte)0, (byte)0, (byte)0, (byte)0);
  }

  bool ReadBaudRateSettingCentral(byte baudcode, byte serialline) {
    return Send(Address, (byte)85, baudcode, serialline, (byte)0, (byte)0, (byte)0, (byte)0);
  }

  bool ReadSystemInfoCentral(byte var) {
    return Send(Address, (byte)101, var, (byte)0, (byte)0, (byte)0, (byte)0, (byte)0);
  }

  bool ReadJunctionBoxMonitoringCentral(byte cf, byte rn, byte njt, byte jal, byte jah) {
    return Send(Address, (byte)103, cf, rn, njt, jal, jah, (byte)0);
  }

  bool ReadSystemPNCentral() {
    return Send(Address, (byte)105, (byte)0, (byte)0, (byte)0, (byte)0, (byte)0, (byte)0);
  }

  bool ReadSystemSerialNumberCentral() {
    return Send(Address, (byte)107, (byte)0, (byte)0, (byte)0, (byte)0, (byte)0, (byte)0);
  }

};

clsAurora Inverter2 = clsAurora(2);

void leggiProduzione();

String stampaDataTime(unsigned long scn)
{
  String rtn;

  if (scn > 0) {
    setTime(0, 0, 0, 1, 1, 2000);
    if (timeStatus() == timeSet) {
    adjustTime(scn);

      rtn = String(day());
      rtn += String(("/"));
      rtn += String(month());
      rtn += String(("/"));
      rtn += String(year());
      rtn += String((" "));
      rtn += String(hour());
      rtn += String((":"));
      rtn += String(minute());
      rtn += String((":"));
      rtn += String(second());
    }
  }

  return rtn;
}

void setup_wifi() {
  delay(10);
  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(ssid);
  WiFi.mode(WIFI_STA); //disable AP mode, only station
  WiFi.hostname(mqttClientName);
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("");
  Serial.println("WiFi connected");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());
}


bool MqttReconnect() {
  if (!mqttClient.connected()) {
    Serial.print("Attempting MQTT connection...");
    // Attempt to connect with last will retained
    if (mqttClient.connect(mqttClientName, mqttUser, mqttPass, mqttTopicStatus, 1, true, "offline")) {
      Serial.println("connected");
      // Once connected, publish an announcement...
      char curIp[16];
      sprintf(curIp, "%d.%d.%d.%d", WiFi.localIP()[0], WiFi.localIP()[1], WiFi.localIP()[2], WiFi.localIP()[3]);
      mqttClient.publish(mqttTopicStatus, "online", true);
      mqttClient.publish(mqttTopicIp, curIp, true);
    } else {
      Serial.print("failed, rc=");
      Serial.print(mqttClient.state());
      Serial.println(" try again in 5 seconds");
    }
  }
  return mqttClient.connected();
}

void setup() {
  Serial.begin(115200);
  Serial.println("------------------------------------------");
  Serial.println("               AVVIO");
  Serial.println("------------------------------------------");
  serAurora.setTimeout(500);
  serAurora.begin(19200);

  sprintf(mqttTopicStatus, "%sstatus", mqttTopicPrefix);
  sprintf(mqttTopicIp, "%sip", mqttTopicPrefix);
  sprintf(mqttTopicPower, "%spower", mqttTopicPrefix);
  sprintf(mqttTopicEnergy, "%senergy", mqttTopicPrefix);

  setup_wifi();
  mqttClient.setServer(mqttServer, 15288);
  tasker.setInterval(leggiProduzione, measureDelay);

  //----------- OTA
  ArduinoOTA.setHostname(mqttClientName);

  ArduinoOTA.onStart([]() {
    String type;
    if (ArduinoOTA.getCommand() == U_FLASH) {
      type = "sketch";
    } else { // U_SPIFFS
      type = "filesystem";
    }
    // NOTE: if updating SPIFFS this would be the place to unmount SPIFFS using SPIFFS.end()
    Serial.println("Start updating " + type);
  });

  ArduinoOTA.onEnd([]() {
    Serial.println("\nEnd");
    delay(1000);
    ESP.restart();
  });

  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
    Serial.printf("Progress: %u%%\r", (progress / (total / 100)));
  });

  ArduinoOTA.onError([](ota_error_t error) {
    Serial.printf("Error[%u]: ", error);
    if (error == OTA_AUTH_ERROR) Serial.println("Auth Failed");
    else if (error == OTA_BEGIN_ERROR) Serial.println("Begin Failed");
    else if (error == OTA_CONNECT_ERROR) Serial.println("Connect Failed");
    else if (error == OTA_RECEIVE_ERROR) Serial.println("Receive Failed");
    else if (error == OTA_END_ERROR) Serial.println("End Failed");
  });
  ArduinoOTA.begin();

  Debug.begin(mqttClientName);
  Debug.setSerialEnabled(true);
}

void loop() {
  //handle mqtt connection, non-blocking
  if (!mqttClient.connected()) {
    long now = millis();
    if (now - lastReconnectAttempt > 5000) {
      lastReconnectAttempt = now;
      // Attempt to reconnect
      if (MqttReconnect()) {
        lastReconnectAttempt = 0;
      }
    }
  }
  mqttClient.loop();
  tasker.loop();
  //handle OTA
  ArduinoOTA.handle();
  Debug.handle();
}

void leggiProduzione() {
  Inverter2.ReadDSP(3, 1);
  float potenza = Inverter2.DSP.Valore;
  Inverter2.ReadCumulatedEnergy(0);
  float energia = Inverter2.CumulatedEnergy.Energia;
  if (!isnan(potenza)) {
    mqttClient.publish(mqttTopicPower, String(potenza, 2).c_str(), true);
    Serial.printf("Potenza istantanea: %sW\n", String(potenza, 2).c_str());
    //rdebugA.println("Potenza istantanea: %sW\n", String(potenza, 2).c_str());
  }

  if (!isnan(energia)) {
    mqttClient.publish(mqttTopicEnergy, String(energia, 2).c_str(), true);
    Serial.printf("Energia accumulata: %sWh\n", String(energia, 2).c_str());
    //rdebugA.println("Energia accumulata: %sWh\n", String(energia, 2).c_str());
  }
}
