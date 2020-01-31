// MISEA-UC3M 2014-15
// Base program
// Proyectos Experimentales II
// Copy Right Universidad Carlos III de Madrid
// --------------------------   Librerías  ---------------------------------------//
#include <stdint.h>              // standard library for integers (used in the next libraries)
#include "driverlib\systick.h"   // standard library for the SysTick (header)
#include "driverlib\systick.c"   // standard library for the SysTick (functions)
#include "wiring_private.h"      // library to access the PWM with configurable frequency                                    
#include "Ethernet.h"

#define TickerPeriod 2721       // f = 44100Hz (T = 22,67 useg)

// --------------------------   Declaración pines  ---------------------------------------//
#define PWMnote PM_6            //señal pwm cuadrada nota musical
#define PWMenv  PM_0            //señal pwm envolvente
#define PWMfc   PM_2            //señal pwm control frecuencia LP
#define ReadV A1                //señal analogica lectura voltaje 
                                //potenciometro para control frecuencia

// --------------------------   Declaración constantes  -----------------------------------//
#define FENV 10000              //Frecuencia PWM envolvente

// --------------------------   Declaración variables  ------------------------------------//
char key;                       //caracter leido terminal serie
int pitch = 440;                //frecuencia nota Hz
int tickercount = 0;            //contador
int trig = 0;                   //indica pulsación nueva nota
int DC_env;                     //ciclo de trabajo PWM envolvente

byte MIDIcommand;               //lectura puerto MIDI
byte MIDInote;                  //byte MIDI correspondiente a la nota
byte MIDIvelocity;              //byte MIDI correspondiente a la velocity

double fc;                      //frecuencia de corte del LP
int sensorValue = 0;            //lectura potenciómetro para LP

//Matriz de escalas
int matrix[9][11]={{0, 2, 4, 5, 7, 9, 11, 12, 14, 16, 17},           //Mayor
                   {0, 2, 3, 5, 7, 9, 10, 12, 14 ,15, 17},           //Dorico
                   {0, 1, 3, 5, 7, 8, 10, 12, 13, 15, 17},           //Frigio
                   {0, 2, 4, 6, 7, 9, 11, 12, 14, 16, 17},           //Lidio
                   {0, 2, 4, 5, 7, 9, 10, 12, 14, 16, 17},           //Mixolidio
                   {0, 2, 3, 5, 7, 8, 10, 12, 14, 15, 17},           //Menor
                   {0, 1, 3, 5, 6, 8, 10, 12, 13, 15, 17},           //Locrio
                   {0, 3, 5, 6, 7, 10, 12, 15, 17, 18, 19},          //Blues
                   {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10}};              //Cromática

int scale = 0;                //variable de asignación de la escala
int traspose = 0;             //variable de trasposición 
int octave = 2;               //variable de asignación de osctava

int state = 0;                //variable maquina de estados

bool flag_on = 0;             //flag de comienzo del loop secuenciador

EthernetServer server(80);
EthernetClient client;
String currentLine;

// --------------------------   Declaración funciones  ---------------------------------------//
void printConfig();
void printEthernetData();
void cliente();
void checkMIDI();
void mf10filter();
int pitch_calc (int MIDInote, int transpose, int scale, int octave);
int note2number (byte note);



/////////////////////////////////////////////////////////////////////////////////////////////////
//--------------------------------------- SET UP ----------------------------------------------//
/////////////////////////////////////////////////////////////////////////////////////////////////

