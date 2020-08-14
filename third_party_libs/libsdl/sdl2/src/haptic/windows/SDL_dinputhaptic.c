/*
  Simple DirectMedia Layer
  Copyright (C) 1997-2020 Sam Lantinga <slouken@libsdl.org>

  This software is provided 'as-is', without any express or implied
  warranty.  In no event will the authors be held liable for any damages
  arising from the use of this software.

  Permission is granted to anyone to use this software for any purpose,
  including commercial applications, and to alter it and redistribute it
  freely, subject to the following restrictions:

  1. The origin of this software must not be misrepresented; you must not
     claim that you wrote the original software. If you use this software
     in a product, an acknowledgment in the product documentation would be
     appreciated but is not required.
  2. Altered source versions must be plainly marked as such, and must not be
     misrepresented as being the original software.
  3. This notice may not be removed or altered from any source distribution.
*/
#include "../../SDL_internal.h"

#include "SDL_error.h"
#include "SDL_haptic.h"
#include "../SDL_syshaptic.h"

#if SDL_HAPTIC_DINPUT

#include "SDL_stdinc.h"
#include "SDL_timer.h"
#include "SDL_windowshaptic_c.h"
#include "SDL_dinputhaptic_c.h"
#include "../../joystick/windows/SDL_windowsjoystick_c.h"

/*
 * External stuff.
 */
extern HWND SDL_HelperWindow;


/*
 * Internal stuff.
 */
static SDL_bool coinitialized = SDL_FALSE;
static LPDIRECTINPUT8 dinput = NULL;


/*
 * Like SDL_SetError but for DX error codes.
 */
static int
DI_SetError(const char *str, HRESULT err)
{
    /*
       SDL_SetError("Haptic: %s - %s: %s", str,
       DXGetErrorString8A(err), DXGetErrorDescription8A(err));
     */
    return SDL_SetError("Haptic error %s", str);
}

/*
 * Callback to find the haptic devices.
 */
static BOOL CALLBACK
EnumHapticsCallback(const DIDEVICEINSTANCE * pdidInstance, VOID * pContext)
{
    (void) pContext;
    SDL_DINPUT_MaybeAddDevice(pdidInstance);
    return DIENUM_CONTINUE;  /* continue enumerating */
}

int
SDL_DINPUT_HapticInit(void)
{
    HRESULT ret;
    HINSTANCE instance;

    if (dinput != NULL) {       /* Already open. */
        return SDL_SetError("Haptic: SubSystem already open.");
    }

    ret = WIN_CoInitialize();
    if (FAILED(ret)) {
        return DI_SetError("Coinitialize", ret);
    }

    coinitialized = SDL_TRUE;

    ret = CoCreateInstance(&CLSID_DirectInput8, NULL, CLSCTX_INPROC_SERVER,
        &IID_IDirectInput8, (LPVOID)& dinput);
    if (FAILED(ret)) {
        SDL_SYS_HapticQuit();
        return DI_SetError("CoCreateInstance", ret);
    }

    /* Because we used CoCreateInstance, we need to Initialize it, first. */
    instance = GetModuleHandle(NULL);
    if (instance == NULL) {
        SDL_SYS_HapticQuit();
        return SDL_SetError("GetModuleHandle() failed with error code %lu.",
            GetLastError());
    }
    ret = IDirectInput8_Initialize(dinput, instance, DIRECTINPUT_VERSION);
    if (FAILED(ret)) {
        SDL_SYS_HapticQuit();
        return DI_SetError("Initializing DirectInput device", ret);
    }

    /* Look for haptic devices. */
    ret = IDirectInput8_EnumDevices(dinput,
        0,
        EnumHapticsCallback,
        NULL,
        DIEDFL_FORCEFEEDBACK |
        DIEDFL_ATTACHEDONLY);
    if (FAILED(ret)) {
        SDL_SYS_HapticQuit();
        return DI_SetError("Enumerating DirectInput devices", ret);
    }
    return 0;
}

int
SDL_DINPUT_MaybeAddDevice(const DIDEVICEINSTANCE * pdidInstance)
{
    HRESULT ret;
    LPDIRECTINPUTDEVICE8 device;
    const DWORD needflags = DIDC_ATTACHED | DIDC_FORCEFEEDBACK;
    DIDEVCAPS capabilities;
    SDL_hapticlist_item *item = NULL;

    if (dinput == NULL) {
        return -1;  /* not initialized. We'll pick these up on enumeration if we init later. */
    }

    /* Make sure we don't already have it */
    for (item = SDL_hapticlist; item; item = item->next) {
        if ((!item->bXInputHaptic) && (SDL_memcmp(&item->instance, pdidInstance, sizeof(*pdidInstance)) == 0)) {
            return -1;  /* Already added */
        }
    }

    /* Open the device */
    ret = IDirectInput8_CreateDevice(dinput, &pdidInstance->guidInstance, &device, NULL);
    if (FAILED(ret)) {
        /* DI_SetError("Creating DirectInput device",ret); */
        return -1;
    }

    /* Get capabilities. */
    SDL_zero(capabilities);
    capabilities.dwSize = sizeof(DIDEVCAPS);
    ret = IDirectInputDevice8_GetCapabilities(device, &capabilities);
    IDirectInputDevice8_Release(device);
    if (FAILED(ret)) {
        /* DI_SetError("Getting device capabilities",ret); */
        return -1;
    }

    if ((capabilities.dwFlags & needflags) != needflags) {
        return -1;  /* not a device we can use. */
    }

    item = (SDL_hapticlist_item *)SDL_calloc(1, sizeof(SDL_hapticlist_item));
    if (item == NULL) {
        return SDL_OutOfMemory();
    }

    item->name = WIN_StringToUTF8(pdidInstance->tszProductName);
    if (!item->name) {
        SDL_free(item);
        return -1;
    }

    /* Copy the instance over, useful for creating devices. */
    SDL_memcpy(&item->instance, pdidInstance, sizeof(DIDEVICEINSTANCE));
    SDL_memcpy(&item->capabilities, &capabilities, sizeof(capabilities));

    return SDL_SYS_AddHapticDevice(item);
}

