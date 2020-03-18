// can't upload code after sw reset.

#include <SPI.h>

#include <SoftwareSerial.h>
#include <EEPROM.h>
SoftwareSerial esp8266(6, 7); //RX,TX

#define pinwifi 10 //3v3
#define pinpump 12 //5v
#define pinvalve 11 //12v
#define pinflux 2
#define pinhum 8

#define pinhumidity A1

//declare funcions:
int freeRam();
boolean wifiStart();
void sendData(String cmd, const int tout, boolean debug);
boolean timeAdjust();
boolean getLHumidity();
boolean sendVars();
void hourModify(int lhour, int lmin);
void sleepMins(int minutes);
boolean irrigate(int seconds);
int readHumidity();
void addMinutes(int minutes);
void addIrrigation();
boolean wifiRestart();
void EEPROMWriteInt(int address, int value); 
int EEPROMReadInt(int address);

int wetWait, sleep, irr, manWait, irrigation, humidity;

int lHumidity = 150;
bool onBoot = true;

int hour, minute;
boolean wifi;

void setup(){
   EEPROMWriteInt(16,EEPROMReadInt(16) + 1);
   Serial.begin(115200);
   esp8266.begin(9600);
   delay(2000);
   Serial.print(F("Inicio: "));
   Serial.println(EEPROMReadInt(16));
   if (false){
       //sleep:
       EEPROMWriteInt(0,60);
       //irr:
       EEPROMWriteInt(2,60);
       //wetWait:
       EEPROMWriteInt(4,120);
       //manWait:
       EEPROMWriteInt(6,130);
       //lHumidity:
       EEPROMWriteInt(8,150);
       //minute:
       EEPROMWriteInt(10,0);
       //hour:
       EEPROMWriteInt(12,3);
       //irrigation
       EEPROMWriteInt(14,0);
       //reboots
       EEPROMWriteInt(16,0);
       //irrigation control
       EEPROMWriteInt(18,0);
       delay(3000);
    }
    pinMode(pinwifi, OUTPUT);
    pinMode(pinpump, OUTPUT);
    pinMode(pinvalve, OUTPUT);
    pinMode(pinhum, OUTPUT);
    
    digitalWrite(pinwifi, LOW);
    digitalWrite(pinpump, HIGH);
    digitalWrite(pinvalve, HIGH);
     
    sleep = EEPROMReadInt(0);
    irr = EEPROMReadInt(2);
    wetWait = EEPROMReadInt(4);
    manWait = EEPROMReadInt(6);
    lHumidity = EEPROMReadInt(8);
    minute = EEPROMReadInt(10);
    hour = EEPROMReadInt(12);
    irrigation = EEPROMReadInt(14);
    wifi = wifiStart();
    timeAdjust();
    getLHumidity();
} 

int rh, rf = 0;
int rt = 0;
char wresp[500];
char chour[2];
char cmin[2];
boolean debug = false;

void loop()
{ 
  if (hour == -1 || hour >= 7 && hour <= 21){
     Serial.print(F("hora: "));
     Serial.print(String(hour));
     Serial.print(F(":"));
     Serial.println(String(minute));
     int irrcontrol = 0;
     int rh = readHumidity();
     if(rh < lHumidity){
         int aux = 0;
         for (int i = 1; i <= 2; i++){
             sleepMins(1);
             aux = readHumidity();
             Serial.print(F("medicao "));
             Serial.print(i);
             Serial.print(F(": "));
             Serial.println(aux);
             if (aux > rh)
                 rh = aux;
             if (aux > lHumidity)
                 break;
         }
         if(rh < lHumidity){
             irrcontrol = 1;
             Serial.print(F("umidade utilizada: "));
             Serial.println(rh);
         }
     }
     if (irrcontrol == 1){
         if (EEPROMReadInt(18) == 0){
             EEPROMWriteInt(18,1);
             irrigate(irr);
             Serial.print(F("Esperando molhar por "));
             Serial.print(String(wetWait));
             Serial.println(F(" minutos"));
             sleepMins(wetWait);
             getLHumidity();
         }
         else{
             Serial.print(F("Esperando molhar por "));
             Serial.print(String(wetWait));
             Serial.println(F(" minutos"));
             sleepMins(wetWait);
             rh = readHumidity();
         }
         EEPROMWriteInt(18,0);
     }
  Serial.println("-------------------------------------");
  }
  else{
     Serial.print(F("Fora do horario de trabalho: "));
     Serial.print(String(hour));
     Serial.print(F(":"));
     Serial.println(String(minute));
  }
Serial.print(F("Dormindo por "));
Serial.print(String(sleep));
Serial.println(F(" minuto(s)."));
sleepMins(sleep);
}

