/*
  Matrice LED 16 x 16 pour simracing
  Arduino Uno Rev3 (ou Nano) + matrice WS2812B 16 x 16 (256 LED),
  pilotée par SimHub (Custom serial devices)

  SimHub envoie une ligne de texte à chaque mise à jour :
      $<22 nombres séparés par des virgules>*<somme de contrôle>
  Le détail des champs est dans l'énumération Champ ci-dessous et dans
  simhub/matrice_message.js.

  Écrans (bouton sur D2 ou bouton du volant via SimHub) :
      0 tableau   : régime sur les bords, rapport en gros, vitesse
      1 régime    : jauge plein écran qui monte ligne par ligne
      2 pédales   : embrayage, frein, accélérateur, témoins ABS et TC
      3 G         : point sur un cercle de friction
      4 position  : P3, P12... et delta par rapport au meilleur tour
  Par-dessus : drapeaux, feux de départ, animation de fin de tour,
  et une horloge quand aucun jeu ne tourne.

  Bibliothèque nécessaire : FastLED (Outils > Gérer les bibliothèques).
  Les écrans sont dessinés pour une matrice carrée de 16 x 16.
*/

#include <FastLED.h>
#include <stdlib.h>

// ----- Branchements -----
#define PIN_MATRICE 6        // entrée DIN de la matrice, via une résistance de 330 à 470 ohms
const uint8_t PIN_BOUTON = 2;  // bouton optionnel entre D2 et GND pour changer d'écran

// ----- Câblage de la matrice -----
// Au démarrage : coin rouge = haut gauche, vert = haut droit, bleu = bas gauche.
// Si ce n'est pas le cas, change ces réglages un par un et téléverse à nouveau.
const bool SERPENTIN   = true;   // true : une ligne sur deux est câblée en sens inverse
const bool ECHANGER_XY = false;  // true : la matrice est câblée par colonnes
const bool MIROIR_X    = false;  // true : retourne l'image gauche / droite
const bool MIROIR_Y    = false;  // true : retourne l'image haut / bas

// ----- Luminosité et courant -----
const uint8_t  LUMINOSITE     = 40;    // sur 255 : 40 = environ 15 %
const uint16_t COURANT_MAX_MA = 2000;  // FastLED baisse la luminosité pour ne pas dépasser ce courant

// ----- Comportement -----
const long VITESSE_SERIE              = 115200;
const unsigned long DELAI_VEILLE_MS   = 3000;  // sans message pendant 3 s : mode veille
const unsigned long DRAPEAU_PLEIN_MS  = 3000;  // drapeau plein écran, puis simple bordure
const uint8_t ECRAN_DEPART            = 0;
const uint8_t SEUIL_ORANGE            = 70;    // % du régime où la jauge passe à l'orange
const uint8_t SEUIL_ROUGE             = 88;    // % du régime où la jauge passe au rouge
const float G_ECHELLE                 = 2.0f;  // force (en G) qui amène le point au bord

const uint8_t  LARGEUR = 16;
const uint8_t  HAUTEUR = 16;
const uint16_t NB_LEDS = LARGEUR * HAUTEUR;

CRGB leds[NB_LEDS];

// ----- Message SimHub -----
enum Champ {
  C_JEU,        // 1 = en piste, 0 = pas de jeu
  C_REGIME,     // % du régime max
  C_SHIFT,      // 1 = passe le rapport
  C_RAPPORT,    // -1 = R, 0 = N, 1 à 9
  C_VITESSE,    // km/h
  C_POSITION,   // position en course
  C_ACCEL,      // accélérateur 0-100
  C_FREIN,      // frein 0-100
  C_EMBRAYAGE,  // embrayage 0-100
  C_ABS,        // 1 = ABS actif
  C_TC,         // 1 = antipatinage actif
  C_DRAPEAU,    // 0 aucun, 1 jaune, 2 bleu, 3 blanc, 4 noir, 5 vert, 6 damier, 7 noir rond orange
  C_GX,         // force latérale en centièmes de G, positif = point vers la droite
  C_GY,         // force longitudinale en centièmes de G, positif = point vers le bas
  C_TOURS,      // tours terminés
  C_RECORD,     // 1 = le dernier tour est le meilleur
  C_GRILLE,     // 1 = arrêté sur la grille avant le départ
  C_DELTA,      // écart au meilleur tour en centièmes de seconde
  C_HEURE,
  C_MINUTE,
  C_SECONDE,
  C_ECRAN,      // écran choisi dans SimHub, -1 = celui du bouton D2
  NB_CHAMPS
};

