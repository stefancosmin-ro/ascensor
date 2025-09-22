#include <AccelStepper.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <EEPROM.h>

#define BUTTON_FLOOR0 2
#define BUTTON_FLOOR1 3
#define BUTTON_FLOOR2 4

#define HALL_FLOOR0 5
#define HALL_FLOOR1 6
#define HALL_FLOOR2 7

#define DOOR_HALL_FLOOR0 17
#define DOOR_HALL_FLOOR1 18
#define DOOR_HALL_FLOOR2 19

#define DOOR_OPTIC_FLOOR0 14
#define DOOR_OPTIC_FLOOR1 15
#define DOOR_OPTIC_FLOOR2 16

#define DOOR_MOTOR_PIN1 22
#define DOOR_MOTOR_PIN2 23
#define DOOR_MOTOR_PIN3 24
#define DOOR_MOTOR_PIN4 25

#define BUZZER_PIN 12

#define STEPS_PER_FLOOR 1000
#define MAX_FLOORS 3

#define DOOR_STEPS_SPEED 1
#define WAIT_TIME 5000
#define INACTIVITY_TIMEOUT 15000
#define STATS_DISPLAY_DURATION 5000
#define SAVE_INTERVAL 5000

#define EEPROM_SIGNATURE_VALUE 42
#define NUM_BANKS 10
#define BANK_SIZE 8
#define BANK_INDEX_ADDR 0
#define DOOR_CYCLE_COUNT_ADDR (1 + (NUM_BANKS * BANK_SIZE))
#define TRIP_COUNT_ADDR (DOOR_CYCLE_COUNT_ADDR + 2)

AccelStepper elevatorStepper(AccelStepper::FULL4WIRE, 8, 9, 10, 11);
LiquidCrystal_I2C lcd(0x27, 16, 2);

int doorSteps[8][4] = {
    {1, 0, 0, 0}, {1, 1, 0, 0}, {0, 1, 0, 0}, {0, 1, 1, 0},
    {0, 0, 1, 0}, {0, 0, 1, 1}, {0, 0, 0, 1}, {1, 0, 0, 1}
};

int currentFloor = 0, targetFloor = 0, doorStepCount = 0;

bool elevatorMoving = false, doorMoving = false, doorOpen = false;
bool lastButtonStates[3] = {false, false, false};
bool safetyError = false, safetyErrorDisplayed = false, arrivalDisplayed = false;
bool floorRequests[MAX_FLOORS] = {false, false, false};
bool waitingAfterDoorClose = false, statisticsDisplayed = false;

unsigned int doorCycleCount = 0, tripCount = 0;
unsigned long lastSaveTime = 0, doorOpenTime = 0, doorCloseTime = 0;
unsigned long lastActivityTime = 0, statsDisplayTime = 0;

void displayCenteredText(String text, int row) {
    int spaces = (16 - text.length()) / 2;
    lcd.setCursor(spaces, row);
    lcd.print(text);
}

void displayTwoLines(String line1, String line2) {
    lcd.clear();
    displayCenteredText(line1, 0);
    displayCenteredText(line2, 1);
}

String getFloorText(int floor) {
    return floor == 0 ? "Parter" : "Etaj " + String(floor);
}

void updateActivityTime() {
    lastActivityTime = millis();
    statisticsDisplayed = false;
}

void saveCountersToEEPROM() {
    EEPROM.update(DOOR_CYCLE_COUNT_ADDR, doorCycleCount & 0xFF);
    EEPROM.update(DOOR_CYCLE_COUNT_ADDR + 1, (doorCycleCount >> 8) & 0xFF);
    EEPROM.update(TRIP_COUNT_ADDR, tripCount & 0xFF);
    EEPROM.update(TRIP_COUNT_ADDR + 1, (tripCount >> 8) & 0xFF);
}

void loadCountersFromEEPROM() {
    doorCycleCount = EEPROM.read(DOOR_CYCLE_COUNT_ADDR) | (EEPROM.read(DOOR_CYCLE_COUNT_ADDR + 1) << 8);
    tripCount = EEPROM.read(TRIP_COUNT_ADDR) | (EEPROM.read(TRIP_COUNT_ADDR + 1) << 8);
}

void displayStatistics() {
    lcd.clear();
    displayCenteredText("Cicluri usa: " + String(doorCycleCount), 0);
    displayCenteredText("Calatorii: " + String(tripCount), 1);
}

void playStartupSound() {
    tone(BUZZER_PIN, 440, 100); delay(120);
    tone(BUZZER_PIN, 554, 100); delay(120);
    tone(BUZZER_PIN, 659, 100); delay(120);
    tone(BUZZER_PIN, 880, 200); delay(200);
    noTone(BUZZER_PIN);
}

void playButtonPressSound() {
    tone(BUZZER_PIN, 1000, 50); delay(50);
    noTone(BUZZER_PIN);
}

void playDoorOpenSound() {
    tone(BUZZER_PIN, 500, 200); delay(200);
    tone(BUZZER_PIN, 600, 200); delay(200);
    tone(BUZZER_PIN, 700, 200); delay(200);
    delay(100);
    tone(BUZZER_PIN, 800, 300); delay(300);
    noTone(BUZZER_PIN);
}

