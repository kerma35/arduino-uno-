/*
  Boîtier à boisson pour simracing
  Arduino Uno Rev3 + module MOSFET + pompe péristaltique 12 V

  Tant que tu appuies sur le bouton, la pompe envoie de l'eau dans le tuyau.
  Par sécurité, une gorgée dure au plus DOSE_MAX_MS : il faut relâcher le
  bouton pour en reprendre une, après une petite pause.

  Deux façons d'appuyer :
    - un bouton sur le boîtier, entre D2 et GND ;
    - un bouton du volant, via SimHub (Custom serial devices), qui envoie
      "B1" tant qu'il est appuyé et "B0" sinon.

  Aucune bibliothèque nécessaire.
*/

// ----- Branchements -----
const int PIN_BOUTON = 2;   // bouton du boîtier, l'autre patte sur GND
const int PIN_POMPE  = 9;   // entrée SIG du module MOSFET
const int PIN_LED    = 13;  // LED de l'Uno : allumée quand la pompe tourne

// ----- Réglages -----
const unsigned long DOSE_MAX_MS     = 4000;  // durée maximale d'une gorgée
const unsigned long PAUSE_MS        = 1000;  // pause obligatoire entre deux gorgées
const unsigned long DELAI_SERIE_MS  = 500;   // sans message SimHub depuis 0,5 s : bouton du volant considéré relâché
const uint8_t PUISSANCE             = 255;   // 255 = pleine vitesse, baisse vers 150 pour un débit plus doux
const unsigned long AMORCAGE_MS     = 0;     // > 0 : la pompe tourne ce temps au démarrage pour remplir le tuyau
const long VITESSE_SERIE            = 115200;

bool boutonSerie = false;
unsigned long dernierMessage = 0;

bool pompeEnMarche = false;
bool attendRelache = false;          // dose maximale atteinte : on attend que le bouton soit relâché
unsigned long debutGorgee = 0;
unsigned long finGorgee = 0;

char ligne[16];
uint8_t longueur = 0;

void appliquerLigne(const char *texte) {
  if (texte[0] != 'B') return;
  boutonSerie = (texte[1] == '1');
  dernierMessage = millis();
}

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

void pompe(bool marche) {
  if (marche == pompeEnMarche) return;
  pompeEnMarche = marche;
  analogWrite(PIN_POMPE, marche ? PUISSANCE : 0);
  digitalWrite(PIN_LED, marche ? HIGH : LOW);
}

void setup() {
  Serial.begin(VITESSE_SERIE);
  pinMode(PIN_BOUTON, INPUT_PULLUP);
  pinMode(PIN_POMPE, OUTPUT);
  pinMode(PIN_LED, OUTPUT);
  analogWrite(PIN_POMPE, 0);

  if (AMORCAGE_MS > 0) {
    pompe(true);
    delay(AMORCAGE_MS);
    pompe(false);
  }
}

void loop() {
  while (Serial.available() > 0) recevoirOctet((char)Serial.read());

  unsigned long maintenant = millis();

  // Sécurité : SimHub fermé ou câble débranché -> bouton du volant relâché
  if (maintenant - dernierMessage > DELAI_SERIE_MS) boutonSerie = false;

  bool appui = (digitalRead(PIN_BOUTON) == LOW) || boutonSerie;

  if (!appui) {
    if (pompeEnMarche) finGorgee = maintenant;
    pompe(false);
    attendRelache = false;
    return;
  }

  if (pompeEnMarche) {
    if (maintenant - debutGorgee >= DOSE_MAX_MS) {
      pompe(false);  // dose maximale : on coupe même si le bouton reste appuyé
      finGorgee = maintenant;
      attendRelache = true;
    }
  } else if (!attendRelache && maintenant - finGorgee >= PAUSE_MS) {
    debutGorgee = maintenant;
    pompe(true);
  }
}
