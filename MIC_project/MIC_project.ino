// Libraries

#include <stdlib.h>;


// Setup das variaveis

volatile bool flag_480s = false; //flag tempo


volatile uint16_t segundos = 0; // timer
uint16_t tempo_limite = 10; 
uint16_t tempo_restante = 0;
 
uint8_t erros = 0;

// Setup dos pinos
#define chave_4 4
#define chave_5 5
#define chave_6 6
#define chave_7 7

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

  // Setup do timer 
  cli();
  TCCR1A = 0; /* nenhum pino ligado ao timer */
  TCCR1B = 0; /* timer parado durante a configuração */
  OCR1A = 62499; /* 62 500 passos de 16 us = 1 s */
  TIMSK1 = (1 << OCIE1A); /* interrupção de comparação A */
  TCCR1B = (1 << WGM12) | (1 << CS12); /* modo CTC + prescaler 256 */
  sei();
  
}

// funcao auto-explicativa
void gameover(){
  TCCR1B = 0; /* timer parado */
  Serial.println("Game Over");
  Serial.println("Faça tal para reiniciar o jogo");
  
}



void loop(){

  //jogo 1
  
  while (1){
    
    // os switches disponiveis sao os switches 4 a 7
    
    int escolhido = (rand() % 4) + 4; // escolhe random entre 4 a 7 

    //Aparece um engima no painel associado ao switch escolhido, que o jogador com a bomba deve descrever para o jogador com o manual
    //o jogador com o manual deve descobrir qual eh o switch correto para ligar
    //o jogador com a bomba liga o switch e aperta um botao de confirmacao

    Serial.println(escolhido);
    
    // Se estiver correto o jogo acaba
    if (digitalRead(botao) == LOW and digitalRead(escolhido) == LOW){ 
      break
    }

    //se estiver errado incrementa aos erros
    elif (digitalRead(botao) == LOW){ 
      erros++
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

  
   
  tempo_restante = tempo_limite - segundos;
  
}
