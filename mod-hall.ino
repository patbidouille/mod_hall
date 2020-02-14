/* Mod-Hall
 * Centrale de controle et d'alarme
 * Afficheur pour clavier arrêt/marche alarme.
 * Autre fonctions :
 * - Aff Heure
 * - Aff conso eau
 * - aff conso electrique
 * - Aff de status :
 *   * Alarme
 *   *
 *   -------------
 *   Boucle principale :
 *   - lecture heure, aff
 *   - lecture capteur, affichage
 *   Si TSPoint -> suivant état
 *   - lit code marche/arrêt alarme
 *   - touche menu
 *   -> action en fonction du choix.
 *   retour boucle principale
 *   
 *   Resolution  480*320 (Pixel) 
 */

/*=============================*/
/*______Import Libraries_______*/
#include <stdint.h>
#include <Adafruit_GFX.h>   // Core graphics library
#include <MCUFRIEND_kbv.h>  // Hardware-specific library
#include <TouchScreen.h>    // touch control
#include <HRCSwitch.h>
#include <RTClib.h>         // RTC Horloge

#include <WiFiEspClient.h>
#include <WiFiEsp.h>
#include <WiFiEspUdp.h>
#include "SoftwareSerial.h"

#include <ESP8266WiFi.h>    // wifi
#include <PubSubClient.h>   //mqtt
#include <EEPROM.h>         //
#include <ArduinoJson.h>    // converti en JSON

/*______End of Libraries_______*/

// Validation des Libraries, Création objet
MCUFRIEND_kbv tft;
RTC_DS1307 rtc;
HRCSwitch mySwitch = HRCSwitch();
WiFiClient espClient;
PubSubClient client(espClient);

// Fonts
#include <Fonts/FreeSans9pt7b.h>
#include <Fonts/FreeSans12pt7b.h>
#include <Fonts/FreeSerif12pt7b.h>

#include <FreeDefaultFonts.h>

#define YP A2  // must be an analog pin, use "An" notation!
#define XM A3  // must be an analog pin, use "An" notation!
#define YM 8   // can be a digital pin
#define XP 9   // can be a digital pin

#define LCD_CS A3 // Chip Select goes to Analog 3
#define LCD_CD A2 // Command/Data goes to Analog 2
#define LCD_WR A1 // LCD Write goes to Analog 1
#define LCD_RD A0 // LCD Read goes to Analog 0
#define LCD_RESET A4 // Can alternately just connect to Arduino's reset pin

// For better pressure precision, we need to know the resistance
// between X+ and X- Use any multimeter to read it
// For the one we're using, its 300 ohms across the X plate
TouchScreen ts = TouchScreen(XP, YP, XM, YM, 300);

/*______Assign names to colors and pressure_______*/
#define BLACK   0x0000
#define BLUE    0x001F
#define RED     0xF800
#define GREEN   0x07E0
#define CYAN    0x07FF
#define MAGENTA 0xF81F
#define YELLOW  0xFFE0
#define WHITE   0xFFFF
#define GRAY    0x8410
#define MINPRESSURE 10
#define MAXPRESSURE 1000
/*_______Assigned______*/

/*____Calibrate TFT LCD_____*/
#define TS_MINX 125
#define TS_MINY 85
#define TS_MAXX 965
#define TS_MAXY 905
/*______End of Calibration______*/

/*__________Code RF____________*/
#define RX_PIN      18  // The receiver pin.
#define Con         1361  // Allume vitrine    
#define Coff        1364  // eteint vitrine
#define pir_hall    5592405  //pir hall
#define pir_couloir 5264467 //pir couloir
//#define Acouloir    5558768   // Commande d'allumage du couloir
#define ctc_porte   13460842 //5510992  //contact porte d'entrée.
#define bp_vit      70960    // Bp pour allumer/eteindre vitrine
/*_______End Code RF____________*/

#define tempsexec   10000 // Temps d'exécution 20s.
#define tempscoul   4500  // Temps de présence dans le couloir
#define bouton      9 // pin du bouton b0
#define LEDR        13  // LED used to blink when volume too high
#define LEDV        12  // LED used to blink when volume too high
#define lum_ambiante 920  // Lumière ambiante -> 1000 si plus sombre
#define lum_allume   750  // Lumiére allumée -> 0 si plus lumineux
#define relais1   8   //relais


