#include <FS.h>                   //this needs to be first, or it all crashes and burns...

#include <ESP8266WiFi.h>          //https://github.com/esp8266/Arduino
//needed for library
#include <DNSServer.h>
#include <ESP8266WebServer.h>
#include <WiFiManager.h>          //https://github.com/tzapu/WiFiManager
#include <ArduinoJson.h>          //https://github.com/bblanchon/ArduinoJson
#include <PubSubClient.h>
#include <WiFiClient.h>


#ifndef UNIT_TEST
#include <Arduino.h>
#endif
#include <IRremoteESP8266.h>
#include <IRrecv.h>
#include <IRsend.h>
#include <IRutils.h>
#if DECODE_AC
#include <ir_Daikin.h>
#include <ir_Fujitsu.h>
#include <ir_Gree.h>
#include <ir_Haier.h>
#include <ir_Kelvinator.h>
#include <ir_Midea.h>
#include <ir_Toshiba.h>
#endif  // DECODE_AC

#define RECV_PIN 2
#define IR_LED 3  // ESP8266 GPIO pin to use. Recommended: 4 (D2).
IRsend irsend(IR_LED);  // Set the GPIO to be used to sending the message.
#define BAUD_RATE 115200
#define CAPTURE_BUFFER_SIZE 1024

#if DECODE_AC
#define TIMEOUT 50U  // Some A/C units have gaps in their protocols of ~40ms.
                     // e.g. Kelvinator
                     // A value this large may swallow repeats of some protocols
#else  // DECODE_AC
#define TIMEOUT 15U  // Suits most messages, while not swallowing many repeats.
#endif  // DECODE_AC
#define MIN_UNKNOWN_SIZE 12
// ==================== end of TUNEABLE PARAMETERS ====================


// Use turn on the save buffer feature for more complete capture coverage.
IRrecv irrecv(RECV_PIN, CAPTURE_BUFFER_SIZE, TIMEOUT, true);

decode_results results;  // Somewhere to store the results


ESP8266WebServer server(80);
//flag for saving data
bool shouldSaveConfig = false;


uint64_t Code;
char Codigo=20;
long humiditySetPoint = 38;
WiFiClient espClient;


//Check if header is present and correct
bool is_authentified(){
  Serial.println("Enter is_authentified");
  if (server.hasHeader("Cookie")){   
    Serial.print("Found cookie: ");
    String cookie = server.header("Cookie");
    Serial.println(cookie);
    if (cookie.indexOf("ESPSESSIONID=1") != -1) {
      Serial.println("Authentification Successful");
      return true;
    }
  }
  Serial.println("Authentification Failed");
  return false;  
}

//login page, also called for disconnect
void handleLogin(){
  String msg;
  if (server.hasHeader("Cookie")){   
    Serial.print("Found cookie: ");
    String cookie = server.header("Cookie");
    Serial.println(cookie);
  }
  String datos;
  if (server.hasArg("DISCONNECT")){
    Serial.println("Disconnection");
    String header = "HTTP/1.1 301 OK\r\nSet-Cookie: ESPSESSIONID=0\r\nLocation: /login\r\nCache-Control: no-cache\r\n\r\n";
    server.sendContent(header);
    return;
  }
  
  if (server.hasArg("USERNAME") && server.hasArg("PASSWORD")){
    if (server.arg("USERNAME") == "admin" &&  server.arg("PASSWORD") == "admin" ){
      String header = "HTTP/1.1 301 OK\r\nSet-Cookie: ESPSESSIONID=1\r\nLocation: /config\r\nCache-Control: no-cache\r\n\r\n";
      server.sendContent(header);
      Serial.println("Log in Successful");
      return;
    }
    
    datos=server.arg("USERNAME");
    msg = "Wrong username/password! try again.";
    Serial.println("Log in Failed");
  }  
  
  String content = "<IMG align='left' src='https://static.wixstatic.com/media/fb76f2_1477c542ccc24b95b393959a3130f403%7Emv2.png/v1/fill/w_32%2Ch_32%2Clg_1%2Cusm_0.66_1.00_0.01/fb76f2_1477c542ccc24b95b393959a3130f403%7Emv2.png' alt='logo'><H1 align='center'><html><body><form action='/login' method='POST'>DiElec Ingenieria Configurador MQTT</H1><br>";
  content += "<H4 align='center'>Usuario:<input type='text' name='USERNAME' placeholder='nombre de usuario'></H4><br>";
  content += "<H4 align='center'>Contrasena:<input type='password' name='PASSWORD' placeholder='contraseña'></H4><br>";
  content += "<H3 align='center'><input type='submit' name='SUBMIT' value='Submit'></form>" + msg + "</H3><br>";
  content += "You also can go <a href='/inline'>here</a></body></html><br>";
  content += "Setpoint: " + (String) humiditySetPoint + "%";
  
  //content += "<script>function nn(){x= document.getElementById('campo1').value;}</script>";
  server.send(200, "text/html", content);
}

