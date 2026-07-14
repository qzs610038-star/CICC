#ifndef ARM_BUILD_PROFILE_H
#define ARM_BUILD_PROFILE_H

/*
 * The entry program and arm backend are independent build-time decisions.
 * Keep the defaults fail-safe: competition firmware is built with the arm
 * disabled until a later gate supplies the verified SoC/UART evidence.
 */
#define ARM_PROFILE_COMPETITION  1
#define ARM_PROFILE_ARM_BRINGUP  2

#define ARM_BACKEND_DISABLED     1
#define ARM_BACKEND_SIMULATED    2
#define ARM_BACKEND_READONLY     3
#define ARM_BACKEND_REAL         4

#ifndef APP_PROFILE
#define APP_PROFILE ARM_PROFILE_COMPETITION
#endif

#ifndef ARM_BACKEND
#define ARM_BACKEND ARM_BACKEND_DISABLED
#endif

#if (APP_PROFILE != ARM_PROFILE_COMPETITION) && \
    (APP_PROFILE != ARM_PROFILE_ARM_BRINGUP)
#error "APP_PROFILE must be ARM_PROFILE_COMPETITION or ARM_PROFILE_ARM_BRINGUP"
#endif

#if (ARM_BACKEND != ARM_BACKEND_DISABLED) && \
    (ARM_BACKEND != ARM_BACKEND_SIMULATED) && \
    (ARM_BACKEND != ARM_BACKEND_READONLY) && \
    (ARM_BACKEND != ARM_BACKEND_REAL)
#error "ARM_BACKEND has an unsupported value"
#endif

/* G0-G3 deliberately ship no UART2 implementation.  Do not let an enum
 * value look like a usable transport merely because it compiles elsewhere. */
#if (ARM_BACKEND == ARM_BACKEND_READONLY) || \
    (ARM_BACKEND == ARM_BACKEND_REAL)
#error "readonly/real backends are blocked until the D2 and real-motion gates"
#endif

#endif /* ARM_BUILD_PROFILE_H */
