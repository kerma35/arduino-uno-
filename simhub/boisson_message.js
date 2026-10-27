// Boîtier à boisson – message de mise à jour SimHub
// Custom serial devices > Update messages > coche "Use JavaScript" et colle ce code.
// Envoie "B1" tant que le bouton du volant est appuyé, "B0" sinon.
// Le nombre après la virgule change à chaque envoi pour que SimHub renvoie le message en continu.

// ----- Réglage -----
// Appuie sur le bouton du volant et repère sa propriété dans "Available properties".
var BOUTON = 'JoystickPlugin.MonVolant_B05';

return ($prop(BOUTON) ? 'B1' : 'B0') + ',' + (Date.now() % 1000) + '\n';
