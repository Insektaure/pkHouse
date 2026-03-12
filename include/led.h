#pragma once

// Controller notification LED helpers.
// Uses hidsysSetNotificationLedPattern to flash the player-indicator LEDs
// on Joy-Cons / Pro Controller during I/O operations.

void ledInit();
void ledInitWithPath(const char* basePath);
void ledExit();

// Start a short blink pattern on all connected controllers.
void ledBlink();

// Turn the notification LED off (restore to idle).
void ledOff();