void playDoorCloseSound() {
    tone(BUZZER_PIN, 800, 300); delay(300);
    tone(BUZZER_PIN, 700, 200); delay(200);
    tone(BUZZER_PIN, 600, 200); delay(200);
    tone(BUZZER_PIN, 500, 200); delay(200);
    noTone(BUZZER_PIN);
}

void playFloorArrivalSound() {
    tone(BUZZER_PIN, 1000, 150); delay(150);
    delay(50);
    tone(BUZZER_PIN, 800, 200); delay(200);
    noTone(BUZZER_PIN);
}

void playWarningSound() {
    tone(BUZZER_PIN, 1200, 100); delay(150);
    tone(BUZZER_PIN, 1200, 100); delay(150);
    tone(BUZZER_PIN, 1200, 100); delay(100);
    noTone(BUZZER_PIN);
}

void playEmergencyStopSound() {
    tone(BUZZER_PIN, 2000, 200); delay(250);
    tone(BUZZER_PIN, 1500, 400); delay(450);
    tone(BUZZER_PIN, 1000, 600); delay(650);
    noTone(BUZZER_PIN);
}

void playRecalibrationSound() {
    for (int i = 500; i <= 1500; i += 250) {
        tone(BUZZER_PIN, i, 100); delay(120);
    }
    for (int i = 1500; i >= 500; i -= 250) {
        tone(BUZZER_PIN, i, 100); delay(120);
    }
    noTone(BUZZER_PIN);
}

void playRestoreStateSound() {
    tone(BUZZER_PIN, 262, 100); delay(120);
    tone(BUZZER_PIN, 330, 100); delay(120);
    tone(BUZZER_PIN, 392, 100); delay(120);
    tone(BUZZER_PIN, 523, 200); delay(200);
    noTone(BUZZER_PIN);
}

void playSafePositionSound() {
    tone(BUZZER_PIN, 392, 100); delay(120);
    tone(BUZZER_PIN, 494, 100); delay(120);
    tone(BUZZER_PIN, 587, 100); delay(120);
    tone(BUZZER_PIN, 784, 200); delay(200);
    noTone(BUZZER_PIN);
}

void playErrorResolvedSound() {
    tone(BUZZER_PIN, 1047, 100); delay(120);
    tone(BUZZER_PIN, 784, 100); delay(120);
    tone(BUZZER_PIN, 523, 100); delay(120);
    noTone(BUZZER_PIN);
}

void playElevatorStartSound() {
    for (int i = 300; i <= 600; i += 100) {
        tone(BUZZER_PIN, i, 80); delay(90);
    }
    noTone(BUZZER_PIN);
}

void playDoorDetectionSound() {
    tone(BUZZER_PIN, 800, 100); delay(120);
    tone(BUZZER_PIN, 600, 100); delay(120);
    tone(BUZZER_PIN, 800, 100); delay(120);
    noTone(BUZZER_PIN);
}

void recalibratePosition();
void findSafePosition();
void moveToSafePosition(int floor);
void saveState();

void saveState() {
    byte currentBank = EEPROM.read(BANK_INDEX_ADDR);
    int bankStartAddr = 1 + (currentBank * BANK_SIZE);
    EEPROM.update(bankStartAddr, EEPROM_SIGNATURE_VALUE);
    EEPROM.update(bankStartAddr + 1, currentFloor);
    EEPROM.update(bankStartAddr + 2, targetFloor);
    EEPROM.update(bankStartAddr + 3, doorOpen ? 1 : 0);
    EEPROM.update(bankStartAddr + 4, elevatorMoving ? 1 : 0);
    for (int i = 0; i < MAX_FLOORS; i++) {
        EEPROM.update(bankStartAddr + 5 + i, floorRequests[i] ? 1 : 0);
    }
    EEPROM.update(BANK_INDEX_ADDR, (currentBank + 1) % NUM_BANKS);
}