/*__________VARIABLE___________*/
String symbol[4][4] = {
  { "7", "8", "9", "^" },
  { "4", "5", "6", "<" },
  { "1", "2", "3", ">" },
  { "C", "0", "OK", "v" }
};

const char *aspectname[] = {
        "PORTRAIT", "LANDSCAPE", "PORTRAIT_REV", "LANDSCAPE_REV"
};
const int tempsSortie = 5000;
const char* ssid1 = "skycontrol";
const char* ssid2 = "skycontrol";
const char* password = "fuck your mother";
const char* mqtt_server = "10.0.0.7";
const char* mqttUser = "mod";
const char* mqttPassword = "Plaqpsmdp";
const char* svrtopic = "domoticz/in";
const char* topic_Domoticz_OUT = "domoticz/out";
//const char *colorname[] = { "BLUE", "GREEN", "RED", "GRAY" };
//uint16_t colormask[] = { 0x001F, 0x07E0, 0xF800, 0xFFFF };

boolean porte = false;  // false si la porte n'a pas été ouverte.
boolean hall = false;   // false si le pir du hall n'a pas été activé.
boolean couloir = false;  // false si pir du couloir n'a pas été activé.
boolean bp = false;     // false si bp non actif
boolean bp_flag = false;  // inverse la commande à l'appuie du bp
boolean RF = false;     // Si Rf présente
boolean vitrine = false;  //flag pour ne pas allumer plusieurs fois la vitrine
boolean avitrine = false;  //commande pour allumer la vitrine true=on allume, false=on eteint.
boolean presenceH = false;  // flag pour déterminer la présence dans le hall
boolean presenceC = false;  // flag pour déterminer la présence dans le couloir
boolean hallP = false;  // flag pour valider une détection du pir hall
boolean tallume = false; // flag pour ne pas remettre le temps à 0
boolean entree = false;  // pour dire que l'on sort.
boolean result = false;
boolean alarme, alarmeActif, alarmeBP = false;  // Flag pour l'alarme
boolean debug = true;                           // Flag pour aff sur la liaiason série les infos
boolean clavier = false;                        // Flag pour dire qu'on est en mode clavier.

StaticJsonBuffer<300> jsonBuffer;
JsonObject& JSONencoder = jsonBuffer.createObject();
String messageReceived="";
char msg[100];                        // Buffer pour envoyer message mqtt
uint16_t dx, rgb, n, wid, ht, msglin;
int X,Y;                              // position appuie
int Number;                           // retour clavier
//int cpt=0;                          // compteur
String code;                          // code de retour clavier
char action;
uint8_t hh, mm, ss, dd, ms;           //contient heure, min, sec ,jour, mois
int16_t yy;                           // année
unsigned long value = 0;              //retour RF
long temps = 0;                       // Temps d'exécution
long tempsP = 0;                      // Temps de présence
long tempsA = 0;                      // Temps avant d'allumer
 
//======== Fonction SETUP ================================================================
void setup(void)
{
    Serial.begin(9600);
    uint16_t ID = tft.readID();
    if (ID == 0xD3D3) ID = 0x9481;        //force ID if write-only display
    //ID=0x9486;
    if (debug) {
      Serial.println("Mod-Hall");
      Serial.print("found ID = 0x");
      Serial.println(ID, HEX);
    }
    
    tft.begin(ID);                        // init du tft
    tft.setRotation(0);                   // On se met en pysage
    tft.fillScreen(BLACK);                // On met un fond noir
    wid = tft.width();                    // On enregistre la taille de l'écran en pixel.
    ht = tft.height();  

    if (! rtc.begin()) {                  // init du RTC
      Serial.println("impossible de trouver le RTC");
      while (1);                          // boucle infinie
    }
    if (! rtc.isrunning()) {              // test si le RTC est en route.
      Serial.println("Le RTC n'est pas activé!");
    // to set
    // January 21, 2014 at 3am you would call:
    // rtc.adjust(DateTime(2014, 1, 21, 3, 0, 0));
    }

    mySwitch.enableReceive(5);            // Receiver on inerrupt 5 => that is pin #18
    mySwitch.enableTransmit(46);          // Using Pin #46 PWM,OC5A
    setup_wifi();
    client.setServer(mqtt_server, 1883);
    client.setCallback(callback);
    client.subscribe("#");
    EEPROM.begin(512);
    

    //IntroScreen();
    //delay(1000);
  
    Affstd();                             // Affiche un écran de base
    Aff_heure();                          // aff heure, efface l'ancienne (clignotement)
    Aff_date();                           // aff date, efface l'ancienne
    
    if (debug) {
      Serial.print(tft.width());
      Serial.print(F(" x "));
      Serial.println(tft.height());
    }
    
    //draw_BoxNButtons();
    
}

