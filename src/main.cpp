  // Libraries

  #include <Arduino.h>
  #include <stdlib.h>



  // Setup das variaveis

  //game 2
  bool claro_antes; 
  uint16_t limite_luz = 300;
  unsigned long inicio_estado = 0;

  volatile bool flag_480s = false; //flag tempo

  volatile bool flag_botao = false; //flag botao pressionado

  volatile uint16_t segundos = 0; // timer
  const uint16_t tempo_limite = 480; 
  uint16_t tempo_restante = 0;

  uint8_t escolhido;
  uint8_t erros = 0;

  // Setup dos pinos
  #define ldr 1 //A1

  #define chave_4 4
  #define chave_5 5
  #define chave_6 6
  #define chave_7 7

  #define botao 3 





  // Leitura de um canal analogico, material da prof
  uint16_t meu_analogRead(uint8_t canal) {

      ADMUX = (1 << REFS0) | canal;

      // Inicia a conversao
      ADCSRA |= (1 << ADSC);

      // Espera a conversao terminar
      while (ADCSRA & (1 << ADSC));

      // Retorna o resultado
      return ADC;
  }


  //rotina de interrupcao do botao 
  ISR(INT1_vect) {
    flag_botao = true;
  }


  ISR(TIMER1_COMPA_vect) //chamada a cada 1s, material da prof
  {
    segundos++; 
    
    if (segundos >= tempo_limite){ //checagem de 8 mins
      flag_480s = true; 
    }
      
  }

  void setup()
  {
    
    Serial.begin(9600);

    // Habilita o ADC e configura prescaler 128, material da prof
    ADCSRA = (1 << ADEN) |
            (1 << ADPS2) |
            (1 << ADPS1) |
            (1 << ADPS0);

    // Usa ADC0 como semente para o sorteio
    randomSeed(meu_analogRead(0));

    escolhido = random(4, 8);


    //config pinos para o game1
    DDRD &= ~((1 << 3) | (1 << 4) | (1 << 5) | (1 << 6) | (1 << 7));
    PORTD |= (1 << 3) | (1 << 4) | (1 << 5) | (1 << 6) | (1 << 7);

    Serial.println(escolhido);


    

    //setup registradores principais
    cli();

    //setup interrupcao
    EICRA = 0b1000;
    EIFR = 0b10;
    EIMSK = 0b10;


    // Setup do timer 
    TCCR1A = 0; /* nenhum pino ligado ao timer */
    TCCR1B = 0; /* timer parado durante a configuração */
    OCR1A = 62499; /* 62 500 passos de 16 us = 1 s */
    TIMSK1 = (1 << OCIE1A); /* interrupção de comparação A */
    TCCR1B = (1 << WGM12) | (1 << CS12); /* modo CTC + prescaler 256 */

    sei();
    
  }

  // funcao auto-explicativa
  void gameover(){
    flag_480s = false;
    TCCR1B = 0; /* timer parado */
    Serial.println("Game Over");
    Serial.println("Faça tal para reiniciar o jogo");
    while(1){

    }
    
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

    
    //game3
    
    claro_antes = (meu_analogRead(ldr) < limite_luz);
    inicio_estado = millis();

    while(1) {
      
    // tela mostra protocolo, codigo e condicao inicial

    //jogador interpreta e conta pro jogador com o manual

    //jogador executa procedimento 
    // procedimento eh se cobrir ou n ldr, digitar codigo e depois mais um procedimento com ldr

      uint16_t  lumionosidade = meu_analogRead(ldr);
      bool claro = true;
      



      if (lumionosidade >= limite_luz){
        claro = false;
      }
      else{
        claro = true;
      }
      
      if (claro != claro_antes) {
        inicio_estado = millis();

      if (claro == false) {
          Serial.println("LDR FOI COBERTO");
      }
      else {
          Serial.println("LDR FOI DESCOBERTO");
      }

      claro_antes = claro;
      
  }
      
      unsigned long duracao_estado = millis() - inicio_estado;
      Serial.println(claro);
      Serial.println(lumionosidade);
      
      Serial.print("Estado: ");

      if (claro) {
          Serial.print("CLARO");
      }
      else {
          Serial.print("ESCURO");
      }

      Serial.print(" | Duracao: ");
      Serial.print(duracao_estado);
      Serial.println(" ms");

      delay(1000);

    }


    Serial.println("prox jogo");
    tempo_restante = tempo_limite - segundos;
    delay(5000);
  }
