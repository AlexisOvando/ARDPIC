/*
Este es el codigo para la placa arduino, se usa como programador LVP para el pic16f877a,
no se ha probado en otros pics, ademas que las palabras de control estan especificamente hechas
para este pic.  

Conexiones entre el Arduino y el PIC:
  Arduino   |    PIC
--------------------------------  
  MCLR(8)  ->   MCLR (usualmente pin 1)
  PGM(9)   ->    RB3
  CLK(10)  ->    RB6
  DATA(11) ->    RB7

Si tu PIC ya está configurado con LVP desactivado, simplemente no conectes el pin PGM
a nada, mantén el pin MCLR del PIC en bajo. Luego, después de iniciar la conexión serial
en el programa de Python y antes de elegir una opción, conecta el pin MCLR del PIC a 13V
(12V también sirve). Es posible que no pase la verificación después porque LVP ya estará habilitado,
así que simplemente reconecta el pin MCLR y PGM al Arduino para probarlo.

Programa revisado por AYOP, 01/12/2024, si necesitas ayuda mandame un correo C: 

*/

#define MCLR 8     // Definir el pin MCLR del Arduino
#define PGM 9      // Definir el pin PGM del Arduino
#define CLK 10     // Definir el pin CLK del Arduino
#define DATA 11    // Definir el pin DATA del Arduino

int DATA_SIZE = 0; // Tamaño de los datos a programar
int debugging = 0;  // Variable para activar el modo de depuración

void setup() 
{
    // Configurar los pines de control como salidas
    pinMode(MCLR, OUTPUT);
    pinMode(PGM, OUTPUT);
    pinMode(DATA, OUTPUT);
    pinMode(CLK, OUTPUT);
    
    // Inicializar los pines en bajo
    digitalWrite(DATA, LOW);
    digitalWrite(CLK, LOW);
    digitalWrite(PGM, LOW);
    digitalWrite(MCLR, LOW);
    
    // Iniciar comunicación serial a 9600 baudios
    Serial.begin(9600);
    
    // Obtener el tamaño de los datos (envía 0x04)
    DATA_SIZE = nextWord();
    
    // Solicitar opción al usuario
    Serial.print("Ingrese P para programar, V para verificación, o D para verificación con depuración: ");
}

void loop() {
    // Solicitar al usuario una opción
    Serial.write(2); // Solicitar entrada
    while(Serial.available() == 0); // Esperar a que haya entrada
    int input = Serial.read();
    
    // Procesar la opción recibida
    switch(input)
    {
        case 'p': // Opción para programar
        case 'P':
          LVP_init();  // Inicializar el modo de programación LVP
          Program(DATA_SIZE);  // Iniciar el proceso de programación
          break;
        case 'v': // Opción para verificación
        case 'V':
          debugging = 0;  // Desactivar depuración
          LVP_init();  // Inicializar LVP
          verify(DATA_SIZE);  // Iniciar la verificación
          break;
        case 'd': // Opción para verificación con depuración
        case 'D':
          debugging = 1;  // Activar depuración
          LVP_init();  // Inicializar LVP
          verify(DATA_SIZE);  // Iniciar la verificación
          break;
    }
}

// Función para inicializar el modo de programación LVP
void LVP_init()
{
    // El protocolo LVP mantiene los pines DATA y CLK bajos
    // luego lleva el pin PGM alto y finalmente el pin MCLR alto
    digitalWrite(DATA, LOW);
    digitalWrite(CLK, LOW);
    digitalWrite(PGM, LOW);
    digitalWrite(MCLR, LOW);
    delay(1);
    digitalWrite(PGM, HIGH);  // Activar el pin PGM
    delayMicroseconds(1);
    digitalWrite(MCLR, HIGH);  // Activar el pin MCLR
    delayMicroseconds(10);
}

// Función para programar el PIC
void Program(int _data_size)
{
    chipErase();  // Borrar el chip antes de programar
    for (int i = 0; i < _data_size; i++)
    {
       LoadData(nextWord());  // Cargar los datos a programar
       StartProgram();  // Iniciar la programación
       increment();  // Incrementar la dirección de memoria
    }
    loadConfig();  // Cargar la configuración
    for(int i = 0; i < 7; i++) increment();  // Incrementar 7 veces
    LoadData(0b11111111111010);  // Cargar la palabra de configuración (editar solo si sabes lo que haces)
    StartEraseProgram();  // Iniciar el borrado de la programación
    Serial.println("Programación finalizada");
    Serial.write(3);
}

