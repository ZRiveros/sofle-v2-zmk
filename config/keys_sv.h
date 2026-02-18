/*
 * Copyright (c) 2025 The ZMK Contributors
 * 
 * Localized Keys for Swedish (Sweden) QWERTY Layout
 * Based on Swedish keyboard layout (sv-SE)
 * 
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include <dt-bindings/zmk/keys.h>

// Row 1 (Numbers and symbols)
#define SV_N1       N1         // 1
#define SV_N2       N2         // 2
#define SV_N3       N3         // 3
#define SV_N4       N4         // 4
#define SV_N5       N5         // 5
#define SV_N6       N6         // 6
#define SV_N7       N7         // 7
#define SV_N8       N8         // 8
#define SV_N9       N9         // 9
#define SV_N0       N0         // 0

// US-style shifted symbols (for MBK Choc Glow keycaps)
#define SV_EXCL     LS(N1)     // !
#define SV_AT       RA(N2)     // @
#define SV_HASH     LS(N3)     // #
#define SV_DLLR     RA(N4)     // $
#define SV_PRCNT    LS(N5)     // %
#define SV_CARET    LS(RBKT)   // ^
#define SV_AMPS     LS(N6)     // &
#define SV_STAR     LS(BSLH)   // *
#define SV_LPAR     LS(N8)     // (
#define SV_RPAR     LS(N9)     // )

// Swedish letters
#define SV_A_RING   LBKT       // Å
#define SV_A_UMLAUT APOS       // Ä
#define SV_O_UMLAUT SEMI       // Ö

// Brackets and braces
#define SV_LBKT     RA(N8)     // [
#define SV_RBKT     RA(N9)     // ]
#define SV_LBRC     RA(N7)     // {
#define SV_RBRC     RA(N0)     // }

// Punctuation
#define SV_COMMA    COMMA      // ,
#define SV_DOT      DOT        // .
#define SV_SEMI     LS(COMMA)  // ;
#define SV_COLON    LS(DOT)    // :

// Math and comparison
#define SV_PLUS     MINUS      // +
#define SV_MINUS    FSLH       // -
#define SV_EQUAL    LS(N0)     // =
#define SV_UNDER    LS(FSLH)   // _
#define SV_LT       LS(COMMA)  // <
#define SV_GT       LS(DOT)    // >

// Quotes
#define SV_SQT      BSLH       // '
#define SV_DQT      LS(N2)     // "

// Slashes and backslash
#define SV_FSLH     LS(N7)     // /
#define SV_BSLH     RA(MINUS)  // backslash
#define SV_QUESTION LS(MINUS)  // ?
#define SV_PIPE     RA(NUBS)   // |

// Tilde
#define SV_TILDE    RA(RBKT)   // ~

// Other symbols
#define SV_GRAVE    LS(EQUAL)  // `
#define SV_AT_SIGN  RA(N2)     // @
#define SV_EURO     RA(E)      // €
#define SV_POUND_SIGN RA(N3)   // £
#define SV_MU       RA(M)      // µ

// Convenience aliases (match ZMK naming)
#define SV_AA       SV_A_RING
#define SV_AE       SV_A_UMLAUT
#define SV_OE       SV_O_UMLAUT
