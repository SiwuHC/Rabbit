#pragma once
#ifndef COMPONENTS_H
#define COMPONENTS_H

#include <QList>
#include <QString>

/*
 * This file is used to include all the components.
 * The `IWYU pragma: export' is used to tell IWYU to export the included
 * headers. I declared the pragma to disable the warning.
 */

#include "ButtonComponent.h"         // IWYU pragma: export
#include "DataCapture8Component.h"   // IWYU pragma: export
#include "DecimalInput8Component.h"   // IWYU pragma: export
#include "DataCapture16Component.h"   // IWYU pragma: export
#include "DecimalInput16Component.h"   // IWYU pragma: export
#include "DataCapture32Component.h"   // IWYU pragma: export
#include "DecimalInput32Component.h"   // IWYU pragma: export
#include "DataCaptureFloatComponent.h"   // IWYU pragma: export
#include "DecimalInputFloatComponent.h"   // IWYU pragma: export
#include "GraphicLCDComponent.h"     // IWYU pragma: export
#include "KeyPadComponent.h"         // IWYU pragma: export
#include "LEDComponent.h"            // IWYU pragma: export
#include "LEDMatrixComponent.h"      // IWYU pragma: export
#include "RotaryButtonComponent.h"   // IWYU pragma: export
#include "SegmentDisplayComponent.h" // IWYU pragma: export
#include "SwitchComponent.h"         // IWYU pragma: export
#include "TextLCDComponent.h"        // IWYU pragma: export
#include "PS2KeyboardComponent.h"
#include "NewKeyboardComponent.h"
#include "StreamInput8Component.h"     // IWYU pragma: export
#include "StreamInput16Component.h"    // IWYU pragma: export
#include "StreamInput32Component.h"    // IWYU pragma: export
#include "StreamInputFloatComponent.h" // IWYU pragma: export
#include "StreamOutput8Component.h"    // IWYU pragma: export
#include "StreamOutput16Component.h"   // IWYU pragma: export
#include "StreamOutput32Component.h"   // IWYU pragma: export
#include "StreamOutputFloatComponent.h" // IWYU pragma: export
#include "SeriWrapComponent.h"         // IWYU pragma: export

// add new input component here
inline QList<QString> inputComponents() {
  return QList<QString>{"Switch",       "Button",      "KeyPad",       "SmallKeyPad",
                        "RotaryButton", "PS2Keyboard", "NewKeyboard",  "DecimalInput8",
                        "DecimalInput16", "DecimalInput32", "DecimalInputFloat",
                        "StreamInput8", "StreamInput16", "StreamInput32",
                        "StreamInputFloat",
                        // Drives a whole SeriWrap wrapper (input frame + output
                        // frame) instead of one stream direction at a time.
                        "SeriWrap"};
}

// add new output conponent here
inline QList<QString> outputComponents() {
  return QList<QString>{"LED",
                        "TextLCD",
                        "GraphicLCD",
                        "SegmentDisplay",
                        "FourDigitSegmentDisplay",
                        "LED4x4Matrix",
                        "LED8x8Matrix",
                        "LED16x16Matrix",
                        "DataCapture8",
                        "DataCapture16", "DataCapture32", "DataCaptureFloat",
                        "StreamOutput8", "StreamOutput16", "StreamOutput32",
                        "StreamOutputFloat"};
}

#endif // COMPONENTS_H
