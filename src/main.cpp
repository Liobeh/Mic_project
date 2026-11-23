// Libraries
#include <Arduino.h>
#include <stdlib.h>
#include <avr/pgmspace.h> // p/ mandar coisa pra mem flash

// Bibliotecas p/ tela
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

#define set_bit(reg, bit) ((reg) |= (1 << (bit)))  // coloca o bit em 1
#define clr_bit(reg, bit) ((reg) &= ~(1 << (bit))) // coloca o bit em 0
#define cpl_bit(reg, bit) ((reg) ^= (1 << (bit)))  // inverte o bit
#define tst_bit(reg, bit) ((reg) & (1 << (bit)))   // testa o estado do bit

// Dimensões da tela
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
// setup da tela
#define OLED_RESET -1 // -1 porque não tem pino reset, logo depende do reset do arduino.
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

// Setup das variaveis

// fase 3
bool jogo3_iniciado = false;
uint8_t acao_atual = 0;
bool claro_antes;
uint16_t limite_luz = 500;
unsigned long inicio_estado = 0;
unsigned long acao1;
unsigned long acao3;
bool estadoAcao1;
bool estadoAcao3;
uint8_t escolhido;
uint8_t protocolo;
uint8_t codigo;
uint8_t numeroEsperado3 = 0;
uint8_t condicao_inicial;
#define ldr 1 // A1

// fase 4
uint8_t escolhido2;
bool PodeTocarDenovo = true;
uint8_t digito = 0;
char tentativa[6];
bool fase4Iniciada = false;
uint8_t passoLed4 = 0;
bool led4Aceso = false;
uint32_t instanteLed4 = 0;

// interrupcoes
volatile bool flag_480s = false;  // flag tempo
volatile bool flag_botao = false; // flag botao pressionado

// timers
volatile uint32_t ms = 0;
volatile uint16_t contador_ms = 0;
volatile uint16_t segundos = 0; // timer
const uint16_t tempo_limite = 480;
uint16_t tempo_restante = 0;

// Maquina de estados
enum EstadoJogo
{
  INICIO,
  FASE_1,
  FASE_2,
  FASE_3,
  FASE_4,
  VITORIA,
  GAME_OVER
};

EstadoJogo estadoAtual = INICIO;
bool telaInicioMostrada = false;
bool telaVitoriaMostrada = false;
bool telaGameOverMostrada = false;

// substituindo funções prontas do arduino
uint32_t meu_millis()
{
  uint8_t estado = SREG;
  cli();

  uint32_t tempo = ms;

  SREG = estado;
  return tempo;
}

// Leitura de um canal analogico, material da prof
uint16_t meu_analogRead(uint8_t canal)
{

  ADMUX = (1 << REFS0) | canal;

  // Inicia a conversao
  set_bit(ADCSRA, ADSC);

  // Espera a conversao terminar
  while (tst_bit(ADCSRA, ADSC))
    ;

  // Retorna o resultado
  return ADC;
}

// Keypad 4x3 conectado ao PCF8574T via I2C
#define PCF 0x27

byte pinosLinhas[4] = {0, 1, 2, 4};
byte pinosColunas[3] = {5, 6, 7};

char teclas[4][3] = {
    {'1', '2', '3'},
    {'4', '5', '6'},
    {'7', '8', '9'},
    {'*', '0', '#'}};

void escreverPCF(byte valor)
{
  Wire.beginTransmission(PCF);
  Wire.write(valor);
  Wire.endTransmission();
}

byte lerPCF()
{
  Wire.requestFrom(PCF, 1);

  if (Wire.available())
    return Wire.read();

  return 0xFF;
}

char lerTeclado()
{
  for (int i = 0; i < 4; i++)
  {
    byte estado = 0xFF;

    clr_bit(estado, pinosLinhas[i]);

    escreverPCF(estado);
    delayMicroseconds(100);

    byte leitura = lerPCF();

    for (int j = 0; j < 3; j++)
    {
      if (!tst_bit(leitura, pinosColunas[j]))
      {
        escreverPCF(0xFF);
        return teclas[i][j];
      }
    }
  }

  escreverPCF(0xFF);
  return '\0';
}

char lerTeclaNova()
{
  static char anterior = '\0';
  static uint32_t ultimoTempo = 0;

  if (meu_millis() - ultimoTempo < 40) // debouncing do keypad
  {
    return '\0';
  }

  char atual = lerTeclado();
  char resultado = '\0';

  if (atual != '\0' && anterior == '\0') // quando aperta a tecla
  {
    resultado = atual;
    ultimoTempo = meu_millis();
  }

  else if (atual == '\0' && anterior != '\0') // quando solta a tecla
  {
    ultimoTempo = meu_millis();
  }

  anterior = atual;

  return resultado;
}

// fase1
//  Setup dos pinos
#define chave_4 4
#define chave_5 5
#define chave_6 6
#define chave_7 7
#define botao 3

#define led_1 PB0 // D8
#define led_2 PB1 // D9
#define led_3 PB2 // D10
#define led_4 PB4 // D12

// -------- config buzzer -------
#define buzzer_pino PB3 // D11