// FUNCOES

int freeRam() {
 extern int __heap_start,*__brkval;
 int v;
 return (int)&v - (__brkval == 0 ? (int)&__heap_start : (int) __brkval);  
}

boolean wifiStart(){
    delay(1000);
    Serial.println(F("Iniciando Wifi"));
    digitalWrite(pinwifi,LOW);
    delay(1000);
    Serial.print(F("Free RAM: "));
    Serial.println(freeRam());
    sendData("AT", 5000, false);
    if (strstr(wresp,"OK") == NULL){
        Serial.println(F("ERRO comando AT"));
        return false;
    }
    else
        Serial.println(F("OK comando AT"));
    delay(1000);
    sendData("AT+RST", 2000, false);
    if (strstr(wresp,"OK") == NULL){
        Serial.println(F("ERRO comando RST"));
        return false;
    }
    else
        Serial.println(F("OK comando RST"));
    delay(1000);
    sendData("AT+CWJAP=\"Casismo\",\"n@iusm3rsus\"", 5000, debug);
    if (strstr(wresp,"CONNECTED") == NULL){
        Serial.println(F("ERRO conectando AP"));
        return false;
    }
    else
        Serial.println(F("OK conexao AP"));
    delay(2000);
    sendData("AT+CIPMUX=0", 4000, debug);
    if (strstr(wresp,"OK") == NULL){
        Serial.println(F("ERRO comando CIPMUX"));
        return false;
    }
    else
        Serial.println(F("OK comando CIPMUX"));
    delay(1000);
    return true;
}

void sendData(String cmd, const int tout, boolean debug)
{
  strcpy(wresp,"");
  esp8266.println(cmd);
  long int time = millis();
  int ctl = 0;
  while ( (time + tout) > millis())
  {
    while (esp8266.available())
    {
      char c = esp8266.read();
      int lst = strlen(wresp);
      if ( ctl < 500 ){
         wresp[lst] = c;
         wresp[lst + 1] = '\0';
      }
      ++ctl;
    }
  }
  if (debug)
  {
    Serial.println(F("__"));
    Serial.println(wresp);
    Serial.println(F("__"));
  }
}

boolean timeAdjust(){
  Serial.println(F("Tentando obter a hora certa:"));
  sendData("AT+CIPSTART=\"TCP\",\"192.168.1.1\",80", 5000, debug);
  if (strstr(wresp,"OK") == NULL){
      Serial.println(F("ERRO conectando 192.168.1.1:80"));
      return false;
  }
  else
      Serial.println(F("OK conectando 192.168.1.1:80"));
  String cmd = "GET /cgi-bin/hora HTTP/1.0\r\n";
  cmd += "Host: 192.168.1.1\r\n";
  sendData("AT+CIPSEND=" + String(cmd.length() + 2), 2000, debug);
  sendData(cmd, 2000, debug);
  if (strstr(wresp,"OK") == NULL){
      Serial.println(F("ERRO CIPSEND cmd"));
      return false;
  }
  else{
      char * aux = strstr(wresp,"HORACERTA: ");
      char mhour[3] = "11";
      char mmin[3] = "22";
      if (aux != NULL){
          strncpy(mhour, aux + 11, 2); 
          mhour[2] = '\0';
          strncpy(mmin, aux + 14, 2); 
          mmin[2] = '\0';
          hourModify(atoi(mhour), atoi(mmin));
      }
  }
  delay(500);
  return true;
}

boolean getLHumidity(){
  Serial.println(F("Tentando obter a umidade minima:"));
  Serial.print(F("LHumidity atual: "));
  Serial.println(String(lHumidity));
  sendData("AT+CIPSTART=\"TCP\",\"192.168.1.1\",80", 5000, debug);
  if (strstr(wresp,"OK") == NULL){
      Serial.println(F("ERRO conectando 192.168.1.1:80"));
      return false;
  }
  else
      Serial.println(F("OK conectando 192.168.1.1:80"));
  String cmd = "GET /cgi-bin/lhumidity HTTP/1.0\r\n";
  cmd += "Host: 192.168.1.1\r\n";
  sendData("AT+CIPSEND=" + String(cmd.length() + 2), 2000, debug);
  sendData(cmd, 2000, debug);
  if (strstr(wresp,"OK") == NULL){
      Serial.println(F("ERRO CIPSEND cmd"));
      return false;
  }
  else{
      char * aux = strstr(wresp,"LHUMIDITY: ");
      char mlhumidity[4] = "100";
      if (aux != NULL){
          strncpy(mlhumidity, aux + 11, 3); 
          mlhumidity[3] = '\0';
          int lh1 = atoi(mlhumidity);
          if ( lh1 != lHumidity ){
              lHumidity = lh1;
              Serial.print(F("OK lHumidity: "));
              Serial.println(lHumidity);
              EEPROMWriteInt(8,lHumidity);
          }
          else
              Serial.println(F("OK sem alteracao"));
      }
      else
          Serial.println(F("ERRO problema na captura de lHumidity"));
  }
  delay(500);
  return true;
}

