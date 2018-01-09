/**
    Protótipo de Fechadura Eletrônica Simples(Sem gerenciamento via rede)
    autor: Deividson Calixto da Silva
*/
#include <LiquidCrystal.h>
#include <util.h>
#include <Key.h>
#include <Keypad.h>
#include <avr/wdt.h>
#include <stdlib.h>
#include <EEPROM.h>

unsigned long millisLeitAntTeclado = 0;
unsigned long millisLeituraTeclado = 0;
unsigned long millisAnteriorConex = 0;
unsigned long millisAtualConex = 0;
unsigned long millisAnteriorlSenha = 0;
unsigned long millisAtualRenovaConex = 0;
unsigned long millisAnteriorRenovaConex = 0;

const int TEMPO_LER_TECLADO = 500;
//Tempo de checagem se a conexão, senão reboota o atmega.
const long TEMPO_VERIFICA_CONEX = 120000;
//10 Seg. Tempo que o usuário tem para digitar a senha.
const int TEMPO_LEITURA_SENHA = 10000;

//Led de leitura efetuada do cartão RFID
const int LED_LEITURA_RFID = 24;
//Led de Liberação de acesso;
const int LED_LIB_ACESSO_RFID = 26;
//Led indicado de Acesso Inválido
const int LED_ACESSO_INVALIDO = 22;
//rele de acionamento da porta
const int PORTA_RELE = 36;
//BUZZER;
const int BUZZER = 7;
//Display LCD
const int PINO_PWM_LCD = 6;
//Luz de fundo LCD
const int BACKLIGHT_LED = 25;
//Endereco de memória da EEPROM onde sera gravado. 0-1024(promini 328p)
const int ADDR_SENHAS = 0;

struct Senhas {
	  char[6] senhaMaster;
	  char[6] senha1;
	  char[6] senha2;
	  char[6] senha3;
	  char[6] senha4;
	  char[6] senha5;  
};



//controle de excecução
int parado = 0;
#define ABERTURA_PORTA 1000
#define RST 5
#define BOTAO_ABRIR 28

//LiquidCrystal(rs, enable, d4, d5, d6, d7)
LiquidCrystal lcd(31, 30, 32, 33, 34, 35);

//teclado
const byte ROWS = 4;
const byte COLS = 3;
char keys[ROWS][COLS] = {
  {'1', '2', '3'},
  {'4', '5', '6'},
  {'7', '8', '9'},
  {'*', '0', '#'}
};

//byte rowPins[ROWS] = {42, 43, 44, 45};
byte rowPins[ROWS] = {45, 44, 43, 42};
//byte colPins[COLS] = {38, 39, 40};
byte colPins[COLS] = {40, 39, 38};
Keypad keypad = Keypad(makeKeymap(keys), rowPins, colPins, ROWS, COLS);

void setup() {
	Serial.begin(9600);
	
  //regula o bounce do teclado
  keypad.setDebounceTime(60);

  //ligação teclado
  //linhas
  pinMode(42, OUTPUT);
  pinMode(43, OUTPUT);
  pinMode(44, OUTPUT);
  pinMode(45, OUTPUT);
  //colunas
  pinMode(38, INPUT);
  pinMode(39, INPUT);
  pinMode(40, INPUT);

  //botoeira de abrir porta
  pinMode(BOTAO_ABRIR, INPUT_PULLUP);


  lcd.begin(16, 2);
  pinMode(PINO_PWM_LCD, OUTPUT);

  //contraste do LCD;
  analogWrite(PINO_PWM_LCD, 60);
  lcd.setCursor(0, 0);
  lcd.write("-+#MINHA=CASA#+-");

  //backlight led
  pinMode(BACKLIGHT_LED, OUTPUT);
  digitalWrite(BACKLIGHT_LED, HIGH);

  pinMode(LED_LEITURA_RFID, OUTPUT);
  pinMode(LED_LIB_ACESSO_RFID, OUTPUT);
  pinMode(LED_ACESSO_INVALIDO, OUTPUT);

  digitalWrite(LED_ACESSO_INVALIDO, HIGH);
  digitalWrite(LED_LEITURA_RFID, HIGH);
  digitalWrite(LED_LIB_ACESSO_RFID, HIGH);

  
  lcd.setCursor(0, 1);
  lcd.write("Iniciando.......");
  delay(1000);
  digitalWrite(LED_ACESSO_INVALIDO, LOW);
  digitalWrite(LED_LEITURA_RFID, LOW);
  digitalWrite(LED_LIB_ACESSO_RFID, LOW);

  lcd.setCursor(14, 1);
  lcd.write("OK");
  toqueInicio();
  delay(1000);

  //lcd.clear();

  //coloco o watchDog por 8 segundos;
  wdt_enable(WDTO_8S);
  
  //Excecutar somente no primeiro carregamento do programa! Depois comentar o código entre ****
  //*******************************************************************************************
  Senhas senhas {
	"101010",
	"111111",
	"222222",
	"333333",
	"444444",
	"555555"
  }
  //Gerando senhas na EEPROM
  EEPROM.put(ADDR_SENHAS, senhas);
  //*******************************************************************************************
  
  Senhas senhaAtivas;
  //Lendo senhas da EEPROM
  EEPROM.get(ADDR_SENHAS, senhasAtivas);   
  Serial.print("RESULTADO SENHA MASTER: ");
  Serial.println(senhaAtivas.senhaMaster);
}