volatile uint8_t buzzer_divisor = 0; // 0 = desligado, 1 = 500Hz, 2 = 250Hz, 3 = 167Hz
volatile uint8_t buzzer_contador = 0; // contador (p/ div de freq)

// Cada som corresponde ao número da linha nas matrizes abaixo (0 a 3)
enum sons_buzzer
{
  erro = 0,
  Alerta = 1,
  Vitoria = 2,
  Game_Over = 3,
  Silencio = 4
};

// Frequências em Hz de cada nota
const uint16_t melodias_frequencias[4][5] PROGMEM = {
  {250,   0, 167,   0,   0}, // linha 0: erro
  {500,   0,   0,   0,   0}, // linha 1: Alerta
  {250,   0, 500,   0, 500}, // linha 2: Vitoria
  {500,   0, 250,   0, 167}  // linha 3: Game_Over
};

// Duração em ms de cada nota
const uint16_t melodias_duracao[4][5] PROGMEM = {
  {140,  50, 250,   0,   0}, // linha 0: erro
  { 80,   0,   0,   0,   0}, // linha 1: Alerta
  {100,  40, 100,  40, 260}, // linha 2: Vitoria
  {180,  50, 180,  50, 450}  // linha 3: Game_Over
};

// Quantos passos válidos cada melodia tem
const uint8_t melodias_tamanhos[4] PROGMEM = {3, 1, 5, 5};

// Variáveis de controle do buzzer
sons_buzzer som_atual = Silencio; // começa em silencio
uint8_t indice_nota = 0; //qual "passo" a nota esta
uint32_t inicio_nota = 0; // tempo q a nota começou a tocar
bool som_prioridade = false; // se o som tem prioridade (em relação a Alerta)

void buzzerTick() //'tick' do buzzer (para a gerar frequencia do som)
{
  if (buzzer_divisor > 0)
  {
    buzzer_contador++;
    if (buzzer_contador >= buzzer_divisor)
    {
      buzzer_contador = 0;
      cpl_bit(PORTB, buzzer_pino);
    }
  }
}

void buzzerDesligar()
{
  buzzer_divisor = 0;
  buzzer_contador = 0;
  clr_bit(PORTB, buzzer_pino);
}

void buzzerInicializar()
{
  set_bit(DDRB, buzzer_pino);
  buzzerDesligar();
}

void buzzerLigar(uint16_t freq)
{
  if (freq == 0)
  {
    buzzerDesligar();
    return;
  }

  uint8_t div;
  if (freq >= 375)
  {
    div = 1; // 500 Hz
  }
  else if (freq >= 200)
  {
    div = 2; // 250 Hz
  }
  else
  {
    div = 3; // 167 Hz
  }

  buzzer_divisor = 0;
  buzzer_contador = 0;
  buzzer_divisor = div;
}

void pararSom()
{
  buzzerDesligar();
  som_atual = Silencio;
  indice_nota = 0;
  som_prioridade = false;
}

void tocarSom(sons_buzzer som)
{
  if (som >= Silencio)
  {
    pararSom();
    return;
  }

  // Não interrompe erro, vitória ou game over com o bipe de tempo
  if (som == Alerta && som_prioridade)
  {
    return;
  }

  som_atual = som;
  som_prioridade = (som != Alerta);
  indice_nota = 0;
  inicio_nota = meu_millis();
  buzzerLigar(pgm_read_word(&melodias_frequencias[som_atual][0]));
}

void atualizarBuzzer(bool jogo_ativo, uint16_t tempo_restante)
{
  uint32_t agora = meu_millis();

  // 1. Avança as notas da melodia atual sem bloquear
  if (som_atual != Silencio)
  {
    if (agora - inicio_nota >= pgm_read_word(&melodias_duracao[som_atual][indice_nota]))
    {
      indice_nota++;
      if (indice_nota >= pgm_read_byte(&melodias_tamanhos[som_atual]))
      {
        pararSom();
      }
      else
      {
        inicio_nota = agora;
        buzzerLigar(pgm_read_word(&melodias_frequencias[som_atual][indice_nota]));
      }
    }
  }

  // 2. Aviso sonoro quando o tempo está acabando
  static uint32_t ultimo_alerta = 0;
  if (jogo_ativo && tempo_restante > 0 && tempo_restante <= 60)
  {
    uint16_t intervalo;
    if (tempo_restante <= 15)
    {
      intervalo = 500;
    }
    else
    {
      intervalo = 1000;
    }

    if (agora - ultimo_alerta >= intervalo)
    {
      ultimo_alerta = agora;
      tocarSom(Alerta);
    }
  }
}
// -------- fim config buzzer -------
// erros
uint8_t erros = 0;

void registrarErros(){
  erros++;
  tocarSom(erro);
}

// sequências
const uint8_t sequencias[4][4] = {
    {led_1, led_2, led_3, led_4},
    {led_4, led_3, led_2, led_1},
    {led_1, led_3, led_2, led_4},
    {led_3, led_2, led_4, led_1}};