//========================================================================================
//========================================================================================
/* Boucle principale
 *  Aff écran de base
 *  test appui -> alarme ou menu
 *  test RF -> action, comm MQTT
 *  ----
 *  Alarme :
 *  Aff alarme en route
 *  Attend 2min
 *  test capteur -> désactive alarme, déclenche alarme
 *  ----
 *  Désactive alarme :
 *  dans les 2min faire - sinon déclenche alarme !
 *  aff clavier
 *  code ok - retour boucle principale
 *  ----
 *  déclenche alarme :
 *  sirène on durant 2 min
 *  ----
 *  action :
 *  Envoi info en MQTT
 *  traite RF
 *  - si porte plus pir allume vitrine
 *  - ...
 *  
 */
void loop()
{
    X=Y=0;
    if (!clavier) {
      Aff_heure();
      //Aff_date();
    }

    // Test d'appui sur l'écran, renvoi les coordonnées
    TSPoint p = ts.getPoint();
    // nécessaire pour revalider les sortie pour l'aff
    pinMode(YP, OUTPUT);                    //restore the TFT control pins
    pinMode(XM, OUTPUT);
    //Serial.print("z="); Serial.println(p.z);
    if ((p.z > MINPRESSURE ) && (p.z < MAXPRESSURE)) {
      Serial.println("clavier");
      p.x = map(p.x, TS_MINX, TS_MAXX, 0, 320);
      p.y = map(p.y, TS_MINY, TS_MAXY, 0, 240);
      Y = p.y; X = p.x;
    }

    DetectAlarmeBP();

    if (alarmeBP) {                     // activation de l'alarme
      if (!alarmeActif) {
        alarmeActif=true;             // On active l'alarme
        Affstd();                     // On affiche "Alarme en route"
        Aff_date();                   // On remet la date, le temps de sortie est dans la fonction
        
        alarmeBP=false;
      }  
    }
  
    desactiveAlarme();
    
    // test des capteurs - action
    temps = 0;
    rfEvent();
    if (RF) {
      traiteRF();
    }
    
    // test de connection, sinon reconnecte
    if (!client.connected()) {
      reconnect();
    }
    client.loop();
    
 
    // affiche message reçu en MQTT
    if ( mess ) {
      Serial.println("MESSAGE MQTT");
    }
    delay(200);
           
    if (debug) {
      Serial.println("Lecture du capteur");  
      /*Serial.print("X="); Serial.print(X);
      Serial.print(", Y="); Serial.println(Y);    
      Serial.print("alarmeBP="); Serial.println(alarmeBP);
      Serial.print("alarmeActif="); Serial.println(alarmeActif);*/
    }
}
//------- Fin du loop -----------