int
SDL_DINPUT_MaybeRemoveDevice(const DIDEVICEINSTANCE * pdidInstance)
{
    SDL_hapticlist_item *item;
    SDL_hapticlist_item *prev = NULL;

    if (dinput == NULL) {
        return -1;  /* not initialized, ignore this. */
    }

    for (item = SDL_hapticlist; item != NULL; item = item->next) {
        if (!item->bXInputHaptic && SDL_memcmp(&item->instance, pdidInstance, sizeof(*pdidInstance)) == 0) {
            /* found it, remove it. */
            return SDL_SYS_RemoveHapticDevice(prev, item);
        }
        prev = item;
    }
    return -1;
}

/*
 * Callback to get supported axes.
 */
static BOOL CALLBACK
DI_DeviceObjectCallback(LPCDIDEVICEOBJECTINSTANCE dev, LPVOID pvRef)
{
    SDL_Haptic *haptic = (SDL_Haptic *) pvRef;

    if ((dev->dwType & DIDFT_AXIS) && (dev->dwFlags & DIDOI_FFACTUATOR)) {
        const GUID *guid = &dev->guidType;
        DWORD offset = 0;
        if (WIN_IsEqualGUID(guid, &GUID_XAxis)) {
            offset = DIJOFS_X;
        } else if (WIN_IsEqualGUID(guid, &GUID_YAxis)) {
            offset = DIJOFS_Y;
        } else if (WIN_IsEqualGUID(guid, &GUID_ZAxis)) {
            offset = DIJOFS_Z;
        } else if (WIN_IsEqualGUID(guid, &GUID_RxAxis)) {
            offset = DIJOFS_RX;
        } else if (WIN_IsEqualGUID(guid, &GUID_RyAxis)) {
            offset = DIJOFS_RY;
        } else if (WIN_IsEqualGUID(guid, &GUID_RzAxis)) {
            offset = DIJOFS_RZ;
        } else {
            return DIENUM_CONTINUE;   /* can't use this, go on. */
        }

        haptic->hwdata->axes[haptic->naxes] = offset;
        haptic->naxes++;

        /* Currently using the artificial limit of 3 axes. */
        if (haptic->naxes >= 3) {
            return DIENUM_STOP;
        }
    }

    return DIENUM_CONTINUE;
}

/*
 * Callback to get all supported effects.
 */
#define EFFECT_TEST(e,s)               \
if (WIN_IsEqualGUID(&pei->guid, &(e)))   \
   haptic->supported |= (s)
static BOOL CALLBACK
DI_EffectCallback(LPCDIEFFECTINFO pei, LPVOID pv)
{
    /* Prepare the haptic device. */
    SDL_Haptic *haptic = (SDL_Haptic *) pv;

    /* Get supported. */
    EFFECT_TEST(GUID_Spring, SDL_HAPTIC_SPRING);
    EFFECT_TEST(GUID_Damper, SDL_HAPTIC_DAMPER);
    EFFECT_TEST(GUID_Inertia, SDL_HAPTIC_INERTIA);
    EFFECT_TEST(GUID_Friction, SDL_HAPTIC_FRICTION);
    EFFECT_TEST(GUID_ConstantForce, SDL_HAPTIC_CONSTANT);
    EFFECT_TEST(GUID_CustomForce, SDL_HAPTIC_CUSTOM);
    EFFECT_TEST(GUID_Sine, SDL_HAPTIC_SINE);
    /* !!! FIXME: put this back when we have more bits in 2.1 */
    /* EFFECT_TEST(GUID_Square, SDL_HAPTIC_SQUARE); */
    EFFECT_TEST(GUID_Triangle, SDL_HAPTIC_TRIANGLE);
    EFFECT_TEST(GUID_SawtoothUp, SDL_HAPTIC_SAWTOOTHUP);
    EFFECT_TEST(GUID_SawtoothDown, SDL_HAPTIC_SAWTOOTHDOWN);
    EFFECT_TEST(GUID_RampForce, SDL_HAPTIC_RAMP);

    /* Check for more. */
    return DIENUM_CONTINUE;
}

/*
 * Opens the haptic device.
 *
 *    Steps:
 *       - Set cooperative level.
 *       - Set data format.
 *       - Acquire exclusiveness.
 *       - Reset actuators.
 *       - Get supported features.
 */
