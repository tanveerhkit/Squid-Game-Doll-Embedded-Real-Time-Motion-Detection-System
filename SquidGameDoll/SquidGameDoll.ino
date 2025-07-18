#include <Servo.h>
#include <DFRobotDFPlayerMini.h>
#include <SoftwareSerial.h>

// ----- Pin Definitions -----
#define SERVO_ROTATE_PIN 3
#define SERVO_TILT_PIN   5
#define PIR_PIN          6
#define ULTRASONIC_TRIG  7
#define ULTRASONIC_ECHO  8
#define DFPLAYER_RX      10
#define DFPLAYER_TX      11
#define LED_L_R          9   // Left Eye Red
#define LED_L_G          12  // Left Eye Green
#define LED_L_B          13  // Left Eye Blue
#define LED_R_R          A0  // Right Eye Red
#define LED_R_G          A1  // Right Eye Green
#define LED_R_B          A2  // Right Eye Blue
#define BUTTON_PIN       A3

// ----- Game Timing -----
#define GREEN_LIGHT_DURATION  5000  // ms
#define RED_LIGHT_DURATION    4000  // ms
#define ELIMINATION_DURATION  2000  // ms
#define BUTTON_DEBOUNCE_MS    50

// ----- Servo Angles -----
#define HEAD_AWAY_ANGLE   0    // Facing away (Green Light)
#define HEAD_TOWARD_ANGLE 90   // Facing player (Red Light)
#define HEAD_TILT_ANGLE   20   // Optional tilt

// ----- State Machine -----
enum GameState {
  STATE_GREEN_LIGHT,
  STATE_TRANSITION_TO_RED,
  STATE_RED_LIGHT,
  STATE_ELIMINATED,
  STATE_RESET
};

GameState currentState = STATE_GREEN_LIGHT;

// ----- Hardware Objects -----
Servo servoRotate, servoTilt;
SoftwareSerial dfSerial(DFPLAYER_RX, DFPLAYER_TX);
DFRobotDFPlayerMini dfPlayer;

// ----- Timing -----
unsigned long stateStartTime = 0;
unsigned long lastButtonChange = 0;
bool lastButtonState = HIGH;
bool buttonPressed = false;

// ----- Function Prototypes -----
void setEyesColor(uint8_t r, uint8_t g, uint8_t b);
void playAudio(uint8_t track);
bool detectMotion();
void transitionTo(GameState newState);
void handleButton();
void resetGame();
void eliminationEffect();
void setupServos();
void setupDFPlayer();
void setupEyes();
void setupButton();
void setupSensors();

void setup() {
  Serial.begin(9600);
  setupServos();
  setupDFPlayer();
  setupEyes();
  setupButton();
  setupSensors();
  transitionTo(STATE_GREEN_LIGHT);
}

void loop() {
  handleButton();

  switch (currentState) {
    case STATE_GREEN_LIGHT:
      if (millis() - stateStartTime > GREEN_LIGHT_DURATION) {
        transitionTo(STATE_TRANSITION_TO_RED);
      }
      break;

    case STATE_TRANSITION_TO_RED:
      // Wait for head to finish turning (non-blocking)
      if (millis() - stateStartTime > 800) { // Allow servo to move
        transitionTo(STATE_RED_LIGHT);
      }
      break;

    case STATE_RED_LIGHT:
      if (detectMotion()) {
        transitionTo(STATE_ELIMINATED);
      } else if (millis() - stateStartTime > RED_LIGHT_DURATION) {
        transitionTo(STATE_GREEN_LIGHT);
      }
      break;

    case STATE_ELIMINATED:
      if (millis() - stateStartTime > ELIMINATION_DURATION) {
        transitionTo(STATE_RESET);
      }
      break;

    case STATE_RESET:
      // Wait for button press to restart
      break;
  }
}