//========================================================================================
/* Connection au wifi
- test le premier ssid
- sinon passe au seconds
*/
void setup_wifi() {
 
  int cpt = 0;
  boolean ssid = true;
  delay(10);
  int ss = 1;
 
  // We start by connecting to a WiFi network
  while (WiFi.status() != WL_CONNECTED) {
    if (ssid) {
      WiFi.begin(ssid1, password);
    }
    else {
      ss = 2;
      WiFi.begin(ssid2, password);
    }
    if (debug) {
        Serial.println();
        Serial.print("Connecting to ");
        if (ss == 1) {
          Serial.println(ssid1);
        } else {
          Serial.println(ssid2);
        }
    }
 
    int counter = 0;
    while ((WiFi.status() != WL_CONNECTED) && (cpt <= 20)) {
      delay(500);
      Serial.print(".");
      counter++;
      cpt=cpt+1;
    }
 
    if (cpt >= 20) {
      if (ssid) {
        ssid=false;
      } else {
        ssid=true;
      }
    }
  }

  // on définit l'ip
  // WiFi.config(ip, gateway, subnet);
   
  // SINON On récupère et on prépare le buffer contenant l'adresse IP attibué à l'ESP-01
  IPAddress ip = WiFi.localIP();   
/*  String ipStr = String(ip[0]) + '.' + String(ip[1]) + '.' + String(ip[2]) + '.' + String(ip[3]);
  ipStr.toCharArray(buffer, 20);
*/

  if ( debug ) {
    Serial.println("");
    Serial.println("WiFi connected");
    Serial.print("IP address: ");
    Serial.println(WiFi.localIP());
 
    Serial.print("Connecting to ");
    Serial.println(mqtt_server);
  }
 
}

//========================================================================================
/* tente une reconnection au seveur mqtt */
void reconnect() {
  // Loop until we're reconnected
  int counter = 0;
  int compt = 0;
  boolean noconnect = true;
  // tant qu'il ne trouve pas un serveur affiche rond
  while ((!client.connected()) && (noconnect == true)) {
    counter++;
    delay(500);
 
    if (debug ) {
      Serial.print("Attempting MQTT connection...");
    }
    // Attempt to connect
    if (client.connect("ESP8266Client", mqttUser, mqttPassword )) {
      if (debug ) {
        Serial.println("connected");
      }
      // Once connected, publish an announcement...
      client.publish("mod_lum", "hello world");
      // ... and resubscribe
      client.subscribe("#");
      noconnect=true;
      digitalWrite(lbp,HIGH);
      delay(10000);
      digitalWrite(lbp,LOW);
    } else {
      if ( debug ) {
        Serial.print("failed, rc=");
        Serial.print(client.state());
        Serial.println(" try again in 5 seconds");
      }  
      // Wait 5 seconds before retrying
      if (compt <= 3) {
        delay(3000);
        counter=0;
        compt++;
      } else {
        noconnect=false;
      }
    }
 
  }
  delay(1500);
}
 
//========================================================================================
// Déclenche les actions à la réception d'un message
// D'après http://m2mio.tumblr.com/post/30048662088/a-simple-example-arduino-mqtt-m2mio
void callback(char* topic, byte* payload, unsigned int length) {
 
  // pinMode(lbp,OUTPUT);
  int i = 0;
  if ( debug ) {
    Serial.println("Message recu =>  topic: " + String(topic));
    Serial.print(" | longueur: " + String(length,DEC));
  }
  sujet = String(topic);
  // create character buffer with ending null terminator (string)
  for(i=0; i<length; i++) {
    message_buff[i] = payload[i];
    messageReceived+=((char)payload[i]);
  }
  message_buff[i] = '\0';
 
  String msgString = String(message_buff);
  mesg = msgString;
  if ( debug ) {
    Serial.println(" Payload: " + msgString);
  }
  mess = true;
 
}

//========================================================================================
void desactiveAlarme() {                // test code - desactive si OK
  if ((alarmeActif) && (alarmeBP)) {    // routine de désactivation de l'alarme  
      Serial.println("TOUCH");
      draw_BoxNButtons();
      temps=millis();
      clavier=true;
      DetectCMD();
      if (code=="4003") {
        alarmeActif=false;     
        alarmeBP=false;
        clavier=false;
        Affstd();
        Aff_heure;
        Aff_date();
      } else if (millis()-temps<10000) {
        affcodefaux();
      }
  }
}

//========================================================================================
void AlarmeON() {                       // Traitement alarme après détection
  alarme=true;
  temps=millis();
  /* A FAIRE
   * Envoie MQTT à centrale qui allume toutes les lumières
   * déclenche sirène
  */
  
  while ((alarmeActif) || (temps-millis() < 60000)) {   //test pour désactiver alarme
    DetectAlarmeBP();
    desactiveAlarme();
  }
  /* A FAIRE
   *  Envoi fin alarme en MQTT à la central
   *  éteint alarme
   */
    
}