void setup()
{
  
  Serial.begin(115200);                    // Inicializamos puerto serie
  Serial3.begin(31250);                    // Inicializamos puerto MIDI
  
  //Inicialización ethernet
  Serial.println("Connecting to Ethernet....");  
  Ethernet.begin(0);
  server.begin();
  printEthernetData();

  //Inicialización Ticker y PWM
  SysTickDisable();                      // Disables SysTick during the configuration
  SysTickPeriodSet(TickerPeriod);        // Define the period of the counter. When it reaches 0, it activates the interrupt
  SysTickIntRegister(&Ticker);           // The interrupt is associated to the SysTick ISR
  SysTickIntEnable();                    // The SysTick interrupt is enabled
  SysTickEnable();                       // The SysTick is enabled, after having been configured
  IntMasterEnable();                     // All interrupts are enabled  

  //Inicialización entradas y salidas
  pinMode(PWMnote, OUTPUT);              // Pin PWM sonido como salida
  pinMode(PWMenv, OUTPUT);               // Pin PWM envolvente como salida
  pinMode(PWMfc, OUTPUT);                // Pin PWM frecuencia control LP como salida 
  pinMode(ReadV, INPUT);                 // Pin analógico lectura voltaje potenciometro
                                         // como entrada
  PWMWrite(PWMnote, 100, 0, 0);          // Incialización PWM nota a cero
  PWMWrite(PWMenv, 100, 0, 0);           // Incialización PWM envolvente a cero

  
 
}

/////////////////////////////////////////////////////////////////////////////////////////////////
//--------------------------------------- LOOP  -----------------------------------------------//
/////////////////////////////////////////////////////////////////////////////////////////////////

void loop()
{
  mf10filter();
  
  checkMIDI();
  
  cliente();
}

/////////////////////////////////////////////////////////////////////////////////////////////////
//--------------------------------------- TICKER  ---------------------------------------------//
/////////////////////////////////////////////////////////////////////////////////////////////////

void Ticker()
{
  //Actualiza valor contador
  tickercount++;

  //Verifica contador llega a 100
  if (tickercount == 400) {
    //Verifica que se ha pulsado tecla
    if (trig == 1 ) {
      DC_env = 100;                             //Ciclo trabajo envolvente a 100
      PWMWrite(PWMnote, 100, 50, pitch);        //Generación nota pulsada
      PWMWrite(PWMfc,100,50,pitch*100);         //Genera señal control frecuencia filtro LP
      trig = 0;                                 //Resetea trig
    }

    //Generación PWM envolvente
    PWMWrite(PWMenv, 100, DC_env, FENV);

    //Actualiza ciclo de trabajo envolvente disminuyendolo en una unidad
    if (DC_env > 0) DC_env--;

    //Pone a cero las señales PWM de la señal y la envolvente
    if (DC_env == 0) {
      PWMWrite(PWMnote, 100, 0, 0);
      PWMWrite(PWMenv, 100, 0, 0);
    }

    //Resetea el contador
    tickercount = 0;
  }
}


/////////////////////////////////////////////////////////////////////////////////////////////////
//------------------------------ DECLARACIÓN FUNCIONES  ---------------------------------------//
/////////////////////////////////////////////////////////////////////////////////////////////////


