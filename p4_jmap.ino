#include "IRLremote.h"

/**@utor: José María Amusquívar Poppe.
 * Programa calculadora con dos operandos y decimales.
 * 06/05/19
 */
 
/**Librería necesaria para el control, funcionando a través de ondas infrarrojas,
 * se requiere de un receptor infrarrojo, compuesto de 3 pines, SIGNAL, GND, VCC.
 * El mando tiene una dirección propia, por lo que sus distintos botones producen
 * una codificación distinta a otros mandos. El código de cada pulsación ha sido
 * desglosado en un switch, en el que analiza cada uno.
 */

/*****Variables de acceso y control al bus I2C*****/
const int LEE_SCL = 2; //Puerto de entrada para leer el estado de la línea SCL.
const int LEE_SDA = 3; //Puerto de entrada para leer el estado de la línea SDA.
const int ESC_SCL = 4; //Puerto de salida para escribir el valor de la línea SCL.
const int ESC_SDA = 5; //Puerto de salida para escribir el valor de la línea SDA.
/**************************************************/

/*****Variables de control de los 7-segmentos y los dígitos******/
const byte num[10] = {0x3F,0x06,0x5B,0x4F,0x66,0x6D,0x7D,0x07,0x7F,0x6F}; //Valor de los dígitos 0-9 en los 7-segmentos.
const byte segmentos[4] = {0xFE, 0xFD, 0xFB, 0xF7}; //Valor para activar los digitos de los 7-segmentos.
/****************************************************************/

/**Array de char donde se guardan los dos números introducidos por teclado. Junto a sus respectivos doubles inicializados, y unos apuntadores**/
char primeroC[20] = ""; volatile double primero = -1.0; volatile int avanzaP = 0;
char segundoC[20] = ""; volatile double segundo = -1.0; volatile int avanzaS = 0;
/**********************************************************************************************************************************************/

volatile char opera = 0x00; //Inicialización del operador.
volatile boolean calculadoraFin = false; //Indicador de fin de la calculadora.
volatile boolean finJuego = true; //Indicador de fin de juego.

/*Variables de tiempo para controlar el rebote en cada caso.*/
volatile long timeTecla = 0;
volatile long timeBoton = 0;
volatile long timeComa = 0;
/************************************************************/

/*Apuntadores para cada digito*/
volatile int unid = 0;
volatile int dec = 0;
volatile int cent = 0;
volatile int mil = 0;
/***************************/

/*Variables para controlar la interrupción del Timer*/
volatile int tiempo = 0;
volatile int cont = 0;
/****************************************************/

/**Booleanos para el mando infrarrojo**/
boolean cambio = false; //Predeterminado, calculadora.
boolean inicia = false;
boolean termina = false;
boolean modoJuego = true; //Predeterminado, calculadora.
boolean pausa = false;
boolean reinicio = false;
boolean difMen = false;
boolean difSup = false;
/*************************************/

volatile int dificultad = 1; //Dificultad predeterminada, 1.
volatile int puntaje = 0; //Inicializador del puntaje.

/****Temporizador de juego****/
volatile int unidJuego = 0;
volatile int decJuego = 6;
/*****************************/

/***Actualiza los dígitos de los segmentos, y el rango de los segmentos***/
volatile int contJuego = 0;
volatile int tiempoJuego = 0;
/*************************************************************************/

/***Variables usadas para guardar el puntaje récord y el nombre en memoria****/
volatile int dosCinco = 0; //Indica el número de veces que se le resta 255.
String jugador = ""; //Se guarda en 100..190, 10 caracteres para cada nivel.
boolean sinNombre = false; //Inidica que se cogerá el nombre por defecto.
/*****************************************************************************/

volatile int numeroSecreto = 0; //Variable donde se almacena el número secreto.

char numeroIn[20] = ""; volatile int numero = -1; volatile int avanzaIn = 0; //Se guarda el número introducido en juego.

const int interruptIR = 4; //Nivel de interrupción(PIN 19) conectado al pin SIGNAL del receptor infrarrojor.

/****Variables de control de la conexión infrarroja****/
uint8_t IRProtocol = 0;
uint16_t IRAddress = 0;
uint32_t IRCommand = 0;
/******************************************************/

void setup(){
/************************ TIMER 1 ********************************/
// Programación del timer1 para generar una interrupción cada 10ms
// deshabilitar interrupciones
cli();
// modo normal de funcionamiento
TCCR1A = 0;
TCCR1B = 0;
// cuenta inicial a cero
TCNT1 = 0;
// mode CTC
TCCR1B |= (1<<WGM12);
// prescaler N = 1024
TCCR1B |= (1<<CS12)|(1<<CS10);
// fintr = fclk/(N*(OCR1A+1)) --> OCR1A = [fclk/(N*fintr)] - 1
// para fintr = 100Hz --> OCR1A = [16*10^6/(1024*100)] -1 = 155,25 --> 155

OCR1A = 77; // para 200 Hz programar OCR1A = 77

// enable timer1 compare interrupt (OCR1A)
TIMSK1 |=(1<<OCIE1A);
// habilitar interrupciones
sei();
/*****************************************************************/

attachInterrupt(digitalPinToInterrupt(21), coma, FALLING); //Activación del botón para la coma.

Serial3.begin(9600); //Conexión pantalla LCD(TTL).
Serial.begin(9600); //Conexión Monitor Serial.

randomSeed(analogRead(0)); //Semilla para la función random(), asegura números aleatorios distintos.
IRLbegin<IR_NEC>(interruptIR); //Mando infrarrojo con protocolo NEC.

/*******************Declaración de puertos************************/
DDRA = 0xFF; //Puerto leds 7-segmentos.
DDRL = 0x0F; //Puerto de activacion digito 7-segmentos. 0B(1111)_XXXX lectura botones.
DDRC = 0x01; //Puerto entrada de botones, bit menos significativo a 1, salida speaker.
PORTC = 0xF8; //Activacion pull-up botones, menos speaker.
/*****************************************************************/

/******I2C******/
//Inicialización de los terminales de entrada.
  pinMode(LEE_SDA, INPUT);
  pinMode(LEE_SCL, INPUT);

  //Inicialización de los terminales de salida.
  pinMode(ESC_SDA, OUTPUT);
  pinMode(ESC_SCL, OUTPUT);

  //Para asegurar de no intervenir el bus.
  digitalWrite(ESC_SDA, HIGH);
  digitalWrite(ESC_SCL, HIGH); 
/****************/
}