void restoreState() {
    bool anyDoorOpen = false;
    int openDoorFloor = -1;
    for (int i = 0; i < MAX_FLOORS; i++) {
        int opticPin = i == 0 ? DOOR_OPTIC_FLOOR0 : (i == 1 ? DOOR_OPTIC_FLOOR1 : DOOR_OPTIC_FLOOR2);
        if (digitalRead(opticPin) != HIGH) {
            anyDoorOpen = true;
            openDoorFloor = i;
            break;
        }
    }
    if (anyDoorOpen) {
        displayTwoLines("ALARMA PORNIRE!", "Usa " + getFloorText(openDoorFloor));
        playWarningSound();
        delay(2000);
        while (digitalRead(openDoorFloor == 0 ? DOOR_OPTIC_FLOOR0 : (openDoorFloor == 1 ? DOOR_OPTIC_FLOOR1 : DOOR_OPTIC_FLOOR2)) != HIGH) {
            displayTwoLines("PERICOL!", "Inchide usa " + getFloorText(openDoorFloor));
            playWarningSound();
            delay(2000);
        }
        displayTwoLines("Usa inchisa", "manual!");
        playErrorResolvedSound();
        delay(1500);
        recalibratePosition();
        return;
    }

    byte currentBank = EEPROM.read(BANK_INDEX_ADDR);
    currentBank = (currentBank == 0) ? NUM_BANKS - 1 : currentBank - 1;
    int bankStartAddr = 1 + (currentBank * BANK_SIZE);
    byte signature = EEPROM.read(bankStartAddr);
    if (signature == EEPROM_SIGNATURE_VALUE) {
        byte savedFloor = EEPROM.read(bankStartAddr + 1);
        byte savedTargetFloor = EEPROM.read(bankStartAddr + 2);
        if (savedFloor < MAX_FLOORS && savedTargetFloor < MAX_FLOORS) {
            currentFloor = savedFloor;
            targetFloor = savedTargetFloor;
            bool atCurrentFloor = (currentFloor == 0 && digitalRead(HALL_FLOOR0) == LOW) ||
                                  (currentFloor == 1 && digitalRead(HALL_FLOOR1) == LOW) ||
                                  (currentFloor == 2 && digitalRead(HALL_FLOOR2) == LOW);
            if (!atCurrentFloor) {
                displayTwoLines("Stare invalida", "Recalibrez...");
                delay(1500);
                recalibratePosition();
                return;
            }
            elevatorStepper.setCurrentPosition(currentFloor * STEPS_PER_FLOOR);
            doorOpen = EEPROM.read(bankStartAddr + 3) == 1;
            bool wasMoving = EEPROM.read(bankStartAddr + 4) == 1;
            elevatorMoving = false;
            for (int i = 0; i < MAX_FLOORS; i++) {
                floorRequests[i] = EEPROM.read(bankStartAddr + 5 + i) == 1;
            }
            String doorStatus = doorOpen ? "deschisa" : "inchisa";
            displayTwoLines("Stare restaurata", getFloorText(currentFloor) + " " + doorStatus);
            playRestoreStateSound();
            for (int i = 0; i < MAX_FLOORS; i++) {
                floorRequests[i] = false;
            }
            delay(1500);
        }
    }
}

void stopDoorMotor() {
    digitalWrite(DOOR_MOTOR_PIN1, LOW);
    digitalWrite(DOOR_MOTOR_PIN2, LOW);
    digitalWrite(DOOR_MOTOR_PIN3, LOW);
    digitalWrite(DOOR_MOTOR_PIN4, LOW);
}

void openDoor() {
    doorMoving = true;
    doorOpen = true;
    doorStepCount = 0;
    displayTwoLines("Deschid usa", getFloorText(currentFloor));
    updateActivityTime();
    saveState();
}

void closeDoor() {
    doorMoving = true;
    doorOpen = false;
    doorStepCount = 0;
    displayTwoLines("Inchid usa", getFloorText(currentFloor));
    updateActivityTime();
    saveState();
}

void moveDoor() {
    int hallPin = currentFloor == 0 ? DOOR_HALL_FLOOR0 : (currentFloor == 1 ? DOOR_HALL_FLOOR1 : DOOR_HALL_FLOOR2);
    int opticPin = currentFloor == 0 ? DOOR_OPTIC_FLOOR0 : (currentFloor == 1 ? DOOR_OPTIC_FLOOR1 : DOOR_OPTIC_FLOOR2);

    static int extraStepsAfterOptic = 0;
    static const int EXTRA_CLOSE_STEPS = 30;

    bool cabinAtFloor = (currentFloor == 0 && digitalRead(HALL_FLOOR0) == LOW) ||
                        (currentFloor == 1 && digitalRead(HALL_FLOOR1) == LOW) ||
                        (currentFloor == 2 && digitalRead(HALL_FLOOR2) == LOW);

    if (!cabinAtFloor) {
        doorMoving = false;
        stopDoorMotor();
        displayTwoLines("EROARE USI", "Cabina lipsa!");
        playWarningSound();
        safetyError = true;
        elevatorMoving = false;
        elevatorStepper.stop();
        delay(2000);
        recalibratePosition();
        return;
    }

    if (doorOpen && digitalRead(hallPin) == LOW) {
        doorMoving = false;
        stopDoorMotor();
        extraStepsAfterOptic = 0;
        displayCenteredText("Usa deschisa!", 0);
        doorOpenTime = millis();
        playDoorOpenSound();
        delay(1000);
        saveState();
        return;
    }

    if (!doorOpen && digitalRead(opticPin) == HIGH) {
        if (extraStepsAfterOptic < EXTRA_CLOSE_STEPS) {
            extraStepsAfterOptic++;
            int stepIndex = (doorStepCount % 8 + 8) % 8;
            for (int i = 0; i < 4; i++) {
                digitalWrite(DOOR_MOTOR_PIN1 + i, doorSteps[stepIndex][i]);
            }
            doorStepCount++;
            delay(DOOR_STEPS_SPEED);
            return;
        }
        doorMoving = false;
        stopDoorMotor();
        extraStepsAfterOptic = 0;
        displayCenteredText("Usa inchisa!", 0);
        doorCycleCount++;
        saveCountersToEEPROM();
        updateActivityTime();
        doorCloseTime = millis();
        waitingAfterDoorClose = true;
        playDoorCloseSound();
        delay(1000);
        saveState();
        return;
    }

    int direction = doorOpen ? -1 : 1;
    int stepIndex = (doorStepCount % 8 + 8) % 8;
    for (int i = 0; i < 4; i++) {
        digitalWrite(DOOR_MOTOR_PIN1 + i, doorSteps[stepIndex][i]);
    }
    doorStepCount += direction;
    delay(DOOR_STEPS_SPEED);
}