static int
SDL_DINPUT_HapticOpenFromDevice(SDL_Haptic * haptic, LPDIRECTINPUTDEVICE8 device8, SDL_bool is_joystick)
{
    HRESULT ret;
    DIPROPDWORD dipdw;

    /* Allocate the hwdata */
    haptic->hwdata = (struct haptic_hwdata *)SDL_malloc(sizeof(*haptic->hwdata));
    if (haptic->hwdata == NULL) {
        return SDL_OutOfMemory();
    }
    SDL_memset(haptic->hwdata, 0, sizeof(*haptic->hwdata));

    /* We'll use the device8 from now on. */
    haptic->hwdata->device = device8;
    haptic->hwdata->is_joystick = is_joystick;

    /* !!! FIXME: opening a haptic device here first will make an attempt to
       !!! FIXME:  SDL_JoystickOpen() that same device fail later, since we
       !!! FIXME:  have it open in exclusive mode. But this will allow
       !!! FIXME:  SDL_JoystickOpen() followed by SDL_HapticOpenFromJoystick()
       !!! FIXME:  to work, and that's probably the common case. Still,
       !!! FIXME:  ideally, We need to unify the opening code. */

    if (!is_joystick) {  /* if is_joystick, we already set this up elsewhere. */
        /* Grab it exclusively to use force feedback stuff. */
        ret = IDirectInputDevice8_SetCooperativeLevel(haptic->hwdata->device,
                                                      SDL_HelperWindow,
                                                      DISCL_EXCLUSIVE |
                                                      DISCL_BACKGROUND);
        if (FAILED(ret)) {
            DI_SetError("Setting cooperative level to exclusive", ret);
            goto acquire_err;
        }

        /* Set data format. */
        ret = IDirectInputDevice8_SetDataFormat(haptic->hwdata->device,
                                                &SDL_c_dfDIJoystick2);
        if (FAILED(ret)) {
            DI_SetError("Setting data format", ret);
            goto acquire_err;
        }


        /* Acquire the device. */
        ret = IDirectInputDevice8_Acquire(haptic->hwdata->device);
        if (FAILED(ret)) {
            DI_SetError("Acquiring DirectInput device", ret);
            goto acquire_err;
        }
    }

    /* Get number of axes. */
    ret = IDirectInputDevice8_EnumObjects(haptic->hwdata->device,
                                          DI_DeviceObjectCallback,
                                          haptic, DIDFT_AXIS);
    if (FAILED(ret)) {
        DI_SetError("Getting device axes", ret);
        goto acquire_err;
    }

    /* Reset all actuators - just in case. */
    ret = IDirectInputDevice8_SendForceFeedbackCommand(haptic->hwdata->device,
                                                       DISFFC_RESET);
    if (FAILED(ret)) {
        DI_SetError("Resetting device", ret);
        goto acquire_err;
    }

    /* Enabling actuators. */
    ret = IDirectInputDevice8_SendForceFeedbackCommand(haptic->hwdata->device,
                                                       DISFFC_SETACTUATORSON);
    if (FAILED(ret)) {
        DI_SetError("Enabling actuators", ret);
        goto acquire_err;
    }

    /* Get supported effects. */
    ret = IDirectInputDevice8_EnumEffects(haptic->hwdata->device,
                                          DI_EffectCallback, haptic,
                                          DIEFT_ALL);
    if (FAILED(ret)) {
        DI_SetError("Enumerating supported effects", ret);
        goto acquire_err;
    }
    if (haptic->supported == 0) {       /* Error since device supports nothing. */
        SDL_SetError("Haptic: Internal error on finding supported effects.");
        goto acquire_err;
    }

    /* Check autogain and autocenter. */
    dipdw.diph.dwSize = sizeof(DIPROPDWORD);
    dipdw.diph.dwHeaderSize = sizeof(DIPROPHEADER);
    dipdw.diph.dwObj = 0;
    dipdw.diph.dwHow = DIPH_DEVICE;
    dipdw.dwData = 10000;
    ret = IDirectInputDevice8_SetProperty(haptic->hwdata->device,
                                          DIPROP_FFGAIN, &dipdw.diph);
    if (!FAILED(ret)) {         /* Gain is supported. */
        haptic->supported |= SDL_HAPTIC_GAIN;
    }
    dipdw.diph.dwObj = 0;
    dipdw.diph.dwHow = DIPH_DEVICE;
    dipdw.dwData = DIPROPAUTOCENTER_OFF;
    ret = IDirectInputDevice8_SetProperty(haptic->hwdata->device,
                                          DIPROP_AUTOCENTER, &dipdw.diph);
    if (!FAILED(ret)) {         /* Autocenter is supported. */
        haptic->supported |= SDL_HAPTIC_AUTOCENTER;
    }

    /* Status is always supported. */
    haptic->supported |= SDL_HAPTIC_STATUS | SDL_HAPTIC_PAUSE;

    /* Check maximum effects. */
    haptic->neffects = 128;     /* This is not actually supported as thus under windows,
                                   there is no way to tell the number of EFFECTS that a
                                   device can hold, so we'll just use a "random" number
                                   instead and put warnings in SDL_haptic.h */
    haptic->nplaying = 128;     /* Even more impossible to get this then neffects. */

    /* Prepare effects memory. */
    haptic->effects = (struct haptic_effect *)
        SDL_malloc(sizeof(struct haptic_effect) * haptic->neffects);
    if (haptic->effects == NULL) {
        SDL_OutOfMemory();
        goto acquire_err;
    }
    /* Clear the memory */
    SDL_memset(haptic->effects, 0,
               sizeof(struct haptic_effect) * haptic->neffects);

    return 0;

    /* Error handling */
  acquire_err:
    IDirectInputDevice8_Unacquire(haptic->hwdata->device);
    return -1;
}

int
SDL_DINPUT_HapticOpen(SDL_Haptic * haptic, SDL_hapticlist_item *item)
{
    HRESULT ret;
    LPDIRECTINPUTDEVICE8 device;
    LPDIRECTINPUTDEVICE8 device8;

    /* Open the device */
    ret = IDirectInput8_CreateDevice(dinput, &item->instance.guidInstance,
        &device, NULL);
    if (FAILED(ret)) {
        DI_SetError("Creating DirectInput device", ret);
        return -1;
    }

    /* Now get the IDirectInputDevice8 interface, instead. */
    ret = IDirectInputDevice8_QueryInterface(device,
        &IID_IDirectInputDevice8,
        (LPVOID *)&device8);
    /* Done with the temporary one now. */
    IDirectInputDevice8_Release(device);
    if (FAILED(ret)) {
        DI_SetError("Querying DirectInput interface", ret);
        return -1;
    }

    if (SDL_DINPUT_HapticOpenFromDevice(haptic, device8, SDL_FALSE) < 0) {
        IDirectInputDevice8_Release(device8);
        return -1;
    }
    return 0;
}

int
SDL_DINPUT_JoystickSameHaptic(SDL_Haptic * haptic, SDL_Joystick * joystick)
{
    HRESULT ret;
    DIDEVICEINSTANCE hap_instance, joy_instance;

    hap_instance.dwSize = sizeof(DIDEVICEINSTANCE);
    joy_instance.dwSize = sizeof(DIDEVICEINSTANCE);

    /* Get the device instances. */
    ret = IDirectInputDevice8_GetDeviceInfo(haptic->hwdata->device,
        &hap_instance);
    if (FAILED(ret)) {
        return 0;
    }
    ret = IDirectInputDevice8_GetDeviceInfo(joystick->hwdata->InputDevice,
        &joy_instance);
    if (FAILED(ret)) {
        return 0;
    }

    return WIN_IsEqualGUID(&hap_instance.guidInstance, &joy_instance.guidInstance);
}