// ----- State Transitions -----
void transitionTo(GameState newState) {
  stateStartTime = millis();
  currentState = newState;

  switch (newState) {
    case STATE_GREEN_LIGHT:
      // Head faces away, eyes green, play "Green Light" audio
      servoRotate.write(HEAD_AWAY_ANGLE);
      servoTilt.write(HEAD_TILT_ANGLE);
      setEyesColor(0, 255, 0); // Green
      playAudio(1); // Track 1: "Green Light"
      break;

    case STATE_TRANSITION_TO_RED:
      // Head turns toward player, eyes off
      servoRotate.write(HEAD_TOWARD_ANGLE);
      servoTilt.write(0);
      setEyesColor(0, 0, 0); // Eyes off
      break;

    case STATE_RED_LIGHT:
      // Eyes red, play "Red Light" audio
      setEyesColor(255, 0, 0); // Red
      playAudio(2); // Track 2: "Red Light"
      break;

    case STATE_ELIMINATED:
      // Eyes flash, play elimination sound
      eliminationEffect();
      playAudio(3); // Track 3: "Elimination"
      break;

    case STATE_RESET:
      // Eyes blue, wait for button
      setEyesColor(0, 0, 255); // Blue
      break;
  }
}

// ----- Hardware Setup -----
void setupServos() {
  servoRotate.attach(SERVO_ROTATE_PIN);
  servoTilt.attach(SERVO_TILT_PIN);
}

void setupDFPlayer() {
  dfSerial.begin(9600);
  if (!dfPlayer.begin(dfSerial)) {
    Serial.println("DFPlayer Mini not found!");
    while (true);
  }
  dfPlayer.volume(25); // Set volume (0-30)
}

void setupEyes() {
  pinMode(LED_L_R, OUTPUT); pinMode(LED_L_G, OUTPUT); pinMode(LED_L_B, OUTPUT);
  pinMode(LED_R_R, OUTPUT); pinMode(LED_R_G, OUTPUT); pinMode(LED_R_B, OUTPUT);
  setEyesColor(0, 0, 0);
}

void setupButton() {
  pinMode(BUTTON_PIN, INPUT_PULLUP);
}

void setupSensors() {
  pinMode(PIR_PIN, INPUT);
  pinMode(ULTRASONIC_TRIG, OUTPUT);
  pinMode(ULTRASONIC_ECHO, INPUT);
}

// ----- Eyes Color Control -----
void setEyesColor(uint8_t r, uint8_t g, uint8_t b) {
  analogWrite(LED_L_R, r); analogWrite(LED_L_G, g); analogWrite(LED_L_B, b);
  analogWrite(LED_R_R, r); analogWrite(LED_R_G, g); analogWrite(LED_R_B, b);
}

// ----- Audio Playback -----
void playAudio(uint8_t track) {
  dfPlayer.play(track); // Play track number (1-based)
}

// ----- Motion Detection -----
bool detectMotion() {
  // PIR sensor
  if (digitalRead(PIR_PIN) == HIGH) return true;

  // Ultrasonic sensor (detects if player moves closer)
  static long lastDistance = 0;
  long duration, distance;
  digitalWrite(ULTRASONIC_TRIG, LOW); delayMicroseconds(2);
  digitalWrite(ULTRASONIC_TRIG, HIGH); delayMicroseconds(10);
  digitalWrite(ULTRASONIC_TRIG, LOW);
  duration = pulseIn(ULTRASONIC_ECHO, HIGH, 20000); // 20ms timeout
  distance = duration * 0.034 / 2; // cm

  if (lastDistance == 0) lastDistance = distance;
  bool moved = abs(distance - lastDistance) > 5; // 5cm threshold
  lastDistance = distance;
  return moved;
}

// ----- Elimination Effect -----
void eliminationEffect() {
  // Flash eyes red rapidly
  for (int i = 0; i < 6; i++) {
    setEyesColor(255, 0, 0);
    delay(100);
    setEyesColor(0, 0, 0);
    delay(100);
  }
}

// ----- Button Handling (Debounced) -----
void handleButton() {
  bool reading = digitalRead(BUTTON_PIN);
  if (reading != lastButtonState) {
    lastButtonChange = millis();
  }
  if ((millis() - lastButtonChange) > BUTTON_DEBOUNCE_MS) {
    if (lastButtonState == HIGH && reading == LOW) {
      buttonPressed = true;
    }
  }
  lastButtonState = reading;

  if (buttonPressed) {
    buttonPressed = false;
    if (currentState == STATE_RESET || currentState == STATE_ELIMINATED) {
      resetGame();
    }
  }
}

void resetGame() {
  transitionTo(STATE_GREEN_LIGHT);
} 