void startElevatorMovement() {
    int currentOpticPin = currentFloor == 0 ? DOOR_OPTIC_FLOOR0 : (currentFloor == 1 ? DOOR_OPTIC_FLOOR1 : DOOR_OPTIC_FLOOR2);
    if (digitalRead(currentOpticPin) != HIGH) {
        displayTwoLines("Eroare", "Usa deschisa!");
        playWarningSound();
        delay(2000);
        floorRequests[targetFloor] = true;
        return;
    }
    if (targetFloor != currentFloor && !doorOpen) {
        elevatorMoving = true;
        elevatorStepper.moveTo(targetFloor * STEPS_PER_FLOOR);
        displayTwoLines("De la " + getFloorText(currentFloor), "La " + getFloorText(targetFloor));
        playElevatorStartSound();
        saveState();
    }
}

void moveElevator() {
    bool atTargetFloor = (targetFloor == 0 && digitalRead(HALL_FLOOR0) == LOW) ||
                         (targetFloor == 1 && digitalRead(HALL_FLOOR1) == LOW) ||
                         (targetFloor == 2 && digitalRead(HALL_FLOOR2) == LOW);
    elevatorStepper.run();
    if (atTargetFloor) {
        if (elevatorMoving) {
            tripCount++;
            saveCountersToEEPROM();
            updateActivityTime();
        }
        elevatorMoving = false;
        currentFloor = targetFloor;
        elevatorStepper.setCurrentPosition(targetFloor * STEPS_PER_FLOOR);
        if (!arrivalDisplayed) {
            displayTwoLines("Ajuns la", getFloorText(currentFloor));
            playFloorArrivalSound();
            arrivalDisplayed = true;
            delay(1000);
            openDoor();
            saveState();
        }
    } else {
        arrivalDisplayed = false;
    }
}

void checkSafety() {
    bool incorrectDoorOpen = false;
    int errorFloor = -1;
    for (int i = 0; i < MAX_FLOORS; i++) {
        if (i != currentFloor) {
            int opticPin = i == 0 ? DOOR_OPTIC_FLOOR0 : (i == 1 ? DOOR_OPTIC_FLOOR1 : DOOR_OPTIC_FLOOR2);
            if (digitalRead(opticPin) != HIGH) {
                incorrectDoorOpen = true;
                errorFloor = i;
                break;
            }
        }
    }
    if (incorrectDoorOpen && doorOpen) {
        safetyError = true;
        elevatorMoving = doorMoving = false;
        elevatorStepper.stop();
        stopDoorMotor();
        if (!safetyErrorDisplayed) {
            displayTwoLines("ALARMA!", "Usa " + getFloorText(errorFloor));
            playWarningSound();
            safetyErrorDisplayed = true;
        }
    } else {
        safetyError = false;
        if (safetyErrorDisplayed) {
            lcd.clear();
            safetyErrorDisplayed = false;
        }
    }
}

void checkAllDoors() {
    for (int floor = 0; floor < MAX_FLOORS; floor++) {
        int opticPin = floor == 0 ? DOOR_OPTIC_FLOOR0 : (floor == 1 ? DOOR_OPTIC_FLOOR1 : DOOR_OPTIC_FLOOR2);
        int hallPin = floor == 0 ? HALL_FLOOR0 : (floor == 1 ? HALL_FLOOR1 : HALL_FLOOR2);
        int doorHallPin = floor == 0 ? DOOR_HALL_FLOOR0 : (floor == 1 ? DOOR_HALL_FLOOR1 : DOOR_HALL_FLOOR2);

        bool doorOpen = digitalRead(opticPin) != HIGH;
        bool cabinAtFloor = digitalRead(hallPin) == LOW;
        bool doorFullyOpen = digitalRead(doorHallPin) == LOW;

        if (doorOpen && !cabinAtFloor) {
            safetyError = true;
            elevatorMoving = doorMoving = false;
            elevatorStepper.stop();
            stopDoorMotor();
            if (elevatorStepper.isRunning()) {
                elevatorStepper.setSpeed(0);
                elevatorStepper.stop();
            }
            displayTwoLines("ALARMA!", "Usa " + getFloorText(floor) + " deschisa!");
            playWarningSound();
            delay(2000);
            while (digitalRead(opticPin) != HIGH) {
                displayTwoLines("PERICOL!", "Inchide usa " + getFloorText(floor));
                playWarningSound();
                delay(2000);
            }
            displayTwoLines("Usa " + getFloorText(floor), "inchisa manual!");
            playErrorResolvedSound();
            delay(1500);
            recalibratePosition();
            safetyError = false;
            for (int i = 0; i < MAX_FLOORS; i++) {
                floorRequests[i] = false;
            }
            return;
        }
    }
}