int
SDL_DINPUT_HapticOpenFromJoystick(SDL_Haptic * haptic, SDL_Joystick * joystick)
{
    SDL_hapticlist_item *item;
    int index = 0;
    HRESULT ret;
    DIDEVICEINSTANCE joy_instance;

    joy_instance.dwSize = sizeof(DIDEVICEINSTANCE);
    ret = IDirectInputDevice8_GetDeviceInfo(joystick->hwdata->InputDevice, &joy_instance);
    if (FAILED(ret)) {
        return -1;
    }

    /* Since it comes from a joystick we have to try to match it with a haptic device on our haptic list. */
    for (item = SDL_hapticlist; item != NULL; item = item->next) {
        if (!item->bXInputHaptic && WIN_IsEqualGUID(&item->instance.guidInstance, &joy_instance.guidInstance)) {
            haptic->index = index;
            return SDL_DINPUT_HapticOpenFromDevice(haptic, joystick->hwdata->InputDevice, SDL_TRUE);
        }
        ++index;
    }

    SDL_SetError("Couldn't find joystick in haptic device list");
    return -1;
}

void
SDL_DINPUT_HapticClose(SDL_Haptic * haptic)
{
    IDirectInputDevice8_Unacquire(haptic->hwdata->device);

    /* Only release if isn't grabbed by a joystick. */
    if (haptic->hwdata->is_joystick == 0) {
        IDirectInputDevice8_Release(haptic->hwdata->device);
    }
}

void
SDL_DINPUT_HapticQuit(void)
{
    if (dinput != NULL) {
        IDirectInput8_Release(dinput);
        dinput = NULL;
    }

    if (coinitialized) {
        WIN_CoUninitialize();
        coinitialized = SDL_FALSE;
    }
}

/*
 * Converts an SDL trigger button to an DIEFFECT trigger button.
 */
static DWORD
DIGetTriggerButton(Uint16 button)
{
    DWORD dwTriggerButton;

    dwTriggerButton = DIEB_NOTRIGGER;

    if (button != 0) {
        dwTriggerButton = DIJOFS_BUTTON(button - 1);
    }

    return dwTriggerButton;
}


/*
 * Sets the direction.
 */
static int
SDL_SYS_SetDirection(DIEFFECT * effect, SDL_HapticDirection * dir, int naxes)
{
    LONG *rglDir;

    /* Handle no axes a part. */
    if (naxes == 0) {
        effect->dwFlags |= DIEFF_SPHERICAL;     /* Set as default. */
        effect->rglDirection = NULL;
        return 0;
    }

    /* Has axes. */
    rglDir = SDL_malloc(sizeof(LONG) * naxes);
    if (rglDir == NULL) {
        return SDL_OutOfMemory();
    }
    SDL_memset(rglDir, 0, sizeof(LONG) * naxes);
    effect->rglDirection = rglDir;

    switch (dir->type) {
    case SDL_HAPTIC_POLAR:
        effect->dwFlags |= DIEFF_POLAR;
        rglDir[0] = dir->dir[0];
        return 0;
    case SDL_HAPTIC_CARTESIAN:
        effect->dwFlags |= DIEFF_CARTESIAN;
        rglDir[0] = dir->dir[0];
        if (naxes > 1)
            rglDir[1] = dir->dir[1];
        if (naxes > 2)
            rglDir[2] = dir->dir[2];
        return 0;
    case SDL_HAPTIC_SPHERICAL:
        effect->dwFlags |= DIEFF_SPHERICAL;
        rglDir[0] = dir->dir[0];
        if (naxes > 1)
            rglDir[1] = dir->dir[1];
        if (naxes > 2)
            rglDir[2] = dir->dir[2];
        return 0;

    default:
        return SDL_SetError("Haptic: Unknown direction type.");
    }
}

/* Clamps and converts. */
#define CCONVERT(x)   (((x) > 0x7FFF) ? 10000 : ((x)*10000) / 0x7FFF)
/* Just converts. */
#define CONVERT(x)    (((x)*10000) / 0x7FFF)
/*
 * Creates the DIEFFECT from a SDL_HapticEffect.
 */
