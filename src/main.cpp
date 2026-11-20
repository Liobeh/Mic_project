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




// Definição de variaveis globais fase2 ADC 
//================================
//definindo componentes
int LeD_amarelo     = 11;
int LeD_azul        = 10;
int LeD_verde       = 12;
int Potenciometro_1 = A2;
int Potenciometro_2 = A3;
int buzzer          =  9;

// Parte dos leds
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
int configuracao_led_anterior = -1;
int conf_amarelo = -2; // a função deve ativar aleatoriamente um dos padrões pré existentes para o led acender
int conf_azul = -4;
int conf_verde = -3;
int pisca = HIGH;
int vetor_Leds[3] = {LeD_amarelo , LeD_azul, LeD_verde};
unsigned long tempo_pisca = 300;
unsigned long ultima_troca = 0;

// Parte dos potenciometros
  const int min_max_3x= -15; 
  const int nada = -20;
  const int meio = -11;   // 341<= meio <=682
  const int minimo = -9;  // 0<= minimo <=341
  const int maximo = -16; // 682<= maximo <=1023
  const int nenhuma = -1;
  const int tabela_resposta_pot[4][2]={ // tabela de respostas com potenciometros para cada configuração de leds 
    // pot1      pot2
    {min_max_3x,   nada},
    {nada,         meio},
    {minimo,     minimo},
    {maximo, min_max_3x},
  };
  int pot_valor_lido[2] = {0,0};
  int posicao_pot_lida[2] = {0,0};
  int contador_de_giros[2] = {0,0};
  int posicao_anterior[2] = {nenhuma, nenhuma};
  int vetor_pot[2]= {Potenciometro_1,Potenciometro_2};
  bool passou_pelo_minimo[2]= {0,0};
  bool fase2_concluida = false;
  bool fase2_falhou = false;
  unsigned long tempo_segurar = 1500; // tempo que o jogador deve segurarar o potenciometro na posição para validar a resposta
  unsigned long momento_chegada[2] = {0,0};
  unsigned long tempolimite_fase_2 = 10000; // ideal 1 minuto 60000 ms
  unsigned long momento_inicio = 0;

//=================================




// Funções da fase2 ADC
//=================================================================================================================================================================================
void acender_leds() //void acender_leds(): 
  {
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
  momento_inicio = millis();
  }

  void pisca_led()
  {
  if ((millis() - ultima_troca) >= tempo_pisca)
    {
    pisca = !pisca;// inverte o sentido de pisca
    ultima_troca = millis();
    }
      
  for (int i = 0; i < 3; i++) 
    {
    if (condicao_leds[configuracao_led_anterior][i] == piscando) 
      {
        digitalWrite(vetor_Leds[i], pisca);

    }
  }
  } 

//Funções de resposta dos potenciometros
int converter_posicao(int leitura_pot)
{

  if (leitura_pot <= 341)// condiconal de leitura de cada potenciometro,
  {
      return minimo;
  } 
  else if (leitura_pot <= 682)// condicional de leitura de cada potenciometro,
  {
      return meio;
  } 
  else// condicional de leitura de cada potenciometro,
  {
      return maximo;
  }
}






int confirmar_posicao (int indice_pot)          // indice_pot = 0 ou = 1
{
if (posicao_pot_lida[indice_pot] != posicao_anterior[indice_pot])       // mudou de posição
  {          
    if (posicao_anterior[indice_pot] != nenhuma)              // não é a primeira leitura
    {           
        momento_chegada[indice_pot] = millis();            // chegou: começa a contar
    
    }
    posicao_anterior[indice_pot] = posicao_pot_lida[indice_pot];
  }
if (momento_chegada[indice_pot] != 0 && millis() - momento_chegada[indice_pot] >= tempo_segurar)
{
    return posicao_pot_lida[indice_pot];
}
else{
    return nenhuma;
}
}





  void resposta_potenciometros()  
{

int posicao_confirmada = 0;

if (fase2_concluida || fase2_falhou) // já terminou?
  {
    return;//sai da função
  }

if (millis() - momento_inicio >= tempolimite_fase_2) // tempo esgotado?
  {
    fase2_falhou = true;
    erros ++;                                 // conta um erro para a
    return;
  }



bool certo[2] = {false, false};                 // um por potenciômetro
for (int indice = 0; indice < 2; indice++)
{
  pot_valor_lido[indice] = analogRead(vetor_pot[indice]);

  if (pot_valor_lido[indice] <= 341)// condiconal de leitura de cada potenciometro,
  {
      posicao_pot_lida[indice]= minimo;
  } 
  else if (pot_valor_lido[indice] <= 682)// condicional de leitura de cada potenciometro,
  {
      posicao_pot_lida[indice] = meio;
  } 
  else// condicional de leitura de cada potenciometro,
  {
      posicao_pot_lida[indice] = maximo;
  }

  if (posicao_pot_lida[indice] == minimo)
  {
    passou_pelo_minimo[indice] = true;
  }

  if (posicao_pot_lida[indice] == maximo && passou_pelo_minimo[indice])
  {
    contador_de_giros[indice] ++;
    passou_pelo_minimo[indice] = false;
  }

  posicao_confirmada = confirmar_posicao (indice); // verificar se o jogador está esperando o tempo_segurar 

  if (contador_de_giros[indice] == 3 ) // condicional para quando o potenciometro atinge vai mínimo ao máximo 3 vezes
  {
      posicao_confirmada = min_max_3x;
  }

  if (posicao_confirmada == tabela_resposta_pot[configuracao_led_anterior][indice] || tabela_resposta_pot[configuracao_led_anterior][indice] == nada)// condicional para quando a posição do potenciometro é igual a da tabela ou quando a resposta é não fazer nada
  {
    certo[indice] = true;
  }
  }
  if(certo[0] && certo[1])
  {
    fase2_concluida =true;
  }
  Serial.println(posicao_confirmada); // verificação do valor de posicao_confirmada para teste, deve retornar -1
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
  acender_leds(); // chama acender_leds para a nova tentativa ter outro padrão de leds 
}
//============================================================================================================================================================================

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
  // analogRead(A0) é a semente de aleatoriedade 
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
//=================================================================
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
        tone(buzzer, 300, 300);   //botar barulho do buzer aqui: tone(pino, frequencia_Hz, duracao_ms)
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
//====================================================================



// Jogo 2
//====================================================================
nova_tentativa_fase_2();          // começa a fase: sorteia a 1ª configuração

while (true)
{
    pisca_led();
    resposta_potenciometros();

    if (fase2_falhou)
    { 
      tone(buzzer, 300, 300);   //tone(pino, frequencia, duracao);
      nova_tentativa_fase_2();
    }
    if (fase2_concluida) 
    {
      break;
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
//====================================================================
  Serial.println("prox jogo");
  tempo_restante = tempo_limite - segundos;
  delay(5000);
}