// códigos p/ cada sequência.
const char codigo_sequencias[4][6] = {
    {'7', '7', '3', '8', '9', '4'},  // seq1
    {'7', '7', '3', '4', '9', '2'},  // seq2
    {'7', '1', '3', '8', '9', '2'},  // seq3
    {'7', '7', '8', '4', '6', '2'}}; // seq4

// Definição de variaveis globais fase2 ADC
//================================

bool fase2Iniciada = false;

// Parte dos leds
const int piscando = 2; // condição de quando o led pisca
const int aceso = HIGH;
const int apagado = LOW;
const int condicao_leds[4][3] = {
    // uma lista de tuplas com as diferentes configurações de leds acesos
    // amarelo, azul, verde
    {aceso, aceso, apagado},
    {apagado, apagado, piscando},
    {piscando, piscando, piscando},
    {apagado, piscando, aceso},
};
int configuracao_led_anterior = -1;
int pisca = HIGH;
const uint8_t vetor_Leds[3] = {led_1, led_2, led_3}; // amarelo, azul, verde
unsigned long tempo_pisca = 300;
unsigned long ultima_troca = 0;

// Parte dos potenciometros
const int min_max_3x = -15;
const int nada = -20;
const int meio = -11;   // 341<= meio <=682
const int minimo = -9;  // 0<= minimo <=341
const int maximo = -16; // 682<= maximo <=1023
const int nenhuma = -1;
const int tabela_resposta_pot[4][2] = {
    // tabela de respostas com potenciometros para cada configuração de leds
    // pot1      pot2
    {min_max_3x, nada},
    {nada, meio},
    {minimo, minimo},
    {maximo, min_max_3x},
};
int pot_valor_lido[2] = {0, 0};
int posicao_pot_lida[2] = {0, 0};
int contador_de_giros[2] = {0, 0};
int posicao_anterior[2] = {nenhuma, nenhuma};
int vetor_pot[2] = {2, 3};
bool passou_pelo_minimo[2] = {0, 0};
bool fase2_concluida = false;
bool fase2_falhou = false;
unsigned long tempo_segurar = 1500; // tempo que o jogador deve segurarar o potenciometro na posição para validar a resposta
unsigned long momento_chegada[2] = {0, 0};
unsigned long tempolimite_fase_2 = 60000; // ideal 1 minuto 60000 ms
unsigned long momento_inicio = 0;

//=================================

// Funções da fase2 ADC
//=================================================================================================================================================================================
void acender_leds() // void acender_leds():
{
  int configuracao_led_escolhida = random(0, 4); // um gerador pseudoaletório o qual indica uma das configurações para acender
  while (configuracao_led_escolhida == configuracao_led_anterior)
  {
    configuracao_led_escolhida = random(0, 4);
  }
  configuracao_led_anterior = configuracao_led_escolhida;

  for (int led = 0; led < 3; led++) // o laço itera de 0 a 2 para abranger os 3 leds
  {
    // configuração do led amarelo: e configuração do led azul: configuração do led verde:
    if (condicao_leds[configuracao_led_escolhida][led] == apagado)
    {
      PORTB &= ~(1 << vetor_Leds[led]);
    }
    else // como aceso e piscando começam com o led aceso basta colocar um else após verificar que a condição não é apagado
    {
      PORTB |= (1 << vetor_Leds[led]);
    }
  }
  momento_inicio = meu_millis();
}

void pisca_led()
{
  if ((meu_millis() - ultima_troca) >= tempo_pisca)
  {
    pisca = !pisca; // inverte o sentido de pisca
    ultima_troca = meu_millis();
  }

  for (int led = 0; led < 3; led++)
  {
    if (condicao_leds[configuracao_led_anterior][led] == piscando)
    {
      if (pisca == HIGH)
      {
        PORTB |= (1 << vetor_Leds[led]);
      }
      else
      {
        PORTB &= ~(1 << vetor_Leds[led]);
      }
    }
  }
}

// Funções de resposta dos potenciometros
int converter_posicao(int leitura_pot)
{

  if (leitura_pot <= 341) // condiconal de leitura de cada potenciometro,
  {
    return minimo;
  }
  else if (leitura_pot <= 682) // condicional de leitura de cada potenciometro,
  {
    return meio;
  }
  else // condicional de leitura de cada potenciometro,
  {
    return maximo;
  }
}

int confirmar_posicao(int indice_pot)
{
  // Verifica se o potenciometro mudou de posicao
  if (posicao_pot_lida[indice_pot] != posicao_anterior[indice_pot])
  {
  if (posicao_anterior[indice_pot] != nenhuma) // verifica se não é a primeira leitura do potenciometro
  {
    momento_chegada[indice_pot] = meu_millis(); 
  }
  posicao_anterior[indice_pot] = posicao_pot_lida[indice_pot];
  } 
  // Verifica se permaneceu na posicao pelo tempo necessario
  if (meu_millis() - momento_chegada[indice_pot] >= tempo_segurar && momento_chegada[indice_pot] != 0)
  {
    return posicao_pot_lida[indice_pot];
  }

  return nenhuma;
}