void cliente(){

client = server.available();

  if (client) {                             // if you get a client,
    Serial.print("new client on port ");           // print a message out the serial port
    Serial.println(client.port());
    currentLine = "";
    boolean newConnection = true;     // flag for new connections
    unsigned long connectionActiveTimer;  // will hold the connection start time

    while (client.connected()) {       // loop while the client's connected
      if (newConnection){                 // it's a new connection, so
        connectionActiveTimer = millis(); // log when the connection started
        newConnection = false;          // not a new connection anymore
      }
      if (!newConnection && connectionActiveTimer + 1000 < millis()){ 
        // if this while loop is still active 1000ms after a web client connected, something is wrong
        break;  // leave the while loop, something bad happened
      }


      if (client.available()) {             // if there's bytes to read from the client,    
        char c = client.read();             // read a byte, then
        // This lockup is because the recv function is blocking.
        Serial.print(c);
        if (c == '\n') {                    // if the byte is a newline character
          // if the current line is blank, you got two newline characters in a row.
          // that's the end of the client HTTP request, so send a response:
          if (currentLine.length() == 0) {  
            break;         
          } 
          else {      // if you got a newline, then clear currentLine:
            currentLine = "";
          }
        }     
        else if (c != '\r') {    // if you got anything else but a carriage return character,
          currentLine += c;      // add it to the end of the currentLine
        }
      
        if (currentLine.endsWith("GET / ")) {
          //statusConfig = 0;
          printConfig();
         }
        if (currentLine.endsWith("GET /config.html ")) {
          printConfig();
        }
        // Check to see if the client request scale
        if (currentLine.endsWith("GET /MAYOR")) {
          scale = 0;
          printConfig();
        }         
        if (currentLine.endsWith("GET /JONICO")) {
          scale = 1;
          printConfig();
        }
        if (currentLine.endsWith("GET /FRIGIO")) {
          scale = 2;
          printConfig();
        }      
        if (currentLine.endsWith("GET /LIDIO")) {
          scale = 3;
          printConfig();
        }    
        if (currentLine.endsWith("GET /MIXOLIDIO")) {
          scale = 4;
          printConfig();
        }  
        if (currentLine.endsWith("GET /MENOR")) {
          scale = 5;
          printConfig();
        }       
        if (currentLine.endsWith("GET /LOCRIO")) {
          scale = 6;
          printConfig();
        }     
        if (currentLine.endsWith("GET /BLUES")) {
          scale = 7;
          printConfig();
        }  
        if (currentLine.endsWith("GET /CROMATICA")) {
          scale = 8;
          printConfig();
        }             

        //Traspose upgrade
        if (currentLine.endsWith("GET /TRASPOSE_UP")) {
          traspose = traspose + 1;
          printConfig();
        }     
        if (currentLine.endsWith("GET /TRASPOSE_DOWN")) {
          traspose = traspose - 1;
          printConfig();
        }   

        //Octave upgrade
        if (currentLine.endsWith("GET /OCTAVE_UP")) {
          octave = octave + 1;
          printConfig();
        }     
        if (currentLine.endsWith("GET /OCTAVE_DOWN")) {
          octave = octave - 1;
          printConfig();
        }  

       }
    }
    // close the connection:
    client.stop();
    //Serial.println("client disonnected");
  }
  
}

//////////////////////////////////////////////////////////////////////////////////

void printConfig()
{
  // HTTP headers always start with a response code (e.g. HTTP/1.1 200 OK)
  // and a content-type so the client knows what's coming, then a blank line: 

  client.println("HTTP/1.1 200 OK");
  client.println("Content-type:text/html");
  client.println();
  client.print("\n"); 
  client.println("<html><head><title>Roland Server</title></head><body align=center>");
  client.println("<h1 align=center><font color=\"red\">Roland Interface</font></h1>");
  client.println("</body></html>");

  client.println("Mayor   <button onclick=\"location.href='/MAYOR'\">ON</button><br>");
  client.println("Jonico   <button onclick=\"location.href='/JONICO'\">ON</button><br>");
  client.println("Frigio   <button onclick=\"location.href='/FRIGIO'\">ON</button><br>");
  client.println("Lidio   <button onclick=\"location.href='/LIDIO'\">ON</button><br>");
  client.println("Mixolidio   <button onclick=\"location.href='/MIXOLIDIO'\">ON</button><br>");
  client.println("Menor   <button onclick=\"location.href='/MENOR'\">ON</button><br>");
  client.println("Locrio   <button onclick=\"location.href='/LOCRIO'\">ON</button><br>");
  client.println("Blues   <button onclick=\"location.href='/BLUES'\">ON</button><br>");
  client.println("Cromatica   <button onclick=\"location.href='/CROMATICA'\">ON</button><br>");
  
  client.print("<br><br>");

  client.print("TRASPOSE   <button onclick=\"location.href='/TRASPOSE_UP'\">UP</button>");
  client.println(" <button onclick=\"location.href='/TRASPOSE_DOWN'\">DOWN</button><br>");

  client.print("OCTAVE   <button onclick=\"location.href='/OCTAVE_UP'\">UP</button>");
  client.println(" <button onclick=\"location.href='/OCTAVE_DOWN'\">DOWN</button><br>");
  
  // optional: client.println(" <input type=\"submit\" value=\"Update\"> ");
  client.println(" </form> ");
  // The HTTP response ends with another blank line:
  client.println();
  // break out of the while loop:
}

