# arduino-uno-Q

## Tendeur de filets pour simracing

Deux servos MG90S sur une Arduino UNO Q, pilotés par SimHub pendant que tu roules sur Assetto Corsa. Ils tendent les filets au freinage et en virage.

Guide complet avec schémas : https://claude.ai/artifact/E8kGkR33oEX1xRGrxr3o9y

| Fichier | Rôle |
| --- | --- |
| `tendeur_filets/tendeur_filets.ino` | Programme de l'UNO Q (Arduino IDE 2, bibliothèques Servo ≥ 1.3.0 et Arduino_RouterBridge) |
| `simhub/message_mise_a_jour.js` | Formule JavaScript pour SimHub › Custom serial devices › Update messages |

### Câblage en bref

| De | Vers |
| --- | --- |
| Servo gauche, fil orange | D10 |
| Servo droit, fil orange | D9 |
| Fils rouges des servos | +5 V d'une alimentation séparée de 5 V 3 A |
| Fils marron des servos | − de l'alimentation |
| GND de l'UNO Q | − de l'alimentation (masse commune) |
| Condensateur 1000 µF | entre + et − de l'alimentation, bande côté − |

Ne relie pas le +5 V de l'alimentation à la broche 5V de l'UNO Q. Ses broches sont en 3,3 V.

### Message série

SimHub envoie `L<0-100>R<0-100>` suivi d'un saut de ligne, à 115200 bauds. Le programme écoute à la fois le port USB-C (`Monitor`) et les broches D0/D1 (`Serial1`, pour un adaptateur USB-TTL 3,3 V). Sans message pendant 1 s, les filets se relâchent.