void resposta_potenciometros()
{

  int posicao_confirmada = 0;

  if (fase2_concluida || fase2_falhou) // já terminou?
  {
    return; // sai da função
  }

  if (meu_millis() - momento_inicio >= tempolimite_fase_2) // tempo esgotado?
  {
    fase2_falhou = true;
    registrarErros(); // conta um erro para a
    return;
  }

  bool certo[2] = {false, false}; // um por potenciômetro
  for (int indice = 0; indice < 2; indice++)
  {
    pot_valor_lido[indice] = meu_analogRead(vetor_pot[indice]);

    if (pot_valor_lido[indice] <= 341) // condiconal de leitura de cada potenciometro,
    {
      posicao_pot_lida[indice] = minimo;
    }
    else if (pot_valor_lido[indice] <= 682) // condicional de leitura de cada potenciometro,
    {
      posicao_pot_lida[indice] = meio;
    }
    else // condicional de leitura de cada potenciometro,
    {
      posicao_pot_lida[indice] = maximo;
    }

    if (posicao_pot_lida[indice] == minimo)
    {
      passou_pelo_minimo[indice] = true;
    }

    if (posicao_pot_lida[indice] == maximo && passou_pelo_minimo[indice])
    {
      contador_de_giros[indice]++;
      passou_pelo_minimo[indice] = false;
    }

    posicao_confirmada = confirmar_posicao(indice); // verificar se o jogador está esperando o tempo_segurar

    if (contador_de_giros[indice] >= 3) // condicional para quando o potenciometro atinge vai mínimo ao máximo 3 vezes
    {
      posicao_confirmada = min_max_3x;
    }

    if (posicao_confirmada == tabela_resposta_pot[configuracao_led_anterior][indice] || tabela_resposta_pot[configuracao_led_anterior][indice] == nada) // condicional para quando a posição do potenciometro é igual a da tabela ou quando a resposta é não fazer nada
    {
      certo[indice] = true;
    }
  }
  if (certo[0] && certo[1])
  {
    fase2_concluida = true;
  }
  // Serial.println(posicao_confirmada); // verificação do valor de posicao_confirmada para teste, deve retornar -1
}

void nova_tentativa_fase_2()
{
  fase2_falhou = false;
  fase2_concluida = false;
  contador_de_giros[0] = 0;
  contador_de_giros[1] = 0;
  passou_pelo_minimo[0] = false;
  passou_pelo_minimo[1] = false;
  posicao_anterior[0] = nenhuma;
  posicao_anterior[1] = nenhuma;
  momento_chegada[0] = 0;
  momento_chegada[1] = 0;
  pisca = HIGH;
  ultima_troca = meu_millis();
  acender_leds(); // chama acender_leds para a nova tentativa ter outro padrão de leds
}
//============================================================================================================================================================================

// rotina de interrupcao do botao
ISR(INT1_vect)
{
  flag_botao = true;
}

ISR(TIMER1_COMPA_vect)
{
  ms++;
  buzzerTick();
}

ISR(TIMER2_COMPA_vect) // chama a cada 1 ms
{
  contador_ms++;

  if (contador_ms >= 1000)
  {
    contador_ms = 0;
    segundos++;

    if (segundos >= tempo_limite)
    {
      flag_480s = true;
    }
  }
}

// setup do tempo atual
void atualizarTempo()
{
  uint16_t tempo_decorrido;

  uint8_t estado = SREG;
  cli();

  tempo_decorrido = segundos;

  SREG = estado;

  uint32_t tempo_consumido = tempo_decorrido + (erros * 15);

  if (tempo_consumido >= tempo_limite)
  {
    tempo_restante = 0;
    flag_480s = true;
  }
  else
  {
    tempo_restante = tempo_limite - tempo_consumido;
  }
}

// Auto-explicativa
void reiniciarJogo()
{
  // Para o cronometro
  TCCR2B = 0;
  pararSom();

  // Zera os contadores com seguranca
  uint8_t estado = SREG;
  cli();

  TCNT2 = 0;
  contador_ms = 0;
  segundos = 0;
  flag_480s = false;
  flag_botao = false;

  SREG = estado;

  // Reinicia tempo e erros
  tempo_restante = tempo_limite;
  erros = 0;

  // Reinicia a fase 2
  fase2Iniciada = false;
  fase2_concluida = false;
  fase2_falhou = false;

  configuracao_led_anterior = -1;
  pisca = HIGH;
  ultima_troca = 0;

  // Desliga o LED verde da fase 2
  clr_bit(PORTB, PB4);

  // Reinicia a fase 3
  jogo3_iniciado = false;
  acao_atual = 0;
  inicio_estado = 0;

  // Reinicia a fase 4
  PodeTocarDenovo = true;
  digito = 0;
  fase4Iniciada = false;
  passoLed4 = 0;
  led4Aceso = false;

  PORTB &= 0b11110000; // Desliga PB0 a PB3

  // Sorteia novos desafios
  escolhido = random(4, 8);
  protocolo = random(1, 5);
  codigo = random(1, 5);
  condicao_inicial = random(0, 2);
  escolhido2 = random(0, 4);

  // Reinicia display
  telaInicioMostrada = false;
  telaVitoriaMostrada = false;
  telaGameOverMostrada = false;
  // Volta para a tela inicial
  estadoAtual = INICIO;

  Serial.println(F("PARTIDA REINICIADA"));
}