//Rutina característica de la conexión por infrarrojo.
void IREvent(uint8_t protocol, uint16_t address, uint32_t command) { 
  IRProtocol = protocol;
  IRAddress = address;
  IRCommand = command;
}

//Rutina para reiniciar los registros de memoria, pulsando a la vez en el teclado *,7,4,1.
void reinicioMem(){
  if(millis() > timeBoton + 200){
    //Se escribe 0x00, en las 128 posiciones de memoria. 
    for(byte i = 0x00; i <= 0x7F; i++){
      escribirDatoDir(0x00, i);
    }  
    //Se imprime el mensaje correspondientes.
    Serial3.write(0xFE);
    Serial3.write(0x51);
    
    Serial3.write(0xFE);
    Serial3.write(0x45);
    Serial3.write(0x00);
    Serial3.write("Se han borrado los");
  
    Serial3.write(0xFE);
    Serial3.write(0x45);
    Serial3.write(0x40);
    Serial3.print("registros del juego");  
    delay(2000);

    //Y se salta al método de elegir dificultad.
    elegirDificultad(); 
  } 
}

//Rutina de decodificación de los comandos del mando.
void remoteIRL(){
  uint8_t oldSREG = SREG;
  cli(); //Deshabilita interrupciones.
  if(IRProtocol){
    if(!modoJuego && finJuego){
      if(IRCommand == 0x02FD) cambio = true; //Si no está en modo juego, se cambia con >>|.
    }else if(modoJuego && finJuego){ //Si está en modo juego, y no ha empezado partida.     
      if(IRCommand == 0xC23D){
        inicia = true; //Inicia partida, se cambia con >||.
      }else if(IRCommand == 0x22DD){
        cambio = false; //Si está en modo juego, cambia a calculadora, se cambia con |<<.
      }else if(IRCommand == 0xE01F){
        difMen = true; //Disminuye la dificultad, se cambia con -.
      }else if(IRCommand == 0xA857){
        difSup = true; //Aumenta la dificultad, se cambia con +.
      }
    }else{ //Si está en modo juego, y además está jugando.
      switch(IRCommand){ //Cada valor corresponde a su botón en el mando.
        case 0x6897:
                    numeroIn[avanzaIn] = '0';
                    avanzaIn++;
                    break;
        case 0x30CF:
                    numeroIn[avanzaIn] = '1';
                    avanzaIn++;
                    break;
        case 0x18E7:
                    numeroIn[avanzaIn] = '2';
                    avanzaIn++;
                    break;
        case 0x7A85:
                    numeroIn[avanzaIn] = '3';
                    avanzaIn++;
                    break;
        case 0x10EF:
                    numeroIn[avanzaIn] = '4';
                    avanzaIn++;
                    break;
        case 0x38C7:
                    numeroIn[avanzaIn] = '5';
                    avanzaIn++;
                    break;
        case 0x5AA5:
                    numeroIn[avanzaIn] = '6';
                    avanzaIn++;
                    break;
        case 0x42BD:
                    numeroIn[avanzaIn] = '7';
                    avanzaIn++;
                    break;
        case 0x4AB5:
                    numeroIn[avanzaIn] = '8';
                    avanzaIn++;
                    break;
        case 0x52AD:
                    numeroIn[avanzaIn] = '9';
                    avanzaIn++;
                    break;
        case 0x906F:
                    termina = true; //Final de número, se envía con EQ.
                    break;   
        case 0x22DD:
                    cambio = false; //Cambia de modo a calculadora, se cambia con |<<.
                    break;
        case 0xC23D:
                    pausa = !pausa; //Pausa el juego, se cambia con >||.           
                    break;
        case 0x629D:
                    reinicio = true; //Reinicia el juego, se cambia con CH.
                    break;
      }
    }
    IRProtocol = 0;
  }
  SREG = oldSREG;
}

//Rutina loop(), se ejecuta infinitamente. Aquí se recoge el cambio de modo, así como el reinicio de los registros.
void loop() {
  remoteIRL(); //Recoge las pulsaciones del mando infrarrojo.
  
  byte teclaPulsado = 0xFF; //Se lee el puerto del teclado, se desplaza 4 a la derecha para quedarse con los 4 bits de ls filas.
  teclaPulsado = PINL >> 4;  

  if(teclaPulsado == 0x00 && modoJuego) reinicioMem(); //Si se pulsa *,7,4,1 a la vez, se reinician los registros de memoria.

  if(teclaPulsado == 0x0C && !modoJuego || cambio && !modoJuego){ //Entra a juego, * y 7.    
    if(millis() > timeTecla + 200){
      timeTecla = millis();
      //"Limpia" la pantalla LCD.
      Serial3.write(0xFE);
      Serial3.write(0x51);
      //Situa cursor en inicio.
      Serial3.write(0xFE);
      Serial3.write(0x45); 
      Serial3.write(0x00); 
      //Desactiva la señal del cursor.
      Serial3.write(0xFE);
      Serial3.write(0x48);
      //Imprime el mensaje correspondiente.
      Serial3.print("Modo Juego");      
      delay(1000);

      //"Limpia" la pantalla LCD.
      Serial3.write(0xFE);
      Serial3.write(0x51);
      //Situa cursor en inicio.
      Serial3.write(0xFE);
      Serial3.write(0x45); 
      Serial3.write(0x00);  
      //Imprime mensaje de nombre.  
      Serial3.print("Nombre: ");
      //Imprime el mensaje del nombre, a través del monitor serie.
      Serial.print("Introduzca su nombre");

      //Para omitir la introducción del nombre.
      Serial3.write(0xFE);
      Serial3.write(0x45); 
      Serial3.write(0x14); 
      Serial3.print("Boton central para");
      Serial3.write(0xFE);
      Serial3.write(0x45); 
      Serial3.write(0x54);
      Serial3.print("predeterminado");

      //Cambia de modo.
      modoJuego = true; 
      cambio = true;

      //Se queda en bucle hasta que se introduzca un valor, o se pulse el botón central.
      while(Serial.available() <= 0){
        if(PINC>>3 == 0x1D){
          jugador = "Sin nombre";
          sinNombre = true;
          break;
        }
      }

      //Si no se pulsó el botón central, se lee el nombre introducido.
      if(!sinNombre) jugador = Serial.readStringUntil('\n');          
      
      Serial3.write(0xFE);
      Serial3.write(0x45); 
      Serial3.write(0x08);
      Serial3.print(jugador);
      delay(2000);

      //Se reinician los valores de juego.
      reinicioJuego();      
      elegirDificultad();            
      cont = 0;
      tiempo = 0;
    }
  }else if(teclaPulsado == 0x06 && modoJuego || !cambio && modoJuego){ //Sale de juego, * y 1.
    if(millis() > timeTecla + 200){
      timeTecla = millis();
      //Limpia la pantalla.
      Serial3.write(0xFE);
      Serial3.write(0x51);
      //Situa cursor en inicio.
      Serial3.write(0xFE);
      Serial3.write(0x45);
      Serial3.write(0x00);
      //Desactiva la señal del cursor.
      Serial3.write(0xFE);
      Serial3.write(0x48);
      //Imprime el modo.
      Serial3.print("Modo Calculadora");      
      delay(1000);
      //Habilita la señal del cursor.
      Serial3.write(0xFE);
      Serial3.write(0x47);
      
      //Cambio de modo.
      modoJuego = false;
      cambio = false;
      
      //Limpia la pantalla.
      Serial3.write(0xFE);
      Serial3.write(0x51);
      //Situa cursor en inicio.
      Serial3.write(0xFE);
      Serial3.write(0x45);
      Serial3.write(0x00);
      //Reinicia los valores de la calculadora.
      reinicioCalculadora();
      cont = 0;
      tiempo = 0;
    }
  }
}