int champs[NB_CHAMPS];
char ligne[112];
uint8_t longueur = 0;
unsigned long dernierMessage = 0;
bool messageRecu = false;

// ----- Écrans -----
enum { ECRAN_TABLEAU, ECRAN_REGIME, ECRAN_PEDALES, ECRAN_G, ECRAN_POSITION, NB_ECRANS };
uint8_t ecranLocal = ECRAN_DEPART;
bool boutonAvant = false;
unsigned long dernierAppui = 0;

// ----- Événements -----
uint8_t drapeau = 0;
unsigned long debutDrapeau = 0;

enum { DEPART_AUCUN, DEPART_GRILLE, DEPART_EXTINCTION };
uint8_t etatDepart = DEPART_AUCUN;
unsigned long debutDepart = 0;

int toursAvant = -1;
bool animTour = false;
unsigned long debutTour = 0;
const unsigned long TRAVERSEE_MS = 1200;  // la voiture traverse l'écran
const unsigned long RECORD_MS    = 900;   // puis flash violet si meilleur tour

const uint8_t NB_TRAINEE = 6;
float traineeX[NB_TRAINEE], traineeY[NB_TRAINEE];
uint8_t traineeTete = 0;

unsigned long dernierAffichage = 0;

// ----- Police 3 x 5 (bit 2 = colonne de gauche) -----
const uint8_t POLICE[][5] PROGMEM = {
  {7, 5, 5, 5, 7},  // 0
  {2, 6, 2, 2, 7},  // 1
  {7, 1, 7, 4, 7},  // 2
  {7, 1, 7, 1, 7},  // 3
  {5, 5, 7, 1, 1},  // 4
  {7, 4, 7, 1, 7},  // 5
  {7, 4, 7, 5, 7},  // 6
  {7, 1, 1, 1, 1},  // 7
  {7, 5, 7, 5, 7},  // 8
  {7, 5, 7, 1, 7},  // 9
  {6, 5, 6, 4, 4},  // P
  {6, 5, 6, 5, 5},  // R
  {6, 5, 5, 5, 5},  // N
  {0, 0, 7, 0, 0},  // -
};
enum { GLYPHE_P = 10, GLYPHE_R, GLYPHE_N, GLYPHE_TIRET };

// Voiture vue de côté : # carrosserie, w vitre, o roue
const char *const VOITURE[] = {
  "..#####...",
  ".##ww#ww#.",
  "##########",
  ".oo....oo.",
};
const uint8_t VOITURE_L = 10;
const uint8_t VOITURE_H = 4;

const CRGB ORANGE_VIF = CRGB(255, 90, 0);
const CRGB VIOLET     = CRGB(150, 0, 255);

// ================= Dessin de base =================

uint16_t XY(uint8_t x, uint8_t y) {
  if (MIROIR_X) x = LARGEUR - 1 - x;
  if (MIROIR_Y) y = HAUTEUR - 1 - y;
  if (ECHANGER_XY) { uint8_t t = x; x = y; y = t; }
  if (SERPENTIN && (y & 1)) x = LARGEUR - 1 - x;
  return (uint16_t)y * LARGEUR + x;
}

void pixel(int x, int y, CRGB c) {
  if (x < 0 || y < 0 || x >= LARGEUR || y >= HAUTEUR) return;
  leds[XY(x, y)] = c;
}

void ajouter(int x, int y, CRGB c, uint8_t poids) {
  if (x < 0 || y < 0 || x >= LARGEUR || y >= HAUTEUR) return;
  c.nscale8_video(poids);
  leds[XY(x, y)] += c;
}

void remplir(CRGB c) { fill_solid(leds, NB_LEDS, c); }

CRGB attenuer(CRGB c, uint8_t niveau) {
  c.nscale8_video(niveau);
  return c;
}

