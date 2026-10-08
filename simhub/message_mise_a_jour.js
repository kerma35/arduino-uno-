// Tendeur de filets – message de mise à jour SimHub
// Custom serial devices > Update messages > coche "Use JavaScript" et colle ce code.
// Envoie "L<0-100>R<0-100>" : serrage du filet gauche et droit en %.

// ----- Réglages -----
var G_MAX           = 2.0;   // accélération (en G) qui donne 100 % de serrage
var GAIN_FREINAGE   = 1.0;   // force du serrage au freinage
var GAIN_VIRAGE     = 1.0;   // force du serrage en virage
var GAIN_ACCEL      = 0.0;   // serrage à l'accélération (0 = rien)
var TENSION_BASE    = 0;     // % de serrage permanent pendant que tu roules (0 à 20)
var INVERSER_VIRAGE = false; // passe à true si c'est le mauvais filet qui se serre en virage
var UNITE           = 9.81;  // 9.81 si SimHub donne des m/s², 1 s'il donne déjà des G

function borne(v) { return Math.round(Math.min(100, Math.max(0, v))); }

if (!$prop('DataCorePlugin.GameRunning')) {
  return 'L0R0\n';
}

var surge = ($prop('DataCorePlugin.GameData.AccelerationSurge') || 0) / UNITE; // négatif au freinage
var sway  = ($prop('DataCorePlugin.GameData.AccelerationSway')  || 0) / UNITE;
if (INVERSER_VIRAGE) { sway = -sway; }

var commun = Math.max(0, -surge) * GAIN_FREINAGE + Math.max(0, surge) * GAIN_ACCEL;
var gauche = TENSION_BASE + (commun + Math.max(0,  sway) * GAIN_VIRAGE) / G_MAX * 100;
var droite = TENSION_BASE + (commun + Math.max(0, -sway) * GAIN_VIRAGE) / G_MAX * 100;

return 'L' + borne(gauche) + 'R' + borne(droite) + '\n';