/***Cuenta la longitud del char que contiene los números***/
int cuentaDigitos(char* pos){
  int digitos = 0;
  while(*pos != '\0'){
    digitos++;
    pos++;
  }
  return (digitos-1);
}
/**********************************************************/

/*Imprime los caracteres pasado como char* en la última posición de la pantalla*/
void error(char* mensaje){
  Serial3.write(0xFE);
  Serial3.write(0x48);
       
  Serial3.write(0xFE);
  Serial3.write(0x45);
  Serial3.write(0x67 - cuentaDigitos(mensaje));
  Serial3.print(mensaje); 
}
/*******************************************************************************/

/*****Rutina del Timer*****/
ISR(TIMER1_COMPA_vect){
  if(modoJuego){ //Modo juego.
    if(finJuego){
      ISR_Juego(); //Contador de modo juego, en espera.
    }else{
      cuentaJuego(); //Contador de modo juego, mientras está jugando, 60..0.
    }          
  }else{
    ISR_Calculadora(); //Contador de modo calculadora.
  }
}
/*****************************/

/******Rutina del contador de juego en espera********/
void ISR_Juego(){
  PORTL = segmentos[cont]; //Se habilita el dígito correspondiente.
  //Se imprimen los valores en cada dígito activado.
  switch(cont){
    case 0: 
          PORTA = num[unid];
          break;
    case 1:
          PORTA = num[dec];
          break; 
    case 2:
          PORTA = num[cent];
          break;
    case 3:
          PORTA = num[mil];
          break;
  }
  
  if(inicia){ //Controla el pulsador >|| del mando, para empezar el juego.
    reinicioJuego(); //Reinicia los valores.
    finJuego = false; //Deshabilita el flag.
    numeroSecreto = random(100*dificultad); //Genera un número aleatorio.
    inicia = false; //Reinicia la variable correspondiente al botón del mando a false.
    cuentaJuego(); //Salta directamente a la rutina de juego en curso.
  }
  
  reboteBotonesJuego(); //Rutina de lectura del teclado.
  cont++; 
  tiempo++;
  
  if(cont == 4) cont = 0; //Si se ha llegado al último dígito del 7-segmentos, se reinicia a 0.   
  if(tiempo%200 == 0) contar(); //Se actualiza los valores del 7-segmentos cada 10 ms.
  /**Ya que el timer está configurado como OCR1A = 77, va a 200 Hz, por lo que para actualizar cada
   * 10 ms, se debe dividir el tiempo/200, y obtener su módulo, qué será 0 si han pasado 10 ms.
   */
}
/***********************************************************/

/***********Lectura botone en juego************/
void reboteBotonesJuego(){ 
  byte operacionJuego = 0x00;   
  byte botonPulsado = PINC >> 3; //Se lee el puerto de los botones, y se desplaza 3 a la derecha para quedarse con los 5 botones.

  switch(botonPulsado){
    case 0x1E: //Botón up, para inicio de partida.
              if (millis() > timeBoton + 200) {
                //Se reasigna el tiempo de ejecución a nuestro temporizador, para compararlo en una segunda interrupción.
                timeBoton = millis();
                operacionJuego = 0x2A;
              }
              break;
    case 0x17: //Botón down, para pausar partida.
              if (millis() > timeBoton + 200) {
                timeBoton = millis();
                operacionJuego = 0x2D;
              }
              break;
    case 0x16: //Botón up y down, para reinicio de partida.
              if (millis() > timeBoton + 200) {
                timeBoton = millis();
                operacionJuego = 0x2C;
              }
              break;
    case 0x1B: //Botón izquierdo, para elegir dificultad menor.
              if (millis() > timeBoton + 200) {
                timeBoton = millis();
                operacionJuego = 0x2F;
              }
              break;
    case 0x0F: //Botón derecho, para elegir dificultad mayor.
              if (millis() > timeBoton + 200) {
                timeBoton = millis();
                operacionJuego = 0x2B;
              }
              break;
  }

  if(operacionJuego == 0x2A && finJuego){ //Inicia el juego.
    reinicioJuego();
    finJuego = false;
    numeroSecreto = random(100*dificultad);
    cuentaJuego();
  }else if(operacionJuego == 0x2D && !finJuego){ //Si está en medio de un juego, y se pulsa pdown, se pausa o se quita la pausa.
    pausa = !pausa;
    cuentaJuego();
  }else if(operacionJuego == 0x2C && !finJuego){ //Se reinicia el juego.
    reinicioJuego();
    elegirDificultad();
  }else if(operacionJuego == 0x2B && dificultad <= 9 && finJuego || difSup && dificultad <= 9 && finJuego){ //Se disminuye un nivel de dificultad.
    difSup = false;
    dificultad++;
    elegirDificultad();
  }else if(operacionJuego == 0x2F && dificultad >= 2 && finJuego || difMen && dificultad >=2 && finJuego){ //Se añade un nivel de dificultad.
    difMen = false;
    dificultad--;
    elegirDificultad();    
  }
}