bool clignote(unsigned long t, unsigned long periode) { return (t / periode) & 1; }

void glyphe(uint8_t index, int x, int y, uint8_t echelle, CRGB c) {
  for (uint8_t r = 0; r < 5; r++) {
    uint8_t bits = pgm_read_byte(&POLICE[index][r]);
    for (uint8_t col = 0; col < 3; col++) {
      if (!(bits & (4 >> col))) continue;
      for (uint8_t sy = 0; sy < echelle; sy++)
        for (uint8_t sx = 0; sx < echelle; sx++)
          pixel(x + col * echelle + sx, y + r * echelle + sy, c);
    }
  }
}

// Nombre centré horizontalement, chiffres espacés de 'espace' colonnes.
void nombreCentre(unsigned int v, int y, uint8_t echelle, uint8_t espace, CRGB c) {
  uint8_t chiffres[5];
  uint8_t n = 0;
  do { chiffres[n++] = v % 10; v /= 10; } while (v && n < 5);
  int largeur = n * 3 * echelle + (n - 1) * espace;
  int x = (LARGEUR - largeur) / 2;
  for (int i = n - 1; i >= 0; i--) {
    glyphe(chiffres[i], x, y, echelle, c);
    x += 3 * echelle + espace;
  }
}

void glypheRapport(int rapport, int x, int y, uint8_t echelle, CRGB c) {
  uint8_t g = rapport < 0 ? GLYPHE_R : (rapport == 0 ? GLYPHE_N : min(rapport, 9));
  glyphe(g, x, y, echelle, c);
}

// Tour de l'écran en 60 pixels, en partant du coin haut gauche, sens horaire.
void perimetre(uint8_t p, int &x, int &y) {
  p %= 60;
  if (p < 16)      { x = p;      y = 0; }
  else if (p < 31) { x = 15;     y = p - 15; }
  else if (p < 46) { x = 45 - p; y = 15; }
  else             { x = 0;      y = 60 - p; }
}

void bordure(CRGB c) {
  int x, y;
  for (uint8_t p = 0; p < 60; p++) { perimetre(p, x, y); pixel(x, y, c); }
}

CRGB couleurRegime(uint8_t pourcent) {
  if (pourcent >= SEUIL_ROUGE) return CRGB::Red;
  if (pourcent >= SEUIL_ORANGE) return ORANGE_VIF;
  return CRGB::Green;
}

// Barre verticale qui monte du bas, avec une ligne partielle pour la fluidité.
void barreVerticale(uint8_t x0, uint8_t largeur, int valeur, CRGB c) {
  uint16_t total = (uint16_t)constrain(valeur, 0, 100) * HAUTEUR;  // en centièmes de ligne
  uint8_t pleines = total / 100;
  uint8_t reste = total % 100;
  for (uint8_t i = 0; i < HAUTEUR; i++) {
    CRGB k;
    if (i < pleines) k = c;
    else if (i == pleines && reste > 0) k = attenuer(c, reste * 255 / 100);
    else k = attenuer(c, 20);  // fond de la barre
    for (uint8_t x = x0; x < x0 + largeur; x++) pixel(x, HAUTEUR - 1 - i, k);
  }
}

// Point flou réparti sur 4 pixels : il glisse au lieu de sauter.
void pointFlou(float fx, float fy, CRGB c) {
  int x0 = (int)floor(fx), y0 = (int)floor(fy);
  float ax = fx - x0, ay = fy - y0;
  ajouter(x0,     y0,     c, (1 - ax) * (1 - ay) * 255);
  ajouter(x0 + 1, y0,     c, ax * (1 - ay) * 255);
  ajouter(x0,     y0 + 1, c, (1 - ax) * ay * 255);
  ajouter(x0 + 1, y0 + 1, c, ax * ay * 255);
}

// ================= Écrans =================

