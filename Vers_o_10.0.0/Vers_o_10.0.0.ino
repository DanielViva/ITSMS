#include <ESP8266WiFi.h>
#include <WiFiClient.h>
#include <ESP8266WebServer.h>
#include <ESP8266mDNS.h>
#include <SPI.h>
#include <SD.h>
#include <ArduinoJson.h>

#define DBG_OUTPUT_PORT Serial

const char* ssid = "Labauto";
const char* password = "daniel12";
const char* host = "esp8266sd";

int caractere[2][20];
typeprotocol=0;


ESP8266WebServer server(80);

static bool hasSD = false;
File uploadFile;

unsigned long disp_time = 0;


void returnOK() {
  server.send(200, "text/plain", "");
}

void ReadStringSerial() {
  int i = 0;
  // Enquanto receber algo pela serial
  while (Serial.available() > 0) {
    // Lê byte da serial
    caractere[typeprotocol][i] = Serial.read();
    i++;
  }
}

void returnFail(String msg) {
  server.send(500, "text/plain", msg + "\r\n");
}

void serialreqP () {
  serialreqp (0x50);
  typeprotocol = 2;
}

void serialreqQ () {
  serialreqp (0x51);
  typeprotocol = 1;
}

void serialreq (char protocolo) {
  char a;
  Serial.write(protocolo);
  Serial.write(0x00);
  Serial.write(0x00);
  Serial.write(0x00);
  Serial.write(0x00);
  Serial.write(~protocolo + 0x01);
  Serial.write(0x0D);
}

void serialreqdata (char protocolo, char a, char b, char c, char d) {
  Serial.write(protocolo);
  Serial.write(a);
  Serial.write(b);
  Serial.write(c);
  Serial.write(d);
  char verific = protocolo + a + b;
  Serial.write(~verific + 0x01);
  Serial.write(0x0D);
}




    //serialreq(0x50); //Comando P
    //serialreq(0x4C); //Comando L
    //Comando T
    //serialreq(0x4D); //Comando M
    //Comando S
    //Comando R
    //serialreq(0x43); //Comando C
    //serialreq(0x44); //Comando D
    //serialreq(0x49); //Comando I
    //serialreq(0x46); //Comando F


bool loadFromSdCard(String path) {
  String dataType = "text/plain";
  if (path.endsWith("/")) path += "index.htm";

  if (path.endsWith(".src")) path = path.substring(0, path.lastIndexOf("."));
  else if (path.endsWith(".htm")) dataType = "text/html";
  else if (path.endsWith(".css")) dataType = "text/css";
  else if (path.endsWith(".js")) dataType = "application/javascript";
  else if (path.endsWith(".png")) dataType = "image/png";
  else if (path.endsWith(".gif")) dataType = "image/gif";
  else if (path.endsWith(".jpg")) dataType = "image/jpeg";
  else if (path.endsWith(".ico")) dataType = "image/x-icon";
  else if (path.endsWith(".xml")) dataType = "text/xml";
  else if (path.endsWith(".pdf")) dataType = "application/pdf";
  else if (path.endsWith(".zip")) dataType = "application/zip";

  File dataFile = SD.open(path.c_str());
  if (dataFile.isDirectory()) {
    path += "/index.htm";
    dataType = "text/html";
    dataFile = SD.open(path.c_str());
  }

  if (!dataFile)
    return false;

  if (server.hasArg("download")) dataType = "application/octet-stream";

  if (server.streamFile(dataFile, dataType) != dataFile.size()) {
    DBG_OUTPUT_PORT.println("Sent less data than expected!");
  }

  dataFile.close();
  return true;
}

void printDirectory() {
  if (!server.hasArg("dir")) return returnFail("BAD ARGS");
  String path = server.arg("dir");
  if (path != "/" && !SD.exists((char *)path.c_str())) return returnFail("BAD PATH");
  File dir = SD.open((char *)path.c_str());
  path = String();
  if (!dir.isDirectory()) {
    dir.close();
    return returnFail("NOT DIR");
  }
  dir.rewindDirectory();
  server.setContentLength(CONTENT_LENGTH_UNKNOWN);
  server.send(200, "text/json", "");
  WiFiClient client = server.client();

  server.sendContent("[");
  for (int cnt = 0; true; ++cnt) {
    File entry = dir.openNextFile();
    if (!entry)
      break;

    String output;
    if (cnt > 0)
      output = ',';

    output += "{\"type\":\"";
    output += (entry.isDirectory()) ? "dir" : "file";
    output += "\",\"name\":\"";
    output += entry.name();
    output += "\"";
    output += "}";
    server.sendContent(output);
    entry.close();
  }
  server.sendContent("]");
  dir.close();
}

void handleNotFound() {
  if (hasSD && loadFromSdCard(server.uri())) return;
  String message = "SDCARD Not Detected\n\n";
  message += "URI: ";
  message += server.uri();
  message += "\nMethod: ";
  message += (server.method() == HTTP_GET) ? "GET" : "POST";
  message += "\nArguments: ";
  message += server.args();
  message += "\n";
  for (uint8_t i = 0; i < server.args(); i++) {
    message += " NAME:" + server.argName(i) + "\n VALUE:" + server.arg(i) + "\n";
  }
  server.send(404, "text/plain", message);
  DBG_OUTPUT_PORT.print(message);
}