static int
SDL_SYS_ToDIEFFECT(SDL_Haptic * haptic, DIEFFECT * dest,
                   SDL_HapticEffect * src)
{
    int i;
    DICONSTANTFORCE *constant;
    DIPERIODIC *periodic;
    DICONDITION *condition;     /* Actually an array of conditions - one per axis. */
    DIRAMPFORCE *ramp;
    DICUSTOMFORCE *custom;
    DIENVELOPE *envelope;
    SDL_HapticConstant *hap_constant;
    SDL_HapticPeriodic *hap_periodic;
    SDL_HapticCondition *hap_condition;
    SDL_HapticRamp *hap_ramp;
    SDL_HapticCustom *hap_custom;
    DWORD *axes;

    /* Set global stuff. */
    SDL_memset(dest, 0, sizeof(DIEFFECT));
    dest->dwSize = sizeof(DIEFFECT);    /* Set the structure size. */
    dest->dwSamplePeriod = 0;   /* Not used by us. */
    dest->dwGain = 10000;       /* Gain is set globally, not locally. */
    dest->dwFlags = DIEFF_OBJECTOFFSETS;        /* Seems obligatory. */

    /* Envelope. */
    envelope = SDL_malloc(sizeof(DIENVELOPE));
    if (envelope == NULL) {
        return SDL_OutOfMemory();
    }
    SDL_memset(envelope, 0, sizeof(DIENVELOPE));
    dest->lpEnvelope = envelope;
    envelope->dwSize = sizeof(DIENVELOPE);      /* Always should be this. */

    /* Axes. */
    dest->cAxes = haptic->naxes;
    if (dest->cAxes > 0) {
        axes = SDL_malloc(sizeof(DWORD) * dest->cAxes);
        if (axes == NULL) {
            return SDL_OutOfMemory();
        }
        axes[0] = haptic->hwdata->axes[0];      /* Always at least one axis. */
        if (dest->cAxes > 1) {
            axes[1] = haptic->hwdata->axes[1];
        }
        if (dest->cAxes > 2) {
            axes[2] = haptic->hwdata->axes[2];
        }
        dest->rgdwAxes = axes;
    }

    /* The big type handling switch, even bigger than Linux's version. */
    switch (src->type) {
    case SDL_HAPTIC_CONSTANT:
        hap_constant = &src->constant;
        constant = SDL_malloc(sizeof(DICONSTANTFORCE));
        if (constant == NULL) {
            return SDL_OutOfMemory();
        }
        SDL_memset(constant, 0, sizeof(DICONSTANTFORCE));

        /* Specifics */
        constant->lMagnitude = CONVERT(hap_constant->level);
        dest->cbTypeSpecificParams = sizeof(DICONSTANTFORCE);
        dest->lpvTypeSpecificParams = constant;

        /* Generics */
        dest->dwDuration = hap_constant->length * 1000; /* In microseconds. */
        dest->dwTriggerButton = DIGetTriggerButton(hap_constant->button);
        dest->dwTriggerRepeatInterval = hap_constant->interval;
        dest->dwStartDelay = hap_constant->delay * 1000;        /* In microseconds. */

        /* Direction. */
        if (SDL_SYS_SetDirection(dest, &hap_constant->direction, dest->cAxes) < 0) {
            return -1;
        }

        /* Envelope */
        if ((hap_constant->attack_length == 0)
            && (hap_constant->fade_length == 0)) {
            SDL_free(dest->lpEnvelope);
            dest->lpEnvelope = NULL;
        } else {
            envelope->dwAttackLevel = CCONVERT(hap_constant->attack_level);
            envelope->dwAttackTime = hap_constant->attack_length * 1000;
            envelope->dwFadeLevel = CCONVERT(hap_constant->fade_level);
            envelope->dwFadeTime = hap_constant->fade_length * 1000;
        }

        break;

    case SDL_HAPTIC_SINE:
    /* !!! FIXME: put this back when we have more bits in 2.1 */
    /* case SDL_HAPTIC_SQUARE: */
    case SDL_HAPTIC_TRIANGLE:
    case SDL_HAPTIC_SAWTOOTHUP:
    case SDL_HAPTIC_SAWTOOTHDOWN:
        hap_periodic = &src->periodic;
        periodic = SDL_malloc(sizeof(DIPERIODIC));
        if (periodic == NULL) {
            return SDL_OutOfMemory();
        }
        SDL_memset(periodic, 0, sizeof(DIPERIODIC));

        /* Specifics */
        periodic->dwMagnitude = CONVERT(SDL_abs(hap_periodic->magnitude));
        periodic->lOffset = CONVERT(hap_periodic->offset);
        periodic->dwPhase = 
                (hap_periodic->phase + (hap_periodic->magnitude < 0 ? 18000 : 0)) % 36000;
        periodic->dwPeriod = hap_periodic->period * 1000;
        dest->cbTypeSpecificParams = sizeof(DIPERIODIC);
        dest->lpvTypeSpecificParams = periodic;

        /* Generics */
        dest->dwDuration = hap_periodic->length * 1000; /* In microseconds. */
        dest->dwTriggerButton = DIGetTriggerButton(hap_periodic->button);
        dest->dwTriggerRepeatInterval = hap_periodic->interval;
        dest->dwStartDelay = hap_periodic->delay * 1000;        /* In microseconds. */

        /* Direction. */
        if (SDL_SYS_SetDirection(dest, &hap_periodic->direction, dest->cAxes)
            < 0) {
            return -1;
        }

        /* Envelope */
        if ((hap_periodic->attack_length == 0)
            && (hap_periodic->fade_length == 0)) {
            SDL_free(dest->lpEnvelope);
            dest->lpEnvelope = NULL;
        } else {
            envelope->dwAttackLevel = CCONVERT(hap_periodic->attack_level);
            envelope->dwAttackTime = hap_periodic->attack_length * 1000;
            envelope->dwFadeLevel = CCONVERT(hap_periodic->fade_level);
            envelope->dwFadeTime = hap_periodic->fade_length * 1000;
        }

        break;

    case SDL_HAPTIC_SPRING:
    case SDL_HAPTIC_DAMPER:
    case SDL_HAPTIC_INERTIA:
    case SDL_HAPTIC_FRICTION:
        hap_condition = &src->condition;
        condition = SDL_malloc(sizeof(DICONDITION) * dest->cAxes);
        if (condition == NULL) {
            return SDL_OutOfMemory();
        }
        SDL_memset(condition, 0, sizeof(DICONDITION));

        /* Specifics */
        for (i = 0; i < (int) dest->cAxes; i++) {
            condition[i].lOffset = CONVERT(hap_condition->center[i]);
            condition[i].lPositiveCoefficient =
                CONVERT(hap_condition->right_coeff[i]);
            condition[i].lNegativeCoefficient =
                CONVERT(hap_condition->left_coeff[i]);
            condition[i].dwPositiveSaturation =
                CCONVERT(hap_condition->right_sat[i] / 2);
            condition[i].dwNegativeSaturation =
                CCONVERT(hap_condition->left_sat[i] / 2);
            condition[i].lDeadBand = CCONVERT(hap_condition->deadband[i] / 2);
        }
        dest->cbTypeSpecificParams = sizeof(DICONDITION) * dest->cAxes;
        dest->lpvTypeSpecificParams = condition;

        /* Generics */
        dest->dwDuration = hap_condition->length * 1000;        /* In microseconds. */
        dest->dwTriggerButton = DIGetTriggerButton(hap_condition->button);
        dest->dwTriggerRepeatInterval = hap_condition->interval;
        dest->dwStartDelay = hap_condition->delay * 1000;       /* In microseconds. */

        /* Direction. */
        if (SDL_SYS_SetDirection(dest, &hap_condition->direction, dest->cAxes)
            < 0) {
            return -1;
        }

        /* Envelope - Not actually supported by most CONDITION implementations. */
        SDL_free(dest->lpEnvelope);
        dest->lpEnvelope = NULL;

        break;

    case SDL_HAPTIC_RAMP:
        hap_ramp = &src->ramp;
        ramp = SDL_malloc(sizeof(DIRAMPFORCE));
        if (ramp == NULL) {
            return SDL_OutOfMemory();
        }
        SDL_memset(ramp, 0, sizeof(DIRAMPFORCE));

        /* Specifics */
        ramp->lStart = CONVERT(hap_ramp->start);
        ramp->lEnd = CONVERT(hap_ramp->end);
        dest->cbTypeSpecificParams = sizeof(DIRAMPFORCE);
        dest->lpvTypeSpecificParams = ramp;

        /* Generics */
        dest->dwDuration = hap_ramp->length * 1000;     /* In microseconds. */
        dest->dwTriggerButton = DIGetTriggerButton(hap_ramp->button);
        dest->dwTriggerRepeatInterval = hap_ramp->interval;
        dest->dwStartDelay = hap_ramp->delay * 1000;    /* In microseconds. */

        /* Direction. */
        if (SDL_SYS_SetDirection(dest, &hap_ramp->direction, dest->cAxes) < 0) {
            return -1;
        }

        /* Envelope */
        if ((hap_ramp->attack_length == 0) && (hap_ramp->fade_length == 0)) {
            SDL_free(dest->lpEnvelope);
            dest->lpEnvelope = NULL;
        } else {
            envelope->dwAttackLevel = CCONVERT(hap_ramp->attack_level);
            envelope->dwAttackTime = hap_ramp->attack_length * 1000;
            envelope->dwFadeLevel = CCONVERT(hap_ramp->fade_level);
            envelope->dwFadeTime = hap_ramp->fade_length * 1000;
        }

        break;

    case SDL_HAPTIC_CUSTOM:
        hap_custom = &src->custom;
        custom = SDL_malloc(sizeof(DICUSTOMFORCE));
        if (custom == NULL) {
            return SDL_OutOfMemory();
        }
        SDL_memset(custom, 0, sizeof(DICUSTOMFORCE));

        /* Specifics */
        custom->cChannels = hap_custom->channels;
        custom->dwSamplePeriod = hap_custom->period * 1000;
        custom->cSamples = hap_custom->samples;
        custom->rglForceData =
            SDL_malloc(sizeof(LONG) * custom->cSamples * custom->cChannels);
        for (i = 0; i < hap_custom->samples * hap_custom->channels; i++) {      /* Copy data. */
            custom->rglForceData[i] = CCONVERT(hap_custom->data[i]);
        }
        dest->cbTypeSpecificParams = sizeof(DICUSTOMFORCE);
        dest->lpvTypeSpecificParams = custom;

        /* Generics */
        dest->dwDuration = hap_custom->length * 1000;   /* In microseconds. */
        dest->dwTriggerButton = DIGetTriggerButton(hap_custom->button);
        dest->dwTriggerRepeatInterval = hap_custom->interval;
        dest->dwStartDelay = hap_custom->delay * 1000;  /* In microseconds. */

        /* Direction. */
        if (SDL_SYS_SetDirection(dest, &hap_custom->direction, dest->cAxes) < 0) {
            return -1;
        }

        /* Envelope */
        if ((hap_custom->attack_length == 0)
            && (hap_custom->fade_length == 0)) {
            SDL_free(dest->lpEnvelope);
            dest->lpEnvelope = NULL;
        } else {
            envelope->dwAttackLevel = CCONVERT(hap_custom->attack_level);
            envelope->dwAttackTime = hap_custom->attack_length * 1000;
            envelope->dwFadeLevel = CCONVERT(hap_custom->fade_level);
            envelope->dwFadeTime = hap_custom->fade_length * 1000;
        }

        break;

    default:
        return SDL_SetError("Haptic: Unknown effect type.");
    }

    return 0;
}