//========================================================================================
void traiteRF() {

  if (debug) {
    Serial.print("Reçu ");
    Serial.println(value); 
    Serial.print("hall ");
    Serial.println(hall); 
  }

  if (alarmeActif) {
    if ((porte) || (hall) || (couloir)) {   // Détection sous alarme !
      AlarmeON();
    }
  }
    if ((bp) || (avitrine)) { 
      if (!bp_flag) {
        bp_flag = true;            // Si appui sur BP et pas allumé, on allume la vitrine
        mySwitch.send(Con, 24);       // On allume
        Serial.println("ALLUME VITRINE");
        vitrine = true;
      } else {
        bp_flag = false;            // Si appui sur BP et allumé, on éteins la vitrine
        mySwitch.send(Coff, 24);       // On eteint
        Serial.println("ETEINT VITRINE");
        vitrine = false;
      } 
      mySwitch.resetAvailable();
    }
    
    if (!avitrine) {
       mySwitch.send(Coff, 24);       // On eteint
       Serial.println("ETEINT VITRINE");
       vitrine = false;
    }  
    
    if ((porte) && (!vitrine) ) {           // test si contact de porte et vitrine off
      entree = true;                        // oui on valide le flag
      tempsP = millis();                    // On stock le temps actuel pour voir si on est dans les délais
      mySwitch.send(Con, 24);               // On allume
      Serial.println("ALLUME VITRINE");
      delay(1000);                          // Delais pour fermer la porte.
      mySwitch.resetAvailable();
    }
    
    if ((hall) && (!vitrine)) {      // Si on est dans le hall sans la vitrine allumé
      //hallP = true;
      if (!presenceH) {              // On active le temps de présence min         
        presenceH = true;
        tempsP=millis(); 
        if (!tallume) {               // On active le temps d'attente devant la vitrine
          tallume = true;
          tempsA=millis();
        }
        mySwitch.resetAvailable();
      hall=false;
      }
      mySwitch.resetAvailable();
    }
  
  if (presenceH) {                      // Si on et en detection de presence
    if ((tallume) && (millis() - tempsA > 50000)) {    // On vérifie que c'est juste la presence dans le hall
      // Si le temps de presence dépasse les 50sec on allume
      mySwitch.send(Con, 24);           // On allume
      Serial.println("ALLUME VITRINE");
      //vitrine = true;                   // pour éviter de remmettre les temps à 0
      //tallume = false;                  // On passe en mode présence 
      tempsA = 0;     
    }
    if ((hall) && (millis() - tempsP < tempsexec)) {    // hall avant les 20s de passé ?
      Serial.print("temps passé :");
      Serial.println(millis() - tempsP);
      tempsP=millis();    // oui on réinitialise le temps
    } 
    if ((!hall) && (millis() - tempsP > tempsexec)) {
      Serial.println(millis() - tempsP);
      presenceH = false;                 // Sinon y a plus personne
      tallume = false;
      tempsP = 0;
      mySwitch.send(Coff, 24);      // On éteint
      Serial.println("ETEINT vitrine");
      //vitrine = false;
      if (avitrine) avitrine = false;   // on demande l'extinction
    }
  hall=false;
  }  

}

//========================================================================================
void rfEvent() {             // Test si nouveau signal RF
  porte=false;
  hall=false;
  couloir=false;
  bp=false;
  value="";

  if (mySwitch.available()) {
    value = mySwitch.getReceivedValue();
    RF = true;          // J'ai une valeur
    switch (value) {
    case ctc_porte: 
      porte = true;
    break;
    case pir_hall:
      hall = true;
    break;
    case pir_couloir:
      couloir = true;
    break;
    case Con: 
      avitrine = true;
    break;
    case Coff: 
      avitrine = false;
    break;
    case bp_vit: 
      bp = true;
    break;
    } 
    // Affichage du code sur l'écran
    tft.fillRect(0,455 ,120 ,200 , RED);
    tft.setCursor(10, 460);
    tft.setTextSize(2);
    tft.print(value);
    delay(500);
    tft.fillRect(0,455 ,120 ,200 , BLACK);
  } else {
    RF = false;
  }
  
  if (debug) {
    Serial.print("reçu="); Serial.println(value);
  }

} // Fin du void