void verificarReinicio()
{
  char comando = '\0';

  if (Serial.available() > 0)
  {
    comando = Serial.read();
  }

  char tecla = lerTeclaNova();

  if (comando == 'R' or comando == 'r' or tecla == '*')
  {
    reiniciarJogo();
  }
}

// Display
void atualizarOLED()
{
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);

  uint8_t minutos = tempo_restante / 60;
  uint8_t segundos_restantes = tempo_restante % 60;

  display.setCursor(0, 0);
  display.print(F("Tempo: "));
  display.print(minutos);
  display.print(':');

  if (segundos_restantes < 10)
  {
    display.print('0');
  }

  display.print(segundos_restantes);
  display.setCursor(88, 0);
  display.print(F("E:"));
  display.print(erros);

  display.drawLine(0, 12, 127, 12, SSD1306_WHITE);
}

void mostrarFase1()
{
  static uint32_t ultimaAtualizacao = 0;

  uint32_t agora = meu_millis();

  if (agora - ultimaAtualizacao < 200)
  {
    return;
  }

  ultimaAtualizacao = agora;

  display.clearDisplay();
  atualizarOLED();

  display.setCursor(0, 20);

  switch (escolhido)
  {
  case 4:
    display.println(F("KEY-8B52-O1"));
    break;

  case 5:
    display.println(F("KEY-BB52-01"));
    break;

  case 6:
    display.println(F("KEY-8B5Z-0l"));
    break;

  case 7:
    display.println(F("KEY-B8SZ-Ol"));
    break;
  }

  display.display();
}

void mostrarFase2()
{
  static uint32_t ultimaAtualizacao = 0;

  uint32_t agora = meu_millis();

  if (agora - ultimaAtualizacao < 250)
  {
    return;
  }

  ultimaAtualizacao = agora;

  display.clearDisplay();
  atualizarOLED();

  display.setCursor(40, 20);
  display.println(F("FASE 2"));

  display.setCursor(16, 35);
  display.println(F("Ajuste os"));

  display.setCursor(16, 45);
  display.println(F("potenciometros"));

  display.display();
}

void mostrarFase3()
{
  static uint32_t ultimaAtualizacao = 0;

  uint32_t agora = meu_millis();

  if (agora - ultimaAtualizacao < 250)
  {
    return;
  }

  ultimaAtualizacao = agora;

  display.clearDisplay();
  atualizarOLED();

  // Protocolo sorteado
  display.setCursor(0, 17);
  display.print(F("Protocolo: "));

  switch (protocolo)
  {
  case 1:
    display.print(F("ALFA"));
    break;
  case 2:
    display.print(F("BETA"));
    break;
  case 3:
    display.print(F("GAMA"));
    break;
  case 4:
    display.print(F("DELTA"));
    break;
  }

  // Codigo sorteado
  display.setCursor(0, 28);
  display.print(F("Codigo: "));
  display.print(codigo);

  // Condicao inicial
  display.setCursor(0, 39);
  display.print(F("Inicial: "));

  if (condicao_inicial == 1)
  {
    display.print(F("CLARO"));
  }
  else
  {
    display.print(F("ESCURO"));
  }

  // Instrucao ao jogador
  display.setCursor(0, 54);

  if (!jogo3_iniciado)
  {
    display.print(F("# PARA INICIAR"));
  }
  else
  {
    display.print(F("ACAO "));
    display.print(acao_atual);
  }

  display.display();
}

void mostrarFase4()
{
  static uint32_t ultimaAtualizacao = 0;
  uint32_t agora = meu_millis();

  if (agora - ultimaAtualizacao < 250)
  {
    return;
  }
  ultimaAtualizacao = agora;

  display.clearDisplay();
  atualizarOLED();

  display.setCursor(0, 20);
  if (PodeTocarDenovo)
  {
    display.println(F("*: tocar novamente"));
  }
  else
  {
    display.println(F("Replay usado"));
  }

  display.display();
}