/******Rutina que actualiza la dificultad del juego*****/
void elegirDificultad(){
    //Limpia pantalla.
    Serial3.write(0xFE);
    Serial3.write(0x51);
    //Situa cursor en inicio.
    Serial3.write(0xFE);
    Serial3.write(0x45);
    Serial3.write(0x00);
    
    Serial3.print("Dificultad: ");
    Serial.print(dificultad);
    //Se imprime la dificultad actual.
    Serial3.write(0xFE);
    Serial3.write(0x45);
    Serial3.write(0x40);
    Serial3.print(0);
    Serial3.print(" -> ");
    Serial3.print(100*dificultad);

    //Variables donde se guardará el nombre del récord en memoria.
    String jugadorRecord = "";
    char aux;

    /**Se lee el nombre, caracter por caracter. Los nombres ocupan 10 bytes,
      *y se almacenan a partir de la dirección 20..119, un récord por dificultad.
      **/
    for(int i = 0; i < 10; i++){
      aux = (char) leerDir((byte) ((dificultad-1)*10)+20+i); //Dif=1, i= 0 -> (((1-1)*10)+20+0) = 20...29
      if(aux >= 0x20 && aux <= 0x7E){
        jugadorRecord += aux; //Recupera sólo estos caracteres válidos.
      }
    }

    //Imprime el nombre del jugador.
    Serial3.write(0xFE);
    Serial3.write(0x45);
    Serial3.write(0x14);    
    Serial3.print(jugadorRecord);

    //Variable donde se recupera el puntaje del jugador.
    int resultado = 0; 
    //Recupera las veces que se le restó 255. Entonces se multiplica por 255, se almacena desde 10..19.
    resultado += ((int) leerDir((byte) dificultad-1+10))*255; //Dif=1, (1-1+10) = 10.
    //Recupera el puntaje restante, se almacena desde 0..9.
    resultado += (int) leerDir((byte) dificultad-1); //Dif=1, (1-1) = 0.

    //Imprime el puntaje recuperado.
    Serial3.write(0xFE);
    Serial3.write(0x45);
    Serial3.write(0x54);
    Serial3.print("Record actual: ");
    Serial3.print(resultado);
}

//Rutina que realiza una cuenta atrás, desde 60 hasta 0.
void cuentaJuego(){
  PORTL = segmentos[contJuego]; //Se habilita el dígito correspondiente.
  //Se imprimen los valores en cada dígito activado.
  switch(contJuego){
    case 0: 
          PORTA = num[unidJuego];
          break;
    case 1:
          PORTA = num[decJuego];
          break;
    case 2: 
          PORTA = 0x00; //No muestra nada, pero sí requerido para leer el teclado.
          break;
    case 3:
          PORTA = 0x00; //No muestra nada, pero sí requerido para leer el teclado.
          break;
  }

  if(reinicio){ //Variable obtenida de la pulsación del mando.
    reinicio = false; 
    reinicioJuego(); //Reinicia el juego.
  }
  
  if(termina && !pausa && !reinicio){ //Variable obtenida de la pulsación del mando.   
    numeroIn[avanzaIn] = '\0'; //Finaliza el envío del número en el char[].
    numero = atoi(numeroIn);    
    termina = false; 
    compruebaNumero(numero, avanzaIn); //Lo comprueba, y reinicia contadores.
    avanzaIn = 0;
  }  

  if(pausa){ //Variable obtenida del mando, pausa el juego, detiene contadores. 
    Serial3.write(0xFE);
    Serial3.write(0x4B);
    reboteBotonesJuego();
    contJuego++; 
    if(contJuego == 4) contJuego = 0; //Si se ha llegado al último dígito del 7-segmentos, se reinicia a 0.    
  }else if(!reinicio){ //Si no hay pausa, y tampoco reinicio, el juego sigue su curso.
    Serial3.write(0xFE);
    Serial3.write(0x4C); 
    reboteBotonesJuego(); 
    reboteTecladoJuego();    
    tiempoJuego++;
    contJuego++;   
  
    if(contJuego == 4) contJuego = 0; //Si se ha llegado al último dígito del 7-segmentos, se reinicia a 0.   
    if(tiempoJuego%200 == 0) contarJuego(); //Se actualiza los valores del 7-segmentos cada 10 ms.
    /**Ya que el timer está configurado como OCR1A = 77, va a 200 Hz, por lo que para actualizar cada
    * 10 ms, se debe dividir el tiempo/200, y obtener su módulo, qué será 0 si han pasado 10 ms.
    */
  }
}

/*Actualización de las unidades de los 7-segmentos*/
void contarJuego(){  
  //Se emplea un contador con 60 segundos y un minutero.
  if(unidJuego == 0){    
    if(decJuego == 0){  
      //Si dec es igual a cero, el tiempo ha finalizado, se imprime un mensaje.
      Serial3.write(0xFE);
      Serial3.write(0x51);
      
      Serial3.write(0xFE);
      Serial3.write(0x45);
      Serial3.write(0x00);
      
      Serial3.print("Tiempo agotado");

      Serial3.write(0xFE);
      Serial3.write(0x45);
      Serial3.write(0x40);   
      Serial3.print("Num. Secreto -> ");
      Serial3.print(numeroSecreto); 
      
      tone(37, 200, 20);
      finJuego = true; //Se pone finjuego a true, para que el timer vuelva a ejecutarse.      
    }else{
      decJuego--;
    }          
    unidJuego = 9;
  }else{
    unidJuego--;
  }
}

/**Variables usadas para el control de las filas en la pantalla,
 * así como la limpieza de cada fila.
 */
volatile int posicion = 0;
volatile int fila = 0;