void findSafePosition() {
    displayTwoLines("Caut pozitie", "sigura...");
    bool doorOpen0 = digitalRead(DOOR_OPTIC_FLOOR0) != HIGH;
    bool doorOpen1 = digitalRead(DOOR_OPTIC_FLOOR1) != HIGH;
    bool doorOpen2 = digitalRead(DOOR_OPTIC_FLOOR2) != HIGH;

    if (doorOpen0 || doorOpen1 || doorOpen2) {
        int openDoorFloor = doorOpen0 ? 0 : (doorOpen1 ? 1 : 2);
        displayTwoLines("EROARE CRITICA!", "Usa " + getFloorText(openDoorFloor));
        playWarningSound();
        delay(2000);
        displayTwoLines("PERICOL!", "Cabina intre etaje");
        playWarningSound();
        delay(2000);
        while (digitalRead(openDoorFloor == 0 ? DOOR_OPTIC_FLOOR0 : (openDoorFloor == 1 ? DOOR_OPTIC_FLOOR1 : DOOR_OPTIC_FLOOR2)) != HIGH) {
            displayTwoLines("Inchide usa", getFloorText(openDoorFloor) + " manual.");
            playWarningSound();
            delay(2000);
        }
        displayTwoLines("Usa inchisa", "manual!");
        playErrorResolvedSound();
        delay(1500);

        int targetSafeFloor = -1;
        long currentPos = elevatorStepper.currentPosition();
        long distToFloor[3] = {abs(currentPos), abs(currentPos - STEPS_PER_FLOOR), abs(currentPos - 2 * STEPS_PER_FLOOR)};
        int minIndex = 0;
        for (int i = 1; i < 3; i++) {
            if (distToFloor[i] < distToFloor[minIndex]) minIndex = i;
        }
        targetSafeFloor = minIndex;

        displayTwoLines("Merg la pozitie", "sigura...");
        elevatorStepper.moveTo(targetSafeFloor * STEPS_PER_FLOOR);
        elevatorStepper.setMaxSpeed(100);

        int hallPin = targetSafeFloor == 0 ? HALL_FLOOR0 : (targetSafeFloor == 1 ? HALL_FLOOR1 : HALL_FLOOR2);

        while (elevatorStepper.distanceToGo() != 0) {
            bool anyDoorOpen = false;
            int openDoorFloor = -1;
            for (int i = 0; i < MAX_FLOORS; i++) {
                int opticPin = i == 0 ? DOOR_OPTIC_FLOOR0 : (i == 1 ? DOOR_OPTIC_FLOOR1 : DOOR_OPTIC_FLOOR2);
                if (digitalRead(opticPin) != HIGH) {
                    anyDoorOpen = true;
                    openDoorFloor = i;
                    break;
                }
            }
            if (anyDoorOpen) {
                elevatorStepper.stop();
                displayTwoLines("OPRIRE URGENTA!", "Usa " + getFloorText(openDoorFloor));
                playEmergencyStopSound();
                delay(2000);
                while (digitalRead(openDoorFloor == 0 ? DOOR_OPTIC_FLOOR0 : (openDoorFloor == 1 ? DOOR_OPTIC_FLOOR1 : DOOR_OPTIC_FLOOR2)) != HIGH) {
                    displayTwoLines("PERICOL!", "Inchide usa " + getFloorText(openDoorFloor));
                    playWarningSound();
                    delay(2000);
                }
                displayTwoLines("Usa inchisa", "manual!");
                playErrorResolvedSound();
                delay(1500);
                displayTwoLines("Reiau deplasarea", "la pozitie sigura!");
                delay(1000);
                elevatorStepper.run();
            }
            if (digitalRead(hallPin) == LOW) break;
        }
        elevatorStepper.setCurrentPosition(targetSafeFloor * STEPS_PER_FLOOR);
        currentFloor = targetFloor = targetSafeFloor;
        elevatorStepper.setMaxSpeed(250);

        int opticPin = targetSafeFloor == 0 ? DOOR_OPTIC_FLOOR0 : (targetSafeFloor == 1 ? DOOR_OPTIC_FLOOR1 : DOOR_OPTIC_FLOOR2);
        if (digitalRead(opticPin) != HIGH) {
            doorOpen = true;
            closeDoor();
            while (doorMoving) moveDoor();
        } else {
            doorOpen = false;
        }
        displayTwoLines("Pozitie sigura", "la " + getFloorText(targetSafeFloor));
        playSafePositionSound();
        delay(1500);
        saveState();
    }
}

void recalibratePosition() {
    lcd.clear();
    displayCenteredText("Recalibrez...", 0);
    playRecalibrationSound();
    elevatorMoving = false;
    doorMoving = false;
    stopDoorMotor();
    elevatorStepper.stop();

    bool atFloor0 = digitalRead(HALL_FLOOR0) == LOW;
    bool atFloor1 = digitalRead(HALL_FLOOR1) == LOW;
    bool atFloor2 = digitalRead(HALL_FLOOR2) == LOW;

    if (atFloor0) {
        elevatorStepper.setCurrentPosition(0);
        currentFloor = targetFloor = 0;
    } else if (atFloor1) {
        elevatorStepper.setCurrentPosition(STEPS_PER_FLOOR);
        currentFloor = targetFloor = 1;
    } else if (atFloor2) {
        elevatorStepper.setCurrentPosition(2 * STEPS_PER_FLOOR);
        currentFloor = targetFloor = 2;
    } else {
        displayTwoLines("Pozitie intre", "etaje!");
        delay(1000);
        findSafePosition();
        return;
    }

    int currentOpticPin = currentFloor == 0 ? DOOR_OPTIC_FLOOR0 : (currentFloor == 1 ? DOOR_OPTIC_FLOOR1 : DOOR_OPTIC_FLOOR2);
    int hallDoorPin = currentFloor == 0 ? DOOR_HALL_FLOOR0 : (currentFloor == 1 ? DOOR_HALL_FLOOR1 : DOOR_HALL_FLOOR2);

    if (digitalRead(hallDoorPin) == LOW) {
        doorOpen = true;
        displayTwoLines("Recalibrat", "Usa deschisa!");
    } else if (digitalRead(currentOpticPin) == HIGH) {
        doorOpen = false;
        displayTwoLines("Recalibrat", "Usa inchisa!");
    } else {
        doorOpen = true;
        displayTwoLines("Stare usa", "neclara.");
        delay(1000);
        closeDoor();
        while (doorMoving) moveDoor();
    }
    displayTwoLines("Recalibrat", getFloorText(currentFloor));
    delay(1500);
    saveState();
}