//login page, also called for disconnect
void handleConfig(){
  
}

//root page can be accessed only if authentification is ok
void handleRoot(){
  
  Serial.println("Enter handleRoot");
  
  String header;
  if (!is_authentified()){
    String header = "HTTP/1.1 301 OK\r\nLocation: /login\r\nCache-Control: no-cache\r\n\r\n";
    server.sendContent(header);
    return;
  }
  String content = "<html><body><H2>hello, you successfully connected to esp8266!</H2><br>";
  if (server.hasHeader("User-Agent")){
    content += "the user agent used is : " + server.header("User-Agent") + "<br><br>";
  }
  content += "You can access this page until you <a href=\"/login?DISCONNECT=YES\">disconnect</a></body></html>";
  server.send(200, "text/html", content);
}


//no need authentification
void handleNotFound(){
  
  String message = "File Not Found\n\n";

  message += "URI: ";
  message += server.uri();
  message += "\nMethod: ";
  message += (server.method() == HTTP_GET)?"GET":"POST";
  message += "\nArguments: ";
  message += server.args();
  message += "\n";
  for (uint8_t i=0; i<server.args(); i++){
    message += " " + server.argName(i) + ": " + server.arg(i) + "\n";
  }
  server.send(404, "text/plain", message);
}

void on_rele() {
  server.send(200, "text/plain", "OK ON");
}

void off_rele() {
  server.send(200, "text/plain", "OK OFF");
  Serial.println("Apago desde Server");  
}

void reset(){
  server.send(200, "text/plain", "Resetting WLAN and restarting..." );
  WiFiManager wifiManager;
  wifiManager.resetSettings();
  ESP.restart();
  SPIFFS.format();
}
//callback notifying us of the need to save config
void saveConfigCallback () {
  Serial.println("Should save config");
  shouldSaveConfig = true;
}