void ecranTableau(unsigned long t) {
  int regime = champs[C_REGIME];
  bool shift = champs[C_SHIFT];
  uint8_t allumees = (constrain(regime, 0, 100) * HAUTEUR + 50) / 100;

  for (uint8_t i = 0; i < HAUTEUR; i++) {
    CRGB k;
    if (shift) k = clignote(t, 60) ? CRGB::Red : CRGB::Black;
    else if (i < allumees) k = couleurRegime((i + 1) * 100 / HAUTEUR);
    else k = CRGB::Black;
    pixel(0, HAUTEUR - 1 - i, k);
    pixel(LARGEUR - 1, HAUTEUR - 1 - i, k);
  }

  int rapport = champs[C_RAPPORT];
  CRGB couleur = shift ? CRGB::Red : (rapport < 0 ? ORANGE_VIF : (rapport == 0 ? CRGB::Green : CRGB::White));
  glypheRapport(rapport, 5, 0, 2, couleur);

  nombreCentre(constrain(champs[C_VITESSE], 0, 999), 11, 1, 1, CRGB(0, 150, 255));
}

void ecranRegime(unsigned long t) {
  int regime = constrain(champs[C_REGIME], 0, 100);
  bool shift = champs[C_SHIFT];
  uint16_t total = (uint16_t)regime * HAUTEUR;
  uint8_t pleines = total / 100;
  uint8_t reste = total % 100;

  for (uint8_t i = 0; i < HAUTEUR; i++) {
    CRGB k = couleurRegime((i + 1) * 100 / HAUTEUR);
    if (shift) k = clignote(t, 60) ? CRGB::Red : CRGB::Black;
    else if (i == pleines) k = attenuer(k, reste * 255 / 100);
    else if (i > pleines) k = CRGB::Black;
    for (uint8_t x = 0; x < LARGEUR; x++) pixel(x, HAUTEUR - 1 - i, k);
  }

  glypheRapport(champs[C_RAPPORT], 5, 3, 2, CRGB::White);
}

void ecranPedales(unsigned long t) {
  barreVerticale(0, 4, champs[C_EMBRAYAGE], CRGB(0, 80, 255));
  barreVerticale(5, 4, champs[C_FREIN], CRGB::Red);
  barreVerticale(10, 4, champs[C_ACCEL], CRGB::Green);

  // Colonne de droite : ABS en haut, antipatinage en bas
  CRGB abs = (champs[C_ABS] && clignote(t, 80)) ? ORANGE_VIF : attenuer(ORANGE_VIF, 20);
  CRGB tc  = (champs[C_TC]  && clignote(t, 80)) ? CRGB::Yellow : attenuer(CRGB::Yellow, 20);
  for (uint8_t y = 0; y < 8; y++) pixel(15, y, abs);
  for (uint8_t y = 8; y < 16; y++) pixel(15, y, tc);
}

void ecranG() {
  // Cercles à G_ECHELLE / 2 et G_ECHELLE, en demi-pixels pour rester en calcul entier
  for (uint8_t y = 0; y < HAUTEUR; y++) {
    for (uint8_t x = 0; x < LARGEUR; x++) {
      int dx = 2 * x - 15, dy = 2 * y - 15;
      int d2 = dx * dx + dy * dy;
      if (d2 >= 180 && d2 <= 237) pixel(x, y, CRGB(30, 30, 30));
      else if (d2 >= 38 && d2 <= 67) pixel(x, y, CRGB(14, 14, 14));
      else if (x == 7 || x == 8 || y == 7 || y == 8) pixel(x, y, CRGB(6, 6, 6));
    }
  }

  // Traînée : les positions précédentes, de plus en plus sombres
  for (uint8_t i = 1; i < NB_TRAINEE; i++) {
    uint8_t k = (traineeTete + NB_TRAINEE - i) % NB_TRAINEE;
    pointFlou(traineeX[k], traineeY[k], attenuer(CRGB(0, 120, 255), 120 - i * 20));
  }

  float gx = champs[C_GX] / 100.0f, gy = champs[C_GY] / 100.0f;
  float intensite = sqrt(gx * gx + gy * gy) / G_ECHELLE;
  CRGB c = intensite < 0.5f ? CRGB::Green : (intensite < 0.8f ? CRGB::Yellow : CRGB::Red);
  pointFlou(traineeX[traineeTete], traineeY[traineeTete], c);
}

