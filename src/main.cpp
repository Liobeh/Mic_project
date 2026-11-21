// Libraries
#include <Arduino.h>
#include <stdlib.h>

// Bibliotecas p/ tela
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

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
uint8_t condicao_inicial;
#define ldr 1 // A1

// game 4
uint8_t escolhido2;
bool PodeTocarDenovo = true;
uint8_t digito = 0;
char tentativa[6];

// interrupcoes
volatile bool flag_480s = false;  // flag tempo
volatile bool flag_botao = false; // flag botao pressionado

// timers
volatile uint32_t ms = 0;
volatile uint16_t contador_ms = 0;
volatile uint16_t segundos = 0; // timer
const uint16_t tempo_limite = 480;
uint16_t tempo_restante = 0;

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
  ADCSRA |= (1 << ADSC);

  // Espera a conversao terminar
  while (ADCSRA & (1 << ADSC))
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

    estado &= ~(1 << pinosLinhas[i]);

    escreverPCF(estado);
    delayMicroseconds(100);

    byte leitura = lerPCF();

    for (int j = 0; j < 3; j++)
    {
      if (!(leitura & (1 << pinosColunas[j])))
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

// erros
uint8_t erros = 0;

void registrarErros(){
  erros++;
}

// fase1
//  Setup dos pinos
#define chave_4 4
#define chave_5 5
#define chave_6 6
#define chave_7 7
#define botao 3

#define led_1 8
#define led_2 9
#define led_3 10
#define led_4 11

//sequências
const uint8_t sequencias[4][4] = {
  {led_1, led_2, led_3, led_4},
  {led_4, led_3, led_2, led_1},
  {led_1, led_3, led_2, led_4},
  {led_3, led_2, led_4, led_1}
};

// códigos p/ cada sequência.
const char codigo_sequencias[4][6] = {
  {'7','7','3','8','9','4'}, // seq1
  {'7','7','3','4','9','2'}, // seq2
  {'7','1','3','8','9','2'}, // seq3
  {'7','7','8','4','6','2'}};// seq4
 

void Sequencia (const uint8_t seq[], uint8_t passos) {
  /* acende e desliga LEDs na ordem da lista
  */
  for (uint8_t i = 0; i < passos; i++) {
    uint8_t atual = seq[i];

    digitalWrite(atual, HIGH);
    delay(500);
    digitalWrite(atual, LOW);
    delay(200);
  }
}

void TocaSequencia () {
  Sequencia(sequencias[escolhido2], 4);
}

// rotina de interrupcao do botao
ISR(INT1_vect)
{
  flag_botao = true;
}

ISR(TIMER1_COMPA_vect)
{
  ms++;
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

// Display
void atualizarOLED()
{
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);

  uint8_t minutos = tempo_restante / 60;
  uint8_t segundos_restantes = tempo_restante % 60;

  display.setCursor(0, 0);
  display.print("Tempo: ");
  display.print(minutos);
  display.print(":");

  if (segundos_restantes < 10)
  {
    display.print("0");
  }

  display.print(segundos_restantes);
  display.setCursor(88, 0);
  display.print("E:");
  display.print(erros);

  display.drawLine(0, 12, 127, 12, SSD1306_WHITE);
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
  Serial.print("Switch sorteado: ");
  Serial.println(escolhido);

  protocolo = random(1, 5);
  Serial.print("Protocolo sorteado: ");
  Serial.println(protocolo);

  codigo = random(1, 5);
  Serial.print("Codio sorteado: ");
  Serial.println(codigo);

  condicao_inicial = random(0, 2);
  Serial.print("Condicao inicial sorteada: ");
  Serial.println(condicao_inicial);

  escolhido2 = random(0, 4);
  Serial.print("Sequencia sorteada: ");
  Serial.println(escolhido2);

  // config pinos para a fase1

  pinMode(led_1, OUTPUT); //config dos LEDs
  pinMode(led_2, OUTPUT); // trocar função por registradores
  pinMode(led_3, OUTPUT); // (que nem no codigo em baixo)
  pinMode(led_4, OUTPUT); 
  
  DDRD &= ~((1 << 3) | (1 << 4) | (1 << 5) | (1 << 6) | (1 << 7)); // config dos switches
  PORTD |= (1 << 3) | (1 << 4) | (1 << 5) | (1 << 6) | (1 << 7);

  // setup registradores principais
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
  TCCR2B = (1 << CS22);

  sei();
}

// funcao auto-explicativa
void gameover()
{
  TCCR2B = 0; // Para o timer

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
  display.println(F("Reinicie: RESET"));

  display.display();

  Serial.println("Game Over");

  while (1)
  {
  }
}

void loop()
{

  // jogo 1

  // while (1)
  // {
  //   atualizarTempo(); // calcula tempo restante

  //   // os switches disponiveis sao os switches 1 a 4

  //   // Aparece um engima no painel associado ao switch escolhido, que o jogador com a bomba deve descrever para o jogador com o manual
  //   // o jogador com o manual deve descobrir qual eh o switch correto para ligar
  //   // o jogador com a bomba liga o switch e aperta um botao de confirmacao

  //   // display

  //   display.clearDisplay();
  //   atualizarOLED();

  //   display.setTextSize(1);
  //   display.setTextColor(SSD1306_WHITE);

  //   display.setCursor(0, 20);

  //   // escolhe um códgio diferente p/ cada switch
  //   switch (escolhido)
  //   {
  //   case 4:
  //     display.println(F("KEY-8B52-O1"));
  //     break;
  //   case 5:
  //     display.println(F("KEY-BB52-01"));
  //     break;
  //   case 6:
  //     display.println(F("KEY-8B5Z-0l"));
  //     break;
  //   case 7:
  //     display.println(F("KEY-B8SZ-Ol"));
  //     break;
  //   }

  //   display.display();

  //   if (flag_botao)
  //   {
  //     flag_botao = false;

  //     Serial.println("BOTAO PRESSIONADO!");

  //     // IA

  //     // Identifica o bit correspondente ao switch sorteado
  //     uint8_t mascara = (1 << escolhido);

  //     // Define a combinação correta dos quatro switches
  //     uint8_t esperado = 0b11110000 & ~mascara;

  //     // Verifica se somente o switch sorteado está ligado
  //     if ((PIND & 0b11110000) == esperado)
  //     {
  //       Serial.println("SWITCH CORRETO!");
  //       break;
  //     }

  //     else
  //     {
  //       registrarErros();
  //       Serial.println(erros);
  //     }
  //   }

  //   // precisamos manter a checagem da interrupcao e dos erros

  //   if (erros >= 3)
  //   {
  //     gameover();
  //   }

  //   if (flag_480s)
  //   {
  //     gameover();
  //     flag_480s = false;
  //   }
  // }

  // Fase 3 - Protocolo de Emergencia

  
  while (1)
  {
    atualizarTempo();
    mostrarFase3();

    if (flag_480s)
    {
      gameover();
    }

    char tecla = lerTeclaNova();

    // Preparacao da fase
    if (!jogo3_iniciado)
    {
      // OLED mostra protocolo, codigo e condicao inicial
      // Jogador consulta o manual e pressiona # para iniciar

      if (tecla == '#')
      {

        jogo3_iniciado = true;

        // Registra o estado inicial do LDR
        claro_antes = (meu_analogRead(ldr) >= limite_luz);
        inicio_estado = meu_millis();

        Serial.println(F("FASE 3 INICIADA!"));
      }
    }

    // Execucao da fase
    else
    {
      uint8_t numero;

      switch (protocolo)
      {

      case 1: // ALFA
        estadoAcao1 = condicao_inicial;
        estadoAcao3 = !estadoAcao1;
        numero = codigo + 2;
        acao3 = 3000;

        if (codigo % 2 == 0)
          acao1 = 2000;
        else
          acao1 = 4000;

        break;

      case 2: // BETA
        estadoAcao1 = !condicao_inicial;
        estadoAcao3 = condicao_inicial;
        numero = codigo + 1;
        acao3 = 2000;

        if (codigo % 2 == 0)
          acao1 = 5000;
        else
          acao1 = 3000;

        break;

      case 3: // GAMA
        estadoAcao1 = condicao_inicial;
        estadoAcao3 = !estadoAcao1;
        numero = codigo + 3;
        acao3 = 4000;

        if (codigo % 2 == 0)
          acao1 = 3000;
        else
          acao1 = 5000;

        break;

      case 4: // DELTA
        estadoAcao1 = !condicao_inicial;
        estadoAcao3 = condicao_inicial;
        numero = codigo + 4;
        acao3 = 3000;

        if (codigo % 2 == 0)
          acao1 = 4000;
        else
          acao1 = 2000;

        break;
      }

      acao_atual = 1;

      while (1)
      {

        atualizarTempo();
        mostrarFase3();

        uint16_t luminosidade = meu_analogRead(ldr);
        bool claro = (luminosidade >= limite_luz);

        if (claro != claro_antes)
        {
          inicio_estado = meu_millis();
          claro_antes = claro;
        }

        unsigned long duracao_estado = meu_millis() - inicio_estado;

        if (duracao_estado >= acao1 && claro == estadoAcao1)
        {
          Serial.println(F("Acao 1 concluida"));
          break;
        }

        if (duracao_estado >= acao1 && claro != estadoAcao1)
        {
          registrarErros();
          Serial.println(erros);
          inicio_estado = meu_millis();
        }

        if (erros >= 3)
        {
          gameover();
        }

        if (flag_480s)
        {
          gameover();
        }

        delay(20);
      }
      acao_atual = 2;

      while (1)
      {
        atualizarTempo();
        mostrarFase3();
        tecla = lerTeclaNova();
        if (tecla == numero + '0')
        {
          Serial.println(F("Acao 2 completa"));

          inicio_estado = meu_millis();
          claro_antes = (meu_analogRead(ldr) >= limite_luz);
          break;
        }

        if (tecla != '\0' && tecla != numero + '0')
        {
          registrarErros();
          Serial.println(erros);
        }

        if (erros >= 3)
        {
          gameover();
        }

        if (flag_480s)
        {
          gameover();
        }
      }

      acao_atual = 3;
      while (1)
      {
        atualizarTempo();
        mostrarFase3();

        uint16_t luminosidade = meu_analogRead(ldr);
        bool claro = (luminosidade >= limite_luz);

        if (claro != claro_antes)
        {
          inicio_estado = meu_millis();
          claro_antes = claro;
        }

        unsigned long duracao_estado = meu_millis() - inicio_estado;

        if (duracao_estado >= acao3 && claro == estadoAcao3)
        {
          Serial.println(("Acao 3 concluida"));
          break;
        }

        if (duracao_estado >= acao3 && claro != estadoAcao3)
        {
          registrarErros();

          Serial.print("ERRO ACAO 3 | ADC: ");
          Serial.print(luminosidade);

          Serial.print(" | Esperado: ");
          Serial.print(estadoAcao3 ? "CLARO" : "ESCURO");

          Serial.print(" | Lido: ");
          Serial.println(claro ? "CLARO" : "ESCURO");

          inicio_estado = meu_millis();
        }

        if (erros >= 3)
        {
          gameover();
        }

        if (flag_480s)
        {
          gameover();
        }
        delay(20);
      }
      Serial.println(F("FASE 3 CONCLUIDA!"));
      break;
    }
  }

  
  Serial.println(F("prox jogo"));
  atualizarTempo();
  delay(5000);

  // game 4
  /* 
  LEDs acendem em uma sequência específica
  Cada sequência corresponde a um código de 6 digitos que precisa ser digitado no Keypad
  O jogador pode tocar a sequência novamente uma única vez.
  Digitar um código de 6 digitos errado conta como 1 erro.
  sequências: (Cores a detrminar)
      seq1: LED 1, LED 2, LED 3, LED 4 
      seq2: LED 4, LED 3, LED 2, LED 1
      seq3: LED 1, LED 3, LED 2, LED 4
      seq4: LED 3, LED 2, LED 4, LED 1
  códigos:
      seq1: 773894
      seq2: 773492
      seq3: 713892
      seq4: 778462
  */

//   display.clearDisplay();

//   display.setTextSize(1);
//   display.setTextColor(SSD1306_WHITE);
//   display.setCursor(0, 20);

//   display.println(F("*: tocar novamente")); //testar como fica no display
//   display.display();
//   TocaSequencia(); // toca a seq. 1 vez

//   while(1) 
//   {
//     if (erros >= 3 || flag_480s) {
//     gameover();
//     }
//     char tecla = lerTeclaNova();

//     if (tecla != '\0') 
//     {
//       if (tecla == '*') 
//       {
//         if (PodeTocarDenovo) 
//         {
//           TocaSequencia();
//           PodeTocarDenovo = false;
//           display.println(F("Replay usado")); //testar como fica no display
//           display.display();
//           Serial.println(F("Replay usado"));
//         }
//       }
//       else if (tecla == '#') // limpar a tentativa (caso aperte uma tecla sem querer)
//       {
//         digito = 0;
//         Serial.println(F("Entrada limpa"));
//       }

//       else if (digito < 6)
//       {
//         tentativa[digito] = tecla;
//         digito++;
//         Serial.print(F("Digitado: "));
//         Serial.println(tecla);
//         }
//     }
//     if (digito >= 6)
//       {
//         bool correto = true;
//         for (uint8_t i = 0; i < 6; i++)
//         {
//           if (tentativa[i] != codigo_sequencias[escolhido2][i])
//           {
//             correto = false;
//             break;
//           }
//         }
//         if (correto)
//         {
//           Serial.println(F("Sequencia correta. Jogo Encerrado")); // 
//           break;
//         }
//         else
//         {
//           Serial.println(F("Codigo Incorreto"));
//           registrarErros();
//           Serial.println(erros);
//           digito = 0;
//         }
//       }
//   }
//   while (1){} // pausa o jogo pra ele não resetar automaticamente
// }
}