void setup()
{

  Serial.begin(9600);
  Wire.begin();
  escreverPCF(0xFF);

  // ---------- inicialização da tela ------------
  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C))
  {
    Serial.println(F("SSD1306 allocation failed"));
    for (;;)
      ;
  }

  display.clearDisplay();

  display.setTextSize(1); // Escala 1:1
  display.setTextColor(SSD1306_WHITE);

  display.setCursor(0, 25); // posição do texto

  display.print(F("Iniciando...")); // envia mensagem p/ display
  display.println((char)7);         // Envia caracteres (ver https://www.ascii-codes.com/, CP 437 )

  display.display(); // faz display mostrar mensagem

  // ------- fim da inicialização da tela ---------
  // Habilita o ADC e configura prescaler 128, material da prof
  ADCSRA = (1 << ADEN) |
           (1 << ADPS2) |
           (1 << ADPS1) |
           (1 << ADPS0);

  // Usa ADC0 como semente para o sorteio
  randomSeed(meu_analogRead(0));
  escolhido = random(4, 8);
  Serial.print(F("Switch sorteado: "));
  Serial.println(escolhido);

  protocolo = random(1, 5);
  Serial.print(F("Protocolo sorteado: "));
  Serial.println(protocolo);

  codigo = random(1, 5);
  Serial.print(F("Codio sorteado: "));
  Serial.println(codigo);

  condicao_inicial = random(0, 2);
  Serial.print(F("Condicao inicial sorteada: "));
  Serial.println(condicao_inicial);

  escolhido2 = random(0, 4);
  Serial.print(F("Sequencia sorteada: "));
  Serial.println(escolhido2);

  // config pinos para a fase1

  set_bit(DDRB, led_1);
  clr_bit(PORTB, led_1); // Config leds (PB0 até PB3)
  set_bit(DDRB, led_2);
  clr_bit(PORTB, led_2);
  set_bit(DDRB, led_3);
  clr_bit(PORTB, led_3);
  set_bit(DDRB, led_4);
  clr_bit(PORTB, led_4);

  // LED verde da fase 2 - D12 (PB4)
  set_bit(DDRB, PB4);
  clr_bit(PORTB, PB4);

  // Buzzer - D11 (PB3)
  set_bit(DDRB, PB3);
  clr_bit(PORTB, PB3);

  clr_bit(DDRD, botao);
  set_bit(PORTD, botao); // config botão e switches (PD3 até PD7)
  clr_bit(DDRD, chave_4);
  set_bit(PORTD, chave_4);
  clr_bit(DDRD, chave_5);
  set_bit(PORTD, chave_5);
  clr_bit(DDRD, chave_6);
  set_bit(PORTD, chave_6);
  clr_bit(DDRD, chave_7);
  set_bit(PORTD, chave_7);

  //config do buzzer
  buzzerInicializar();
  cli();

  // setup interrupcao
  EICRA = 0b1000;
  EIFR = 0b10;
  EIMSK = 0b10;

  // Timer1 em CTC - interrupcao a cada 1 ms
  TCCR1A = 0;
  TCCR1B = 0;
  TCNT1 = 0;

  OCR1A = 249;
  TIMSK1 = (1 << OCIE1A);
  TCCR1B = (1 << WGM12) | (1 << CS11) | (1 << CS10);

  // Timer2 em CTC - interrupcao a cada 1 ms
  TCCR2A = 0;
  TCCR2B = 0;
  TCNT2 = 0;

  TCCR2A = (1 << WGM21);
  OCR2A = 249;
  TIMSK2 = (1 << OCIE2A);
  TCCR2B = 0;

  sei();
}

void executarFase1()
{
  mostrarFase1();
  static uint32_t ultimoTempo = 0;

  if (flag_botao)
  {
    flag_botao = false;

    uint32_t agora = meu_millis();
    if (agora - ultimoTempo < 40) // debouncing do botao
    {
      return;
    }
    ultimoTempo = agora; // Start cooldown from this valid press

    // Define qual combinacao de switches esperamos
    uint8_t esperado = 0b11110000;
    clr_bit(esperado, escolhido);

    // Verifica os switches
    if ((PIND & 0b11110000) == esperado)
    {
      Serial.println(F("FASE 1 CONCLUIDA!"));

      estadoAtual = FASE_2;
    }
    else
    {
      registrarErros();

      Serial.println(F("SWITCH INCORRETO!"));
      Serial.print(F("Erros: "));
      Serial.println(erros);
    }
  }
}

void executarFase2()
{
  // Inicializa a fase apenas na primeira entrada
  if (!fase2Iniciada)
  {
    fase2Iniciada = true;

    nova_tentativa_fase_2();

    Serial.println(F("FASE 2 INICIADA!"));
  }

  // Atualiza os LEDs e verifica os potenciometros
  pisca_led();
  resposta_potenciometros();

  // Se o jogador acertou
  if (fase2_concluida)
  {
    Serial.println(F("FASE 2 CONCLUIDA!"));

    // Desliga os LEDs da fase 2
    clr_bit(PORTB, PB0);
    clr_bit(PORTB, PB1);
    clr_bit(PORTB, PB2);

    // Avanca para a fase 3
    estadoAtual = FASE_3;

    return;
  }

  // Se a tentativa terminou por tempo
  if (fase2_falhou)
  {
    // Verifica se o jogador perdeu o jogo
    if (erros >= 3 || flag_480s)
    {
      clr_bit(PORTB, PB0); // D9
      clr_bit(PORTB, PB1); // D10
      clr_bit(PORTB, PB2); // D12

      estadoAtual = GAME_OVER;
      return;
    }

    // Se ainda pode tentar, inicia outra tentativa
    Serial.println(F("NOVA TENTATIVA DA FASE 2"));

    nova_tentativa_fase_2();
  }

  // Atualiza o OLED periodicamente
  mostrarFase2();
}

void configurarProtocolo3()