/*
 * Frees an DIEFFECT allocated by SDL_SYS_ToDIEFFECT.
 */
static void
SDL_SYS_HapticFreeDIEFFECT(DIEFFECT * effect, int type)
{
    DICUSTOMFORCE *custom;

    SDL_free(effect->lpEnvelope);
    effect->lpEnvelope = NULL;
    SDL_free(effect->rgdwAxes);
    effect->rgdwAxes = NULL;
    if (effect->lpvTypeSpecificParams != NULL) {
        if (type == SDL_HAPTIC_CUSTOM) {        /* Must free the custom data. */
            custom = (DICUSTOMFORCE *) effect->lpvTypeSpecificParams;
            SDL_free(custom->rglForceData);
            custom->rglForceData = NULL;
        }
        SDL_free(effect->lpvTypeSpecificParams);
        effect->lpvTypeSpecificParams = NULL;
    }
    SDL_free(effect->rglDirection);
    effect->rglDirection = NULL;
}

/*
 * Gets the effect type from the generic SDL haptic effect wrapper.
 */
static REFGUID
SDL_SYS_HapticEffectType(SDL_HapticEffect * effect)
{
    switch (effect->type) {
    case SDL_HAPTIC_CONSTANT:
        return &GUID_ConstantForce;

    case SDL_HAPTIC_RAMP:
        return &GUID_RampForce;

    /* !!! FIXME: put this back when we have more bits in 2.1 */
    /* case SDL_HAPTIC_SQUARE:
        return &GUID_Square; */

    case SDL_HAPTIC_SINE:
        return &GUID_Sine;

    case SDL_HAPTIC_TRIANGLE:
        return &GUID_Triangle;

    case SDL_HAPTIC_SAWTOOTHUP:
        return &GUID_SawtoothUp;

    case SDL_HAPTIC_SAWTOOTHDOWN:
        return &GUID_SawtoothDown;

    case SDL_HAPTIC_SPRING:
        return &GUID_Spring;

    case SDL_HAPTIC_DAMPER:
        return &GUID_Damper;

    case SDL_HAPTIC_INERTIA:
        return &GUID_Inertia;

    case SDL_HAPTIC_FRICTION:
        return &GUID_Friction;

    case SDL_HAPTIC_CUSTOM:
        return &GUID_CustomForce;

    default:
        return NULL;
    }
}
int
SDL_DINPUT_HapticNewEffect(SDL_Haptic * haptic, struct haptic_effect *effect, SDL_HapticEffect * base)
{
    HRESULT ret;
    REFGUID type = SDL_SYS_HapticEffectType(base);

    if (type == NULL) {
        SDL_SetError("Haptic: Unknown effect type.");
        return -1;
    }

    /* Get the effect. */
    if (SDL_SYS_ToDIEFFECT(haptic, &effect->hweffect->effect, base) < 0) {
        goto err_effectdone;
    }

    /* Create the actual effect. */
    ret = IDirectInputDevice8_CreateEffect(haptic->hwdata->device, type,
        &effect->hweffect->effect,
        &effect->hweffect->ref, NULL);
    if (FAILED(ret)) {
        DI_SetError("Unable to create effect", ret);
        goto err_effectdone;
    }

    return 0;

err_effectdone:
    SDL_SYS_HapticFreeDIEFFECT(&effect->hweffect->effect, base->type);
    return -1;
}