void loop() {
  //reset do contador do watchdog, estou vivo!
  wdt_reset();

  lcd.setCursor(0, 0);
  lcd.write("-+#MINHA-CASA#+-");
  lcd.setCursor(0, 1);
  //lcd.write("0123456789abcdef");
  lcd.write("APERTE #        ");

  //desliga backlight
  digitalWrite(BACKLIGHT_LED, LOW);

  millisLeituraTeclado = millis();

  //Evitar overflow do millis()
  if (millisLeituraTeclado < millisLeitAntTeclado) {
    millisLeitAntTeclado = millisLeituraTeclado;
  }
  
  //##############################################################################################
  //##################################ENTRADA POR SENHA###########################################
  //##############################################################################################
  if (millis() - millisLeituraTeclado >= TEMPO_LER_TECLADO){
	millisLeitAntTeclado = millis();
	char keyPress = keypad.getKey();
	if (keyPress == '#') {
		Serial.println("Memoria Livre RG (ANTES CONSULTA):");
		Serial.println(get_free_memory());
		digitalWrite(BACKLIGHT_LED, HIGH);
		tone(BUZZER, 1500, 200);
		lcd.setCursor(0, 1);
		lcd.write("Senha:          ");
		//Lê a senha e habilita a entrada ou não!
		lerSenha();		
	}else if(keyPress == '*'){
		lcd.setCursor(0, 1);		
		lcd.write("CFG...ESPERANDO!");
		mudarSenhas();
	}
  }
}


//Modo de configuração, permite alterar as senha inseridas!
void mudarSenhas(){
	char senhaDig[] = "000000";
	//contador
	byte c = 0;
	unsigned long millisAnteriorlSenha = millis();
	//loop de leitura de senha, lê o teclado enquanto o tempo não esgotar ou 4 digitos serem digitados.
	do {
		wdt_reset();
		//caso o usuário não termine de digitar a senha no tempo estipulado sai do loop
		if (millis() - millisAnteriorlSenha > TEMPO_LEITURA_SENHA) {
		  break;
		} else {
		  char keyPress = keypad.getKey();
		  if (keyPress != NO_KEY) {
			tone(BUZZER, 1500, 200);
			lcd.setCursor(6 + c, 1);
			lcd.write("*");
			senhaDig[c] = keyPress;
			c++;
		  }
		}
	} while (c < 6);
	//verifico a senha
	if (strcmp(senhasAtivas.senhaMaster, senhaDig) == 0) {
		lcd.setCursor(0,0);		
		lcd.write("Dig NumREF SENHA");
		lcd.setCursor(0,1);		
		lcd.write("0:MST 1:S1..5:S5");
		char [6] senhaAux = "000000";
		char op;
		do{
			op = keypad.getKey();
			switch(op){
				'0':
					//rotina de troca de senha master.
					senhaAux = geNovaSenha();
					if (strcmp(senhaAux, "000000")){
						lcd.setCursor(0,1);		
						lcd.write("SENHA NAO CONF.!");
					}else{
						senhasAtivas.senhaMaster = senhaAux;
						gravaSenhaEEPROM(senhasAtivas);
					}					
					break;
				'1':
					senhaAux = getNovaSenha();
					if (stmcmp(senhaAux, "000000")) {
						lcd.setCursor(0,1);		
						lcd.write("SENHA NAO CONF.!");
					}else{
						senhasAtivas.senha1 = senhaAux;
						gravaSenhaEEPROM(senhasAtivas);
					}
					break;
				'2':
					senhaAux = getNovaSenha();
					if (stmcmp(senhaAux, "000000")) {
						lcd.setCursor(0,1);		
						lcd.write("SENHA NAO CONF.!");
					}else{
						senhasAtivas.senha2 = senhaAux;
						gravaSenhaEEPROM(senhasAtivas);
					}
					break;
				'3':
					senhaAux = getNovaSenha();
					if (stmcmp(senhaAux, "000000")) {
						lcd.setCursor(0,1);		
						lcd.write("SENHA NAO CONF.!");
					}else{
						senhasAtivas.senha3 = senhaAux;
						gravaSenhaEEPROM(senhasAtivas);
					}
					break;
				'4':
					senhaAux = getNovaSenha();
					if (stmcmp(senhaAux, "000000")) {
						lcd.setCursor(0,1);		
						lcd.write("SENHA NAO CONF.!");
					}else{
						senhasAtivas.senha4 = senhaAux;
						gravaSenhaEEPROM(senhasAtivas);
					}
					break;
				'5'
					senhaAux = getNovaSenha();
					if (stmcmp(senhaAux, "000000")) {
						lcd.setCursor(0,1);		
						lcd.write("SENHA NAO CONF.!");
					}else{
						senhasAtivas.senha5 = senhaAux;
						gravaSenhaEEPROM(senhasAtivas);
					}
					break;
				default:				
			}
		}while(keyPress != '*')
	} else {
		lcd.setCursor(0,1);		
		lcd.write("SENHA NAO CONF.!");
	}	
}