{
  switch (protocolo)
  {
  case 1: // ALFA
    estadoAcao1 = condicao_inicial;
    estadoAcao3 = !estadoAcao1;
    numeroEsperado3 = codigo + 2;
    acao3 = 3000;

    if (codigo % 2 == 0)
      acao1 = 2000;
    else
      acao1 = 4000;

    break;

  case 2: // BETA
    estadoAcao1 = !condicao_inicial;
    estadoAcao3 = condicao_inicial;
    numeroEsperado3 = codigo + 1;
    acao3 = 2000;

    if (codigo % 2 == 0)
      acao1 = 5000;
    else
      acao1 = 3000;

    break;

  case 3: // GAMA
    estadoAcao1 = condicao_inicial;
    estadoAcao3 = !estadoAcao1;
    numeroEsperado3 = codigo + 3;
    acao3 = 4000;

    if (codigo % 2 == 0)
      acao1 = 3000;
    else
      acao1 = 5000;

    break;

  case 4: // DELTA
    estadoAcao1 = !condicao_inicial;
    estadoAcao3 = condicao_inicial;
    numeroEsperado3 = codigo + 4;
    acao3 = 3000;

    if (codigo % 2 == 0)
      acao1 = 4000;
    else
      acao1 = 2000;

    break;
  }
}
void executarFase3()

{
  mostrarFase3();

  // Aguardando o jogador iniciar o protocolo
  if (!jogo3_iniciado)
  {
    char tecla = lerTeclaNova();

    if (tecla == '#')
    {
      jogo3_iniciado = true;
      acao_atual = 1;

      configurarProtocolo3();

      // Registra o estado inicial do LDR
      claro_antes = (meu_analogRead(ldr) >= limite_luz);
      inicio_estado = meu_millis();

      Serial.println(F("FASE 3 INICIADA!"));
    }

    return;
  }

  // ACAO 1 - Verificar estado do LDR
  if (acao_atual == 1)
  {
    uint32_t agora = meu_millis();

    bool claro = (meu_analogRead(ldr) >= limite_luz);

    // Detecta mudança de luminosidade
    if (claro != claro_antes)
    {
      claro_antes = claro;
      inicio_estado = agora;
    }

    // Quanto tempo permaneceu nesse estado?
    uint32_t duracao_estado = agora - inicio_estado;

    if (duracao_estado >= acao1)
    {
      if (claro == estadoAcao1)
      {
        Serial.println(F("ACAO 1 CONCLUIDA!"));

        acao_atual = 2;
      }
      else
      {
        registrarErros();
        inicio_estado = agora;

        Serial.println(F("ERRO NA ACAO 1"));
      }
    }
  }
  // ACAO 2 - Verificar numero digitado
  if (acao_atual == 2)
  {
    char tecla = lerTeclaNova();

    if (tecla == numeroEsperado3 + '0')
    {
      Serial.println(F("ACAO 2 CONCLUIDA!"));

      // Prepara a contagem de tempo da acao 3
      claro_antes = (meu_analogRead(ldr) >= limite_luz);
      inicio_estado = meu_millis();

      acao_atual = 3;
    }
    else if (tecla != '\0')
    {
      registrarErros();

      Serial.println(F("ERRO NA ACAO 2"));
    }
  }
  // ACAO 3 - Verificar estado do LDR
  if (acao_atual == 3)
  {
    uint32_t agora = meu_millis();

    bool claro = (meu_analogRead(ldr) >= limite_luz);

    // Detecta mudança de luminosidade
    if (claro != claro_antes)
    {
      claro_antes = claro;
      inicio_estado = agora;
    }

    // Calcula a duração do estado atual
    uint32_t duracao_estado = agora - inicio_estado;

    if (duracao_estado >= acao3)
    {
      if (claro == estadoAcao3)
      {
        Serial.println(F("ACAO 3 CONCLUIDA!"));
        Serial.println(F("FASE 3 CONCLUIDA!"));

        estadoAtual = FASE_4;
      }
      else
      {
        registrarErros();
        inicio_estado = agora;

        Serial.println(F("ERRO NA ACAO 3"));
      }
    }
  }
}

