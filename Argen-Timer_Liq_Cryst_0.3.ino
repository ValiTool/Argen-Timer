#include <Wire.h>
#include <JC_Button.h> // Jack Christensens Button library https://github.com/JChristensen/Button
#include <MenuSystem.h> // https://github.com/jonblack/arduino-menusystem
#include <LiquidCrystal_I2C.h>
#include <EEPROM.h>

/* NOTAS:
Los displays (Ready Review Timer etc) están definidos como una función void, por eso se pueden llamar antes porque están definidos más abajo

Primero defino el menú y sus componentes, después se agregan en Setup con el Addmenu
  */

//Nano SDA = A4;
//Nano SCL = A5;
LiquidCrystal_I2C lcd(0x3F,20,4);


// For buttons
#define PULLUP true
#define INVERT true
#define DEBOUNCE_MS 20
#define LONG_PRESS 1000

Button StartButton(3, PULLUP, INVERT, DEBOUNCE_MS);
Button DownButton(5, PULLUP, INVERT, DEBOUNCE_MS);
Button UpButton(4, PULLUP, INVERT, DEBOUNCE_MS);

// this constant won't change:
const int DetectorPin = 7;
const int BuzzerPin = 9;

// Variables will change:
int ShotCounter = 0;
int TotalShots = 0;
int DetectorState = 0;
int LastDetectorState = 1;
int TimerState = 0;
int XPos = 0;
int DelayedStart = EEPROM.read(1);
int BuzzerEnabled = EEPROM.read(2);
int DebounceDelay = EEPROM.read(3);    //"Deaf-time" in ms after registered shot
int DelayedStartTime = 0;
int SecondBeepTime = 0;


// the following variables are long's because the time, measured in miliseconds,
// will quickly become a bigger number than can be stored in an int.
long lastDebounceTime = 0;  // The last time a shot was detected
long StartTime = 0;
long CurrentShotTime = 0;

long currentMillis;
long previousMillis;
long ShotsArray[99];

float LatestShotTime = 0;
float FirstShotTime = 0;
float PrevShotTime = 0;
float SplitShotTime = 0;
float BestSplitShotTime = 0;

//Como yo uso pull-up, mando a masa a los botones, le cambio el estado a HIGH e invierto el orden en el Debouncer
boolean current_up = HIGH;
boolean last_up = HIGH;
boolean current_down = HIGH;
boolean last_down = HIGH;
boolean current_sel = HIGH;
boolean last_sel = HIGH;

//Custom char variables

//Custom return char
byte back[8] = {
  0b00100,
  0b01000,
  0b11111,
  0b01001,
  0b00101,
  0b00001,
  0b00001,
  0b11111
};
//Custom arrow char
byte arrow[8] = {
  0b00000,
	0b00100,
	0b00110,
	0b00111,
	0b00111,
	0b00110,
	0b00100,
	0b00000
};
//Custom up char
byte arrowup[8] = {
	0b00100,
	0b01110,
	0b11111,
	0b00000,
	0b00000,
	0b00000,
	0b00000,
	0b00000
};

//Multiple Screens variables

String menutitles [] = {"Start Delay","Buzzer","Segundo Beep","Cancelacion de Eco","Config de Fabrica","Brillo y Contraste",};
int page_counter = 1; //Variable que me cuenta la posición en la que me encuentro.
const int totalmenus = sizeof(menutitles) / sizeof(menutitles[1]); //Número total de páginas.
//SubMenu Screens variables

int subpage1_counter=0; //Variable que me cuenta la posición dentro del sub-menu, por cada página que tenga sub menú defino un sub menú.
int subpage2_counter=0; //El valor máximo de esta variable me determina las opciones dentro del menú.
int subpage3_counter=0;
int subpage4_counter=0;
int subpage5_counter=0;
int subpage6_counter=0; //Se podría hacer con un for pero no quiero complicarlo demasiado.

