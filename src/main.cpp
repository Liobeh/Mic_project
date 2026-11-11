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
// Declaration for an SSD1306 display connected to I2C (SDA, SCL pins)
// The pins for I2C are defined by the Wire-library. 
// On an arduino UNO:       A4(SDA), A5(SCL)
#define OLED_RESET     -1 // Reset pin # (or -1 if sharing Arduino reset pin)
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);


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

  // SSD1306_SWITCHCAPVCC = generate display voltage from 3.3V internally
  // Address 0x3C is common for 128x64 I2C displays. Some may use 0x3D.
  if(!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) { 
    Serial.println(F("SSD1306 allocation failed"));
    for(;;); // Don't proceed, loop forever
  }

  // Clear the buffer
  display.clearDisplay();

  // Set text size and color
  display.setTextSize(1);      // Normal 1:1 pixel scale
  display.setTextColor(SSD1306_WHITE); // Draw white text
  
  // Set cursor to top-left corner
  display.setCursor(0, 0);     
  
  // Print text to the display buffer
  display.println(F("Hello, World!"));
  
  // Push the buffer to the screen to actually display it
  display.display();
  
  pinMode(chave_4,  INPUT_PULLUP); // config dos switches 
  pinMode(chave_5,  INPUT_PULLUP);
  pinMode(chave_6,  INPUT_PULLUP);
  pinMode(chave_7,  INPUT_PULLUP);

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

// funcao auto-explicativa
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
