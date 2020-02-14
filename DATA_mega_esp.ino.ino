/*
  Modul RobotDyn
  part of Arduino Mega Server project
*/

#ifdef ROBOTDYN_FEATURE

bool sFlag = true;
unsigned long espTimer = millis();

// Serial request
#define MAX_SERIAL_REQ  32
String serialReq = "";

void robotdynInit() {
  Serial3.begin(115200);
  modulRobotdyn = MODUL_ENABLE;
  started("RobotDyn", true);
}

void printSerialStr() {
  Serial.print("[");
  Serial.print(serialReq);
  Serial.println("]");
}

void parseSerialCmd() {
  String command, parameter;
  
  if (serialReq.indexOf(F("?")) >= 0) {
    int pBegin = serialReq.indexOf(F("?")) + 1;
    if (serialReq.indexOf(F("=")) >= 0) {
       int pParam = serialReq.indexOf(F("="));
       command   = serialReq.substring(pBegin, pParam);              
       parameter = serialReq.substring(pParam + 1);              
    } else {
        command = serialReq.substring(pBegin);              
        parameter = "";
      }

   // if (command != F("esp")) {
      Serial.print(F("command/parameter: "));
      Serial.print(command);
      Serial.print(F("/"));
      Serial.println(parameter);
    //}

    if (command == F("esp")) {
      if (parameter == F("1")) {
        esp = 1;
        espTimer = millis();
      } else {
          esp = 0;
        }
    }
  } // if (request.indexOf(F("?")) >= 0)
} // parseSerialCmd()

void parseSerialStr() {
  if (serialReq[0] == '?') {
    parseSerialCmd();
  } else {
      printSerialStr();
    }
}

void checkSerial() {
  while (Serial3.available() > 0) {
    if (sFlag) {
      serialReq = "";
      sFlag = false;
    }
    char c = Serial3.read();
    if (c == 10) {
      sFlag = true;
      parseSerialStr();
    }
    else if (c == 13) {
      // skip
    }
    else {
      if (serialReq.length() < MAX_SERIAL_REQ) {
        serialReq += c;
      }
    } // if
  } // while (Serial3.available() > 0)
} // checkSerial()

void robotdynWork() {
  checkSerial();
  if (cycle4s) {
    Serial3.println(F("?mega=1"));
    if (millis() - espTimer > 8000) {
      esp = 0;
    }
  }
}

#endif // ROBOTDYN_FEATURE