void setup() {
    // initialize the detector pin as a input:
  pinMode(DetectorPin, INPUT);
  pinMode(BuzzerPin, OUTPUT);
  
  //Initialize the LCD.
  lcd.init();
  lcd.backlight();
  lcd.setCursor(5,1);
  lcd.print("ARGEN-TIMER");
  delay(1250);
  lcd.clear();

  DisplayReady1() ;
  //State=2;
  //delay(1000);
}

void loop() {
  
  
  DetectorState = digitalRead(DetectorPin); //Acá al hacer digitalRead hago que el estado del pin sea igual al estado del detector.

  StartButton.read();
  UpButton.read();
  DownButton.read();

  switch (State) {
    case 0:  //Ready state
      currentMillis = millis();
      if (currentMillis - previousMillis > interval) {
        previousMillis = currentMillis;
        if (intervalState == LOW) {
          intervalState = HIGH;
          DisplayReady2();
        }
        else {
          intervalState = LOW;
          DisplayReady1();
        }
      }
      if (StartButton.wasReleased()) {
        State = 5;
        StartTimer();
      }
      else if (StartButton.pressedFor(LONG_PRESS)) {
        State = 1;
        displayMenu();
      }
      break;

    case 1: //intermediate state before Setup in order to detect button release after switching state
      if (StartButton.wasReleased())
        State = 2;
      break;

    case 2: //Setup
      if (UpButton.wasPressed()) {
        ms.prev();
        displayMenu();
      }
      if (DownButton.wasPressed()) {
        ms.next();
        displayMenu();
      }
      if (StartButton.wasReleased()) {
        ms.select();
        lcd.clear();
        displayMenu();
      }
      if (StartButton.pressedFor(LONG_PRESS)) {
        State = 3;
        lcd.clear();
        DisplayReady1();
      }
      break;

    case 3: //intermediate state before Ready in order to detect button release after switching state
      if (StartButton.wasReleased())
        State = 0;
      break;

    case 5: //Timer running!
      DetectShots();
      SecondBeep();
      if (StartButton.pressedFor(LONG_PRESS)) {
        State = 6;
        //lcd.clear();
        TotalShots = (ShotCounter);
        DisplayReview();
      }
      if (StartButton.wasReleased()) {
        //lcd.backlight();
      }
      break;

    case 6: //intermediate state before Review in order to detect button release after switching state
      if (StartButton.wasReleased())
        State = 7;
      break;

    case 7: //Review
      if ((DownButton.wasPressed()) and ((ShotCounter) > 1)) {
        ShotCounter --;
        CurrentShotTime = ShotsArray[((ShotCounter) - 1)];
        PrevShotTime = ShotsArray[((ShotCounter) - 2)];
        Calculate();
        DisplayReview();
      }
      if ((UpButton.wasPressed()) and ((ShotCounter) < (TotalShots))) {
        ShotCounter ++;
        CurrentShotTime = ShotsArray[((ShotCounter) - 1)];
        PrevShotTime = ShotsArray[((ShotCounter) - 2)];
        Calculate();
        DisplayReview();
      }
      if (StartButton.pressedFor(LONG_PRESS)) {
        State = 3;
        lcd.clear();
        ResetTimer();
        DisplayReady1();
      }
      break;

  } //End of case switch-machine

  LastDetectorState = DetectorState;
}

void DetectShots() { //Función de detección de disparos.
  if (DetectorState != LastDetectorState) {
    if (millis() > lastDebounceTime) {
      if (DetectorState == HIGH) {
        CurrentShotTime = millis();
        lastDebounceTime = (CurrentShotTime + DebounceDelay);
        ShotCounter++;
        ShotsArray[((ShotCounter) - 1)] = (CurrentShotTime);
        Calculate();
        DisplayTimer();
        PrevShotTime = CurrentShotTime;
      }
    }
  }
}

