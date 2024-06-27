#include "BluetoothSerial.h"
#include <DHT.h>
#include <ESP32Servo.h>

// Estruturação dos pinos do ESP32
#define ledr_pin 21 // Pino LED Vermelho conectado ao pino digital 21 do ESP
#define buzzer_pin 22 // Defina o pino conectado ao buzzer
#define servo_pin 23 // Pino SERVO conectado ao pino digital 23 do ESP
#define dht_pin 25 // Pino DHT conectado ao pino digital 25 do ESP
#define ledg_pin 26 // Pino LED Verde conectado ao pino digital 26 do ESP
#define sm_pin 27  // Pino SM conectado ao pino digital 27 do ESP
#define trig_pin 32 // Pino TRIG conectado ao pino digital 32 do ESP
#define echo_pin 33 // Pino ECHO conectado ao pino digital 33 do ESP

// Pinos para a comunicação serial entre os esp32
#define RXD2 16
#define TXD2 17

// ----- Conexão Bluetooth -----
// Nome do dispositivo Bluetooth
String device_name = "Lixeira Inteligente";

// Verifica se o Bluetooth está disponível
#if !defined(CONFIG_BT_ENABLED) || !defined(CONFIG_BLUEDROID_ENABLED)
#error Bluetooth is not enabled! Please run `make menuconfig` to and enable it
#endif

// Verifica o perfil da porta serial
#if !defined(CONFIG_BT_SPP_ENABLED)
#error Serial Port Profile for Bluetooth is not available or not enabled. It is only available for the ESP32 chip.
#endif

BluetoothSerial SerialBT; // Declara o objeto da serial do Bluettoth
// ----- Conexão Bluetooth -----

// ----- Variáveis de Controle -----
// bool estado_tampa = false; // Variável para armazenar o estado da tampa
bool buzzer_ativo = false; // Variável para controlar o estado do buzzer
unsigned long ultimo_som_buzzer = 0; // Variável para registrar o tempo do último acionamento do buzzer

// Variáveis de estado para mapear se os sensores/atuadores estão ativos ou não via Bluetooth
bool sm_ativo = true;
bool dht_ativo = true;
bool su_ativo = true;
bool mostrar_dados = false; // Controle para exibir os dados

float temperatura;
float umidade;
float volume;

long distancia; // Variável para armazenar a distância medida
// ----- Variáveis de Controle -----

// ----- Sensores -----
// DHT
DHT dht(dht_pin, DHT11);

// Servo Motor
Servo servo_motor; // Cria um objeto Servo para controlar o servo motor
// ----- Sensores -----

// ----- Funções -----
long medir_distancia() {
  // Envia um pulso para o pino TRIG
  digitalWrite(trig_pin, LOW); // Define o nível lógico baixo no pino TRIG
  delayMicroseconds(2); // Aguarda 2 microssegundos
  digitalWrite(trig_pin, HIGH); // Define o nível lógico alto no pino TRIG
  delayMicroseconds(10); // Aguarda 10 microssegundos
  digitalWrite(trig_pin, LOW); // Define novamente o nível lógico baixo no pino TRIG

  // Mede o tempo do pulso de retorno (em microssegundos)
  long duracao = pulseIn(echo_pin, HIGH);
  
  // Calcula a distância em centímetros
  long distancia = (duracao / 29.1) / 2; // Velocidade do som em cm/us (343 m/s = 34300 cm/s)
  
  return distancia;
}

void verificar_movimento() {
  // Sensor de Movimento e Servo Motor
  if (sm_ativo) {
    // Verifica se o sensor de presença está ativo
    if (digitalRead(sm_pin) == HIGH) {
      servo_motor.write(90); // Abre a tampa
      digitalWrite(ledg_pin, HIGH); // Acende o LED Verde
      // estado_tampa = true; // Tampa aberta
    } else if (digitalRead(sm_pin) == LOW) {
      servo_motor.write(0); // Fecha a tampa
      digitalWrite(ledg_pin, LOW); // Apaga o LED Verde
      // estado_tampa = false; // Tampa fechada
    }
  }
}

void verificar_ultrassonico() {
  // Sensor Ultrassônico
  if (su_ativo) {
    distancia = medir_distancia(); // Chama a função para medir a distância

    // Lixeira não está cheia
    if (distancia > 5) {
      digitalWrite(ledr_pin, LOW); // Apaga o LED Vermelho

      // Supõe-se que a altura total da lixeira é de 20 cm
      float altura_total = 17; // altura total da lixeira em cm

      // Cálculo do volume ocupado como porcentagem
      volume = ((altura_total - distancia) / altura_total) * 100.0;

    } else { // Lixeira está cheia
      Serial.println("Lixeira está cheia!"); // Lixeira cheia
      digitalWrite(ledr_pin, HIGH); // Acende o LED Vermelho

      // Lixeira cheia -> 100%
      volume = 100.0;
    }
  }
}