boolean sendVars(){
  Serial.println(F("Enviando variaveis"));
  sendData("AT+CIPSTART=\"TCP\",\"192.168.1.1\",80", 2000, debug);
  String cmd = "GET /cgi-bin/armazena/?";
  if (EEPROMReadInt(18) == 1)
    cmd += "waiting=ON&";
  else
    cmd += "waiting=OFF&";
  cmd += "freeRam=" + String(freeRam()) + "&";
  cmd += "lHumidity=" + String(lHumidity) + "&";
  cmd += "reboot=" + String(EEPROMReadInt(16)) + "&";
  int rf, rh = 0;
  
  rh = readHumidity();
  cmd += "humidity=" + String(rh) + "&";
  cmd += "irrigation=" + String(irrigation);
  cmd += " HTTP/1.0\r\n";
  cmd += "Host: 192.168.1.1\r\n";
  sendData("AT+CIPSEND=" + String(cmd.length() + 2), 4000, debug);
  delay(2000);
  sendData(cmd, 4000, debug);
  if (strstr(wresp,"OK") == NULL){
      Serial.println(F("ERRO CIPSEND cmd"));
      return false;
  }
  else{
      Serial.println(F("OK variveis enviadas"));
      return true;
  }
}

void hourModify(int lhour, int lmin){
  hour = lhour;
  minute = lmin;
  Serial.print(F("OK hora obtida: "));
  Serial.print(hour); 
  Serial.print(F(":")); 
  Serial.println(minute); 
}

void sleepMins(int minutes){
   for (int i = 1; i <= minutes; i++){
       delay(30000);
       delay(29000);
       Serial.print(F("Ja dormiu por "));
       Serial.print(String(i));
       Serial.println(F(" minuto(s)."));
       addMinutes(1);
       if ( i % 30 == 0)
           sendVars();
           getLHumidity();
           timeAdjust();
   }
}

boolean irrigate(int seconds){
  Serial.print(F("Irrigando por "));
  Serial.print(String(seconds));
  Serial.println(F(" segundos"));
  digitalWrite(pinvalve, LOW);
  delay(1000);
  digitalWrite(pinpump, LOW);
  for (int i = 0; i < seconds; i++){
       delay(1000);
       Serial.println(i);
  }
  digitalWrite(pinpump, HIGH);
  delay(3000);
  digitalWrite(pinvalve, HIGH);
  delay(1000);
  addIrrigation();
  return true;
}

// água pura: 0
// seco: 1000
int readHumidity(){
  // água: 200
  // seco: 0
  digitalWrite(pinhum, HIGH);
  delay(1000);
  int ret = map(analogRead(pinhumidity), 0, 1024, 200, 0);
  digitalWrite(pinhum, LOW);
  Serial.print(F("Umidade: "));
  Serial.println(ret);
  return ret;
}

void addMinutes(int minutes){
  if (minutes + minute > 59){
     if (hour + 1 > 23)
        hour = 0;
     else
        hour += 1;
     minute = minutes + minute - 60;
  }
  else
     minute = minutes + minute;
  //EEPROMWriteInt(10, minute);
  //EEPROMWriteInt(12, hour); 
  Serial.print(F("hora ajustada: "));
  Serial.print(String(hour));
  Serial.print(F(":"));
  Serial.println(String(minute));
}

void addIrrigation(){
  irrigation += 1;
  EEPROMWriteInt(14, irrigation); 
  Serial.print(F("Irrigacao armazenada: "));
  Serial.println(irrigation);
}

boolean wifiRestart(){
  Serial.println(F("pinwifi desligado"));
  digitalWrite(pinwifi,HIGH);
  delay(3000);
  return wifiStart();  
}

void EEPROMWriteInt(int address, int value){
  byte two = (value & 0xFF);
  byte one = ((value >> 8) & 0xFF);
  EEPROM.write(address, two);
  EEPROM.write(address + 1, one);
}
 
int EEPROMReadInt(int address){
  long two = EEPROM.read(address);
  long one = EEPROM.read(address + 1);
  return ((two << 0) & 0xFFFFFF) + ((one << 8) & 0xFFFFFFFF);
}