// Función para verificar la programación
void verify(int _data_size)
{
    int flag = 0;  // Bandera para indicar errores
    for(int i = 0; i < _data_size; i++)
    {
        int _word = nextWord();  // Leer la siguiente palabra de datos
        if(readData() != _word)  // Verificar si los datos leídos son correctos
        {
            if(debugging == 1)
            {
                Serial.print("Error en la palabra ");
                Serial.print(i, DEC);
                Serial.print(": Esperado ");
                Serial.print(_word, BIN);
                Serial.print(" pero encontrado ");
                Serial.println(readData(), BIN);
            }
            flag = 1;
        }
        delay(1);
        increment();  // Incrementar la dirección de memoria
    }
    loadConfig();  // Cargar la configuración
    for(int i = 0; i < 7; i++) increment();  // Incrementar 7 veces
    if(readData() != 0b11111111111010)  // Verificar la palabra de configuración
    {
        if(debugging == 1)
        {
            Serial.print("Error en la palabra de configuración: ");
            Serial.println(readData(), BIN);
        }
        flag = 1;
    }
    if(flag == 0)
        Serial.println("Verificación completada");
    else if(debugging == 0)
        Serial.println("Verificación fallida, intente reprogramar o depurar.");
    Serial.write(3);
}

// Función para leer la siguiente palabra de datos
int nextWord()
{
    int value = 0;
    if(DATA_SIZE == 0)
        Serial.write(4);
    else
        Serial.write(1);
    
    for(int i = 0; i < 2; i++)
    {
        while(Serial.available() == 0);
        value += Serial.read() << (8 * i);
    }
    return value;
}

// Función para escribir un bit
void writeBit(int a)
{
    digitalWrite(DATA, a);
    digitalWrite(CLK, HIGH);
    delayMicroseconds(1);
    digitalWrite(CLK, LOW);
    delayMicroseconds(1);
    digitalWrite(DATA, LOW);
}

// Función para leer un bit
byte readBit()
{
    byte _bit;
    digitalWrite(CLK, HIGH);
    delayMicroseconds(1);
    _bit = digitalRead(DATA);
    digitalWrite(CLK, LOW);
    delayMicroseconds(1);
    return _bit;
}

// Función para enviar un comando
void sendCommand(char command)
{
    for (int i = 1; i <= 0b100000; i *= 2)
    {
        writeBit((command & i) >= 1);
    }
}

// Función para enviar datos
void sendData(uint16_t dataword)
{
    writeBit(0);
    for (int i = 1; i <= 0b10000000000000; i *= 2)
    {
        writeBit((dataword & i) >= 1);
    }
    writeBit(0);
}

// Función para cargar datos en el PIC
void LoadData(uint16_t dataword)
{
    sendCommand(0b000010);
    delayMicroseconds(2); 
    sendData(dataword);
    delayMicroseconds(2);
}

// Función para leer datos del PIC
uint16_t readData()
{
    sendCommand(0b000100);
    delayMicroseconds(2);
    uint16_t value = 0;
    pinMode(DATA, INPUT);
    readBit();
    for(int i = 0; i < 14; i++)
    {
        value = value | (readBit() << i);
    }
    readBit();
    pinMode(DATA, OUTPUT);
    return value;
}

// Función para iniciar la programación
void StartProgram()
{
    sendCommand(0b011000);
    delay(1);
    sendCommand(0b010111);
    delayMicroseconds(2);
}

// Función para iniciar el borrado y programación
void StartEraseProgram()
{
    sendCommand(0b001000);
    delay(4);
}

// Función para cargar la configuración
void loadConfig()
{
    sendCommand(0b000000);
    delayMicroseconds(2); 
    sendData(0x3FFF);  // Configuración por defecto
    delayMicroseconds(2);
}

// Función para incrementar la dirección de memoria
void increment()
{
    sendCommand(0b010000);
    delay(1);
}

// Función para borrar el chip
void chipErase()
{
    sendCommand(0b001010);
    delay(1);
}
