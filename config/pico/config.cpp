#include "comms/NintendoSwitchBackend.hpp"
#include "comms/XInputBackend.hpp"
#include "config/mode_selection.hpp"
#include "core/CommunicationBackend.hpp"
#include "core/InputMode.hpp"
#include "core/KeyboardMode.hpp"
#include "core/pinout.hpp"
#include "core/socd.hpp"
#include "core/state.hpp"
#include "input/GpioButtonInput.hpp"
#include "joybus_utils.hpp"
#include "stdlib.hpp"

#include <pico/bootrom.h>

CommunicationBackend **backends = nullptr;
size_t backend_count;
KeyboardMode *current_kb_mode = nullptr;

GpioButtonMapping button_mappings[] = {
    {&InputState::l,            18},
    { &InputState::left,        19},
    { &InputState::up,          20},
    { &InputState::right,       21},

    { &InputState::down,        17},
    { &InputState::mod_y,       16},

    { &InputState::select,      22},
    { &InputState::home,        9 },
    { &InputState::start,       11},

    { &InputState::c_left,      13},
    { &InputState::c_up,        12},
    { &InputState::c_right,     10},
    { &InputState::c_down,      15},
    { &InputState::a,           14},

    { &InputState::b,           8 },
    { &InputState::x,           6 },
    { &InputState::z,           1 },
    { &InputState::mod_x,       3 },

    { &InputState::r,           7 },
    { &InputState::y,           0 },
    { &InputState::lightshield, 4 },
    { &InputState::midshield,   5 },
};
size_t button_count = sizeof(button_mappings) / sizeof(GpioButtonMapping);

const Pinout pinout = {
    .joybus_data = 28,
    .mux = -1,
    .nunchuk_detect = -1,
    .nunchuk_sda = -1,
    .nunchuk_scl = -1,
};

void setup() {
    // Create GPIO input source and use it to read button states for checking button holds.
    GpioButtonInput *gpio_input = new GpioButtonInput(button_mappings, button_count);

    InputState button_holds;
    gpio_input->UpdateInputs(button_holds);

    // Bootsel button hold as early as possible for safety.
    if (button_holds.home) {
        reset_usb_boot(0, 0);
    }

    // Turn on LED to indicate firmware booted.
    gpio_init(PICO_DEFAULT_LED_PIN);
    gpio_set_dir(PICO_DEFAULT_LED_PIN, GPIO_OUT);
    gpio_put(PICO_DEFAULT_LED_PIN, 1);

    // Create array of input sources to be used.
    static InputSource *input_sources[] = { gpio_input };
    size_t input_source_count = sizeof(input_sources) / sizeof(InputSource *);

    ConnectedConsole console = detect_console(pinout.joybus_data);

    /* Select communication backend. */
    CommunicationBackend *primary_backend;
    if (console == ConnectedConsole::NONE) {
        if (button_holds.x) {
            // Default to XInput mode if no console detected and no other mode forced.
            backend_count = 2;
            primary_backend = new XInputBackend(input_sources, input_source_count);
            backends = new CommunicationBackend *[backend_count] { primary_backend };
        } else {
            // If no console detected and X is held on plugin then use Switch USB backend.
            NintendoSwitchBackend::RegisterDescriptor();
            backend_count = 1;
            primary_backend = new NintendoSwitchBackend(input_sources, input_source_count);
            backends = new CommunicationBackend *[backend_count] { primary_backend };

            // Default to Ultimate mode on Switch.
            primary_backend->SetGameMode(new Ultimate(socd::SOCD_2IP));
            return;
        }
    }
}

void loop() {
    select_mode(backends[0]);

    for (size_t i = 0; i < backend_count; i++) {
        backends[i]->SendReport();
    }

    if (current_kb_mode != nullptr) {
        current_kb_mode->SendReport(backends[0]->GetInputs());
    }
}