void findNearestFloor() {
    long currentPos = elevatorStepper.currentPosition();
    long distToFloor[3] = {abs(currentPos), abs(currentPos - STEPS_PER_FLOOR), abs(currentPos - 2 * STEPS_PER_FLOOR)};
    int minIndex = 0;
    for (int i = 1; i < 3; i++) {
        if (distToFloor[i] < distToFloor[minIndex]) minIndex = i;
    }
    moveToSafePosition(minIndex);
}

void moveToSafePosition(int floor) {
    lcd.clear();
    displayCenteredText("Merg la " + getFloorText(floor), 0);
    displayCenteredText("In siguranta...", 1);
    elevatorStepper.moveTo(floor * STEPS_PER_FLOOR);
    elevatorStepper.setMaxSpeed(100);
    int hallPin = floor == 0 ? HALL_FLOOR0 : (floor == 1 ? HALL_FLOOR1 : HALL_FLOOR2);
    while (elevatorStepper.distanceToGo() != 0) {
        elevatorStepper.run();
        if (digitalRead(hallPin) == LOW) break;
    }
    elevatorStepper.setCurrentPosition(floor * STEPS_PER_FLOOR);
    currentFloor = targetFloor = floor;
    elevatorStepper.setMaxSpeed(250);
    displayTwoLines("Pozitie sigura", "la " + getFloorText(floor));
    playSafePositionSound();
    delay(2000);
}

void findInitialPosition() {
    displayTwoLines("Cobor la", "parter!");
    while (digitalRead(HALL_FLOOR0) == HIGH) {
        bool anyDoorOpen = false;
        int openDoorFloor = -1;
        for (int i = 0; i < MAX_FLOORS; i++) {
            int opticPin = i == 0 ? DOOR_OPTIC_FLOOR0 : (i == 1 ? DOOR_OPTIC_FLOOR1 : DOOR_OPTIC_FLOOR2);
            if (digitalRead(opticPin) != HIGH) {
                anyDoorOpen = true;
                openDoorFloor = i;
                break;
            }
        }
        if (anyDoorOpen) {
            elevatorStepper.stop();
            displayTwoLines("OPRIRE URGENTA!", "Usa " + getFloorText(openDoorFloor));
            playEmergencyStopSound();
            delay(2000);
            while (digitalRead(openDoorFloor == 0 ? DOOR_OPTIC_FLOOR0 : (openDoorFloor == 1 ? DOOR_OPTIC_FLOOR1 : DOOR_OPTIC_FLOOR2)) != HIGH) {
                displayTwoLines("PERICOL!", "Inchide usa " + getFloorText(openDoorFloor));
                playWarningSound();
                delay(2000);
            }
            displayTwoLines("Usa inchisa", "manual!");
            playErrorResolvedSound();
            delay(1500);
            displayTwoLines("Reiau coborarea", "la parter!");
            delay(1000);
            elevatorStepper.moveTo(elevatorStepper.currentPosition() - 100);
            elevatorStepper.run();
        }
    }
    elevatorStepper.setCurrentPosition(0);
    currentFloor = targetFloor = 0;
    displayTwoLines("Ajuns la", "parter!");
    playFloorArrivalSound();
    delay(1000);
    if (digitalRead(DOOR_OPTIC_FLOOR0) != HIGH) {
        doorOpen = true;
        closeDoor();
        while (doorMoving) moveDoor();
    }
    saveState();
}