void verificar_dht() {
  // DHT11
  if (dht_ativo) {
    umidade = dht.readHumidity(); // Lê a umidade relativa
    temperatura = dht.readTemperature(); // Lê a temperatura em Celsius

    // Verifica se alguma leitura falhou
    if (isnan(umidade) || isnan(temperatura)) {
      Serial.println("Falha no sensor DHT11!");
      return;
    }

    // Identificar se a temperatura é alta (indício de fogo) ou baixa (sem indício de fogo)
    if (temperatura >= 25) { 
      // Verifica se o buzzer não está ativo e se passaram 10 segundos desde o último acionamento
      if (!buzzer_ativo && (millis() - ultimo_som_buzzer > 10000)) {
        // Dispara o Buzzer e envia notificação
        Serial.println("Temperatura alta -> Ativa o Buzzer.");
        digitalWrite(buzzer_pin, HIGH);
        delay(1000); // Mantém o tom por 1 s
        digitalWrite(buzzer_pin, LOW);
        buzzer_ativo = true;
        ultimo_som_buzzer = millis();
      }
    } else {
      buzzer_ativo = false; // Reseta o estado do buzzer
      // Serial.println("Temperatura baixa.");
    }
  }
}

void mostrar_dados_sensores() {
  String dados = "<";
  if(dht_ativo){
    // Temperatura
    Serial.print(F("Temperatura: "));
    Serial.print(temperatura);
    Serial.println(F("°C"));

    // Umidade
    Serial.print(F("Umidade: "));
    Serial.print(umidade);
    Serial.println(F("%"));

    // Bluetooth
    SerialBT.print(temperatura);
    SerialBT.print(";");
    SerialBT.print(umidade);
    SerialBT.print(";");

    dados += String(temperatura) + ";" + String(umidade) + ";"; // Para o receptor
  }else{
    // Bluetooth
    SerialBT.print("---");
    SerialBT.print(";");
    SerialBT.print("---");
    SerialBT.print(";");

    dados += "---;---;"; // Para o receptor
  }

  if(su_ativo){
    // Imprime a distância
    Serial.print(F("Distância Medida: "));
    Serial.print(distancia);
    Serial.println(F("cm"));

    // Bluetooth
    SerialBT.print(volume);
    SerialBT.print(";");

    dados += String(volume); // Para o receptor
  }else{
    // Bluetooth
    SerialBT.print("---");
    SerialBT.print(";");

    dados += "---"; // Para o receptor
  }

  if (sm_ativo) {
    int movimento = digitalRead(sm_pin);
    Serial.print(F("Movimento: "));
    Serial.println(movimento ? F("Detectado") : F("Não detectado"));
  }

  dados += ">"; // Para o receptor
  Serial2.println(dados); // Para o receptor
  Serial.println(dados); 
}

// Função para verificar os comandos recebidos do dispositivo bluetooth e fazer o controle
void controle(char cmd) {
  switch(cmd) {
    case 't':
      dht_ativo = !dht_ativo;
      break;
    case 'm':
      sm_ativo = !sm_ativo;
      break;
    case 'u':
      su_ativo = !su_ativo;
      break;
    case 'd':
      mostrar_dados = !mostrar_dados;
      break;
  }
}
// ----- Funções -----

// ----- Setup -----
void setup() {
  // Inicializa a porta serial
  Serial.begin(115200);

  // Inicializa a comunicação serial Bluetooth
  SerialBT.begin(device_name); // Nome do dispositivo Bluetooth

  Serial.printf("O dispositivo com nome \"%s\" comecou.\nAgora você pode emparelhá-lo com Bluetooth!\n", device_name.c_str());

  // Inicializa o sensor DHT11
  dht.begin();

  // Inicializa os pinos do sensor Ultrassônico
  pinMode(trig_pin, OUTPUT);
  pinMode(echo_pin, INPUT);

  // Inicializa o pino do LED Vermelho
  pinMode(ledr_pin, OUTPUT);

  // Inicializa o pino do sensor de Movimento
  pinMode(sm_pin, INPUT);

  // Inicializa o pino do LED Verde
  pinMode(ledg_pin, OUTPUT);

  // Inicializa o Servo Motor
  servo_motor.attach(servo_pin);
  servo_motor.write(0); // Garante que a tampa comece fechada

  // Inicializa o pino do buzzer
  pinMode(buzzer_pin, OUTPUT);

  // Configura a Serial2 nos pinos 16 (RX) e 17 (TX)
  Serial2.begin(9600, SERIAL_8N1, RXD2, TXD2);
  Serial.println("ESP32 Transmissor pronto.");

  Serial.println("Sistema inicializado");
  Serial.println("-----------------------------");
}
// ----- Setup -----

// ----- Loop -----
void loop() {
  // Verifica se passaram 2 segundos desde a última leitura
  static unsigned long ultima_leitura = 0;
  unsigned long tempo_atual = millis();

  if (tempo_atual - ultima_leitura >= 3000) {
    // Chama as funções de verificação dos sensores
    verificar_movimento();
    verificar_ultrassonico();
    verificar_dht();

    if (mostrar_dados){
      mostrar_dados_sensores();
    }

    // Verifica se há dados disponíveis na porta serial do Bluetooth
    if (SerialBT.available()) {
      char comando = SerialBT.read();
      controle(comando);

      // Aguarda um pequeno intervalo para limpar o buffer de entrada
      delay(10);

      // Limpa o buffer de entrada para garantir que não haja caracteres não lidos
      while (SerialBT.available()) {
        SerialBT.read(); // Descarta caracteres adicionais
      }
    }

    ultima_leitura = tempo_atual;
  }
}
// ----- Loop -----