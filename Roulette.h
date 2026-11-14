#ifndef ROULETTE_H
#define ROULETTE_H

// ROULETTE — tirage de la « super contrainte » du prochain Kino.
//
// Tout le manteau V1 passe d'une couleur à l'autre (rouge, jaune, bleu), vite au
// début puis de plus en plus lentement, s'arrête sur une couleur, la fait
// clignoter 4 fois, puis la garde fixe jusqu'au prochain lancement ou jusqu'au
// choix d'une autre animation.
//
// Équité : le nombre de pas est toujours le même (ROULETTE_STEPS). La couleur
// finale vaut donc (couleur de départ + ROULETTE_STEPS) % 3, et la couleur de
// départ est tirée avec le générateur matériel de l'ESP32 (esp_random). Chaque
// couleur a une chance sur trois, sans biais lié à la durée de la boucle.
//
// La partie logique (struct Roulette et fonctions roulette*) ne dépend ni
// d'Arduino ni de FastLED, pour pouvoir être testée sur ordinateur. Elle est
// identique à celle de la veste proto 2 (LedVestProto2) et de la télécommande
// Kino (dépôt GitHub looping-boy/Commander-Kino-,
// RouletteLogic.h) : si tu changes un réglage, change les deux.

#include <stdint.h>

#define ROULETTE_COLOR_COUNT 3
#define ROULETTE_FIRST_INTERVAL_MS 50.0f  // premier pas : 50 ms par couleur
#define ROULETTE_SLOWDOWN 1.1f            // chaque pas dure 10 % de plus que le précédent
#define ROULETTE_STEPS 26                 // 26 pas : 50 ms -> 541 ms, environ 5,5 s en tout
#define ROULETTE_STOP_HOLD_MS 650         // la couleur gagnante reste allumée avant de clignoter
#define ROULETTE_BLINKS 4                 // nombre de clignotements
#define ROULETTE_BLINK_MS 230             // durée d'une demi-période de clignotement

enum RoulettePhase : uint8_t {
  ROULETTE_IDLE,
  ROULETTE_SPINNING,
  ROULETTE_BLINKING,
  ROULETTE_DONE
};

struct Roulette {
  RoulettePhase phase;
  uint8_t color;        // 0 rouge, 1 jaune, 2 bleu
  uint8_t stepsDone;
  uint8_t togglesLeft;
  bool lit;             // false pendant les phases éteintes du clignotement
  float interval;       // durée du pas en cours, en ms
  uint32_t nextAt;      // date du prochain changement, en ms (millis)
};

inline bool rouletteIsBusy(const Roulette &r) {
  return r.phase == ROULETTE_SPINNING || r.phase == ROULETTE_BLINKING;
}

inline void rouletteStart(Roulette &r, uint32_t now, uint32_t randomValue) {
  r.phase = ROULETTE_SPINNING;
  r.color = randomValue % ROULETTE_COLOR_COUNT;
  r.stepsDone = 0;
  r.togglesLeft = 0;
  r.lit = true;
  r.interval = ROULETTE_FIRST_INTERVAL_MS;
  r.nextAt = now + (uint32_t)(r.interval + 0.5f);
}

// À appeler à chaque tour de boucle. Les dates sont calculées à partir de la
// date prévue, pas de l'heure d'appel : un tour de boucle en retard ne change ni
// la durée totale ni le nombre de pas, donc ni le résultat.
inline void rouletteUpdate(Roulette &r, uint32_t now) {
  while (rouletteIsBusy(r) && (int32_t)(now - r.nextAt) >= 0) {
    if (r.phase == ROULETTE_SPINNING) {
      r.color = (r.color + 1) % ROULETTE_COLOR_COUNT;
      r.stepsDone++;
      if (r.stepsDone >= ROULETTE_STEPS) {
        r.phase = ROULETTE_BLINKING;
        r.togglesLeft = ROULETTE_BLINKS * 2;
        r.nextAt += ROULETTE_STOP_HOLD_MS;
      } else {
        r.interval *= ROULETTE_SLOWDOWN;
        r.nextAt += (uint32_t)(r.interval + 0.5f);
      }
    } else {  // ROULETTE_BLINKING
      r.lit = !r.lit;
      r.togglesLeft--;
      if (r.togglesLeft == 0) {
        r.phase = ROULETTE_DONE;
        r.lit = true;
      } else {
        r.nextAt += ROULETTE_BLINK_MS;
      }
    }
  }
}