//Grava a senha passada na posicao dela
void gravaSenhaEEPROM(Senhas s){
	EEPROM.put(ADDR_SENHAS, s);
	Serial.print("GRAVANDO SENHA NA EEPROM!");
}

//lê uma nova senha e valida está, para armazenamento futuro!
char [6] getNovaSenha(){
	char sNova = "000000";
	char sConf = "000000";
	byte c = 0;
	unsigned long millisAnteriorlSenha = millis();
	//loop de leitura de senha, lê o teclado enquanto o tempo não esgotar ou 4 digitos serem digitados.
	do {
		wdt_reset();
		//caso o usuário não termine de digitar a senha no tempo estipulado sai do loop
		if (millis() - millisAnteriorlSenha > TEMPO_LEITURA_SENHA) {
		  break;
		} else {
		  char keyPress = keypad.getKey();
		  if (keyPress != NO_KEY) {
			tone(BUZZER, 1500, 200);
			lcd.setCursor(6 + c, 1);
			lcd.write("*");
			sNova[c] = keyPress;
			c++;
		  }
		}
	} while (c < 6);
	
	//confirmar a senha:
	c = 0;
	millisAnteriorlSenha = millis();
	do {
		wdt_reset();
		//caso o usuário não termine de digitar a senha no tempo estipulado sai do loop
		if (millis() - millisAnteriorlSenha > TEMPO_LEITURA_SENHA) {
		  break;
		} else {
		  char keyPress = keypad.getKey();
		  if (keyPress != NO_KEY) {
			tone(BUZZER, 1500, 200);
			lcd.setCursor(6 + c, 1);
			lcd.write("*");
			sNova[c] = keyPress;
			c++;
		  }
		}
	} while (c < 6);
	
	//verifico a senha digitadas são iguais
	if (strcmp(sNova, sConf) == 0) {
		return sNova;
	} else {
		return "000000";
	}	
}

void lerSenha(){
	char senhaDig[] = "000000";
	//contador
	byte c = 0;
	unsigned long millisAnteriorlSenha = millis();
	//loop de leitura de senha, lê o teclado enquanto o tempo não esgotar ou 4 digitos serem digitados.
	do {
		wdt_reset();
		//caso o usuário não termine de digitar a senha no tempo estipulado sai do loop
		if (millis() - millisAnteriorlSenha > TEMPO_LEITURA_SENHA) {
		  break;
		} else {
		  char keyPress = keypad.getKey();
		  if (keyPress != NO_KEY) {
			tone(BUZZER, 1500, 200);
			lcd.setCursor(6 + c, 1);
			lcd.write("*");
			senhaDig[c] = keyPress;
			c++;
		  }
		}
	} while (c < 6);
	//verifico a senha
	if (strcmp(senhaBco, senhaDig) == 0) {
		acessoPermitido();
	} else {
		acessoNegado();
	}	
}

void acessoPermitido() {
	digitalWrite(LED_LIB_ACESSO_RFID, HIGH);
	digitalWrite(PORTA_RELE, LOW);
	tone(BUZZER, 2900, 300);
	delay(ABERTURA_PORTA);
	digitalWrite(PORTA_RELE, HIGH);
	digitalWrite(LED_LIB_ACESSO_RFID, LOW);
	//digitalWrite(PORTA_RELE, HIGH);
}

void acessoNegado() {
	//Serial.println("Acesso nao autorizado! Chave nao encontrada!");
	lcd.setCursor(0, 1);
	lcd.write(" NAO AUTORIZADO ");
	for (byte c = 0; c <= 1; c++) {
	tone(BUZZER, 2900);
	digitalWrite(LED_ACESSO_INVALIDO, HIGH);
	delay(400);
	digitalWrite(LED_ACESSO_INVALIDO, LOW);
	noTone(BUZZER);
	}
}

//toque inicio
void toqueInicio() {
	for (int i = 0; i <= 2500; i += 500) {
	tone(BUZZER, i + 1000);
	delay(400);
	}
	noTone(BUZZER);
}

//reset via comando
void soft_reset() {
	asm volatile("jmp 0");
}

int get_free_memory() {
	extern char __bss_end;
	extern char *__brkval;
	int free_memory;
	if ((int)__brkval == 0)
	free_memory = ((int)&free_memory) - ((int)&__bss_end);
	else
	free_memory = ((int)&free_memory) - ((int)__brkval);
	return free_memory;
}