void setup(void) {

  //Configura comunicação serial com baudrate igual a 9600
  DBG_OUTPUT_PORT.begin(9600);
  DBG_OUTPUT_PORT.setDebugOutput(true);
  DBG_OUTPUT_PORT.print("\n");
  
  //Configura o Wifi para SSID e password configurados hardcoded
  WiFi.begin(ssid, password);
  DBG_OUTPUT_PORT.print("Connecting to ");
  DBG_OUTPUT_PORT.println(ssid);

  //Aguarda a conexão do Wifi
  uint8_t i = 0;
  while (WiFi.status() != WL_CONNECTED && i++ < 20) {
    //wait 10 seconds
    delay(500);
  }

  //Caso não seja possível conectar ao Wifi, após 10 segundos para de aguardar e é enviado uma mensagem de erro através da porta serial
  if (i == 21) {
    DBG_OUTPUT_PORT.print("Could not connect to");
    DBG_OUTPUT_PORT.println(ssid);
    while (1) delay(500);
  }

  //No caso de sucesso é enviado uma mensagem contendo o endereço IP
  DBG_OUTPUT_PORT.print("Connected! IP address: ");
  DBG_OUTPUT_PORT.println(WiFi.localIP());

  //Inicia o servidor de mDNS para que seja possível se conectar usando um um endereço de web 
  if (MDNS.begin(host)) {
    MDNS.addService("http", "tcp", 80);
    DBG_OUTPUT_PORT.println("MDNS responder started");
    DBG_OUTPUT_PORT.print("You can now connect to http://");
    DBG_OUTPUT_PORT.print(host);
    DBG_OUTPUT_PORT.println(".local");
  }

  //Começa a escutar as rotas servidor HTTP
  server.on("/list", HTTP_GET, printDirectory);





  server.on("/Comando", []() {
    String comando;
    String messager = "";
    messager += "<html>";
    messager += "<body>";
    messager += "<form action=\"/Comando\">";
    messager += "Comando";
    messager += "<input type=\"text\" name=\"comando\" >";
    messager += "<br><br>";
    messager += " <input type=\"submit\" value=\"Solicitar\">";
    messager += "</form>";
    messager += "</body>";
    messager += "</html>";
    for (uint8_t i = 0; i < server.args(); i++) {
      comando = server.arg(i);
      messager += " NAME:" + server.argName(i) + "<br> VALUE: " + comando + "<br>";
    }

     

  });
  server.onNotFound(handleNotFound);

  server.begin();
  DBG_OUTPUT_PORT.println("HTTP server started");

  if (SD.begin(SS)) {
    DBG_OUTPUT_PORT.println("SD Card initialized.");
    hasSD = true;
  }

// File printFile = SD.open("Part1.txt");

//  if (!printFile) {
//    Serial.print("The text file cannot be opened");
//    while(1);
//  }



server.on("/reqM", []() {
    String messager = "Enviado M beef on/off";
    serialreq(0x4D);

       
    server.send(200, "text/text", messager);
  });

  server.on("/infoP", []() {
    String messager = "";
    serialreq(0x50);
if (caractere [2][0] = 0x50) {
    int freq_in = caractere[1] * 256 + caractere[2];
    int tens_bypass = caractere[3] * 256 + caractere[4];
    int W_out = caractere[5] * 256 + caractere[6];
    int S_out = caractere[7] * 256 + caractere[8];
    int V_barramento_pos = caractere[9] * 256 + caractere[10];
    int V_barramento_neg = caractere[11] * 256 + caractere[12];
    int temp_boost = caractere[13] * 256 + caractere[14];
} // pode dar erro por causa da declaracao do int
    StaticJsonBuffer<200> jsonBuffer;
    JsonObject& root = jsonBuffer.createObject();
    root["freq_in"] = freq_in;
    root["tens_bypass"] = tens_bypass;
    root["W_out"] = W_out;
    root["S_out"] = S_out;
    root["V_barramento_pos"] = V_barramento_pos;
    root["V_barramento_neg"] = V_barramento_neg;
    root["temp_boost"] = temp_boost;


    root.printTo(messager);
    
    server.send(200, "text/json", messager);
  });

  server.on("/infoQ", []() {
    serialreq(0x51);

    int voltage = caractere[1] * 256 + caractere[2];
    int voltagein = caractere[3] * 256 + caractere[4];
    int voltageout = caractere[5] * 256 + caractere[6];
    int potout = caractere[7] * 256 + caractere[8];
    int frq = caractere[9] * 256 + caractere[10];
    int bat = caractere[11] * 256 + caractere[12];
    int temp = caractere[13] * 256 + caractere[14];

    StaticJsonBuffer<200> jsonBuffer;
    JsonObject& root = jsonBuffer.createObject();
    root["voltage"] = voltage;
    root["voltagein"] = voltagein;
    root["voltageout"] = voltageout;
    root["potout"] = potout;
    root["frq"] = frq;
    root["bat"] = bat;
    root["temp"] = temp;

    String messager;
    root.printTo(messager);

    server.send(200, "text/json", messager);


  });


}

void loop(void) {

  //Verifica se o servidor está 
  server.handleClient();

  //Verifica se a Serial está recebendo alguma mensagem
  if (Serial.available() > 0) {
    //Lê a mensagem
    ReadStringSerial();
  }

  // não faz nada, sei lá o que era
  if (disp_time > millis()) {
     disp_time = millis() + 5000;
     
  }
}
