#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <Wire.h>
#include <EEPROM.h>

// Définition des broches
const int ledPin[] = {13, 14, 27, 26};
const int button[] = {15, 4, 16, 18};
const int buzzerPin = 5;
const int powerSwitchPin = 19; // Broche de l'interrupteur d'alimentation
const int N_LEDS = 4;

// Paramètres OLED
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET -1
#define SCREEN_ADDRESS 0x3C

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

// Variables pour le jeu Simon
int sequence[100];
int level = 1;
int highScore[3] = {0, 0, 0};
int buttonPressed = -1;
bool gameStarted = false;
int gameMode = 0;
unsigned long lastInputTime = 0;
int menuSelection = 1;
bool powerOn = false; // État de l'alimentation
bool displayOn = false; // État de l'affichage

// Fréquences pour le buzzer
const int tones[] = {262, 330, 392, 494};
const int menuTone = 523;

// Durées selon le mode de jeu
int sequenceSpeed = 500;
int pauseSpeed = 300;

// Adresses EEPROM
#define EEPROM_SIZE 12
#define SIMPLE_HS_ADDR 0
#define NORMAL_HS_ADDR 4
#define HARD_HS_ADDR 8

void setup() {
  Serial.begin(9600);
  
  // Initialisation des broches
    for(int i = 0; i < N_LEDS; i++) {
    pinMode(ledPin[i], OUTPUT);
    pinMode(button[i], INPUT_PULLUP);
  }
  pinMode(buzzerPin, OUTPUT);
  pinMode(powerSwitchPin, INPUT_PULLUP); // Interrupteur avec résistance de pull-up
  
  // Initialisation OLED (mais pas encore affichage)
  if(!display.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS)) {
    Serial.println(F("SSD1306 allocation failed"));
    for(;;);
  }
  
  // Initialisation EEPROM
  EEPROM.begin(EEPROM_SIZE);
  loadHighScores();
  
  // Éteindre tout au démarrage
  turnOffGame();
}

void turnOnGame() {
  powerOn = true;
  displayOn = true;
  display.ssd1306_command(SSD1306_DISPLAYON); // Allumer l'écran
  display.clearDisplay();
  
  // Son de démarrage
  for(int i = 0; i < 4; i++) {
    tone(buzzerPin, tones[i], 200);
    delay(250);
  }
  noTone(buzzerPin);
  
  showMainMenu();
}

void turnOffGame() {
  // Éteindre toutes les LEDs
  for(int i = 0; i < N_LEDS; i++) {
    digitalWrite(ledPin[i], LOW);
  }
  
  // Éteindre le buzzer
  noTone(buzzerPin);
  
  // Éteindre l'affichage
  display.ssd1306_command(SSD1306_DISPLAYOFF);
  displayOn = false;
  
  powerOn = false;
  gameStarted = false;
  gameMode = 0;
}

void loadHighScores() {
  EEPROM.get(SIMPLE_HS_ADDR, highScore[0]);
  EEPROM.get(NORMAL_HS_ADDR, highScore[1]);
  EEPROM.get(HARD_HS_ADDR, highScore[2]);
  
  if(highScore[0] < 0 || highScore[0] > 100) highScore[0] = 0;
  if(highScore[1] < 0 || highScore[1] > 100) highScore[1] = 0;
  if(highScore[2] < 0 || highScore[2] > 100) highScore[2] = 0;
}

void saveHighScore() {
  if(level-1 > highScore[gameMode-1]) {
    highScore[gameMode-1] = level-1;
    
    switch(gameMode) {
      case 1: EEPROM.put(SIMPLE_HS_ADDR, highScore[0]); break;
      case 2: EEPROM.put(NORMAL_HS_ADDR, highScore[1]); break;
      case 3: EEPROM.put(HARD_HS_ADDR, highScore[2]); break;
    }
    
    EEPROM.commit();
    
    for(int i = 0; i < 3; i++) {
      tone(buzzerPin, 880, 100);
      delay(150);
      noTone(buzzerPin);
      delay(50);
    }
  }
}

void loop() {
  // Vérifier l'état de l'interrupteur d'alimentation
  bool currentPowerState = !digitalRead(powerSwitchPin); // Inversé car pull-up
  
  if(currentPowerState != powerOn) {
    if(currentPowerState) {
      turnOnGame();
    } else {
      turnOffGame();
    }
  }
  
  if (!powerOn) {
    delay(100); // Réduire la consommation CPU quand éteint
    return;
  }
  
  if (gameMode == 0) {
    handleMenuInput();
  } 
  else if (gameStarted) {
    playSequence();
    if (readPlayerInput()) {
      delay(500);
      level++;
      sequence[level-1] = random(0, N_LEDS);
      updateDisplay();
    }
  }
}