//////////////////////////////////////////////////////////////////////////////////

void printEthernetData() {
  // print your IP address:
  Serial.println();
  Serial.println("IP Address Information:");  
  IPAddress ip = Ethernet.localIP();
  Serial.print("IP Address:\t");
  Serial.println(ip);

  // print your MAC address:

  IPAddress subnet = Ethernet.subnetMask();
  Serial.print("NetMask:\t");
  Serial.println(subnet);

  // print your gateway address:
  IPAddress gateway = Ethernet.gatewayIP();
  Serial.print("Gateway:\t");
  Serial.println(gateway);

  // print your gateway address:
  IPAddress dns = Ethernet.dnsServerIP();
  Serial.print("DNS:\t\t");
  Serial.println(dns);

}

//////////////////////////////////////////////////////////////////////////////////

void checkMIDI(){
    
    if(Serial3.available()){
    MIDIcommand = Serial3.read();
     
    if(MIDIcommand == 0xFA){
      
      flag_on = 1;
      
    
    }else if(MIDIcommand == 0xFC){
      state = 0;
      //PWMWrite(PWMenv, 100, 0, 0);
      flag_on = 0;
    
    }else if(flag_on){
        
      if(MIDIcommand != 0xF8){
        if(state == 0 && MIDIcommand == 0x99){                      //Si hacemos 0x90 <= MIDIcommand <= 0x9F leemos todos los canales MIDI
         state = 1;
        }
        else if(state == 1){
          if(MIDIcommand == 0xFE){
            state = 0;
          }
          else{
             MIDInote = MIDIcommand;
             state = 2;
          }
        }else if(state == 2){
           MIDIvelocity = MIDIcommand;
           state = 1;
           if(MIDIvelocity > 0x00){
             pitch = pitch_calc(MIDInote,traspose,scale,octave);
             trig = 1;
             
              
            }
         }
      }
    }
  
  }
}

//////////////////////////////////////////////////////////////////////////////////

void mf10filter(){

  int sensorValue = analogRead(ReadV);
  fc = 500*(sensorValue/1023.0)+250;
  PWMWrite(PWMfc,100,50,fc*100);
  
}

//////////////////////////////////////////////////////////////////////////////////


int pitch_calc (int MIDInote, int transpose, int scale, int octave){
  int pitch;
  int key;
  
  key = note2number(MIDInote);

  pitch = pow(1.0594, matrix[scale][key-1])*pow(1.0594, transpose)*pow(2, octave)*32.7;

  return(pitch);
}


//////////////////////////////////////////////////////////////////////////////////
int note2number (byte note) {

  int number;

  switch (note) {

    case 35:  
      number = 1;         //BD
      
      break;
    case 36:
      number = 1;         
      
      break;

    case 38:
      number = 2;         //SD
      
      break;
    case 40: 
      number = 2;
      
      break;
      
    case 41:
      number = 3;         //LT         
      
      break;
    case 43:  
      number = 3;
      
      break;

    case 45: 
      number = 4;         //MT
      
      break;
    case 47:
      number = 4;
      
      break;
      
    case 48: 
      number = 5;         //HT
      
      break;
    case 50: 
      number = 5;
      
      break;

    case 37: 
      number = 6;         //RS
      
      break;
        
    case 39:
      number = 7;         //HC
      
      break;
      
    case 42:
      number = 8;         //CH
      
      break;
    case 44:
      number = 8;
      
      break;

    case 46:
      number = 9;         //OH
      
      break;

    case 49:
      number = 10;        //CC
      
      break;
      
    case 51:
      number = 11;        //RC
      
      break;

    default:
      number = 11;
      break;
  }

  return(number);
}