void ecranPosition() {
  int position = constrain(champs[C_POSITION], 0, 99);
  CRGB c = CRGB::White;
  if (position == 1) c = CRGB(255, 170, 0);
  else if (position == 2) c = CRGB(170, 170, 200);
  else if (position == 3) c = CRGB(205, 100, 30);

  glyphe(GLYPHE_P, 0, 0, 1, CRGB(80, 80, 80));
  if (position == 0) glyphe(GLYPHE_TIRET, 6, 0, 2, c);
  else if (position < 10) glyphe(position, 6, 0, 2, c);
  else {
    glyphe(position / 10, 3, 0, 2, c);
    glyphe(position % 10, 10, 0, 2, c);
  }

  // Delta au meilleur tour : vert vers la gauche si tu gagnes du temps, rouge vers la droite si tu en perds.
  // Barre pleine = 1 seconde.
  int delta = champs[C_DELTA];
  uint8_t longueurBarre = min(abs(delta) * 8 / 100, 8);
  for (uint8_t i = 0; i < 8; i++) {
    for (uint8_t y = 13; y < 15; y++) {
      pixel(7 - i, y, (delta < 0 && i < longueurBarre) ? CRGB::Green : CRGB(8, 8, 8));
      pixel(8 + i, y, (delta > 0 && i < longueurBarre) ? CRGB::Red : CRGB(8, 8, 8));
    }
  }
}

// ================= Surimpressions =================

CRGB couleurDrapeau(uint8_t d) {
  switch (d) {
    case 1: return CRGB::Yellow;
    case 2: return CRGB::Blue;
    case 3: return CRGB(150, 150, 150);
    case 4: return CRGB::White;  // drapeau noir : bordure blanche
    case 5: return CRGB::Green;
    case 7: return ORANGE_VIF;
    default: return CRGB::White;
  }
}

void drapeauPleinEcran(unsigned long t) {
  switch (drapeau) {
    case 1: remplir(attenuer(CRGB::Yellow, 140 + scale8(sin8(t / 4), 115))); break;
    case 2: remplir(clignote(t, 250) ? CRGB::Blue : CRGB::Black); break;
    case 3: remplir(CRGB(150, 150, 150)); break;
    case 4: bordure(CRGB::White); break;
    case 5: remplir(CRGB::Green); break;
    case 6: {
      uint8_t decalage = t / 150;  // le damier défile
      for (uint8_t y = 0; y < HAUTEUR; y++)
        for (uint8_t x = 0; x < LARGEUR; x++)
          pixel(x, y, (((x + decalage) / 2 + y / 2) & 1) ? CRGB::White : CRGB::Black);
      break;
    }
    case 7:  // drapeau noir à disque orange : problème mécanique
      for (uint8_t y = 0; y < HAUTEUR; y++)
        for (uint8_t x = 0; x < LARGEUR; x++) {
          int dx = 2 * x - 15, dy = 2 * y - 15;
          if (dx * dx + dy * dy <= 100) pixel(x, y, ORANGE_VIF);
        }
      break;
  }
}

void feuxDepart(uint8_t allumes) {
  for (uint8_t i = 0; i < 5; i++) {
    int x = 1 + i * 3;
    for (uint8_t y = 3; y <= 12; y++) {
      bool lampe = (y == 7 || y == 8 || y == 10 || y == 11);
      CRGB c = (lampe && i < allumes) ? CRGB::Red : CRGB(30, 30, 30);
      pixel(x, y, c);
      pixel(x + 1, y, c);
    }
  }
}

void dessinerVoiture(int x0, int y0, CRGB carrosserie) {
  for (uint8_t r = 0; r < VOITURE_H; r++) {
    for (uint8_t c = 0; c < VOITURE_L; c++) {
      char k = VOITURE[r][c];
      if (k == '#') pixel(x0 + c, y0 + r, carrosserie);
      else if (k == 'w') pixel(x0 + c, y0 + r, CRGB(0, 60, 120));
      else if (k == 'o') pixel(x0 + c, y0 + r, CRGB(60, 60, 60));
    }
  }
}