int
SDL_DINPUT_HapticUpdateEffect(SDL_Haptic * haptic, struct haptic_effect *effect, SDL_HapticEffect * data)
{
    HRESULT ret;
    DWORD flags;
    DIEFFECT temp;

    /* Get the effect. */
    SDL_memset(&temp, 0, sizeof(DIEFFECT));
    if (SDL_SYS_ToDIEFFECT(haptic, &temp, data) < 0) {
        goto err_update;
    }

    /* Set the flags.  Might be worthwhile to diff temp with loaded effect and
    *  only change those parameters. */
    flags = DIEP_DIRECTION |
        DIEP_DURATION |
        DIEP_ENVELOPE |
        DIEP_STARTDELAY |
        DIEP_TRIGGERBUTTON |
        DIEP_TRIGGERREPEATINTERVAL | DIEP_TYPESPECIFICPARAMS;

    /* Create the actual effect. */
    ret =
        IDirectInputEffect_SetParameters(effect->hweffect->ref, &temp, flags);
    if (ret == DIERR_NOTEXCLUSIVEACQUIRED) {
        IDirectInputDevice8_Unacquire(haptic->hwdata->device);
        ret = IDirectInputDevice8_SetCooperativeLevel(haptic->hwdata->device, SDL_HelperWindow, DISCL_EXCLUSIVE | DISCL_BACKGROUND);
        if (SUCCEEDED(ret)) {
            ret = DIERR_NOTACQUIRED;
        }
    }
    if (ret == DIERR_INPUTLOST || ret == DIERR_NOTACQUIRED) {
        ret = IDirectInputDevice8_Acquire(haptic->hwdata->device);
        if (SUCCEEDED(ret)) {
            ret = IDirectInputEffect_SetParameters(effect->hweffect->ref, &temp, flags);
        }
    }
    if (FAILED(ret)) {
        DI_SetError("Unable to update effect", ret);
        goto err_update;
    }

    /* Copy it over. */
    SDL_SYS_HapticFreeDIEFFECT(&effect->hweffect->effect, data->type);
    SDL_memcpy(&effect->hweffect->effect, &temp, sizeof(DIEFFECT));

    return 0;

err_update:
    SDL_SYS_HapticFreeDIEFFECT(&temp, data->type);
    return -1;
}

int
SDL_DINPUT_HapticRunEffect(SDL_Haptic * haptic, struct haptic_effect *effect, Uint32 iterations)
{
    HRESULT ret;
    DWORD iter;

    /* Check if it's infinite. */
    if (iterations == SDL_HAPTIC_INFINITY) {
        iter = INFINITE;
    } else {
        iter = iterations;
    }

    /* Run the effect. */
    ret = IDirectInputEffect_Start(effect->hweffect->ref, iter, 0);
    if (FAILED(ret)) {
        return DI_SetError("Running the effect", ret);
    }
    return 0;
}

int
SDL_DINPUT_HapticStopEffect(SDL_Haptic * haptic, struct haptic_effect *effect)
{
    HRESULT ret;

    ret = IDirectInputEffect_Stop(effect->hweffect->ref);
    if (FAILED(ret)) {
        return DI_SetError("Unable to stop effect", ret);
    }
    return 0;
}

void
SDL_DINPUT_HapticDestroyEffect(SDL_Haptic * haptic, struct haptic_effect *effect)
{
    HRESULT ret;

    ret = IDirectInputEffect_Unload(effect->hweffect->ref);
    if (FAILED(ret)) {
        DI_SetError("Removing effect from the device", ret);
    }
    SDL_SYS_HapticFreeDIEFFECT(&effect->hweffect->effect, effect->effect.type);
}

int
SDL_DINPUT_HapticGetEffectStatus(SDL_Haptic * haptic, struct haptic_effect *effect)
{
    HRESULT ret;
    DWORD status;

    ret = IDirectInputEffect_GetEffectStatus(effect->hweffect->ref, &status);
    if (FAILED(ret)) {
        return DI_SetError("Getting effect status", ret);
    }

    if (status == 0)
        return SDL_FALSE;
    return SDL_TRUE;
}