void Calculate() { //Cálculo de los tiempos.
  LatestShotTime = (float(((CurrentShotTime) - StartTime)) / 1000);
  //ShotCounter++;

  if (ShotCounter == 1) { //Only on first shot
    FirstShotTime = LatestShotTime;
    PrevShotTime = StartTime;
  }

  SplitShotTime = (float(CurrentShotTime - PrevShotTime) / 1000);

  if (ShotCounter == 2) { //Is thiss needed?
    BestSplitShotTime = SplitShotTime;
  }
  if ((SplitShotTime) < (BestSplitShotTime)) {
    BestSplitShotTime = SplitShotTime;
  }
  }

void DetectCalibrationShots() { //Función de calibración de cancelación de eco.
  if (DetectorState != LastDetectorState) {
    if (DetectorState == HIGH) {
      lastDebounceTime = (millis());
    }
    if (DetectorState == LOW) {
      DebounceDelay = ((millis() - lastDebounceTime) + 10);
      EEPROM.write(3, (DebounceDelay));
      ShotCounter++;
    }
  }

}

void StartTimer() { //Activación función del timer
  //delay (DelayedStartTime);
  if (DelayedStartTime > 0) //De esta manera cada vez que se deje en 0 queda desactivada la función
  {
    DisplayStandby();
    delay (DelayedStartTime);
  }
  /*if (DelayedStart == true) {
    DisplayStandby();
    delay (DelayedStartTime);
  }
   Posibilidad para Delayed Start flotante:
   

  if (DelayedStartTime > 0) //De esta manera cada vez que se deje en 0 queda desactivada la función
  {
    DisplayStandby();
    delay (DelayedStartTime);
  }

  U otra posibilidad es quitar el IF y directamente hacer la función delay, como el DelayedStartTime es flotante, si queda en 0 es instantáneo,
  sólo debería tener bien traducido el valor a milisegundos, o directamente escribir el valor en segundos y hacer DelayedStartTime*1000 y listo.
  */
  DisplayTimer();
  lastDebounceTime = (millis()) + 750; //To make sure buzzer doesn't trigger at shot
  StartTime = millis();
  Beep();
  
  }

void SecondBeep() {
  
  if (millis() - StartTime == SecondBeepTime)
  {
    Beep();
  }
}  


void ResetTimer() {
  ShotCounter = 0;
  LatestShotTime = 0;
  FirstShotTime = 0;
  SplitShotTime = 0;
  BestSplitShotTime = 0;
}

void ClearEEPROM() {
  for (int i = 0; i < 512; i++)
    EEPROM.write(i, 0);
}

void SetDefaults() {
  EEPROM.write(1, 1);
  EEPROM.write(2, 1);
  EEPROM.write(3, 10);
}

void Beep() {
  if (BuzzerEnabled == true) {
    digitalWrite(BuzzerPin, HIGH);
    delay(750);
    digitalWrite(BuzzerPin, LOW);
  }
}

///////////////////////////////////////////////////// Display functions

void DisplayTimer() {
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Primer disparo:");
  lcd.setCursor(16, 0);
  lcd.print(FirstShotTime);

  if (ShotCounter < 10) {
    XPos = 11;
  }
  else {
    XPos = 10;
  }
  lcd.setCursor(0, 1);
  lcd.print("Disparos:");
  lcd.setCursor(XPos, 1);
  lcd.print(ShotCounter);

  lcd.setCursor(0, 2);
  lcd.print("Mejor split:");
  lcd.setCursor(13, 2);
  lcd.print((BestSplitShotTime));

  if (LatestShotTime < 10) {
    XPos = 8;
  }
  else if (LatestShotTime < 100) {
    XPos = 8;
  }
  else {
    XPos = 8;
  }
  lcd.setCursor(0, 3);
  lcd.print("Ultimo:");
  lcd.setCursor(XPos, 3);
  lcd.print(LatestShotTime);
}

