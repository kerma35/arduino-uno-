# arduino-uno-Q

## Tendeur de filets pour simracing

Deux servos MG90S pilotés par SimHub pendant que tu roules sur Assetto Corsa. Ils tendent les filets au freinage et en virage. Deux versions du montage, avec la même formule SimHub :

| Version | Guide illustré | Programme |
| --- | --- | --- |
| Arduino Uno Rev3 + shield moteur TB6612/PCA9685 (V2) | https://claude.ai/artifact/VT9eSiaNRSdstRyenzpKT9 | `tendeur_filets_uno_r3/tendeur_filets_uno_r3.ino` |
| Arduino UNO Q | https://claude.ai/artifact/E8kGkR33oEX1xRGrxr3o9y | `tendeur_filets/tendeur_filets.ino` |

`simhub/message_mise_a_jour.js` est la formule JavaScript à coller dans SimHub › Custom serial devices › Update messages. Elle sert aux deux versions.

### Uno Rev3 + shield moteur V2

| Élément | Où |
| --- | --- |
| Shield | Enfiché sur l'Uno |
| Servo gauche | Prise SERVO 1 du shield, fil marron sur − |
| Servo droit | Prise SERVO 2 du shield, fil marron sur − |
| Câble USB | Uno → PC (SimHub) |
| Adaptateur 9 V 1 A | Jack de l'Uno, conseillé pour rouler |

Les prises servo du shield sont reliées à D9 et D10 et prennent leur courant sur le 5 V de l'Uno. Les puces PCA9685 et TB6612 ne servent pas : rien sur les borniers moteur, aucune bibliothèque Adafruit. Seule la bibliothèque Servo, fournie avec Arduino IDE, est utilisée.

### UNO Q

| De | Vers |
| --- | --- |
| Servo gauche, fil orange | D10 |
| Servo droit, fil orange | D9 |
| Fils rouges des servos | +5 V d'une alimentation séparée de 5 V 3 A |
| Fils marron des servos | − de l'alimentation |
| GND de l'UNO Q | − de l'alimentation (masse commune) |
| Condensateur 1000 µF | entre + et − de l'alimentation, bande côté − |

Ne relie pas le +5 V de l'alimentation à la broche 5V de l'UNO Q. Ses broches sont en 3,3 V. Bibliothèques : Servo ≥ 1.3.0 et Arduino_RouterBridge.

### Message série

SimHub envoie `L<0-100>R<0-100>` suivi d'un saut de ligne, à 115200 bauds. Sans message pendant 1 s, les filets se relâchent.