//========================================================================================
bool ButtonCheck(TSPoint p)
{
  p = ts.getPoint(); 
  pinMode(YP, OUTPUT);  //restore the TFT control pins
  pinMode(XM, OUTPUT);
  if ((p.z > MINPRESSURE ) && (p.z < MAXPRESSURE)) {
    //p.x = map(p.x, TS_MINX, TS_MAXX, 0, 320);
    //p.y = map(p.y, TS_MINY, TS_MAXY, 0, 240);
    return true;
  }
  return false;
}

//========================================================================================
void affcodefaux() {
  Serial.println("CODE FAUX");
  tft.fillRect(0,0,wid,160,GRAY);
  tft.setTextSize(5);
  tft.setTextColor(RED);
  tft.setCursor(20, 50);
  tft.print("CODE FAUX");
  delay(600);
}

TSPoint waitTouch() {
  TSPoint p;
  do {
    p = ts.getPoint(); 
    pinMode(XM, OUTPUT);
    pinMode(YP, OUTPUT);
  } while((p.z < MINPRESSURE ) || (p.z > MAXPRESSURE));
  p.x = map(p.x, TS_MINX, TS_MAXX, 0, 320);
  p.y = map(p.y, TS_MINY, TS_MAXY, 0, 240);;
  return p;
  /* ATTENTION !
   *  Bug dans la conf, elle est défini pour un écran 320x240
   *  le mien fait 480x320 mais j'avais la flemme de recalculer la détection 
   */
}

//========================================================================================
// Detecte la commande du clavier, retourne un code si OK
void DetectCMD() {
  code="";
  action='1';
  Number=200;
  X=Y=0;
   
  while ((action=='1') || (Number<=13)) {
    TSPoint p = waitTouch();
    Y = p.y; X = p.x;
    DetectButtons();
    if (Number<=9) {
      code.concat(Number);                      // On agrége le Number de la topuche au code
      tft.fillRect(0,0,wid,160,GRAY);           // On affiche le code  tapé
      tft.setTextSize(5);
      tft.setTextColor(RED);
      tft.setCursor(50, 50);
      tft.print(code);
    }
  //  Serial.print(Number); Serial.print(">"); Serial.println(code);    
  }
 
  if (action=="C") {
    Number=200;
    code="";
  }
}

//========================================================================================
// si on a appuyé sur le bouton alarme -true.      
void DetectAlarmeBP() {
  if (X<50 && X>0) {
    alarmeBP=true;
  }

}

//========================================================================================
// Affiche l'écran de base
void Affstd() {
  tft.fillScreen(BLACK);                    // On efface tout
  if (alarmeBP) {
    tft.fillRect(0,0 ,ht ,100 , RED);       // aff le bouton rouge si alarme
    tft.setTextColor(WHITE);
  } else {
    tft.fillRect(0,0 ,ht ,100 , GREEN);     // sinon vert
    tft.setTextColor(RED);
  }
  tft.drawFastHLine(0, 0, 320, WHITE);
  tft.drawFastHLine(0, 100, 320, WHITE);
  tft.drawFastVLine(0, 0, 100, WHITE);
  tft.drawFastVLine(319, 0, 100, WHITE);
  tft.setTextSize(3);
  //tft.setCursor(75, 30);
  tft.setCursor(25, 40);
  if  (alarmeBP) {
    tft.print("ALARME en route");
  } else {
    tft.print("Mettre ALARME !");
  }
  tft.drawFastHLine(0, 260, 320, WHITE);
  if (alarmeBP) {
    tft.fillRect(0,262 ,ht ,220 , RED);
    tft.setCursor(80, 300);
    tft.setTextSize(2);
    tft.print("ATTENTION !");
    tft.setCursor(25, 330);
    tft.print("vous avez deux minutes");
    tft.setCursor(25, 360);
    tft.print("pour sortir !");
    delay(tempsSortie);                      // On attends le delais d'alarme.
    tft.fillRect(0,262 ,ht ,220 , BLACK);
  }
}