// Lancement par la télécommande Kino : c'est elle qui tire la couleur de
// départ, l'affiche sur sa roue et l'envoie à la veste (ESP-NOW, type 11). Le
// message part ROULETTE_RESENDS fois, à ROULETTE_RESEND_GAP_MS d'intervalle.
// Sa valeur vaut départ + 3 x numéro de répétition : si la veste rate les
// premiers, elle sait de combien elle est en retard et se recale sur l'écran.
#define ROULETTE_RESENDS 4
#define ROULETTE_RESEND_GAP_MS 60

inline uint8_t rouletteEncodeStart(uint8_t start, uint8_t resend) {
  return start % ROULETTE_COLOR_COUNT + ROULETTE_COLOR_COUNT * resend;
}

inline uint8_t rouletteDecodeStart(uint8_t value) {
  return value % ROULETTE_COLOR_COUNT;
}

inline uint32_t rouletteDecodeLateness(uint8_t value) {
  return (uint32_t)(value / ROULETTE_COLOR_COUNT) * ROULETTE_RESEND_GAP_MS;
}

#ifdef ARDUINO
// ----- Partie manteau V1 (FastLED, 4 bandes) -----

// Couleurs franches, bien distinctes de loin. Le jaune tire un peu vers l'or :
// sur les WS2812, 255/255/0 paraît verdâtre.
const CRGB ROULETTE_COLORS[ROULETTE_COLOR_COUNT] = {
  CRGB(255, 0, 0),    // rouge
  CRGB(255, 190, 0),  // jaune
  CRGB(0, 0, 255)     // bleu
};

#define ROULETTE_RANDOM_START 0xFF

Roulette roulette = {};
volatile bool rouletteRequested = false;
volatile uint8_t rouletteRequestedValue = ROULETTE_RANDOM_START;

// Appelée par l'ESP-NOW (télécommande). Le lancement
// réel se fait dans loop(), pour ne pas toucher aux LED depuis un callback.
// Sans valeur : le manteau tire lui-même la couleur de départ.
void requestRoulette() {
  rouletteRequestedValue = ROULETTE_RANDOM_START;
  rouletteRequested = true;
}

// Avec valeur (télécommande Kino) : départ et retard encodés par rouletteEncodeStart.
void requestRouletteFrom(uint8_t value) {
  rouletteRequestedValue = value;
  rouletteRequested = true;
}

// Lance la roulette si un appui est arrivé. Un appui pendant qu'elle tourne ou
// clignote est ignoré (double appui, message répété par la télécommande).
void handleRouletteRequest() {
  if (!rouletteRequested) return;
  rouletteRequested = false;
  uint8_t value = rouletteRequestedValue;
  if (pattern == ANIM_ROULETTE && rouletteIsBusy(roulette)) return;
  uint32_t now = millis();
  if (value == ROULETTE_RANDOM_START) {
    rouletteStart(roulette, now, esp_random());
  } else {
    rouletteStart(roulette, now - rouletteDecodeLateness(value), rouletteDecodeStart(value));
  }
  pattern = ANIM_ROULETTE;
}

bool isRouletteSpinning() {
  return pattern == ANIM_ROULETTE && rouletteIsBusy(roulette);
}

// Longueur de chaque bande (voir setup() : leds[0] et leds[1] = 451, leds[2] et leds[3] = 477).
const uint16_t ROULETTE_STRIP_LENGTHS[NUM_STRIPS] = {451, 451, 477, 477};

void animRoulette(uint32_t now) {
  rouletteUpdate(roulette, now);
  CRGB color = (roulette.phase != ROULETTE_IDLE && roulette.lit) ? ROULETTE_COLORS[roulette.color] : CRGB::Black;
  // Le V1 ne règle pas FastLED.setBrightness à chaque tour : les animations
  // dosent leurs couleurs avec « brightness » (molette ou télécommande).
  color.nscale8_video(brightness);
  for (uint8_t strip = 0; strip < NUM_STRIPS; strip++) {
    fill_solid(leds[strip], ROULETTE_STRIP_LENGTHS[strip], color);
  }
}
#endif  // ARDUINO

#endif  // ROULETTE_H