void setup() {
  // put your setup code here, to run once:
  
  //Serial.begin(115200);
  Serial.begin(115200,SERIAL_8N1,SERIAL_TX_ONLY);
  
  Serial.println();
  delay(5000);

  //WiFiManager
  //Local intialization. Once its business is done, there is no need to keep it around
  WiFiManager wifiManager;

// Reset Wifi settings for testing  
  //wifiManager.resetSettings();

  //set config save notify callback
  wifiManager.setSaveConfigCallback(saveConfigCallback);

  //set static ip
  wifiManager.setSTAStaticIPConfig(IPAddress(192,168,100,80), IPAddress(192,168,100,0), IPAddress(255,255,255,0));

  if (!wifiManager.autoConnect("AutoConnectAP")) {
    Serial.println("failed to connect and hit timeout");
    delay(3000);
    //reset and try again, or maybe put it to deep sleep
    ESP.reset();
    delay(5000);
  }

  //if you get here you have connected to the WiFi
  Serial.println("connected...yeey :)");
  Serial.println("local ip");
  Serial.println(WiFi.localIP());

  server.on("/", handleRoot);
  server.on("/login", handleLogin);  
  server.on("/on", on_rele);
  server.on("/off", off_rele);
  server.on("/reset", reset);
  
  server.on("/inline", [](){
    server.send(200, "text/plain", "this works without need of authentification");
  });  

  server.on("/config", [](){
    Serial.print("HTTP REQUEST > ");
    for (uint8_t i = 0; i < server.args(); i++)
    {
      if (server.argName(i) == "TV")
      {
        if(server.arg(i) == "ON"){
          irsend.sendNEC(0x20DF10EF, 32);
          Serial.println("Enciende-Apaga TV! ");
        }
      }
      else if(server.argName(i) == "AumentaCH")
      {
        if(server.arg(i) == "ON"){
          irsend.sendNEC(0x80BF53AC, 32);
          Serial.println("Aumenta Channel! ");
        }
      }
      else if(server.argName(i) == "DisminuyeCH")
      {
        if(server.arg(i) == "ON"){
          irsend.sendNEC(0x80BF4BB4, 32);
          Serial.println("Disminuye Channel! ");
        }
      }
      else if(server.argName(i) == "AumentaVOL")
      {
        if(server.arg(i) == "ON"){
          irsend.sendNEC(0x80BF837C, 32);
          Serial.println("Aumenta Volume! ");
        }
      }
      else if(server.argName(i) == "DisminuyeVOL")
      {
        if(server.arg(i) == "ON"){
          irsend.sendNEC(0x80BF9966, 32);
          Serial.println("Disminuye Volume! ");
        }
      }
      else if(server.argName(i) == "1")
      {
        if(server.arg(i) == "ON"){
          irsend.sendNEC(0x80BF49B6, 32);
          Serial.println("1! ");
        }
      }
      else if(server.argName(i) == "2")
      {
        if(server.arg(i) == "ON"){
          irsend.sendNEC(0x80BFC936, 32);
          Serial.println("2! ");
        }
      }
      else if(server.argName(i) == "3")
      {
        if(server.arg(i) == "ON"){
          irsend.sendNEC(0x80BF33CC, 32);
          Serial.println("3! ");
        }
      }
      else if(server.argName(i) == "4")
      {
        if(server.arg(i) == "ON"){
          irsend.sendNEC(0x80BF718E, 32);
          Serial.println("4! ");
        }
      }
      else if(server.argName(i) == "5")
      {
        if(server.arg(i) == "ON"){
          irsend.sendNEC(0x80BFF10E, 32);
          Serial.println("5! ");
        }
      }
      else if(server.argName(i) == "6")
      {
        if(server.arg(i) == "ON"){
          irsend.sendNEC(0X80BF13EC, 32);
          Serial.println("6! ");
        }
      }
      else if(server.argName(i) == "7")
      {
        if(server.arg(i) == "ON"){
          irsend.sendNEC(0x80BF51AE, 32);
          Serial.println("7! ");
        }
      }
      else if(server.argName(i) == "8")
      {
        if(server.arg(i) == "ON"){
          irsend.sendNEC(0x80BFD12E, 32);
          Serial.println("8! ");
        }
      }
      else if(server.argName(i) == "9")
      {
        if(server.arg(i) == "ON"){
          irsend.sendNEC(0x80BF23DC, 32);
          Serial.println("9! ");
        }
      }
      else if(server.argName(i) == "0")
      {
        if(server.arg(i) == "ON"){
          irsend.sendNEC(0x80BFE11E, 32);
          Serial.println("0! ");
        }
      }
      else if(server.argName(i) == "RCN")
      {
        if(server.arg(i) == "ON"){
          irsend.sendNEC(0x80BF49B6, 32);
          irsend.sendNEC(0x80BFE11E, 32);
          irsend.sendNEC(0x80BFD12E, 32);
          Serial.println("108! ");
        }
      }
      else if(server.argName(i) == "CARACOL")
      {
        if(server.arg(i) == "ON"){
          irsend.sendNEC(0x80BF49B6, 32);
          irsend.sendNEC(0x80BFE11E, 32);
          irsend.sendNEC(0x80BF13EC, 32);
          Serial.println("106! ");
        }
      }
      else if(server.argName(i) == "DK")
      {
        if(server.arg(i) == "ON"){
          irsend.sendNEC(0x80BFC936, 32);
          irsend.sendNEC(0x80BFE11E, 32);
          irsend.sendNEC(0x80BFC936, 32);
          Serial.println("202! ");
        }
      }
      else if(server.argName(i) == "ESPN")
      {
        if(server.arg(i) == "ON"){
          irsend.sendNEC(0x80BFF10E, 32);
          irsend.sendNEC(0x80BF49B6, 32);
          irsend.sendNEC(0x80BFE11E, 32);
          Serial.println("510! ");
        }
      }
      else if(server.argName(i) == "ESPNM")
      {
        if(server.arg(i) == "ON"){
          irsend.sendNEC(0x80BFF10E, 32);
          irsend.sendNEC(0x80BF49B6, 32);
          irsend.sendNEC(0x80BF49B6, 32);
          Serial.println("511! ");
        }
      }
      else if(server.argName(i) == "FOXSPORTS")
      {
        if(server.arg(i) == "ON"){
          irsend.sendNEC(0x80BFF10E, 32);
          irsend.sendNEC(0x80BF49B6, 32);
          irsend.sendNEC(0x80BF33CC, 32);
          Serial.println("513! ");
        }
      }
      else if(server.argName(i) == "WIN")
      {
        if(server.arg(i) == "ON"){
          irsend.sendNEC(0x80BFF10E, 32);
          irsend.sendNEC(0x80BFC936, 32);
          irsend.sendNEC(0x80BF49B6, 32);
          Serial.println("521! ");
        }
      }
      else
      {
        Serial.println("unknown argument! ");
      }
      Serial.print(server.argName(i));
      Serial.print(": ");
      Serial.print(server.arg(i));
      Serial.print(" > ");
    }
  
  /*String webpage ="<form method='get' action='/config?TV=ON'>";
  webpage += "<button type='submit'>ON-OFF TV</button></form>";
  webpage += "<form method=\'get\' action='/config?TV=CH'>";
  webpage += "<button type='submit'>CH</button></form>";*/
  String webpage = "Encendido y apagado de TV <a href=\"/config?TV=ON\">TV_ON</a><br/>";
  webpage+= "Aumento de Canal <a href=\"/config?AumentaCH=ON\">Aumenta_CH</a><br/>";
  webpage+= "Disminucion de Canal <a href=\"/config?DisminuyeCH=ON\">Disminuye_CH</a><br/>";
  webpage+= "Aumento de Canal <a href=\"/config?AumentaVOL=ON\">Aumenta_Vol</a><br/>";
  webpage+= "Disminucion de Canal <a href=\"/config?DisminuyeVOL=ON\">Disminuye_Vol</a><br/>";

  webpage+= "Tecla 1 <a href=\"/config?1=ON\">Tecla_1</a><br/>";
  webpage+= "Tecla 2 <a href=\"/config?2=ON\">Tecla_2</a><br/>";
  webpage+= "Tecla 3 <a href=\"/config?3=ON\">Tecla_3</a><br/>";
  webpage+= "Tecla 4 <a href=\"/config?4=ON\">Tecla_4</a><br/>";
  webpage+= "Tecla 5 <a href=\"/config?5=ON\">Tecla_5</a><br/>";
  webpage+= "Tecla 6 <a href=\"/config?6=ON\">Tecla_6</a><br/>";
  webpage+= "Tecla 7 <a href=\"/config?7=ON\">Tecla_7</a><br/>";
  webpage+= "Tecla 8 <a href=\"/config?8=ON\">Tecla_8</a><br/>";
  webpage+= "Tecla 9 <a href=\"/config?9=ON\">Tecla_9</a><br/>";
  webpage+= "Tecla 0 <a href=\"/config?0=ON\">Tecla_0</a><br/>";

  webpage+= "Canal RCN <a href=\"/config?RCN=ON\">RCN</a><br/>";
  webpage+= "Canal Caracol <a href=\"/config?CARACOL=ON\">Caracol</a><br/>";
  webpage+= "Canal Discovery Kids <a href=\"/config?DK=ON\">DiscoveryKids</a><br/>";
  webpage+= "Canal ESPN <a href=\"/config?ESPN=ON\">ESPN</a><br/>";
  webpage+= "Canal ESPNM <a href=\"/config?ESPNM=ON\">ESPNM</a><br/>";
  webpage+= "Canal FOXSPORTS <a href=\"/config?FOXSPORTS=ON\">FOXSPORTS</a><br/>";
  webpage+= "Canal WIN <a href=\"/config?WIN=ON\">WIN</a><br/>";
  server.send(200, "text/html", webpage);
  });

  server.onNotFound(handleNotFound);
  //here the list of headers to be recorded
  const char * headerkeys[] = {"User-Agent","Cookie"} ;
  size_t headerkeyssize = sizeof(headerkeys)/sizeof(char*);
  //ask server to track these headers
  server.collectHeaders(headerkeys, headerkeyssize );
  server.begin();
  Serial.println("HTTP server started");
#if DECODE_HASH
  // Ignore messages with less than minimum on or off pulses.
  irrecv.setUnknownThreshold(MIN_UNKNOWN_SIZE);
#endif  // DECODE_HASH
  irrecv.enableIRIn();  // Start the receiver
  irsend.begin();
}

void loop() {
  // put your main code here, to run repeatedly:
  server.handleClient();
}