//========================================================================================
void Aff_heure() {
  char daysOfTheWeek[7][12] = {"Dimanche", "Lundi", "Mardi", "Mercredi", "Jeudi", "Vendredi", "Samadi"};
  DateTime now = rtc.now();
  
  //buffer can be defined using following combinations:
  //hh - the hour with a leading zero (00 to 23) 
  //mm - the minute with a leading zero (00 to 59)
  //ss - the whole second with a leading zero where applicable (00 to 59)
  //YYYY - the year as four digit number
  //YY - the year as two digit number (00-99)
  //MM - the month as number with a leading zero (01-12)
  //MMM - the abbreviated English month name ('Jan' to 'Dec')
  //DD - the day as number with a leading zero (01 to 31)
  //DDD - the abbreviated English day name ('Mon' to 'Sun')
  
  char dat[] = "DD-MM-YYYY";
  char heure[] = "hh:mm:ss";
  
  if (debug)  {
    //Serial.println(now.toString(dat));
    Serial.println(now.toString(heure));
  }
   
  // Effacement de l'ancienne heure, puis on aff la nouvelle
  tft.fillRect(0, 120 , 320, 45, BLACK);
  tft.setTextSize(5);
  tft.setTextColor(GREEN);
  tft.setCursor(42, 127);
  tft.print(heure);
  delay(200);
      
}

//========================================================================================
// Affiche le jour et la date sous l'heure
void Aff_date() {
  char daysOfTheWeek[7][12] = {"Dimanche", "Lundi", "Mardi", "Mercredi", "Jeudi", "Vendredi", "Samadi"};
  DateTime now = rtc.now(); 
  char dat[] = "DD-MM-YYYY";
  Serial.println(now.toString(dat));
  tft.fillRect(0, 170 , 320, 55, BLACK);
  tft.setTextSize(3);
  tft.setTextColor(GRAY);
  tft.setCursor(88, 172);
  tft.print(daysOfTheWeek[now.dayOfTheWeek()]);
  tft.setCursor(65, 200);
  tft.print(dat);
  delay(200);
      
}

//========================================================================================
void showmsgXY(int x, int y, int sz, const GFXfont *f, const char *msg)
{
//    int16_t x1, y1;
//    uint16_t wid, ht;
    //tft.drawFastHLine(0, y, tft.width(), WHITE);
    tft.setFont(f);
    tft.setCursor(x, y);
    tft.setTextColor(GREEN);
    tft.setTextSize(sz);
    tft.print(msg);
    delay(1000);
}

//========================================================================================
void DetectButtons() {
  
  //Number="";
  action='1';
  
  if (Y<243 && Y>184) //Detecting Buttons on Column 1
  {
    //Serial.println("colonne 1");
    if (X>256 && X<313) //If cancel Button is pressed
    {//Serial.println ("Button Cancel"); 
    action='C'; Number=14; result=false;}
    
     if (X>200 && X<255) //If Button 1 is pressed
    {//Serial.println ("Button 1"); 
    action='1'; Number=1; }
    
     if (X>146 && X<199) //If Button 4 is pressed
    {//Serial.println ("Button 4"); 
    action='1'; Number=4; }
    
     if (X>89 && X<145) //If Button 7 is pressed
    {//Serial.println ("Button 7"); 
    Number=7; } 
  }

    if (Y<184 && Y>123) //Detecting Buttons on Column 2
  {
    if (X>256 && X<313) //If cancel Button is pressed
    {//Serial.println ("Button 0"); 
    Number=0; }
    
     if (X>200 && X<255) //If Button 1 is pressed
    {//Serial.println ("Button 2"); 
    Number=2; }
    
     if (X>146 && X<199) //If Button 4 is pressed
    {//Serial.println ("Button 5"); 
    Number=5; }
    
     if (X>89 && X<145) //If Button 7 is pressed
    {//Serial.println ("Button 8"); 
    Number=8; } 
  }

    if (Y<121 && Y>63) //Detecting Buttons on Column 3
  {
    if (X>256 && X<313) //If cancel Button is pressed
    {//Serial.println ("Button OK"); 
    action='O'; Number=15; result=true;}
    
     if (X>200 && X<255) //If Button 1 is pressed
    {//Serial.println ("Button 3"); 
    Number=3; }
    
     if (X>146 && X<199) //If Button 4 is pressed
    {//Serial.println ("Button 6"); 
    Number=6; }
    
     if (X>89 && X<145) //If Button 7 is pressed
    {//Serial.println ("Button 9"); 
    Number=9; }   
    
  }

    if (Y<64 && Y>0) //Detecting Buttons on Column 4
  {
//    Num1 = Number;    
    Number =0;
    tft.setCursor(200, 20);
    tft.setTextColor(RED);
    if (X>256 && X<313)
    {//Serial.println ("Bas"); 
      action = 'b'; Number=10; tft.println('v');}
     if (X>200 && X<255)
    {//Serial.println ("Droite"); 
      action = 'd'; Number=11; tft.println('>');}
     if (X>146 && X<199)
    {//Serial.println ("Gauche"); 
      action = 'g'; Number=12; tft.println('<');}
     if (X>89 && X<145)
    {//Serial.println ("Haut"); 
      action = 'h'; Number=13; tft.println('^');}  
  } 
  if (debug) {
    Serial.println ("Detection bouton");
    Serial.print("X="); Serial.print(X);
    Serial.print(", Y="); Serial.println(Y);
    Serial.print(", Button="); Serial.println(Number);
    Serial.print(", Action="); Serial.println(action);
  }
  delay(300); 
}

