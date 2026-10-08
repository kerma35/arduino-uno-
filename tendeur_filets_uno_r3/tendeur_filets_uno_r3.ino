/*
  Tendeur de filets pour simracing
  Arduino Uno Rev3 + shield moteur TB6612/PCA9685 (type Motor Shield V2) + 2 servos MG90S,
  piloté par SimHub (Custom serial devices)

  SimHub envoie une ligne de texte à chaque mise à jour :
      L<0-100>R<0-100>        exemple : "L35R80"
  L = serrage du filet gauche en %, R = serrage du filet droit en %.

  Les deux prises servo du shield sont reliées directement à D9 et D10 de l'Uno :
  les puces PCA9685 et TB6612 du shield ne servent pas ici, aucune bibliothèque
  Adafruit n'est nécessaire. Seule la bibliothèque Servo (fournie avec Arduino IDE).
*/

#include <Servo.h>
#include <string.h>
#include <stdlib.h>

// ----- Branchements -----
const int PIN_SERVO_GAUCHE = 10;  // prise servo du shield reliée à D10
const int PIN_SERVO_DROIT  = 9;   // prise servo du shield reliée à D9

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
const long VITESSE_SERIE              = 115200;

Servo servoGauche;
Servo servoDroit;

float consigneG = 0, consigneD = 0;  // % demandé par SimHub
float positionG = 0, positionD = 0;  // % lissé envoyé aux servos
int angleG = -1, angleD = -1;        // dernier angle écrit
unsigned long dernierMessage = 0;
unsigned long derniereMaj = 0;

char ligne[32];
uint8_t longueur = 0;

// Lit "L35R80" et met à jour les consignes.
void appliquerLigne(const char *texte) {
  const char *pL = strchr(texte, 'L');
  const char *pR = strchr(texte, 'R');
  if (pL == nullptr || pR == nullptr) return;
  consigneG = constrain(atoi(pL + 1), 0, 100);
  consigneD = constrain(atoi(pR + 1), 0, 100);
  dernierMessage = millis();
}

// Assemble les octets reçus en lignes terminées par \n ou \r.
void recevoirOctet(char c) {
  if (c == '\n' || c == '\r') {
    if (longueur > 0) {
      ligne[longueur] = '\0';
      appliquerLigne(ligne);
      longueur = 0;
    }
  } else if (longueur < sizeof(ligne) - 1) {
    ligne[longueur++] = c;
  } else {
    longueur = 0;  // ligne trop longue : on l'ignore
  }
}

int versAngle(float pourcent, int repos, int max) {
  float decalage = (max - repos) * pourcent / 100.0f;
  return repos + (int)(decalage >= 0 ? decalage + 0.5f : decalage - 0.5f);
}

void setup() {
  Serial.begin(VITESSE_SERIE);

  servoGauche.attach(PIN_SERVO_GAUCHE);
  servoDroit.attach(PIN_SERVO_DROIT);
  servoGauche.write(GAUCHE_REPOS);
  servoDroit.write(DROIT_REPOS);
}

void loop() {
  while (Serial.available() > 0) recevoirOctet((char)Serial.read());

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