//Rutina que comprueba el número introducido.
void compruebaNumero(int compNum, int tamano){
  if(compNum == numeroSecreto){ //Si coincide.
    //Limpia pantalla.
    Serial3.write(0xFE);
    Serial3.write(0x51);
    
    tone(37, 200, 50);
    tone(37, 500, 100);
    tone(37, 200, 50);
    guardaPuntaje(); //Se llama a otra rutina para comprobar si el puntaje obtenido es un nuevo récord.
    finJuego = true; //Se habilita el flag para que el timer vuelva a ejecutar la rutina.
  }else{
    //LCD de 20 posiciones y 4 filas. 1 posición añadida por el símbolo "< o >".
    posicion += tamano+1; 
    if(posicion > 20){ //Si sobrepasa los 20, se limpia la fila siguiente.
      if(fila == 0){  
        limpiaFila(0x40);
        fila = 1;
      }else if(fila == 1){
        limpiaFila(0x14);
        fila = 2;
      }else if(fila == 2){
        limpiaFila(0x54);
        fila = 3;
      }else if(fila == 3){
        limpiaFila(0x00);
        fila = 0;
      }
      posicion = tamano+2; //Si se ha limpiado la fila, se reinicia posicion a 2 más el tamaño del dígito.
    }else{
      posicion++; //Si no, se suma uno que equivale al espacio.
    }

    /**En mi caso, el número se imprime después de envíar el caracter control '#', esto ayuda a controlar los 
     * saltos de línea, ya que se verifica antes de imprimir si hay espacio disponible para hacerlo en esa línea.
     */
    if(compNum > numeroSecreto){ // Si es mayor al número a encontrar, se impime numIntroducido > numSecreto.
      Serial3.write('<');
      Serial3.print(compNum);
      Serial3.write(' '); 
    }else{ // Si es menor al número a encontrar, se impime numIntroducido < numSecreto.
      Serial3.write('>');
      Serial3.print(compNum);
      Serial3.write(' '); 
    }     
  }
}

//Método que limpia la fila pasada por parámetro, y sitúa el cursor a su inicio.
void limpiaFila(byte fila){
  //Se limpia cada fila, imprimiendo un espacio en cada posición.
  for(int i = 0; i < 20; i++){
    Serial3.write(0xFE);
    Serial3.write(0x45);
    Serial3.write(fila + i);
    Serial3.write(' ');
  }
  Serial3.write(0xFE);
  Serial3.write(0x45);
  Serial3.write(fila);
}

/**Rutina que verifica si el puntaje obtenido es mayor al récord guardado en memoria.
 * Cada dificultad tiene su propio récord guardado en memoria.
 **/
void guardaPuntaje(){
  int bonus = (decJuego*10) + unidJuego; //Se obtiene el tiempo restante.
  int puntos = dificultad*50; //El factor que depende de la dificultad equivale a 50.
  puntaje += puntos + bonus; //Se suma ambos.

  int resultado = 0;
  resultado += ((int) leerDir((byte) dificultad-1+10))*255; //Posiciones en memoria: 10..19, recupera nVeces le resta 255..
  resultado += (int) leerDir((byte) dificultad-1); //Posiciones en memoria: 0..9, recupera el puntaje restante.

  //Imprime el número secreto.
  Serial3.write(0xFE);
  Serial3.write(0x45);
  Serial3.write(0x40);   
  Serial3.print("Num. Secreto -> ");
  Serial3.print(numeroSecreto); 

  //Si el puntaje obtenido es mayor al récord actual. Se guarda en memoria.
  if(puntaje > resultado){    
    Serial3.write(0xFE);
    Serial3.write(0x45);
    Serial3.write(0x03);    
    Serial3.print("Nuevo record!!");
    
    Serial3.write(0xFE);
    Serial3.write(0x45);
    Serial3.write(0x14);    
    Serial3.print(jugador);

    Serial3.write(0xFE);
    Serial3.write(0x45);
    Serial3.write(0x58);
    Serial3.print(puntaje);
    Serial3.print(" puntos");

    //Entonces, se guarda el puntaje en memoria.
    while(puntaje > 255){
      puntaje = puntaje - 255; 
      dosCinco++; //Se guarda las veces que se le resta 255.
    }
    escribirDatoDir((byte) dosCinco, (byte) dificultad-1+10); //Se guarda nVeces le resta 255, en posiciones 10..19.   
    escribirDatoDir((byte) puntaje, (byte) dificultad-1); //Y el puntaje restante, en posiciones 0..9.

    //Se guarda también, el nombre del jugador, sólo 10 caracteres.
    for(int i = 0; i < 10; i++){
      escribirDatoDir((byte) jugador.charAt(i), (byte) ((dificultad-1)*10)+20+i); //En posiciones 20..119.
    }
    
  }else{  
    //Si no, se imprime el mensaje de ganado, con su respectivo puntaje.
    Serial3.write(0xFE);
    Serial3.write(0x45);
    Serial3.write(0x00);    
    Serial3.print("HAS GANADO");
     
    Serial3.write(0xFE);
    Serial3.write(0x45);
    Serial3.write(0x14);
    Serial3.print("Tiene ");
    Serial3.print(puntaje);
    Serial3.print(" puntos");
  }
  //Se reinician ambas variables.
  puntaje = 0; 
  dosCinco = 0;
}

