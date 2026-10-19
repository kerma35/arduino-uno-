// Matrice LED 16 x 16 – message de mise à jour SimHub
// Custom serial devices > Update messages > coche "Use JavaScript" et colle ce code.
// Envoie "$<22 valeurs séparées par des virgules>*<somme de contrôle>".

// ----- Réglages -----
var SEUIL_SHIFT     = 96;    // % du régime max où la jauge clignote pour passer le rapport
var UNITE           = 9.81;  // 9.81 si SimHub donne des m/s², 1 s'il donne déjà des G
var INVERSER_VIRAGE = false; // passe à true si le point G part du mauvais côté en virage
var DEPART_ACTIF    = true;  // feux de départ quand tu attends à l'arrêt sur la grille en course
var BOUTON_ECRAN    = '';    // bouton du volant pour changer d'écran, ex. 'JoystickPlugin.MonVolant_B05' ('' = bouton D2 de l'Arduino)
var ECRAN_DEPART    = 0;     // 0 tableau, 1 régime, 2 pédales, 3 G, 4 position
var NB_ECRANS       = 5;

function nombre(nom) {
  var v = $prop(nom);
  if (v === null || v === undefined) { return 0; }
  v = Number(v);
  return isNaN(v) ? 0 : v;
}

function borne(v, min, max) { return Math.round(Math.min(max, Math.max(min, v))); }

// Temps au tour en secondes, que SimHub le donne en nombre, en TimeSpan ou en texte "hh:mm:ss.fff".
function secondes(v) {
  if (v === null || v === undefined) { return 0; }
  if (typeof v === 'number') { return v; }
  if (v.TotalSeconds !== undefined) { return Number(v.TotalSeconds); }
  var morceaux = String(v).split(':');
  var total = 0;
  for (var i = 0; i < morceaux.length; i++) { total = total * 60 + (parseFloat(morceaux[i]) || 0); }
  return total;
}

function envoyer(valeurs) {
  var texte = valeurs.join(',');
  var somme = 0;
  for (var i = 0; i < texte.length; i++) { somme = (somme + texte.charCodeAt(i)) & 255; }
  return '$' + texte + '*' + somme + '\n';
}

// Écran choisi avec un bouton du volant (front montant)
var ecran = -1;
if (BOUTON_ECRAN) {
  if (root.ecran === undefined) { root.ecran = ECRAN_DEPART; root.bouton = false; }
  var appui = !!$prop(BOUTON_ECRAN);
  if (appui && !root.bouton) { root.ecran = (root.ecran + 1) % NB_ECRANS; }
  root.bouton = appui;
  ecran = root.ecran;
}

var maintenant = new Date();
var heure = maintenant.getHours(), minute = maintenant.getMinutes(), seconde = maintenant.getSeconds();

if (!$prop('DataCorePlugin.GameRunning')) {
  return envoyer([0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, heure, minute, seconde, ecran]);
}

var D = 'DataCorePlugin.GameData.';

var rpm    = nombre(D + 'Rpms');
var rpmMax = nombre(D + 'MaxRpm');
var regime = rpmMax > 0 ? rpm / rpmMax * 100 : 0;
var shift  = regime >= SEUIL_SHIFT ? 1 : 0;

var g = String($prop(D + 'Gear') || 'N').toUpperCase();
var rapport = g === 'R' ? -1 : (parseInt(g, 10) || 0);

var vitesse = nombre(D + 'SpeedKmh');
var tours   = nombre(D + 'CompletedLaps');

var drapeau = 0;
if      (nombre(D + 'Flag_Checkered')) { drapeau = 6; }
else if (nombre(D + 'Flag_Black'))     { drapeau = 4; }
else if (nombre(D + 'Flag_Orange'))    { drapeau = 7; }
else if (nombre(D + 'Flag_Yellow'))    { drapeau = 1; }
else if (nombre(D + 'Flag_Blue'))      { drapeau = 2; }
else if (nombre(D + 'Flag_White'))     { drapeau = 3; }
else if (nombre(D + 'Flag_Green'))     { drapeau = 5; }

var sway  = nombre(D + 'AccelerationSway') / UNITE;
var surge = nombre(D + 'AccelerationSurge') / UNITE;  // négatif au freinage
if (INVERSER_VIRAGE) { sway = -sway; }
var gx = -sway * 100;  // virage à droite : le point part à gauche
var gy = surge * 100;  // freinage : le point monte

var dernierTour  = secondes($prop(D + 'LastLapTime'));
var meilleurTour = secondes($prop(D + 'BestLapTime'));
var record = (dernierTour > 0 && Math.abs(dernierTour - meilleurTour) < 0.001) ? 1 : 0;

var session = String($prop(D + 'SessionTypeName') || '').toUpperCase();
var grille = (DEPART_ACTIF && session.indexOf('RACE') >= 0 && tours === 0 && vitesse < 2) ? 1 : 0;

var delta = nombre('PersistantTrackerPlugin.SessionBestLiveDeltaSeconds') * 100;

return envoyer([
  1,
  borne(regime, 0, 100),
  shift,
  rapport,
  borne(vitesse, 0, 999),
  borne(nombre(D + 'Position'), 0, 99),
  borne(nombre(D + 'Throttle'), 0, 100),
  borne(nombre(D + 'Brake'), 0, 100),
  borne(nombre(D + 'Clutch'), 0, 100),
  nombre(D + 'ABSActive') ? 1 : 0,
  nombre(D + 'TCActive') ? 1 : 0,
  drapeau,
  borne(gx, -500, 500),
  borne(gy, -500, 500),
  borne(tours, 0, 9999),
  record,
  grille,
  borne(delta, -999, 999),
  heure, minute, seconde,
  ecran
]);