void checkInitialSafety() {
    for (int i = 0; i < MAX_FLOORS; i++) {
        int opticPin = i == 0 ? DOOR_OPTIC_FLOOR0 : (i == 1 ? DOOR_OPTIC_FLOOR1 : DOOR_OPTIC_FLOOR2);
        if (digitalRead(opticPin) != HIGH) {
            displayTwoLines("ALARMA!", "Usa " + getFloorText(i) + "!");
            playWarningSound();
            delay(2000);

            int hallPin = i == 0 ? HALL_FLOOR0 : (i == 1 ? HALL_FLOOR1 : HALL_FLOOR2);
            bool cabinAtFloor = digitalRead(hallPin) == LOW;

            if (cabinAtFloor) {
                displayTwoLines("Incerc inchidere", "automata...");
                currentFloor = i;
                elevatorStepper.setCurrentPosition(i * STEPS_PER_FLOOR);
                doorOpen = true;
                closeDoor();
                unsigned long startTime = millis();
                while (doorMoving && (millis() - startTime < 8000)) {
                    moveDoor();
                }
                if (digitalRead(opticPin) == HIGH) {
                    displayTwoLines("Usa inchisa", "automat!");
                    delay(1500);
                } else {
                    doorMoving = false;
                    stopDoorMotor();
                    displayTwoLines("Inchidere esuata", "Necesita manual!");
                    while (digitalRead(opticPin) != HIGH) {
                        playWarningSound();
                        delay(2000);
                    }
                    displayTwoLines("Usa inchisa", "manual!");
                    playErrorResolvedSound();
                    delay(1500);
                }
            } else {
                displayTwoLines("PERICOL GRAV!", "Cabina intre etaje");
                playEmergencyStopSound();
                delay(2000);
                while (digitalRead(opticPin) != HIGH) {
                    displayTwoLines("INCHIDE MANUAL", "Usa " + getFloorText(i) + "!");
                    playWarningSound();
                    delay(2000);
                }
                displayTwoLines("Usa inchisa", "manual!");
                playErrorResolvedSound();
                delay(1500);
            }
        }
    }

    int hallPin = currentFloor == 0 ? HALL_FLOOR0 : (currentFloor == 1 ? HALL_FLOOR1 : HALL_FLOOR2);
    if (digitalRead(hallPin) == LOW) {
        displayTwoLines(getFloorText(currentFloor), "confirmat!");
        playFloorArrivalSound();
        delay(1000);
    } else {
        displayTwoLines("Pozitie invalida", "Recalibrare...");
        delay(1500);
        findInitialPosition();
    }

    int opticPin = currentFloor == 0 ? DOOR_OPTIC_FLOOR0 : (currentFloor == 1 ? DOOR_OPTIC_FLOOR1 : DOOR_OPTIC_FLOOR2);
    int hallDoorPin = currentFloor == 0 ? DOOR_HALL_FLOOR0 : (currentFloor == 1 ? DOOR_HALL_FLOOR1 : DOOR_HALL_FLOOR2);
    bool doorHallState = digitalRead(hallDoorPin) == LOW;
    bool doorOpticState = digitalRead(opticPin) == HIGH;

    if (doorOpen && !doorHallState) {
        displayCenteredText("Calibrare usa...", 0);
        doorOpen = false;
        openDoor();
        doorStepCount = 0;
        int doorTries = 0;
        while (doorMoving && doorTries < 100) {
            moveDoor();
            doorTries++;
        }
        stopDoorMotor();
        doorMoving = false;
        delay(500);
    } else if (!doorOpen && !doorOpticState) {
        displayCenteredText("Calibrare usa...", 0);
        doorOpen = true;
        closeDoor();
        doorStepCount = 0;
        int doorTries = 0;
        while (doorMoving && doorTries < 100) {
            moveDoor();
            doorTries++;
        }
        stopDoorMotor();
        doorMoving = false;
        delay(500);
    }

    for (int i = 0; i < MAX_FLOORS; i++) {
        floorRequests[i] = false;
    }
}

void processButtonPress(int floor, bool currentState, bool lastState) {
    if (currentState && !lastState) {
        updateActivityTime();
        playButtonPressSound();
        if (safetyError) return;
        if (currentFloor == floor && !elevatorMoving) {
            if (doorOpen) {
                closeDoor();
            } else if (!doorMoving) {
                openDoor();
            }
        } else if (!doorOpen && !doorMoving) {
            floorRequests[floor] = true;
            if (!elevatorMoving && !waitingAfterDoorClose) {
                processNextRequest();
            }
            if (!elevatorMoving) {
                displayTwoLines("Cerere adaugata", getFloorText(floor));
                delay(1000);
            }
        }
        saveState();
    }
}

void processNextRequest() {
    if (doorOpen || doorMoving || elevatorMoving) return;

    int currentOpticPin = currentFloor == 0 ? DOOR_OPTIC_FLOOR0 : (currentFloor == 1 ? DOOR_OPTIC_FLOOR1 : DOOR_OPTIC_FLOOR2);
    if (digitalRead(currentOpticPin) != HIGH) {
        displayTwoLines("Usa detectata", "deschisa!");
        playDoorDetectionSound();
        delay(1000);
        doorOpen = true;
        closeDoor();
        return;
    }

    int nextFloor = findOptimalNextFloor();
    if (nextFloor != -1) {
        floorRequests[nextFloor] = false;
        targetFloor = nextFloor;
        startElevatorMovement();
    }
}

int findOptimalNextFloor() {
    bool hasRequest = false;
    for (int i = 0; i < MAX_FLOORS; i++) {
        if (floorRequests[i]) {
            hasRequest = true;
            break;
        }
    }
    if (!hasRequest) return -1;

    int direction = 0;
    if (elevatorStepper.targetPosition() > elevatorStepper.currentPosition()) direction = 1;
    else if (elevatorStepper.targetPosition() < elevatorStepper.currentPosition()) direction = -1;

    if (direction >= 0) {
        for (int i = currentFloor + 1; i < MAX_FLOORS; i++) {
            if (floorRequests[i]) return i;
        }
        for (int i = currentFloor - 1; i >= 0; i--) {
            if (floorRequests[i]) return i;
        }
    } else {
        for (int i = currentFloor - 1; i >= 0; i--) {
            if (floorRequests[i]) return i;
        }
        for (int i = currentFloor + 1; i < MAX_FLOORS; i++) {
            if (floorRequests[i]) return i;
        }
    }
    return floorRequests[currentFloor] ? currentFloor : -1;
}

