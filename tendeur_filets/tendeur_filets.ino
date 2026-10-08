/*
  Tendeur de filets pour simracing
  Arduino UNO Q + 2 servos MG90S, piloté par SimHub (Custom serial devices)

  SimHub envoie une ligne de texte à chaque mise à jour :
      L<0-100>R<0-100>        exemple : "L35R80"
  L = serrage du filet gauche en %, R = serrage du filet droit en %.

  Le programme écoute deux entrées en même temps, utilise celle que tu veux :
    - Monitor : le port COM du câble USB-C (passe par le côté Linux de l'UNO Q)
    - Serial1 : broches D0 (RX) / D1 (TX), pour un adaptateur USB-TTL 3,3 V

  Bibliothèques : Servo (version 1.3.0 ou plus, qui gère l'UNO Q)
                  Arduino_RouterBridge (fournit Monitor)
*/

#include <Arduino_RouterBridge.h>
#include <Servo.h>
#include <math.h>
#include <string.h>
#include <stdlib.h>

// ----- Branchements -----
const int PIN_SERVO_GAUCHE = 10;  // fil orange du servo gauche
const int PIN_SERVO_DROIT  = 9;   // fil orange du servo droit

// ----- Course des servos (en degrés, entre 0 et 180) -----
// REPOS = filet relâché, MAX = filet tiré au maximum.
// Les servos sont montés en miroir : l'un tourne dans un sens, l'autre dans l'autre.
const int GAUCHE_REPOS = 20;
const int GAUCHE_MAX   = 110;
const int DROIT_REPOS  = 160;
const int DROIT_MAX    = 70;

// ----- Comportement -----
const unsigned long DELAI_SECURITE_MS = 1000;  // sans message pendant 1 s : retour au repos
const unsigned long PERIODE_MS        = 10;    // mise à jour des servos toutes les 10 ms
const float LISSAGE                   = 0.35f; // 0.1 = très doux, 1.0 = immédiat
const long VITESSE_UART               = 115200;

Servo servoGauche;
Servo servoDroit;

float consigneG = 0, consigneD = 0;  // % demandé par SimHub
float positionG = 0, positionD = 0;  // % lissé envoyé aux servos
int angleG = -1, angleD = -1;        // dernier angle écrit
unsigned long dernierMessage = 0;
unsigned long derniereMaj = 0;

struct Lecteur {
  char ligne[32];
  uint8_t n;
};
Lecteur lecteurUSB  = {{0}, 0};
Lecteur lecteurUART = {{0}, 0};

// Lit "L35R80" et met à jour les consignes.
void appliquerLigne(const char *ligne) {
  const char *pL = strchr(ligne, 'L');
  const char *pR = strchr(ligne, 'R');
  if (pL == nullptr || pR == nullptr) return;
  consigneG = constrain(atoi(pL + 1), 0, 100);
  consigneD = constrain(atoi(pR + 1), 0, 100);
  dernierMessage = millis();
}

// Assemble les octets reçus en lignes terminées par \n ou \r.
void recevoirOctet(Lecteur &l, char c) {
  if (c == '\n' || c == '\r') {
    if (l.n > 0) {
      l.ligne[l.n] = '\0';
      appliquerLigne(l.ligne);
      l.n = 0;
    }
  } else if (l.n < sizeof(l.ligne) - 1) {
    l.ligne[l.n++] = c;
  } else {
    l.n = 0;  // ligne trop longue : on l'ignore
  }
}

int versAngle(float pourcent, int repos, int max) {
  return repos + (int)lroundf((max - repos) * pourcent / 100.0f);
}

void setup() {
  Monitor.begin();
  Serial1.begin(VITESSE_UART);

  servoGauche.attach(PIN_SERVO_GAUCHE);
  servoDroit.attach(PIN_SERVO_DROIT);
  servoGauche.write(GAUCHE_REPOS);
  servoDroit.write(DROIT_REPOS);
}

void loop() {
  while (Monitor.available() > 0) recevoirOctet(lecteurUSB, (char)Monitor.read());
  while (Serial1.available() > 0) recevoirOctet(lecteurUART, (char)Serial1.read());

  unsigned long maintenant = millis();

  // Sécurité : SimHub fermé, jeu quitté ou câble débranché -> filets relâchés
  if (maintenant - dernierMessage > DELAI_SECURITE_MS) {
    consigneG = 0;
    consigneD = 0;
  }

  if (maintenant - derniereMaj >= PERIODE_MS) {
    derniereMaj = maintenant;
    positionG += (consigneG - positionG) * LISSAGE;
    positionD += (consigneD - positionD) * LISSAGE;

    int a = versAngle(positionG, GAUCHE_REPOS, GAUCHE_MAX);
    if (a != angleG) { servoGauche.write(a); angleG = a; }

    a = versAngle(positionD, DROIT_REPOS, DROIT_MAX);
    if (a != angleD) { servoDroit.write(a); angleD = a; }
  }
}
