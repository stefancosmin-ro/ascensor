# [RO] Sistem automat neenergofag pentru ascensoare

> **Notă**: Proiectarea și implementarea unui prototip de ascensor cu consum energetic redus, cu mecanisme robuste de siguranță și persistență a stării în EEPROM, destinat demonstrării conceptelor de automatizare și securitate operațională în context de cercetare.

## Cuprins
- [Prezentare generală](#prezentare-generală)
- [Arhitectură & componente](#arhitectură--componente)
- [Funcționalități cheie](#funcționalități-cheie)
- [Schema de pini & conexiuni](#schema-de-pini--conexiuni)
- [Software & dependințe](#software--dependințe)
- [Configurare](#configurare)
- [Utilizare](#utilizare)
- [Securitate operațională](#securitate-operațională)
- [Persistență & telemetrie](#persistență--telemetrie)
- [Alimentare & eficiență](#alimentare--eficiență)
- [Limitări & direcții viitoare](#limitări--direcții-viitoare)

## Prezentare generală
Prototipul este un **sistem automat neenergofag pentru ascensoare** pe 3 niveluri (parter, etaj 1, etaj 2), axat pe **reducerea consumului**, **siguranță** și **continuitate în funcționare** (backup la cădere de tensiune). Controlul este realizat cu **Arduino Mega 2560**, motoare pas cu pas pentru cabină și uși, senzori Hall și optici, un afișaj LCD I2C și buzzer pentru feedback auditiv.

<p align="center">
  <img src="https://i.imgur.com/bl9QmEL.png" alt="Schema arhitectură" width="350">
</p>

## Arhitectură & componente
**Blocuri funcționale**:
- **Unitate de control:** Arduino Mega 2560
- **Acționare cabină:** motor pas cu pas **NEMA17 17HS4401** + driver **L298N**
- **Acționare uși:** motor pas cu pas **28BYJ-48 5V** + driver **ULN2003**
- **Senzori de poziție cabină:** 3 × senzori **Hall** (parter, etaje 1–2)
- **Senzori uși:** 3 × senzori **optici** (stare „închis”), 3 × senzori **Hall** (stare „deschis”)
- **Interfață:** LCD I2C 16×2, butoane etaje, buzzer
- **Alimentare:** sursă comutație 12V/5A, **modul comutare automată** (YX850) + **acumulator 6V/4Ah**, convertor coborâtor reglabil

## Funcționalități cheie
- **3 niveluri** (parter/etaj 1/etaj 2) cu **butoane de apel** dedicate.
- **Control mișcare cabină** cu profil liniar și **aliniere pe etaj** pe baza senzorilor Hall.
- **Control uși**: deschidere/închidere secvențială.
- **Interblocări de siguranță**: interzicere mișcare cu ușa deschisă; detecție „ușă deschisă”; oprire de urgență și **recalibrare**.
- **Persistență stări** în **EEPROM** (jurnale ciclice/bank-uri) — restaurare la repornire.
- **Telemetrie**: contorizare **cicluri ușă** și **călătorii**; **statistici pe LCD** după inactivitate.
- **Feedback sonor**: coduri audio pentru evenimente (pornire, sosire, erori, recalibrare etc.).

## Schema de pini & conexiuni
> **Notă**: alimentați **motoarele** din surse dedicate; **masa comună** între logică și putere; protecții pentru zgomot; respectați curenții driverelor.

**Butoane etaje**  
- FLOOR0: D2  
- FLOOR1: D3  
- FLOOR2: D4  

**Senzori Hall (poziție cabină)**  
- Etaj 0: D5  
- Etaj 1: D6  
- Etaj 2: D7  

**Stepper cabină (AccelStepper FULL4WIRE)**  
- Bobine: D8, D9, D10, D11

**Buzzer**  
- D12

**Senzori uși**  
- Optici (ușă închisă): A0(D14), A1(D15), A2(D16)  
- Hall (ușă deschisă complet): D17, D18, D19

**Motor ușă (28BYJ-48 + ULN2003)**  
- D22, D23, D24, D25

**LCD I2C 16×2**  
- I2C @ 0x27 (SDA/SCL — pini hardware Mega)

## Software & dependințe
- **Arduino IDE**
- Biblioteci:
  - `AccelStepper`
  - `LiquidCrystal_I2C`
  - `Wire` (I2C)
  - `EEPROM`

## Configurare
Parametri configurabili în cod:
- `STEPS_PER_FLOOR` — **1000** (pași între etaje)
- `MAX_FLOORS` — **3**
- `DOOR_STEPS_SPEED` — viteză pași ușă
- `WAIT_TIME` — timp așteptare cu ușa deschisă
- `INACTIVITY_TIMEOUT` — după cât timp se afișează statistici
- Adrese **EEPROM**: semnătură și **bănci ciclice** pentru backup-uri
- Timpi salvare periodică (`SAVE_INTERVAL`)

## Utilizare
- La **pornire**, sistemul verifică dacă există **vreo ușă deschisă**; dacă da, emite alarme și solicită închiderea manuală, apoi **recalibrează** poziția.
- **Selectează etajul** cu butoanele. Dacă ușa e deschisă, comanda e memorată și executată după închidere.
- În timpul deplasării, la **atingerea senzorului Hall** al etajului țintă: contorizează **călătoria**, aliniază, **anunță sosirea** și deschide ușa.
- După `WAIT_TIME`, ușa se închide; dacă senzorul optic validează închidere, se aplică **suprapas** (pași suplimentari) pentru etanșare.
- După `INACTIVITY_TIMEOUT`, LCD afișează **statistici** (cicluri ușă / călătorii).

## Securitate operațională
- **Interblocare ușă**: mișcare interzisă dacă senzorul optic indică ușa deschisă.
- **Ușă deschisă fără cabină la etaj**: **stop imediat**, alarmă, ghid pentru închiderea manuală, apoi **recalibrare**.
- **Restaurare stare la reboot**: dintr-un **ring buffer EEPROM** (ultimul snapshot valid).
- **Poziție sigură**: algoritm pentru găsirea celui mai apropiat etaj sigur și aducerea cabinei acolo.
- **Semnalizare acustică** pentru toate evenimentele critice (warning, oprire de urgență, eroare rezolvată).

## Persistență & telemetrie
- **EEPROM**: stochează **etajul curent/țintă**, starea ușii, dacă era în mișcare, **cereri pe etaje**. Se folosesc **10 bănci** ciclice (anti-uzură).  
- Contoare **persistente**: `doorCycleCount` (cicluri ușă), `tripCount` (călătorii).  
- **Afișare statistici** pe LCD după perioade de inactivitate; salvare periodică la intervale fixe.

## Alimentare & eficiență
- **12V/5A** pentru motoare și conversii; **5V** logică.  
- **Backup**: modul **comutare automată** + **acumulator 6V/4Ah** pentru continuitate la cădere de rețea.  
- Selectarea motoarelor (NEMA17 pentru cabină, 28BYJ-48 pentru uși) optimizează **precizia**, **cuplul** și **costul**.  
- **L298N** este simplu și robust; pentru eficiență/curenți mai mari se pot considera drivere dedicate (A4988).

## Limitări & direcții viitoare
- **3 etaje** preconfigurate (`MAX_FLOORS=3`, `STEPS_PER_FLOOR=1000`).  
- **Micro-pas** limitat cu L298N; îmbunătățiri posibile la vibrații și eficiență cu drivere curente dedicate.  
- Integrare **diagnostic avansat**, jurnalizare pe UART/SD, interfață HMI extinsă, comunicare CAN/RS-485, **sursă redundantă** supervizată.  

> **Notă**: Prototipul urmează bune practici din **EN 81**, însă **nu este conform integral EN 81**.
---
# [ENG] Low-Power Automatic Elevator System

> **Note**: Design and implementation of a low-energy prototype elevator with robust safety mechanisms and EEPROM state persistence, intended to demonstrate automation and operational safety concepts in a research context.

## Table of Contents
- [Overview](#overview)
- [Architecture & Components](#architecture--components)
- [Key Features](#key-features)
- [Pinout & Connections](#pinout--connections)
- [Software & Dependencies](#software--dependencies)
- [Configuration](#configuration)
- [Usage](#usage)
- [Operational Safety](#operational-safety)
- [Persistence & Telemetry](#persistence--telemetry)
- [Power & Efficiency](#power--efficiency)
- [Limitations & Future Work](#limitations--future-work)

## Overview
The prototype is a **low-power automatic elevator system** for 3 levels (ground floor, floor 1, floor 2), focused on **energy reduction**, **safety**, and **continuity of operation** (backup on power loss). Control is handled by an **Arduino Mega 2560**, stepper motors for the car and doors, Hall and optical sensors, an I2C LCD display, and a buzzer for audible feedback.

<p align="center">
  <img src="https://i.imgur.com/bl9QmEL.png" alt="Architecture diagram" width="350">
</p>

## Architecture & Components
**Functional blocks**:
- **Control unit:** Arduino Mega 2560
- **Car actuation:** **NEMA17 17HS4401** stepper + **L298N** driver
- **Door actuation:** **28BYJ-48 5V** stepper + **ULN2003** driver
- **Car position sensors:** 3 × **Hall** sensors (ground, floors 1–2)
- **Door sensors:** 3 × **optical** sensors (door “closed”), 3 × **Hall** sensors (door “open”)
- **Interface:** I2C LCD 16×2, floor buttons, buzzer
- **Power:** 12V/5A switching supply, **automatic switchover module** (YX850) + **6V/4Ah battery**, adjustable buck converter

## Key Features
- **3 levels** (ground/1st/2nd) with dedicated **call buttons**.
- **Car motion control** with a linear profile and **floor alignment** using Hall sensors.
- **Door control**: sequential open/close.
- **Safety interlocks**: movement inhibited with door open; “door open” detection; emergency stop and **recalibration**.
- **State persistence** in **EEPROM** (cyclic journals/banks) — restore on reboot.
- **Telemetry**: **door cycles** and **trips** counters; **LCD statistics** after inactivity.
- **Audio feedback**: tones for events (startup, arrival, errors, recalibration, etc.).

## Pinout & Connections
> **Note**: power **motors** from dedicated supplies; **common ground** between logic and power; noise protection; observe driver current limits.

**Floor buttons**  
- FLOOR0: D2  
- FLOOR1: D3  
- FLOOR2: D4  

**Hall sensors (car position)**  
- Floor 0: D5  
- Floor 1: D6  
- Floor 2: D7  

**Car stepper (AccelStepper FULL4WIRE)**  
- Coils: D8, D9, D10, D11

**Buzzer**  
- D12

**Door sensors**  
- Optical (door closed): A0(D14), A1(D15), A2(D16)  
- Hall (door fully open): D17, D18, D19

**Door motor (28BYJ-48 + ULN2003)**  
- D22, D23, D24, D25

**I2C LCD 16×2**  
- I2C @ 0x27 (SDA/SCL — Mega hardware pins)

## Software & Dependencies
- **Arduino IDE**
- Libraries:
  - `AccelStepper`
  - `LiquidCrystal_I2C`
  - `Wire` (I2C)
  - `EEPROM`

## Configuration
Configurable parameters in code:
- `STEPS_PER_FLOOR` — **1000** (steps between floors)
- `MAX_FLOORS` — **3**
- `DOOR_STEPS_SPEED` — door step speed
- `WAIT_TIME` — dwell time with door open
- `INACTIVITY_TIMEOUT` — when to show statistics
- **EEPROM** addresses: signature and **cyclic banks** for backups
- Periodic save timings (`SAVE_INTERVAL`)

## Usage
- On **startup**, the system checks whether **any door is open**; if so, it emits alarms and requests manual closing, then **recalibrates** position.
- **Select a floor** via buttons. If the door is open, the command is queued and executed after closing.
- During travel, on **hitting the target floor’s Hall sensor**: it increments the **trip** counter, aligns, **announces arrival**, and opens the door.
- After `WAIT_TIME`, the door closes; if the optical sensor validates closure, a **overshoot** (extra steps) is applied for sealing.
- After `INACTIVITY_TIMEOUT`, the LCD shows **statistics** (door cycles / trips).

## Operational Safety
- **Door interlock**: movement is prohibited if the optical sensor indicates the door is open.
- **Door open without car at floor**: **immediate stop**, alarm, guidance for manual closing, then **recalibration**.
- **State restore on reboot**: from an **EEPROM ring buffer** (last valid snapshot).
- **Safe position**: algorithm to find the nearest safe floor and bring the car there.
- **Audible signaling** for all critical events (warning, emergency stop, error resolved).

## Persistence & Telemetry
- **EEPROM** stores **current/target floor**, door state, whether it was moving, **floor requests**. **10 cyclic banks** are used (wear leveling).  
- Persistent counters: `doorCycleCount` (door cycles), `tripCount` (trips).  
- **Statistics display** on the LCD after inactivity periods; periodic saves at fixed intervals.

## Power & Efficiency
- **12V/5A** for motors and converters; **5V** logic.  
- **Backup**: **automatic switchover module** + **6V/4Ah battery** for continuity during mains dropouts.  
- Motor selection (NEMA17 for car, 28BYJ-48 for doors) optimizes **precision**, **torque**, and **cost**.  
- **L298N** is simple and robust; for better efficiency/higher currents, consider dedicated current drivers (A4988).

## Limitations & Future Work
- **3 floors** preconfigured (`MAX_FLOORS=3`, `STEPS_PER_FLOOR=1000`).  
- **Microstepping** limited with L298N; potential improvements to vibration and efficiency with dedicated current drivers.  
- Integrate **advanced diagnostics**, UART/SD logging, extended HMI, CAN/RS-485 comms, **supervised redundant supply**.  

> **Note**: The prototype follows **EN 81** good practices but is **not fully EN 81 compliant**.