void handleDoorTimers() {
    if (waitingAfterDoorClose && !doorOpen && !doorMoving && !elevatorMoving) {
        if (millis() - doorCloseTime >= WAIT_TIME) {
            waitingAfterDoorClose = false;
            processNextRequest();
        }
    }
}

void setup() {
    for (int i = BUTTON_FLOOR0; i <= HALL_FLOOR2; i++) {
        pinMode(i, INPUT_PULLUP);
    }
    for (int i = DOOR_HALL_FLOOR0; i <= DOOR_OPTIC_FLOOR2; i++) {
        pinMode(i, INPUT_PULLUP);
    }
    for (int i = DOOR_MOTOR_PIN1; i <= DOOR_MOTOR_PIN4; i++) {
        pinMode(i, OUTPUT);
    }
    pinMode(BUZZER_PIN, OUTPUT);

    elevatorStepper.setMaxSpeed(250);
    elevatorStepper.setAcceleration(150);

    lcd.init();
    lcd.backlight();
    lcd.clear();
    displayCenteredText("Initializare", 0);
    displayCenteredText("Lift...", 1);
    playStartupSound();
    delay(1000);

    elevatorMoving = false;
    doorMoving = false;
    waitingAfterDoorClose = false;
    for (int i = 0; i < MAX_FLOORS; i++) {
        floorRequests[i] = false;
    }

    loadCountersFromEEPROM();
    updateActivityTime();

    bool anyDoorOpen = false;
    for (int i = 0; i < MAX_FLOORS; i++) {
        int opticPin = i == 0 ? DOOR_OPTIC_FLOOR0 : (i == 1 ? DOOR_OPTIC_FLOOR1 : DOOR_OPTIC_FLOOR2);
        if (digitalRead(opticPin) != HIGH) {
            anyDoorOpen = true;
            break;
        }
    }
    if (anyDoorOpen) {
        displayTwoLines("ATENTIE!", "Usa deschisa!");
        playWarningSound();
        delay(2000);
    } else {
        restoreState();
    }

    checkInitialSafety();
    saveState();
}

void loop() {
    unsigned long currentTime = millis();

    checkAllDoors();

    if (safetyError) return;

    if ((!statisticsDisplayed && !elevatorMoving && !doorMoving && (currentTime - lastActivityTime >= INACTIVITY_TIMEOUT))) {
        displayStatistics();
        statisticsDisplayed = true;
        statsDisplayTime = currentTime;
    }

    if (statisticsDisplayed && (currentTime - statsDisplayTime < STATS_DISPLAY_DURATION)) {
        bool anyButtonPressed = !digitalRead(BUTTON_FLOOR0) || !digitalRead(BUTTON_FLOOR1) || !digitalRead(BUTTON_FLOOR2);
        if (anyButtonPressed) {
            updateActivityTime();
            lcd.clear();
        }
        delay(50);
        return;
    }

    bool buttonStates[3] = {!digitalRead(BUTTON_FLOOR0), !digitalRead(BUTTON_FLOOR1), !digitalRead(BUTTON_FLOOR2)};

    if (elevatorMoving) {
        bool anyDoorOpen = digitalRead(DOOR_OPTIC_FLOOR0) != HIGH ||
                           digitalRead(DOOR_OPTIC_FLOOR1) != HIGH ||
                           digitalRead(DOOR_OPTIC_FLOOR2) != HIGH;
        if (anyDoorOpen) {
            elevatorMoving = false;
            elevatorStepper.stop();
            int openDoorFloor = -1;
            for (int i = 0; i < MAX_FLOORS; i++) {
                int opticPin = i == 0 ? DOOR_OPTIC_FLOOR0 : (i == 1 ? DOOR_OPTIC_FLOOR1 : DOOR_OPTIC_FLOOR2);
                if (digitalRead(opticPin) != HIGH) {
                    openDoorFloor = i;
                    break;
                }
            }
            displayTwoLines("OPRIRE URGENTA", "Usa " + getFloorText(openDoorFloor) + " deschisa!");
            playEmergencyStopSound();
            delay(2000);
            while (digitalRead(openDoorFloor == 0 ? DOOR_OPTIC_FLOOR0 : (openDoorFloor == 1 ? DOOR_OPTIC_FLOOR1 : DOOR_OPTIC_FLOOR2)) != HIGH) {
                displayTwoLines("PERICOL!", "Inchide usa " + getFloorText(openDoorFloor));
                playWarningSound();
                delay(2000);
            }
            displayTwoLines("Usa inchisa", "manual!");
            playErrorResolvedSound();
            delay(1500);
            recalibratePosition();
            for (int i = 0; i < MAX_FLOORS; i++) {
                floorRequests[i] = false;
            }
        }
    }

    checkSafety();
    if (safetyError) return;

    for (int i = 0; i < 3; i++) {
        processButtonPress(i, buttonStates[i], lastButtonStates[i]);
        lastButtonStates[i] = buttonStates[i];
    }

    handleDoorTimers();

    if (elevatorMoving) moveElevator();
    if (doorMoving) moveDoor();

    if (!elevatorMoving && !doorMoving && !doorOpen && !waitingAfterDoorClose) {
        processNextRequest();
    }

    if (currentTime - lastSaveTime >= SAVE_INTERVAL && (elevatorMoving || doorMoving)) {
        saveState();
        lastSaveTime = currentTime;
    }
}