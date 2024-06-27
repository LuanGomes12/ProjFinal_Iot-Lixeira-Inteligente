#include <WiFi.h>
#include <WebServer.h>

// ----- Conexão Wi-Fi -----
const char* ssid = ""; // Colocar o nome da rede Wi-Fi
const char* pass = ""; // Colocar a senha da rede Wi-Fi
const char* thingspeakServer = "api.thingspeak.com";
const String apiKey = "2EYVPK3THYDTOYH3";
WiFiClient client;
// ----- Conexão Wi-Fi -----

// Servidor web na porta 80
WebServer webServer(80);

// Pinos da conexão serial entre os esp32
#define RXD2 16
#define TXD2 17

// ----- Variáveis de Controle -----
// Variáveis para verificar se todos os dados chegaram
String entrada_string = "";
bool string_completa = false;

// Variáveis para verificar se os sensores estão ativos
bool temperatura_valido = false;
bool umidade_valido = false;
bool volume_valido = false;

// Variáveis para armazenar os valores de temperatura, umidade e volume do lixo, respectivamente
float valor1;
float valor2;
float valor3;
// ----- Variáveis de Controle -----

// ----- Setup -----
void setup() {
  Serial.begin(115200);  // Inicializa a comunicação serial para monitoramento
  Serial2.begin(9600, SERIAL_8N1, RXD2, TXD2); // Inicializa a Serial2 nos pinos RXD2 e TXD2

  Serial.println("Conectando à ");
  Serial.println(ssid);

  WiFi.begin(ssid, pass);

  while (WiFi.status() != WL_CONNECTED){
    delay(500);
    Serial.print(".");
  }

  Serial.println("");
  Serial.println("Wi-Fi conectado");

  // Adiciona um pequeno retardo para garantir que o endereço IP seja obtido
  delay(1000);

  // Verifica se obteve um endereço IP
  if (WiFi.localIP()) {
    Serial.print("Endereço IP: ");
    Serial.println(WiFi.localIP());
  } else {
    Serial.println("Falha ao obter endereço IP.");
  }

  Serial.println("ESP32 Receptor pronto.");

  // Rota para a página principal
  webServer.on("/", handleRoot);

  // Rota para obter os valores dos sensores
  webServer.on("/sensor_values", handleSensorValues);

  // Inicia o servidor
  webServer.begin();
}
// ----- Setup -----

// ----- Loop -----
void loop() {
  // Lê os dados da Serial2
  if (Serial2.available()) {
    String receivedData = Serial2.readStringUntil('\n'); // Lê a string até encontrar uma quebra de linha
    receivedData.trim(); // Remove espaços em branco no início e no final

    // Verifica se a string começa com '<' e termina com '>'
    if (receivedData.startsWith("<") && receivedData.endsWith(">")) {
      // Remove os caracteres '<' e '>'
      receivedData = receivedData.substring(1, receivedData.length() - 1);

      // Divide a string em partes usando o caractere ';'
      String valores[3];
      int pos = 0;
      while (receivedData.length() > 0) {
        int idx = receivedData.indexOf(';');
        if (idx != -1) {
          valores[pos] = receivedData.substring(0, idx);
          receivedData = receivedData.substring(idx + 1);
        } else {
          valores[pos] = receivedData;
          receivedData = "";
        }
        pos++;
      }

      // Condicionais para verificar se os valores recebidos do transmissor são válidos ou não
      if(valores[0] == "---"){
        Serial.println("Sensor DHT (Temperatura) desativado!");
        temperatura_valido = false;
      }else{
        valor1 = valores[0].toFloat();
        Serial.print("Valor 1: ");
        Serial.println(valor1);
        temperatura_valido = true;
      }

      if(valores[1] == "---"){
        Serial.println("Sensor DHT (Umidade) desativado!");
        umidade_valido = false;
      }else{
        valor2 = valores[1].toFloat();
        Serial.print("Valor 2: ");
        Serial.println(valor2);
        umidade_valido = true;
      }

      if(valores[2] == "---"){
        Serial.println("Sensor Ultrassônico desativado!");
        volume_valido = false;
      }else{
        valor3 = valores[2].toFloat();
        Serial.print("Valor 3: ");
        Serial.println(valor3);
        volume_valido = true;
      }

      // Verifica se o cliente está conectado e envia os valores para o ThingSpeak
      if (client.connect(thingspeakServer, 80)) {
        String postStr = apiKey;
        if(temperatura_valido){
          postStr += "&field1=";
          postStr += String(valor1);
        }

        if(umidade_valido){
          postStr += "&field2=";
          postStr += String(valor2);
        }
        
        if(volume_valido){
          postStr += "&field3=";
          postStr += String(valor3);
        }
        
        postStr += "\r\n\r\n";

        client.print("POST /update HTTP/1.1\n");
        client.print("Host: api.thingspeak.com\n");
        client.print("Connection: close\n");
        client.print("X-THINGSPEAKAPIKEY: " + apiKey + "\n");
        client.print("Content-Type: application/x-www-form-urlencoded\n");
        client.print("Content-Length: ");
        client.print(postStr.length());
        client.print("\n\n");
        client.print(postStr);

        Serial.println("Dados enviados para o ThingSpeak.");

        // Aguarde até que o cliente tenha enviado os dados
        delay(1000);

      } else {
        Serial.println("Falha ao conectar ao servidor ThingSpeak.");
      }
      
      client.stop(); // Fecha a conexão após enviar os dados
    }
  }

  // Processa as requisições HTTP
  webServer.handleClient();
}
// ----- Loop -----

// ----- Funções -----
// Função para manipular a rota raiz
void handleRoot() {
  String html = "<html><head><style>";
  html += "body { font-family: Arial, sans-serif; text-align: center; }";
  html += "h1 { font-size: 2em; }";
  html += "p { font-size: 1.5em; font-weight: bold; }";
  html += "</style></head><body>";
  html += "<h1>Valores dos Sensores</h1>";
  html += "<p>Valor 1 (Temperatura): <span id='valor1'>---</span></p>";
  html += "<p>Valor 2 (Umidade): <span id='valor2'>---</span></p>";
  html += "<p>Valor 3 (Volume do Lixo): <span id='valor3'>---</span></p>";
  html += "<script>";
  html += "setInterval(function() {";
  html += "  fetch('/sensor_values').then(response => response.json()).then(data => {";
  html += "    document.getElementById('valor1').innerText = data.valor1;";
  html += "    document.getElementById('valor2').innerText = data.valor2;";
  html += "    document.getElementById('valor3').innerText = data.valor3;";
  html += "  });";
  html += "}, 1000);"; // Atualiza a cada 1 segundo
  html += "</script>";
  html += "</body></html>";

  webServer.send(200, "text/html", html);
}

// Função para manipular a rota dos valores dos sensores
void handleSensorValues() {
  String json = "{";
  json += "\"valor1\": " + String(valor1) + ",";
  json += "\"valor2\": " + String(valor2) + ",";
  json += "\"valor3\": " + String(valor3);
  json += "}";

  webServer.send(200, "application/json", json);
}
// ----- Funções -----