// Libraries

#include <Arduino.h>
#include <stdlib.h>



// Setup das variaveis

volatile bool flag_480s = false; //flag tempo

volatile bool flag_botao = false; //flag botao pressionado

volatile uint16_t segundos = 0; // timer
uint16_t tempo_limite = 240; 
uint16_t tempo_restante = 0;

uint8_t escolhido;
uint8_t erros = 0;

// Setup dos pinos
#define chave_4 4
#define chave_5 5
#define chave_6 6
#define chave_7 7

#define botao 3 

// Definição de pinos fase2 ADC 
//================================
  int LeD_amarelo     = 11;
  int LeD_azul        = 10;
  int LeD_verde       = 12;
  int Potenciometro_1 = A0;
  int Potenciometro_2 = A1;
  int buzzer          =  9;
  int configuracao_led_anterior = -1;
//=================================

// Funções da fase2 ADC
//===================================================================================
void acender_leds() //void acender_leds(): 
  {
  int conf_amarelo; // a função deve ativar aleatoriamente um dos padrões pré existentes para o led acender
  int conf_azul;
  int conf_verde; 
  const int piscando = 2; // condição de quando o led pisca
  const int aceso = HIGH;
  const int apagado = LOW;
  const int condicao_leds[4][3] = {        // uma lista de tuplas com as diferentes configurações de leds acesos
  // amarelo, azul, verde
  {aceso, aceso, apagado},
  {apagado, apagado, piscando},
  {piscando, piscando, piscando},
  {apagado, piscando, aceso},
  };
  int configuracao_led_escolhida = random(0,4);   // um gerador pseudoaletório o qual indica uma das configurações para acender
  while (configuracao_led_escolhida == configuracao_led_anterior)
  {
    configuracao_led_escolhida = random(0,4);
  }
  configuracao_led_anterior = configuracao_led_escolhida;  
// a função deve ativar aleatoriamente um dos padrões pré existentes para o led acender
  conf_amarelo = condicao_leds[configuracao_led_escolhida][0]; 
  conf_azul    = condicao_leds[configuracao_led_escolhida][1];
  conf_verde   = condicao_leds[configuracao_led_escolhida][2];

  digitalWrite(LeD_amarelo,conf_amarelo);  // configuração do led amarelo:
  digitalWrite(LeD_azul,conf_azul);  // e configuração do led azul:
  digitalWrite(LeD_verde,conf_verde);  // configuração do led verde: 

  }


  void resposta_potenciometros()  //void resposta_potenciometros()
{
  // tabela de respostas com potenciometros para cada configuração de leds 
  // condicional de leitura de cada potenciometro, 
  // enquanto potenciometro_1 == valor da tabela e potenciometro_2 == valor da tabela
     // se já se passaram tantos segundos da ativação da função led 
     // retorna falso
  // retorna verdadeiro
}
//==========================================================================================

void interrupcaoBotao () {
  flag_botao = true;
}


ISR(TIMER1_COMPA_vect) /* chamada a cada 1 s */ 
{
  segundos++; 
  
  if (segundos >= tempo_limite){ //checagem de 8 mins
    flag_480s = true; 
  }
    
}

void setup()
{
  
  Serial.begin(9600);
  
  pinMode(chave_4,  INPUT_PULLUP); // config dos switches 
  pinMode(chave_5,  INPUT_PULLUP);
  pinMode(chave_6,  INPUT_PULLUP);
  pinMode(chave_7,  INPUT_PULLUP);

  
  // configuração de pinos da Fase 2 
  //====================================================
  randomSeed(analogRead(A5)); // analogRead(A5) é a semente de aleatoriedade 
  pinMode(LeD_azul, OUTPUT);
  pinMode(LeD_amarelo, OUTPUT);
  pinMode(LeD_verde, OUTPUT);
  pinMode(Potenciometro_1, INPUT);
  pinMode(Potenciometro_2 , INPUT);// Declarar os três leds como saída e o botão como entrada
  pinMode(buzzer, OUTPUT);

  //====================================================
  randomSeed(analogRead(0));
  escolhido = random(4, 8);
  
  DDRD &= ~((1 << 3) | (1 << 4) | (1 << 5) | (1 << 6) | (1 << 7));
  PORTD |= (1 << 3) | (1 << 4) | (1 << 5) | (1 << 6) | (1 << 7);

  Serial.println(escolhido);


  pinMode(botao, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(botao), interrupcaoBotao, FALLING);

  // Setup do timer 
  cli();
  TCCR1A = 0; /* nenhum pino ligado ao timer */
  TCCR1B = 0; /* timer parado durante a configuração */
  OCR1A = 62499; /* 62 500 passos de 16 us = 1 s */
  TIMSK1 = (1 << OCIE1A); /* interrupção de comparação A */
  TCCR1B = (1 << WGM12) | (1 << CS12); /* modo CTC + prescaler 256 */
  sei();
  
}

// funcao auto-explicativa, fim de jogo
void gameover(){
  TCCR1B = 0; /* timer parado */
  Serial.println("Game Over");
  Serial.println("Faça tal para reiniciar o jogo");
  delay(5000);
  
}



void loop(){

  //jogo 1
  
  while (1) {
    
    // os switches disponiveis sao os switches 1 a 4
    
    //Aparece um engima no painel associado ao switch escolhido, que o jogador com a bomba deve descrever para o jogador com o manual
    //o jogador com o manual deve descobrir qual eh o switch correto para ligar
    //o jogador com a bomba liga o switch e aperta um botao de confirmacao

    
    if (flag_botao) {
    
      
      Serial.println("BOTAO PRESSIONADO!");

        //IA
    
        // Identifica o bit correspondente ao switch sorteado
      uint8_t mascara = (1 << escolhido);

      // Define a combinação correta dos quatro switches
      uint8_t esperado = 0b11110000 & ~mascara;

      // Verifica se somente o switch sorteado está ligado
      if ((PIND & 0b11110000) == esperado) {
          Serial.println("SWITCH CORRETO!");
          break;
      }
      
      else {
        erros++;
        Serial.println(erros);
      }

      flag_botao = false;
    }
    

    //precisamos manter a checagem da interrupcao e dos erros
    
    if (erros >= 3){
      gameover(); 
      
    }
    
    if(flag_480s) {
     gameover();
     flag_480s = false;
    }
    
  }

  
  Serial.println("prox jogo");
  tempo_restante = tempo_limite - segundos;
  delay(5000);
}