int
SDL_DINPUT_HapticSetGain(SDL_Haptic * haptic, int gain)
{
    HRESULT ret;
    DIPROPDWORD dipdw;

    /* Create the weird structure thingy. */
    dipdw.diph.dwSize = sizeof(DIPROPDWORD);
    dipdw.diph.dwHeaderSize = sizeof(DIPROPHEADER);
    dipdw.diph.dwObj = 0;
    dipdw.diph.dwHow = DIPH_DEVICE;
    dipdw.dwData = gain * 100;  /* 0 to 10,000 */

    /* Try to set the autocenter. */
    ret = IDirectInputDevice8_SetProperty(haptic->hwdata->device,
        DIPROP_FFGAIN, &dipdw.diph);
    if (FAILED(ret)) {
        return DI_SetError("Setting gain", ret);
    }
    return 0;
}

int
SDL_DINPUT_HapticSetAutocenter(SDL_Haptic * haptic, int autocenter)
{
    HRESULT ret;
    DIPROPDWORD dipdw;

    /* Create the weird structure thingy. */
    dipdw.diph.dwSize = sizeof(DIPROPDWORD);
    dipdw.diph.dwHeaderSize = sizeof(DIPROPHEADER);
    dipdw.diph.dwObj = 0;
    dipdw.diph.dwHow = DIPH_DEVICE;
    dipdw.dwData = (autocenter == 0) ? DIPROPAUTOCENTER_OFF :
        DIPROPAUTOCENTER_ON;

    /* Try to set the autocenter. */
    ret = IDirectInputDevice8_SetProperty(haptic->hwdata->device,
        DIPROP_AUTOCENTER, &dipdw.diph);
    if (FAILED(ret)) {
        return DI_SetError("Setting autocenter", ret);
    }
    return 0;
}

int
SDL_DINPUT_HapticPause(SDL_Haptic * haptic)
{
    HRESULT ret;

    /* Pause the device. */
    ret = IDirectInputDevice8_SendForceFeedbackCommand(haptic->hwdata->device,
        DISFFC_PAUSE);
    if (FAILED(ret)) {
        return DI_SetError("Pausing the device", ret);
    }
    return 0;
}

int
SDL_DINPUT_HapticUnpause(SDL_Haptic * haptic)
{
    HRESULT ret;

    /* Unpause the device. */
    ret = IDirectInputDevice8_SendForceFeedbackCommand(haptic->hwdata->device,
        DISFFC_CONTINUE);
    if (FAILED(ret)) {
        return DI_SetError("Pausing the device", ret);
    }
    return 0;
}

int
SDL_DINPUT_HapticStopAll(SDL_Haptic * haptic)
{
    HRESULT ret;

    /* Try to stop the effects. */
    ret = IDirectInputDevice8_SendForceFeedbackCommand(haptic->hwdata->device,
        DISFFC_STOPALL);
    if (FAILED(ret)) {
        return DI_SetError("Stopping the device", ret);
    }
    return 0;
}

#else /* !SDL_HAPTIC_DINPUT */

typedef struct DIDEVICEINSTANCE DIDEVICEINSTANCE;
typedef struct SDL_hapticlist_item SDL_hapticlist_item;

int
SDL_DINPUT_HapticInit(void)
{
    return 0;
}

int
SDL_DINPUT_MaybeAddDevice(const DIDEVICEINSTANCE * pdidInstance)
{
    return SDL_Unsupported();
}

int
SDL_DINPUT_MaybeRemoveDevice(const DIDEVICEINSTANCE * pdidInstance)
{
    return SDL_Unsupported();
}

int
SDL_DINPUT_HapticOpen(SDL_Haptic * haptic, SDL_hapticlist_item *item)
{
    return SDL_Unsupported();
}

int
SDL_DINPUT_JoystickSameHaptic(SDL_Haptic * haptic, SDL_Joystick * joystick)
{
    return SDL_Unsupported();
}

int
SDL_DINPUT_HapticOpenFromJoystick(SDL_Haptic * haptic, SDL_Joystick * joystick)
{
    return SDL_Unsupported();
}

void
SDL_DINPUT_HapticClose(SDL_Haptic * haptic)
{
}

void
SDL_DINPUT_HapticQuit(void)
{
}

int
SDL_DINPUT_HapticNewEffect(SDL_Haptic * haptic, struct haptic_effect *effect, SDL_HapticEffect * base)
{
    return SDL_Unsupported();
}

int
SDL_DINPUT_HapticUpdateEffect(SDL_Haptic * haptic, struct haptic_effect *effect, SDL_HapticEffect * data)
{
    return SDL_Unsupported();
}

int
SDL_DINPUT_HapticRunEffect(SDL_Haptic * haptic, struct haptic_effect *effect, Uint32 iterations)
{
    return SDL_Unsupported();
}

int
SDL_DINPUT_HapticStopEffect(SDL_Haptic * haptic, struct haptic_effect *effect)
{
    return SDL_Unsupported();
}

void
SDL_DINPUT_HapticDestroyEffect(SDL_Haptic * haptic, struct haptic_effect *effect)
{
}

int
SDL_DINPUT_HapticGetEffectStatus(SDL_Haptic * haptic, struct haptic_effect *effect)
{
    return SDL_Unsupported();
}

int
SDL_DINPUT_HapticSetGain(SDL_Haptic * haptic, int gain)
{
    return SDL_Unsupported();
}

int
SDL_DINPUT_HapticSetAutocenter(SDL_Haptic * haptic, int autocenter)
{
    return SDL_Unsupported();
}

int
SDL_DINPUT_HapticPause(SDL_Haptic * haptic)
{
    return SDL_Unsupported();
}

int
SDL_DINPUT_HapticUnpause(SDL_Haptic * haptic)
{
    return SDL_Unsupported();
}

int
SDL_DINPUT_HapticStopAll(SDL_Haptic * haptic)
{
    return SDL_Unsupported();
}

#endif /* SDL_HAPTIC_DINPUT */

/* vi: set ts=4 sw=4 expandtab: */