void animationTour(unsigned long ecoule, unsigned long t) {
  if (ecoule < TRAVERSEE_MS) {
    int x = -VOITURE_L + (int)((LARGEUR + VOITURE_L) * ecoule / TRAVERSEE_MS);
    for (int i = 2; i < 8; i++) pixel(x - i, 9 - (i & 1) * 2, CRGB(20, 20, 20));  // traits de vitesse
    dessinerVoiture(x, 6, CRGB::Red);
  } else if (clignote(t, 150)) {
    remplir(VIOLET);  // meilleur tour
  }
}

void veille(unsigned long t, bool heureConnue) {
  int x, y;
  if (heureConnue) {
    CRGB c = CRGB(0, 90, 140);
    glyphe(champs[C_HEURE] / 10, 4, 1, 1, c);  // chiffre par chiffre pour garder le 0 de tête
    glyphe(champs[C_HEURE] % 10, 8, 1, 1, c);
    glyphe(champs[C_MINUTE] / 10, 4, 10, 1, c);
    glyphe(champs[C_MINUTE] % 10, 8, 10, 1, c);
    if (champs[C_SECONDE] & 1) { pixel(7, 6, c); pixel(7, 9, c); }
    perimetre(8 + champs[C_SECONDE], x, y);  // la trotteuse fait le tour de l'écran
    pixel(x, y, CRGB(120, 40, 0));
  } else {
    // SimHub absent : un point fait lentement le tour de l'écran
    perimetre(t / 200, x, y);
    pixel(x, y, CRGB(0, 40, 80));
  }
}

// ================= Réception =================

// Vérifie "$a,b,c,...*somme" et remplit champs[]. Retourne false si la ligne est abîmée.
bool lireMessage(const char *texte) {
  if (texte[0] != '$') return false;
  const char *etoile = strchr(texte, '*');
  if (etoile == nullptr) return false;

  uint8_t somme = 0;
  for (const char *p = texte + 1; p < etoile; p++) somme += (uint8_t)*p;
  if (atoi(etoile + 1) != somme) return false;

  int valeurs[NB_CHAMPS];
  const char *p = texte + 1;
  for (uint8_t n = 0; n < NB_CHAMPS; n++) {
    char *fin;
    long v = strtol(p, &fin, 10);
    if (fin == p) return false;
    valeurs[n] = (int)v;
    bool dernier = (n == NB_CHAMPS - 1);
    if (dernier ? (fin != etoile) : (*fin != ',')) return false;
    p = fin + 1;
  }
  memcpy(champs, valeurs, sizeof(champs));
  return true;
}

// Met à jour les événements (drapeau, tour, départ) à partir du nouveau message.
void traiterMessage(unsigned long t) {
  dernierMessage = t;
  messageRecu = true;

  bool enJeu = champs[C_JEU] == 1;
  uint8_t nouveauDrapeau = enJeu ? constrain(champs[C_DRAPEAU], 0, 7) : 0;
  if (nouveauDrapeau != drapeau) { drapeau = nouveauDrapeau; debutDrapeau = t; }

  int tours = champs[C_TOURS];
  if (enJeu && toursAvant >= 0 && tours == toursAvant + 1) { animTour = true; debutTour = t; }
  toursAvant = enJeu ? tours : -1;

  if (champs[C_GRILLE] && enJeu) {
    if (etatDepart != DEPART_GRILLE) { etatDepart = DEPART_GRILLE; debutDepart = t; }
  } else if (etatDepart == DEPART_GRILLE) {
    etatDepart = DEPART_EXTINCTION;
    debutDepart = t;
  }

  float fx = 7.5f + champs[C_GX] / (100.0f * G_ECHELLE) * 7.5f;
  float fy = 7.5f + champs[C_GY] / (100.0f * G_ECHELLE) * 7.5f;
  traineeTete = (traineeTete + 1) % NB_TRAINEE;
  traineeX[traineeTete] = constrain(fx, 0.0f, 15.0f);
  traineeY[traineeTete] = constrain(fy, 0.0f, 15.0f);
}