//========================================================================================
void IntroScreen()
{
/*  
 *   Test de dégradé - loupé !
 uint8_t aspect;
  aspect = "PORTRAIT";
  tft.setRotation(0);
//  wid = tft.width();
//  ht = tft.height();
  dx = wid / 64;
  for (n = 0; n < 64; n++) {
    rgb = n * 8;
    rgb = tft.color565(rgb, rgb, rgb);
    tft.fillRect(n * dx, 0, dx, ht, rgb & colormask[aspect]);
  }
  */
  tft.setTextColor(WHITE);
  tft.setCursor (65, 120);
  tft.setTextSize (3);
  tft.setTextColor(RED);
  //tft.println("Clavier numérique");
  tft.setCursor (40, 160);
  tft.println("Bienvenue");
  tft.setCursor (40, 220);
  tft.setTextSize (2);
  tft.setTextColor(BLUE);
  tft.println("Patbidouille");
  delay(1800);
}

//========================================================================================
void draw_BoxNButtons()
{
  // On met tous en BLACK
  tft.fillScreen(BLACK);
  
  //Draw the Result Box
  tft.fillRect(0,0,wid,160,GRAY);

 //Draw First Column
  tft.fillRect  (0,400,80,80,RED);
  /*tft.fillRect  (0,400,80,80,BLACK);
  tft.fillRect  (0,320,80,80,BLACK);
  tft.fillRect  (0,240,80,80,BLACK);*/

 //Draw Third Column  
  tft.fillRect  (160,400,80,80,GREEN);
  /*tft.fillRect  (160,400,80,80,RED);
  tft.fillRect  (160,320,80,80,GRAY);
  tft.fillRect  (160,240,80,80,BLACK);*/

  //Draw Secound & Fourth Column  
  for (int b=480; b>=160; b-=80)
 { tft.fillRect  (240,b,80,80,BLUE); 
   tft.fillRect  (80,b,80,80,BLACK);}

  //Draw Horizontal Lines
  for (int h=160; h<=400; h+=80)
  tft.drawFastHLine(0, h, 320, WHITE);
  tft.drawFastVLine(0, 479, 320, WHITE);

  //Draw Vertical Lines
  for (int v=0; v<=240; v+=80)
  tft.drawFastVLine(v, 160, 320, WHITE);
  tft.drawFastVLine(239, 160, 320, WHITE);

  //Display keypad lables 
  for (int j=0;j<4;j++) {
    for (int i=0;i<4;i++) {
      tft.setCursor(28 + (80*i), 187 + (80*j));
      tft.setTextSize(3);
      tft.setTextColor(WHITE);
      tft.println(symbol[j][i]);
    }
  }
}
