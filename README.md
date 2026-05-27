# Memory Rush

Memory Rush este un joc de memorie si reflexe construit in jurul reproducerii unor secvente de culori contra cronometru, inspirat din clasicul "Simon Says". Proiectul este dezvoltat pe un microcontroler ATmega328P (placa xmini).

## Cum se joaca

* Un LCD 16x2 afiseaza o secventa de culori, iar LED-urile se aprind in ordinea corespunzatoare.
* Jucatorul trebuie sa reproduca exact secventa apasand butoanele fizice.
* Timpul de reactie este limitat la 8 secunde per actiune.
* O greseala sau expirarea timpului activeaza buzzerul si incheie runda.
* Jocul mentine un clasament cu primele 5 scoruri (Highscores) salvat persistent in memoria EEPROM.

## Hardware Design

Componente folosite:
* ATmega328P-xmini
* LCD 16x2 I2C
* 4x Butoane (Rosu, Albastru, Verde, Galben)
* 4x LED-uri 3mm (Rosu, Albastru, Verde, Galben)
* Buzzer activ
* Rezistente 220 Ohm, fire de conexiune, breadboards

### Pinout
* **Butoane (INPUT_PULLUP, Intreruperi Hardware):**
  * BLUE: D2 (INT0)
  * YELLOW: D3 (INT1)
  * GREEN: A3 (PCINT11)
  * RED: D5 (PCINT21)
* **LED-uri (OUTPUT):**
  * BLUE: D4
  * YELLOW: D6
  * GREEN: A0
  * RED: D7
* **Buzzer:** A2
* **LCD I2C:** SDA (A4), SCL (A5)

## Software

Dezvoltat in PlatformIO. Se bazeaza masiv pe intreruperi hardware pentru a nu rata nicio apasare de buton (fara polling) si pe folosirea Timer1 pentru functii non-blocante, inlocuind complet functia clasica `delay()`.