void executarFase4()
{
  uint32_t agora = meu_millis();

  // Primeira entrada na fase 4
  if (!fase4Iniciada)
  {
    agora = meu_millis(); // Atualiza o tempo apos o delay
    fase4Iniciada = true;

    passoLed4 = 0;
    led4Aceso = true;
    instanteLed4 = agora;

    delay(2000); // Espera 2 segundos antes de mostrar a sequencia
    set_bit(PORTB, sequencias[escolhido2][passoLed4]);
  }

  // Ainda existem LEDs na sequencia?
  if (passoLed4 < 4)
  {
    // Verifica se o LED ficou ligado por 500 ms
    if (led4Aceso && agora - instanteLed4 >= 500)
    {
      clr_bit(PORTB, sequencias[escolhido2][passoLed4]);

      led4Aceso = false;
      instanteLed4 = agora;
    }

    // Verifica se ficou desligado por 200 ms
    else if (!led4Aceso && agora - instanteLed4 >= 200)
    {
      passoLed4++;

      if (passoLed4 < 4)
      {
        set_bit(PORTB, sequencias[escolhido2][passoLed4]);

        led4Aceso = true;
        instanteLed4 = agora;
      }
    }
  }
  // Sequencia terminou: permite repetir uma vez
  // A sequência terminou: libera o keypad
  if (passoLed4 >= 4)
  {
    char tecla = lerTeclaNova();

    // Repetir a sequência uma única vez
    if (tecla == '*' && PodeTocarDenovo)
    {
      PodeTocarDenovo = false;

      passoLed4 = 0;
      led4Aceso = true;
      instanteLed4 = agora;

      set_bit(PORTB, sequencias[escolhido2][passoLed4]);

      Serial.println(F("REPLAY UTILIZADO!"));
    }

    // Limpar os dígitos digitados
    else if (tecla == '#')
    {
      digito = 0;
      Serial.println(F("ENTRADA LIMPA"));
    }

    // Registrar um dígito de 0 a 9
    else if (tecla >= '0' && tecla <= '9' && digito < 6)
    {
      tentativa[digito] = tecla;
      digito++;

      Serial.print(F("Digitado: "));
      Serial.println(tecla);

      // Recebemos os seis dígitos?
      if (digito == 6)
      {
        bool correto = true;

        for (uint8_t i = 0; i < 6; i++)
        {
          if (tentativa[i] != codigo_sequencias[escolhido2][i])
          {
            correto = false;
            break;
          }
        }

        if (correto)
        {
          Serial.println(F("FASE 4 CONCLUIDA!"));
          estadoAtual = VITORIA;
        }
        else
        {
          registrarErros();
          digito = 0;

          Serial.println(F("CODIGO INCORRETO!"));
          Serial.print(F("Erros: "));
          Serial.println(erros);
        }
      }
    }
  }
  mostrarFase4();
}

void loop()
{
  bool jogo_ativo = (estadoAtual != INICIO &&
                     estadoAtual != VITORIA &&
                     estadoAtual != GAME_OVER);

  if (estadoAtual != INICIO &&
      estadoAtual != VITORIA &&
      estadoAtual != GAME_OVER)
  {
    atualizarTempo();

    if (erros >= 3 or flag_480s)
    {
      estadoAtual = GAME_OVER;
    }
  }

  atualizarBuzzer(jogo_ativo, tempo_restante);

  switch (estadoAtual)
  {
  case INICIO:
  {

    // Tela inicial e comando para começar

    if (!telaInicioMostrada)
    {
      display.clearDisplay();
      display.setTextSize(1);
      display.setTextColor(SSD1306_WHITE);

      display.setCursor(37, 1);
      display.println(F("BOMB GAME"));

      display.setCursor(43, 12);
      display.println(F("Made by"));

      display.setCursor(16, 23);
      display.println(F("Leonardo, Xavier"));

      display.setCursor(37, 33);
      display.println(F("& Matheus"));

      display.setCursor(7, 45);
      display.println(F("Escolham as funcoes"));

      display.setCursor(22, 55);
      display.println(F("# PARA INICIAR"));

      display.display();

      telaInicioMostrada = true;
    }

    char tecla = lerTeclaNova();

    if (tecla == '#')
    {
      TCNT2 = 0;
      contador_ms = 0;
      segundos = 0;
      erros = 0;
      flag_480s = false;
      flag_botao = false;

      TCCR2B = (1 << CS22);

      estadoAtual = FASE_1;

      Serial.println(F("JOGO INICIADO!"));
    }

    break;
  }

  case FASE_1:
    // Executar fase 1
    executarFase1();
    break;

  case FASE_2:
    // Executar fase 2
    executarFase2();
    break;

  case FASE_3:
    // Executar fase 3
    executarFase3();
    break;

  case FASE_4:
    // Executar fase 4
    executarFase4();
    break;

  case VITORIA:
  {

    if (!telaVitoriaMostrada)
    {
      TCCR2B = 0; // Para o cronometro

      atualizarTempo();

      display.clearDisplay();
      atualizarOLED();

      display.setCursor(15, 22);
      display.println(F("BOMBA DESARMADA!"));

      display.setCursor(15, 36);
      display.println(F("VOCES VENCERAM!"));

      display.setCursor(0, 52);
      display.println(F("R:Serial *:Keypad"));

      display.display();

      Serial.println(F("VITORIA!"));
      tocarSom(Vitoria);

      telaVitoriaMostrada = true;
    }

    verificarReinicio();

    break;
  }
  case GAME_OVER:
  {

    if (!telaGameOverMostrada)
    {
      TCCR2B = 0;          // Para o cronometro
      PORTB &= 0b11110000; // Desliga os 4 LEDs
      clr_bit(PORTB, PB4);
      atualizarTempo();

      display.clearDisplay();
      atualizarOLED();

      display.setCursor(0, 20);
      display.println(F("GAME OVER"));

      display.setCursor(0, 34);

      if (erros >= 3)
      {
        display.println(F("3 erros cometidos"));
      }
      else if (flag_480s)
      {
        display.println(F("Tempo esgotado"));
      }

      display.setCursor(0, 50);
      display.println(F("R:Serial *:Keypad"));

      display.display();

      Serial.println(F("Game Over"));
      tocarSom(Game_Over);

      telaGameOverMostrada = true;
    }

    verificarReinicio();

    break;
  }
  }
}