void DisplayReview() {
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Revision");
  lcd.setCursor(0, 2);
  lcd.print("Primer disparo:");
  lcd.setCursor(16, 2);
  lcd.print(FirstShotTime);

  if (ShotCounter < 10) {
    XPos = 9;
  }
  else {
    XPos = 9;
  }
  lcd.setCursor(0, 1);
  lcd.print("Disparo:");
  lcd.setCursor(XPos, 1);
  lcd.print(ShotCounter);

  lcd.setCursor(13, 0);
  lcd.print("Split:");
  lcd.setCursor(14, 1);
  lcd.print((SplitShotTime), 2);

  if (LatestShotTime < 10) {
    XPos = 8;
  }
  else if (LatestShotTime < 100) {
    XPos = 8;
  }
  else {
    XPos = 8;
  }
  lcd.setCursor(0, 3);
  lcd.print("Tiempo:");
  lcd.setCursor(XPos, 3);
  lcd.print(LatestShotTime);
}

void DisplayReady1() {
  lcd.setCursor(6, 1);
  lcd.print("TIRADOR");
  lcd.setCursor(7, 2);
  lcd.print ("LISTO...");
}

void DisplayReady2() {
  lcd.setCursor(6, 1);
  lcd.print("TIRADOR");
  lcd.setCursor(7, 2);
  lcd.print ("LISTO   ");
}

void DisplayStandby() {
  lcd.clear();
  lcd.setCursor(6, 1);
  lcd.print("ATENCION");
}

void DisplayCalibration() {
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Dispare una vez");
  }


//PRUEBA DE MENU NUEVO
void displayMenu() { //La idea es reemplazar todo este menú por el de pantallas individuales
  lcd.clear();
  lcd.setCursor(0, 0);
  Menu const* cp_menu = ms.get_current_menu();
  
  MenuComponent const* cp_menu_sel = cp_menu->get_selected();
  
  for (int i = 0; i < cp_menu->get_num_menu_components(); ++i)
  {
    MenuComponent const* cp_m_comp = cp_menu->get_menu_component(i);
    if (cp_menu_sel == cp_m_comp) {
      lcd.setCursor(0,i);
      lcd.print("> ");
          
    }
    else {
      lcd.print("  ");
    }
    lcd.setCursor(1,i); //Agregué la variable del FOR para que imprima renglón a renglón.
    lcd.print(cp_m_comp->get_name());
    lcd.print("");
  }
}


///////////////////////////////////////////////////// Menu callback functions
void on_item3_selected(MenuItem * p_menu_item) {
  lcd.clear();
  lcd.setCursor(0, 1);
  lcd.print("Delay On");
  DelayedStart = true;
  EEPROM.write(1, (DelayedStart));
  delay(1500);
}

void on_item4_selected(MenuItem * p_menu_item) {
  lcd.clear();
  lcd.setCursor(0, 1);
  lcd.print("Delay Off");
  DelayedStart = false;
  EEPROM.write(1, (DelayedStart));
  delay(1500);
}

void on_item5_selected(MenuItem * p_menu_item) {
  ms.back();
  displayMenu();
}

void on_item6_selected(MenuItem * p_menu_item) {
  displayMenu();
}

void on_item7_selected(MenuItem * p_menu_item) {
  displayMenu();
}

void on_item8_selected(MenuItem * p_menu_item) {
  DisplayCalibration();
  do {
    DetectorState = digitalRead(DetectorPin);
    DetectCalibrationShots();
    LastDetectorState = DetectorState;
  }
  while ((ShotCounter) < 2);
  ShotCounter = 0;
  lcd.clear();
  lcd.setCursor(0, 1);
  lcd.print (DebounceDelay);
  lcd.print (" ms");
  delay(1000);
}

void on_item9_selected(MenuItem * p_menu_item) {
  ms.back();
  displayMenu();
}

void on_item10_selected(MenuItem * p_menu_item) {
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Buzzer On");
  BuzzerEnabled = true;
  EEPROM.write(2, (BuzzerEnabled));
  delay(1500);
}

void on_item11_selected(MenuItem * p_menu_item) {
  lcd.clear();
  lcd.setCursor(0, 1);
  lcd.print("Buzzer Off");
  BuzzerEnabled = false;
  EEPROM.write(2, (BuzzerEnabled));
  delay(1500);
}

void on_item12_selected(MenuItem * p_menu_item) {
  ClearEEPROM();
  SetDefaults();
}
