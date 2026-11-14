#ifndef ESP_RECEIVER_H
#define ESP_RECEIVER_H

#include "Constant.h"
#include <esp_now.h>
#include <WiFi.h>
#include <esp_wifi.h>

const uint8_t MESSAGE_TYPE_BRIGHTNESS = 0;
const uint8_t MESSAGE_TYPE_HUE = 1;
const uint8_t MESSAGE_TYPE_BPM = 2;
const uint8_t MESSAGE_TYPE_PATTERN = 3;
const uint8_t MESSAGE_TYPE_LIGHT_DEVICE = 4;
const uint8_t MESSAGE_TYPE_FORCE_BRIGHTNESS = 5; 

const uint8_t MESSAGE_TWEAK_QUANTITY = 6;
const uint8_t MESSAGE_TWEAK_BPM = 7;    
const uint8_t MESSAGE_TWEAK_COLOR = 8;     
const uint8_t MESSAGE_TWEAK_PHASE = 9;    
const uint8_t MESSAGE_TYPE_ROULETTE = 10;       // Lance la roulette, le manteau tire la couleur (valeur ignorée)
const uint8_t MESSAGE_TYPE_ROULETTE_FROM = 11;  // Lance la roulette depuis la couleur tirée par la télécommande Kino

void requestRoulette();                   // Roulette.h
void requestRouletteFrom(uint8_t value);  // Roulette.h

typedef struct Message {
  uint8_t message_type;   
  uint8_t value;          
} Message;

Message message;

void data_received(const uint8_t *mac, const uint8_t *incomingData, int len) {
    if (len < (int) sizeof(message)) {
        return;
    }
    memcpy(&message, incomingData, sizeof(message));

    switch (message.message_type) {
        case MESSAGE_TYPE_BRIGHTNESS:
            brightness = message.value;
            break;
        case MESSAGE_TYPE_HUE:
            hue = message.value;
            break;
        case MESSAGE_TYPE_BPM:
            bpm = message.value;
            break;
        case MESSAGE_TYPE_PATTERN:
            if (message.value == ANIM_ROULETTE) {
                requestRoulette();
            } else {
                pattern = message.value;
            }
            break;
        case MESSAGE_TYPE_ROULETTE:
            requestRoulette();
            break;
        case MESSAGE_TYPE_ROULETTE_FROM:
            requestRouletteFrom(message.value);
            break;
        case MESSAGE_TYPE_LIGHT_DEVICE:
            // This won't be used for the coat code, because the ESP32S3 board is attached to the coat, so it can't connect to a WristBand or Feet or HeadBand.
            break;
        case MESSAGE_TWEAK_QUANTITY:
            tweakQuantity = message.value;
            break;
        case MESSAGE_TWEAK_BPM:
            tweakBPM = message.value;
            break;
        case MESSAGE_TWEAK_COLOR:  
            tweakColor = message.value;
            break;
        case MESSAGE_TWEAK_PHASE: 
            tweakPhase = message.value;
            break;
        case MESSAGE_TYPE_FORCE_BRIGHTNESS: 
            isForcedBrightness = (message.value == 1) ? true : false;
            break;
        default:
            break;
    }
}

#endif 