// Assemble les octets reçus en lignes. Retourne true quand un message valide vient d'arriver.
bool recevoirOctet(char c) {
  if (c == '\n' || c == '\r') {
    bool valide = false;
    if (longueur > 0) {
      ligne[longueur] = '\0';
      valide = lireMessage(ligne);
      longueur = 0;
    }
    return valide;
  }
  if (c == '$') longueur = 0;  // début de message : on repart de zéro
  if (longueur < sizeof(ligne) - 1) ligne[longueur++] = c;
  else longueur = 0;  // ligne trop longue : on l'ignore
  return false;
}

void lireBouton(unsigned long t) {
  bool appui = digitalRead(PIN_BOUTON) == LOW;
  if (appui && !boutonAvant && t - dernierAppui > 200) {
    ecranLocal = (ecranLocal + 1) % NB_ECRANS;
    dernierAppui = t;
  }
  boutonAvant = appui;
}

// ================= Affichage =================

void dessiner(unsigned long t) {
  FastLED.clear();

  bool connecte = messageRecu && (t - dernierMessage < DELAI_VEILLE_MS);
  if (!connecte || champs[C_JEU] != 1) {
    veille(t, connecte);
    return;
  }

  if (animTour) {
    unsigned long ecoule = t - debutTour;
    unsigned long duree = TRAVERSEE_MS + (champs[C_RECORD] ? RECORD_MS : 0);
    if (ecoule < duree) { animationTour(ecoule, t); return; }
    animTour = false;
  }

  if (etatDepart == DEPART_GRILLE) {
    feuxDepart(min((t - debutDepart) / 1000, 5UL));  // une lampe de plus chaque seconde
    return;
  }
  if (etatDepart == DEPART_EXTINCTION) {
    if (t - debutDepart < 1500) { feuxDepart(0); return; }
    etatDepart = DEPART_AUCUN;
  }

  if (drapeau == 6 || (drapeau != 0 && t - debutDrapeau < DRAPEAU_PLEIN_MS)) {
    drapeauPleinEcran(t);
    return;
  }

  int ecran = champs[C_ECRAN] >= 0 ? champs[C_ECRAN] % NB_ECRANS : ecranLocal;
  switch (ecran) {
    case ECRAN_TABLEAU:  ecranTableau(t); break;
    case ECRAN_REGIME:   ecranRegime(t); break;
    case ECRAN_PEDALES:  ecranPedales(t); break;
    case ECRAN_G:        ecranG(); break;
    case ECRAN_POSITION: ecranPosition(); break;
  }

  if (drapeau != 0) bordure(couleurDrapeau(drapeau));  // drapeau toujours en cours : simple rappel
}

void testCoins() {
  FastLED.clear();
  pixel(0, 0, CRGB::Red);
  pixel(LARGEUR - 1, 0, CRGB::Green);
  pixel(0, HAUTEUR - 1, CRGB::Blue);
  for (uint8_t i = 1; i < 5; i++) pixel(i, i, CRGB::White);  // petite diagonale depuis le coin rouge
  FastLED.show();
  delay(2000);
}

void setup() {
  Serial.begin(VITESSE_SERIE);
  pinMode(PIN_BOUTON, INPUT_PULLUP);

  FastLED.addLeds<WS2812B, PIN_MATRICE, GRB>(leds, NB_LEDS);
  FastLED.setBrightness(LUMINOSITE);
  FastLED.setMaxPowerInVoltsAndMilliamps(5, COURANT_MAX_MA);

  for (uint8_t i = 0; i < NB_TRAINEE; i++) { traineeX[i] = 7.5f; traineeY[i] = 7.5f; }
  champs[C_ECRAN] = -1;

  testCoins();
}

void loop() {
  unsigned long t = millis();
  bool nouveau = false;
  while (Serial.available() > 0) {
    if (recevoirOctet((char)Serial.read())) { nouveau = true; break; }
  }
  if (nouveau) traiterMessage(t);

  lireBouton(t);

  // FastLED.show() coupe les interruptions pendant environ 8 ms : les octets qui arrivent
  // à ce moment sont perdus. On affiche donc juste après un message complet, quand la
  // ligne est calme. Sans message récent, on affiche quand même à 30 images/s.
  if (nouveau || (t - dernierMessage > 60 && t - dernierAffichage >= 33)) {
    dessiner(t);
    FastLED.show();
    dernierAffichage = t;
  }
}