/****Función que controla el teclado****/
void reboteTecladoJuego(){    
  byte entero = 0x00; //Se almacena el valor pulsado.
  byte teclaPulsado = 0x00; //Se lee el puerto del teclado, se desplaza 4 a la derecha para quedarse con los 4 bits de ls filas.
  teclaPulsado = PINL >> 4;  

  //La variable "cont" es el apuntador al dígito activado en los 7-segmentos.
  switch(teclaPulsado){
    case 0x07: //Si se ha pulsado la primera fila {1, 2, 3}.
              if (millis() > timeTecla + 200) {
                //Se reasigna el tiempo de ejecución a nuestro temporizador, para compararlo en una segunda interrupción.
                timeTecla = millis();
                if(contJuego == 0){ //Se ha activado las unidades.
                    entero = 0x31; //Valor 1.                        
                }else if(contJuego == 1){ //Se ha activado las decenas.
                    entero = 0x32; //Valor 2.   
                }else if(contJuego == 2){ //Se ha activado las centenas.
                    entero = 0x33; //Valor 3.
                }
              }
              break;
    case 0x0B: //Si se ha pulsado la segunda fila {4, 5, 6}.
              if (millis() > timeTecla + 200) {
                timeTecla = millis();
                if(contJuego == 0){ //Se ha activado las unidades.
                    entero = 0x34; //Valor 4.     
                }else if(contJuego == 1){ //Se ha activado las decenas.
                    entero = 0x35; //Valor 5.     
                }else if(contJuego == 2){ //Se ha activado las centenas.
                    entero = 0x36; //Valor 6.
                }
              }
              break;
    case 0x0D: //Si se ha pulsado la tercera fila {7, 8, 9}.
              if (millis() > timeTecla + 200) {
                timeTecla = millis();
                if(contJuego == 0){ //Se ha activado las unidades.
                    entero = 0x37; //Valor 7.       
                }else if(contJuego == 1){ //Se ha activado las decenas.
                    entero = 0x38; //Valor 8.     
                }else if(contJuego == 2){ //Se ha activado las centenas.
                    entero = 0x39; //Valor 9.
                }
              }
              break;
    case 0x0E: //Si se ha pulsado la cuarta fila {*, 0, #}.
              if (millis() > timeTecla + 200) {
                timeTecla = millis();
                if(contJuego == 0){ //Se ha activado las unidades.
                    entero = 0x2A; //Valor '*'.  
                }else if(contJuego == 1){ //Se ha activado las decenas.
                    entero = 0x30; //Valor 0.     
                }else if(contJuego == 2){ //Se ha activado las centenas.
                    entero = 0x23; //Valor '#'.
                }
              }
              break;
  }

  //Sólo si no es '*', '#' o no se ha introducido algún valor.
  if(entero != 0x00 && entero != 0x23 && entero != 0x2A){
    numeroIn[avanzaIn] = entero; //Se almacena el entero introducido.
    avanzaIn++;    
  }else if(entero == 0x23){ //Es el caracter control, fin de dígito '#'.
    numeroIn[avanzaIn] = '\0';    
    numero = atoi(numeroIn); 
    compruebaNumero(numero, avanzaIn); //Se comprueba el número introducido.
    avanzaIn = 0;
  }              
}

/****************************************************************/





/***********CALCULADORA************/
void ISR_Calculadora(){
  PORTL = segmentos[cont]; //Se habilita el dígito correspondiente.
  //Se imprimen los valores en cada dígito activado.
  switch(cont){
    case 0: 
          PORTA = num[unid];
          break;
    case 1:
          PORTA = num[dec];
          break; 
    case 2:
          PORTA = num[cent];
          break;
    case 3:
          PORTA = num[mil];
          break;
  } 
  
  reboteTeclado(); //Rutina de lectura de los botones.
  reboteBotones(); //Rutina de lectura del teclado.
  cont++; 
  tiempo++;
  
  if(cont == 4) cont = 0; //Si se ha llegado al último dígito del 7-segmentos, se reinicia a 0.   
  if(tiempo%200 == 0) contar(); //Se actualiza los valores del 7-segmentos cada 10 ms.
  /**Ya que el timer está configurado como OCR1A = 77, va a 200 Hz, por lo que para actualizar cada
   * 10 ms, se debe dividir el tiempo/200, y obtener su módulo, qué será 0 si han pasado 10 ms.
   */
}

/****Función que realiza la operación correspondiente****/
void operar(){
  double res = 0.0; //variable donde se almacena el valor final.
  char aux[25]; //Array de char donde se almacena el valor final.
  boolean divisorCero = false; //Señala si hay un cero en el divisor.

  //Se realiza la respectiva operación.
  switch(opera){
    case '+':
            res = primero + segundo;
            break;
    case '-':
            res = primero - segundo;
            break;
    case '*':
            res = primero * segundo;
            break;
    case '/':
            if(segundo != 0.0){
              res = primero / segundo;
            }else{
              error((char*) "Error!!");
              divisorCero = true; //Si el divisor es 0, se manda un mensaje de error, y se activa el booleano.
            }
  } 
    if(!divisorCero){ //Se convierte el resultado double a char[], con 2 decimales.
      dtostrf(res, sizeof(res), 2, aux); 
      calculadoraFin = true;
      error((char*) aux); 
    }             
}
/*********************************************************/

/**Rutina para poner numeros decimales.*/
void coma(){
  if(millis() > timeComa + 200){
    timeComa = millis();
    Serial3.write(0x2E); //Caracter para la coma.
    //Se actualiza los arrays de char y sus apuntadores.
    if(opera == 0x00){
      primeroC[avanzaP] = 0x2E;   
      avanzaP++;
    }else{
      segundoC[avanzaS] = 0x2E;   
      avanzaS++;
    }     
  }
}
/***************************************/

/*Actualización de las unidades de los 7-segmentos*/
void contar(){  
  //Se emplea un contador con 60 segundos y un minutero.
  if(unid == 9){
    unid = 0;
    if(dec == 5){
      dec = 0;
      if(cent == 9){
        cent = 0;
        if(mil == 9){
          mil = 0;
        }else{
          mil++;
        }
      }else{
        cent++;
      }
    }else{
      dec++;
    }
  }else{
    unid++;
  }
}

/***Rutina de control y lectura de los botones***/