void showMainMenu() {
  if (!displayOn) return;
  
  display.clearDisplay();
  
  // Titre
  display.setTextSize(1,2);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(30, 0);
  display.println("MENU SIMON");
  
  // Options avec high scores
  display.setTextSize(1);
  
  // Simple
  display.setCursor(10, 20);
  display.print(menuSelection == 1 ? "> " : "  ");
  display.print("Simple");
  display.setCursor(80, 20);
  display.print("HS:");
  display.print(highScore[0]);
  
  // Normal
  display.setCursor(10, 30);
  display.print(menuSelection == 2 ? "> " : "  ");
  display.print("Normal");
  display.setCursor(80, 30);
  display.print("HS:");
  display.print(highScore[1]);
  
  // Difficile
  display.setCursor(10, 40);
  display.print(menuSelection == 3 ? "> " : "  ");
  display.print("Difficile");
  display.setCursor(80, 40);
  display.print("HS:");
  display.print(highScore[2]);
  
  // Instructions
  display.setCursor(0, 55);
  display.println("Appuyez pour Choisir");
  
  display.display();
}

void handleMenuInput() {
  for(int j = 0; j < N_LEDS; j++) {
    if(digitalRead(button[j]) == LOW && millis() - lastInputTime > 200) {
      lastInputTime = millis();
      tone(buzzerPin, menuTone, 100);
      
      if (j == 0 || j == 1) {
        menuSelection += (j == 0) ? -1 : 1;
        if (menuSelection < 1) menuSelection = 3;
        if (menuSelection > 3) menuSelection = 1;
        showMainMenu();
      } 
      else if (j == 2 || j == 3) {
        startGame(menuSelection);
      }
      
      while(digitalRead(button[j]) == LOW);
    }
  }
}

void startGame(int mode) {
  gameMode = mode;
  gameStarted = true;
  level = 1;
  
  switch(mode) {
    case 1: // Simple
      sequenceSpeed = 800;
      pauseSpeed = 500;
      break;
    case 2: // Normal
      sequenceSpeed = 500;
      pauseSpeed = 300;
      break;
    case 3: // Difficile
      sequenceSpeed = 300;
      pauseSpeed = 150;
      break;
  }
  
  sequence[0] = random(0, N_LEDS);
  updateDisplay();
  
  for(int i = 0; i < 3; i++) {
    tone(buzzerPin, tones[i], 100);
    delay(150);
  }
  noTone(buzzerPin);
}

void playSequence() {
  for(int i = 0; i < level; i++) {
    digitalWrite(ledPin[sequence[i]], HIGH);
    tone(buzzerPin, tones[sequence[i]], sequenceSpeed/2);
    updateDisplay();
    delay(sequenceSpeed);
    digitalWrite(ledPin[sequence[i]], LOW);
    noTone(buzzerPin);
    updateDisplay();
    delay(pauseSpeed);
  }
}

bool readPlayerInput() {
  for(int i = 0; i < level; i++) {
    bool correctButton = false;
    unsigned long startTime = millis();
    
    while(!correctButton && millis() - startTime < 5000) {
      for(int j = 0; j < N_LEDS; j++) {
        if(digitalRead(button[j]) == LOW) {
          buttonPressed = j;
          digitalWrite(ledPin[j], HIGH);
          tone(buzzerPin, tones[j], 200);
          updateDisplay();
          delay(300);
          digitalWrite(ledPin[j], LOW);
          noTone(buzzerPin);
          updateDisplay();
          
          if(buttonPressed != sequence[i]) {
            gameOver();
            return false;
          }
          
          correctButton = true;
          delay(200);
          break;
        }
      }
    }
    
    if(!correctButton) {
      gameOver();
      return false;
    }
  }
  return true;
}

void gameOver() {
  saveHighScore();
  
  display.clearDisplay();
  display.setTextSize(2);
  display.setCursor(10, 10);
  display.println("GAME");
  display.setCursor(20, 35);
  display.println("OVER");
  
  display.setTextSize(1);
  display.setCursor(0, 55);
  display.print("Score:");
  display.print(level-1);
  display.print(" HS:");
  display.print(highScore[gameMode-1]);
  
  display.display();
  
  for(int i = 0; i < 2; i++) {
    tone(buzzerPin, 200, 300);
    delay(400);
  }
  
  for(int i = 0; i < 3; i++) {
    for(int j = 0; j < N_LEDS; j++) {
      digitalWrite(ledPin[j], HIGH);
    }
    tone(buzzerPin, 150, 300);
    delay(300);
    for(int j = 0; j < N_LEDS; j++) {
      digitalWrite(ledPin[j], LOW);
    }
    noTone(buzzerPin);
    delay(300);
  }
  
  gameStarted = false;
  gameMode = 0;
  showMainMenu();
}

void updateDisplay() {
  if (!displayOn) return;
  
  display.clearDisplay();
  
  // Titre et mode
  display.setTextSize(1);
  display.setCursor(0, 0);
  display.print("Niveau: ");
  display.print(level);
  
  display.setCursor(70, 0);
  switch(gameMode) {
    case 1: display.print("Simple"); break;
    case 2: display.print("Normal"); break;
    case 3: display.print("Difficile"); break;
  }
  
  // High score
  display.setCursor(0, 10);
  display.print("HS: ");
  display.print(highScore[gameMode-1]);
  
  // Instructions
  display.setCursor(0, 20);
  display.println("Repetez la sequence");
  
  // Boutons
  display.setTextSize(1);
  display.setCursor(0, 40);
  display.print("1:Rouge 2:Jaune");
  display.setCursor(0, 50);
  display.print("3:Vert 4:Bleu");
  
  display.display();
}