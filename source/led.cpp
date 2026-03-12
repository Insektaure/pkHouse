#include "led.h"

#ifdef __SWITCH__
#include <switch.h>
#include <cstring>
#include <string>
#include <sys/stat.h>

// GPIO device code for the Switch Lite motherboard notification LED.
#define LITE_GPIO_DEVICE_CODE 0x35000065U
#define LITE_GPIO_ACCESS_MODE 3U

static bool sDisabled    = false;
static bool sHidsysReady = false;
static bool sIsLite      = false;
static bool sGpioReady   = false;
static GpioPadSession sGpioSession;

// Check basePath for noled.cfg; called before NRO path is known,
// so we use argv[0] from ledInitWithPath, or fall back to the
// default app directory.
static bool hasNoLedFile(const char* basePath) {
    std::string path = std::string(basePath) + "noled.cfg";
    struct stat st;
    return stat(path.c_str(), &st) == 0;
}

void ledInit() { ledInitWithPath("sdmc:/switch/pkHouse/"); }

void ledInitWithPath(const char* basePath) {
    if (hasNoLedFile(basePath)) {
        sDisabled = true;
        return;
    }

    // Detect Switch Lite (Hoag).
    setsysInitialize();
    SetSysProductModel model = SetSysProductModel_Invalid;
    setsysGetProductModel(&model);
    sIsLite = (model == SetSysProductModel_Hoag);
    setsysExit();

    if (sIsLite) {
        // Lite: use GPIO for the built-in LED.
        if (R_SUCCEEDED(gpioInitialize())) {
            Result rc = gpioOpenSession2(&sGpioSession, LITE_GPIO_DEVICE_CODE,
                                         LITE_GPIO_ACCESS_MODE);
            if (R_SUCCEEDED(rc)) {
                gpioPadSetDirection(&sGpioSession, GpioDirection_Output);
                gpioPadSetValue(&sGpioSession, GpioValue_Low);
                sGpioReady = true;
            }
        }
    }

    // Always try hidsys — Lite can still have external controllers.
    if (R_SUCCEEDED(hidsysInitialize()))
        sHidsysReady = true;
}

void ledExit() {
    if (sDisabled) return;
    if (sGpioReady) {
        gpioPadSetValue(&sGpioSession, GpioValue_Low);
        gpioPadClose(&sGpioSession);
        sGpioReady = false;
    }
    if (sIsLite)
        gpioExit();
    if (sHidsysReady) {
        hidsysExit();
        sHidsysReady = false;
    }
}

// --- hidsys (controller LEDs) ---

static int getPads(HidsysUniquePadId* out, int max) {
    static const HidNpadIdType types[] = {
        HidNpadIdType_Handheld,
        HidNpadIdType_No1, HidNpadIdType_No2, HidNpadIdType_No3,
        HidNpadIdType_No4, HidNpadIdType_No5, HidNpadIdType_No6,
        HidNpadIdType_No7,
    };
    int total = 0;
    for (auto type : types) {
        // Skip handheld on Lite — it has no hidsys-accessible LED.
        if (sIsLite && type == HidNpadIdType_Handheld)
            continue;
        HidsysUniquePadId ids[4];
        s32 count = 0;
        if (R_SUCCEEDED(hidsysGetUniquePadsFromNpad(type, ids, 4, &count))) {
            for (s32 i = 0; i < count && total < max; i++)
                out[total++] = ids[i];
        }
    }
    return total;
}

static void applyPattern(const HidsysNotificationLedPattern& pat) {
    if (!sHidsysReady) return;
    HidsysUniquePadId pads[16];
    int n = getPads(pads, 16);
    for (int i = 0; i < n; i++)
        hidsysSetNotificationLedPattern(&pat, pads[i]);
}

// --- GPIO (Switch Lite built-in LED) ---

static void liteOn() {
    if (sGpioReady)
        gpioPadSetValue(&sGpioSession, GpioValue_High);
}

static void liteOff() {
    if (sGpioReady)
        gpioPadSetValue(&sGpioSession, GpioValue_Low);
}

// --- Public API ---

void ledBlink() {
    if (sDisabled) return;
    // Controller LEDs: hardware-driven blink pattern.
    HidsysNotificationLedPattern pat{};
    pat.baseMiniCycleDuration = 0x04;
    pat.totalMiniCycles = 0x02;
    pat.totalFullCycles = 0x03;
    pat.startIntensity  = 0x00;

    pat.miniCycles[0].ledIntensity      = 0x0F;
    pat.miniCycles[0].transitionSteps   = 0x02;
    pat.miniCycles[0].finalStepDuration = 0x02;

    pat.miniCycles[1].ledIntensity      = 0x00;
    pat.miniCycles[1].transitionSteps   = 0x02;
    pat.miniCycles[1].finalStepDuration = 0x02;

    applyPattern(pat);

    // Lite built-in LED: simple on (turned off by ledOff after write completes).
    liteOn();
}

void ledOff() {
    if (sDisabled) return;
    // Controller LEDs: clear pattern.
    HidsysNotificationLedPattern pat{};
    applyPattern(pat);

    // Lite built-in LED: off.
    liteOff();
}

#else
// Stubs for non-Switch builds.
void ledInit() {}
void ledInitWithPath(const char*) {}
void ledExit() {}
void ledBlink() {}
void ledOff() {}
#endif