void reboteBotones(){    
  byte operacion = 0x00; //Se recoge en una variable el operando pulsado.
  byte botonPulsado = PINC >> 3; //Se lee el puerto de los botones, y se desplaza 3 a la derecha para quedarse con los 5 botones.

  switch(botonPulsado){
    case 0x1E: //Botón superior, operando '*'.
              if (millis() > timeBoton + 200) {
                //Se reasigna el tiempo de ejecución a nuestro temporizador, para compararlo en una segunda interrupción.
                timeBoton = millis();
                operacion = 0x2A;
              }
              break;
    case 0x17: //Botón inferior, operando '-'.
              if (millis() > timeBoton + 200) {
                timeBoton = millis();
                operacion = 0x2D;
              }
              break;
    case 0x1B: //Botón izquierdo, operando '/'.
              if (millis() > timeBoton + 200) {
                timeBoton = millis();
                operacion = 0x2F;
              }
              break;
    case 0x1D: //Botón central, operando '='.
              if (millis() > timeBoton + 200) {
                timeBoton = millis();
                operacion = 0xFF;
              }
              break;
    case 0x0F: //Botón derecho, operando '+'.
              if (millis() > timeBoton + 200) {
                timeBoton = millis();
                operacion = 0x2B;
              }
              break;
  }

  if(opera == 0x00 && operacion != 0x00 && operacion != 0xFF){ //Se contempla que un botón se haya pulsado y no sea el central. Y que se haya pulsado una vez (opera == 0x00).  
    Serial3.write(operacion); 
    if((operacion == '-' || operacion == '+') && avanzaP == 0){ //Se trabaja con números negativos o positivos al inicio.    
      primeroC[avanzaP] = operacion;
      avanzaP = 1;
    }else if(operacion != 0xFF && avanzaP == 0){
      error((char*)"Formato erroneo"); //Si se pulsa '*' o '/' al inicio, salta un mensaje de error.
    }else{
      opera = operacion; //En cualquier otro caso, se registra la pulsación y se guarda el primer valor en su char[].
      primeroC[avanzaP] = '\0';        
      primero = atof(primeroC); //Se transforma el primer char[] a un double, con sus respectivos decimales.
    }   
  }else if(operacion == 0xFF){ //Se contempla que se haya pulsado el botón central.
    segundoC[avanzaS] = '\0';     
    segundo = atof(segundoC); //Se convierte el segundo char[] a un double, con sus respectivos decimales.
    if(avanzaP == 0){
       error((char*)"No hay datos"); //Si no se ha introducido datos, salta un error.       
    }else if(avanzaS == 0) {
      if(opera == 0x00){ //Si no se ha pulsado un operador ni un operando, se supone que es un '+' con el segundo valor igual a 0.
        primeroC[avanzaP] = '\0';        
        primero = atof(primeroC);
        opera = '+';
        segundo = 0.0;
        operar(); //Función que realiza la operación.
      }else{
        error((char*)"Formato erroneo"); //Si no se ha pulsado el segundo operando y se ha pulsado un operador, salta un error.
      }
    }else{
      operar(); //Función que realiza la operación.
    }
  }          
}

/****Función que controla el teclado****/
void reboteTeclado(){    
  byte entero = 0x00; //Se almacena el valor pulsado.
  byte teclaPulsado = 0x00; //Se lee el puerto del teclado, se desplaza 4 a la derecha para quedarse con los 4 bits de ls filas.
  teclaPulsado = PINL >> 4;  

  //La variable "cont" es el apuntador al dígito activado en los 7-segmentos.
  switch(teclaPulsado){
    case 0x07: //Si se ha pulsado la primera fila {1, 2, 3}.
              if (millis() > timeTecla + 150) {
                //Se reasigna el tiempo de ejecución a nuestro temporizador, para compararlo en una segunda interrupción.
                timeTecla = millis();
                if(cont == 0){ //Se ha activado las unidades.
                    entero = 0x31; //Valor 1.                        
                }else if(cont == 1){ //Se ha activado las decenas.
                    entero = 0x32; //Valor 2.   
                }else if(cont == 2){ //Se ha activado las centenas.
                    entero = 0x33; //Valor 3.
                }
              }
              break;
    case 0x0B: //Si se ha pulsado la segunda fila {4, 5, 6}.
              if (millis() > timeTecla + 150) {
                timeTecla = millis();
                if(cont == 0){ //Se ha activado las unidades.
                    entero = 0x34; //Valor 4.     
                }else if(cont == 1){ //Se ha activado las decenas.
                    entero = 0x35; //Valor 5.     
                }else if(cont == 2){ //Se ha activado las centenas.
                    entero = 0x36; //Valor 6.
                }
              }
              break;
    case 0x0D: //Si se ha pulsado la tercera fila {7, 8, 9}.
              if (millis() > timeTecla + 150) {
                timeTecla = millis();
                if(cont == 0){ //Se ha activado las unidades.
                    entero = 0x37; //Valor 7.       
                }else if(cont == 1){ //Se ha activado las decenas.
                    entero = 0x38; //Valor 8.     
                }else if(cont == 2){ //Se ha activado las centenas.
                    entero = 0x39; //Valor 9.
                }
              }
              break;
    case 0x0E: //Si se ha pulsado la cuarta fila {*, 0, #}.
              if (millis() > timeTecla + 150) {
                timeTecla = millis();
                if(cont == 0){ //Se ha activado las unidades.
                    entero = 0x2A; //Valor '*'.  
                }else if(cont == 1){ //Se ha activado las decenas.
                    entero = 0x30; //Valor 0.     
                }else if(cont == 2){ //Se ha activado las centenas.
                    entero = 0x23; //Valor '#'.
                }
              }
              break;
  }
    
  if(entero != 0x00 && entero != 0x2A && entero != 0x23){ //Se contempla si se ha pulsado algún botón que no sea '*' y '#'.
    Serial3.write(entero);
    if(opera == 0x00){ //No se ha pulsado el operador, se almacena en el primero char[].
      primeroC[avanzaP] = entero; 
      avanzaP++;
    }else{ //Se ha pulsado el operador, se almacena en el segundo char[].
      segundoC[avanzaS] = entero; 
      avanzaS++;
    }
  }else if(entero == 0x23 && !calculadoraFin){ //Si se ha pulsado '#', borra un elemento.
      if(avanzaS >= 0 && opera != 0x00){ //Si se ha introducido el segundo operando y un operador.      
        segundoC[avanzaS] = '\0'; //se disminuye el char[] poniendo su valor final en una posición menor.        
        if(avanzaS == 0){ 
          opera = 0x00; //Si se ha borrado todo el segundo operando, se borra el operador.   
        }else{
          avanzaS--;
        }
      }else if(avanzaP >= 0){ //Si no se ha introducido un operador ni el segundo operando.       
        primeroC[avanzaP] = '\0'; //se disminuye el char[] poniendo su valor final en una posición menor.
        if(avanzaP == 0){ //Si se ha llegado a la posición inicial, no se borra más.
          Serial3.write(0xFE);
          Serial3.write(0x45);
          Serial3.write(0x01);
        }else{
          avanzaP--;
        }        
      }
    //Señales para el borrado de un caracter.
    Serial3.write(0xFE); 
    Serial3.write(0x4E);   
  }else if(entero == 0x2A){ //Si se ha pulsado '*', limpia la pantalla.
    reinicioCalculadora();
  }            
}
/****************************************************************/

/*****Se reinicia todas las variables de la calculadora******/
void reinicioCalculadora(){
  //Reinicia todos los contadores a sus valores iniciales.
    segundo = -1.0;
    primero = -1.0;
    opera = 0x00;
    //Clear screen.
    if(avanzaP > 0){
      avanzaP = 0;
      avanzaS = 0;
      primeroC[avanzaP] = '\0';
      segundoC[avanzaS] = '\0';
    }
    calculadoraFin = false;
    finJuego = true;
    pausa = false;

    Serial3.write(0xFE);
    Serial3.write(0x47);
    
    Serial3.write(0xFE);
    Serial3.write(0x4C);
    
    Serial3.write(0xFE);
    Serial3.write(0x51);
    
    Serial3.write(0xFE);
    Serial3.write(0x45);
    Serial3.write(0x00);
}
/************************************************************/

/**********Se reinicia todas las variables del juego*********/
void reinicioJuego(){
  //Se imprime los valores de inicio  
  numeroIn[0] = '\0';
  unidJuego = 0;
  decJuego = 6;
  posicion = 0;
  fila = 0;
  Serial3.write(0xFE);
  Serial3.write(0x51);
    
  Serial3.write(0xFE);
  Serial3.write(0x45);
  Serial3.write(0x00);
  finJuego = true;
}
/************************************************************************************************/



/**Método que escribe un byte en una determinada dirección, se sigue el mismo esquema que el explicado
 * en clase, primero un inicioComun(), luego se envía la dirección en la que se escribirá y se espera
 * su respectivo ACK, luego se envía el dato a escribir y se espera su respectivo ACK, finalmente 
 * se envía la señal de STOP.
 */
void escribirDatoDir(byte dato, byte dir){  
  do{  
    startMi();
    enviar(0xA0);  
  }while(R_BitAck() == 1);
      
  enviar(dir);
  while(R_BitAck() == 1){}
  
  enviar(dato);
  while(R_BitAck() == 1){}
  
  stopMi();
}

/**Método que lee un dato contenido en la dirección pasada como parámetro, puesto que en esta operación
 * primero se debe escribir en el bus la dirección del módulo, se cambia el valor de "leido" a 1. 
 * Se llama a inicioComun(), se envía la direccion en la que se leerá y se espera su respectivo ACK.
 * Luego de estar direccionado correctamente, se deberá habilitar la opción de lectura, por lo que
 * se cambia el valor de "leido" a 2. Se llama a recibir() para obtener el byte, se espera su respectivo
 * ACK negado, se manda la señal de STOP, y finalmente se devuelve el byte leído.
 */
byte leerDir(byte dir){
  do{  
    startMi();
    enviar(0xA0);  
  }while(R_BitAck() == 1);
  enviar(dir);
  while(R_BitAck() == 1){}

  do{  
    startMi();
    enviar(0xA1);  
  }while(R_BitAck() == 1);
  byte recibido = recibir();
  E_Bit1(); //No ACK.
  stopMi();
  return (recibido);
}
/************************************************************************************************/




/**Método que envía un byte al módulo de memoria, se usa el método de la máscara, se empieza
 * con B1000_0000 (0x80), se aplica un AND a la máscara y al byte enviado, se obtendrá un 0
 * si los bits en esa pocisión no coiniciden (se llama a E_Bit0()), sin embargo, no se puede 
 * aplicar lo mismo en el caso de que coincidan, dado que no se obtendrá un 1, sino 00100...
 */
void enviar(byte v){
  for(byte msk = 0x80; msk != 0x00; msk >>= 1){ //Con >>= se desplaza los bits a la derecha.
    if((v & msk) == 0){
      E_Bit0();
    }else{
      E_Bit1();
    }
  }
}

/**Método que recibe un byte del módulo de memoria, se usa el método de la máscara, se empieza
 * con B1000_0000 (0x80), y en este caso se aplica un OR a la máscara y al byte. El byte se obtiene
 * bit a bit, empezando por el lado izquierdo, se utiliza un OR para no perder los bits obtenidos
 * anteriormente, y sólo es necesario hacer esta operación cuando se recibe un R_BitAck() alto.
 */
byte recibir(){
  byte recibido = 0x00;
  for(byte msk = 0x80; msk != 0x00; msk >>= 1){
    if(R_BitAck() == 1){
      recibido = recibido | msk;
    }
  }
  return (recibido);
}
/************************************************************************************************/



/****************Desarrollo de las distintas señales de comunicación del BUS I2C*****************/

/************************************************************************************************/

void startMi() {
  digitalWrite(ESC_SDA, HIGH);
  digitalWrite(ESC_SCL, HIGH);

  while (digitalRead(LEE_SDA) == 0 || digitalRead(LEE_SCL) == 0){
    digitalWrite(ESC_SDA, HIGH);
    digitalWrite(ESC_SCL, HIGH);
  }

  digitalWrite(ESC_SDA, LOW);
  digitalWrite(ESC_SCL, HIGH);
  digitalWrite(ESC_SCL, LOW);
}

void E_Bit1() {
  digitalWrite(ESC_SDA, HIGH);
  digitalWrite(ESC_SCL, LOW);
  digitalWrite(ESC_SCL, HIGH);
  digitalWrite(ESC_SCL, LOW);
}

void E_Bit0() {
  digitalWrite(ESC_SDA, LOW);
  digitalWrite(ESC_SCL, LOW);
  digitalWrite(ESC_SCL, HIGH);
  digitalWrite(ESC_SCL, LOW);
}

int R_BitAck() {
  digitalWrite(ESC_SDA, HIGH);
  digitalWrite(ESC_SCL, LOW);
  digitalWrite(ESC_SCL, HIGH);

  while (digitalRead(LEE_SCL) == 0) {
    digitalWrite(ESC_SDA, HIGH);
    digitalWrite(ESC_SCL, LOW);
    digitalWrite(ESC_SCL, HIGH);
  }
  int retorno = digitalRead(LEE_SDA);
  digitalWrite(ESC_SCL, LOW);  
  return (retorno);
}

void stopMi() {
  digitalWrite(ESC_SDA, LOW);
  digitalWrite(ESC_SCL, LOW);
  digitalWrite(ESC_SCL, HIGH);
  digitalWrite(ESC_SDA, HIGH);
}
