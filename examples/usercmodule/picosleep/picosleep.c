#include "py/runtime.h"
#include "pico/sleep.h"
#include "pico/stdlib.h"
#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include <time.h>
#include <stdlib.h>
#include "hardware/clocks.h"
#include "hardware/rosc.h"
#include "hardware/structs/scb.h"
#include "hardware/rtc.h"
#include <sys/time.h>
#include "pico/runtime_init.h"

// --- Deep Sleep GPIO Wakeup Extension ---
#define PICOSLEEP_WAKE_TIMER 0
#define PICOSLEEP_WAKE_GPIO  1
static int wakeup_pin = -1;
static int last_wake_reason = PICOSLEEP_WAKE_TIMER;

// GPIO interrupt callback
void gpio_irq_callback(uint gpio, uint32_t events) {
    if (gpio == wakeup_pin) {
        last_wake_reason = PICOSLEEP_WAKE_GPIO;
        // Wakeup from interrupt: __wfi() will exit automatically
    }
}

// set_wakeup_pin: Accepts only pin number (int)
static mp_obj_t picosleep_set_wakeup_pin(mp_obj_t pin_obj) {
    wakeup_pin = mp_obj_get_int(pin_obj);
    gpio_init(wakeup_pin);
    gpio_set_dir(wakeup_pin, GPIO_IN);
    gpio_set_irq_enabled_with_callback(wakeup_pin, GPIO_IRQ_EDGE_FALL, true, &gpio_irq_callback);
    return mp_const_none;
}
MP_DEFINE_CONST_FUN_OBJ_1(picosleep_set_wakeup_pin_obj, picosleep_set_wakeup_pin);

// wake_reason: Returns the reason for wakeup
static mp_obj_t picosleep_wake_reason(void) {
    // PICOSLEEP_WAKE_TIMER or PICOSLEEP_WAKE_GPIO
    return mp_obj_new_int(last_wake_reason);
}
MP_DEFINE_CONST_FUN_OBJ_0(picosleep_wake_reason_obj, picosleep_wake_reason);

static void sleep_callback(void) {
    return;
}

// from datetime_t to struct timespec
static void datetime_to_timespec(const datetime_t *dt, struct timespec *ts) {
    struct tm t = {
        .tm_year = dt->year - 1900,
        .tm_mon = dt->month - 1,
        .tm_mday = dt->day,
        .tm_hour = dt->hour,
        .tm_min = dt->min,
        .tm_sec = dt->sec
    };
    time_t epoch = mktime(&t);
    ts->tv_sec = epoch;
    ts->tv_nsec = 0;
}

static void rtc_sleep_seconds(int8_t seconds_to_sleep) {
    //Hangs if we attempt to sleep for 1 second....
    //Guard against this and perform a normal sleep
    if(seconds_to_sleep == 1){
        sleep_ms(1000);
        return;
    }

    int y=2020, m=6, d=5, hour=15, mins=45, secs=0;
    struct tm t = { .tm_year=y-1900,
                    .tm_mon=m-1,
                    .tm_mday=d,
                    .tm_hour=hour,
                    .tm_min=mins,
                    .tm_sec=secs
                  };

    t.tm_sec += seconds_to_sleep;
    mktime(&t);

    datetime_t t_alarm = {
            .year  = t.tm_year+1900,
            .month = t.tm_mon+1,
            .day   = t.tm_mday,
            .dotw  = t.tm_wday, // 0 is Sunday, so 5 is Friday
            .hour  = t.tm_hour,
            .min   = t.tm_min,
            .sec   = t.tm_sec
    };

    struct timespec ts_alarm;
    datetime_to_timespec(&t_alarm, &ts_alarm);
    sleep_goto_sleep_until(&ts_alarm, &sleep_callback);
}

void recover_from_sleep(uint scb_orig, uint clock0_orig, uint clock1_orig){
    //Re-enable ring Oscillator control
    rosc_write(&rosc_hw->ctrl, ROSC_CTRL_ENABLE_BITS);

    //reset procs back to default
    scb_hw->scr = scb_orig;
    clocks_hw->sleep_en0 = clock0_orig;
    clocks_hw->sleep_en1 = clock1_orig;

    //reset clocks
    clocks_init();
    stdio_init_all();

    return;
}


static mp_obj_t picosleep_seconds(mp_obj_t seconds_obj) {
    mp_int_t seconds = mp_obj_get_int(seconds_obj);
    stdio_init_all();
    uint scb_orig = scb_hw->scr;
    uint clock0_orig = clocks_hw->sleep_en0;
    uint clock1_orig = clocks_hw->sleep_en1;

    last_wake_reason = PICOSLEEP_WAKE_TIMER; // Initialize as timer

    if (seconds == 0) {
        if (wakeup_pin < 0) {
            mp_raise_ValueError("wakeup_pin is not set. Please call set_wakeup_pin() to specify a wakeup pin.");
        }
        // Infinite sleep, wakeup only by GPIO interrupt
        while (last_wake_reason == PICOSLEEP_WAKE_TIMER) {
            __wfi(); // Wait For Interrupt
        }
    } else {
        // Sleep by RTC timer as usual
        datetime_t t = {
                .year  = 2020,
                .month = 06,
                .day   = 05,
                .dotw  = 5,
                .hour  = 15,
                .min   = 45,
                .sec   = 00
        };
        rtc_init();
        sleep_run_from_xosc();
        rtc_set_datetime(&t);
        rtc_sleep_seconds(seconds);
    }
    recover_from_sleep(scb_orig, clock0_orig, clock1_orig);
    return mp_const_none;
}
MP_DEFINE_CONST_FUN_OBJ_1(picosleep_seconds_obj, picosleep_seconds);


static const mp_rom_map_elem_t picosleep_module_globals_table[] = {
    { MP_ROM_QSTR(MP_QSTR___name__), MP_ROM_QSTR(MP_QSTR_picosleep) },
    { MP_ROM_QSTR(MP_QSTR_seconds), MP_ROM_PTR(&picosleep_seconds_obj) },
    { MP_ROM_QSTR(MP_QSTR_set_wakeup_pin), MP_ROM_PTR(&picosleep_set_wakeup_pin_obj) },
    { MP_ROM_QSTR(MP_QSTR_wake_reason), MP_ROM_PTR(&picosleep_wake_reason_obj) },
    { MP_ROM_QSTR(MP_QSTR_WAKE_TIMER), MP_ROM_INT(PICOSLEEP_WAKE_TIMER) },
    { MP_ROM_QSTR(MP_QSTR_WAKE_GPIO), MP_ROM_INT(PICOSLEEP_WAKE_GPIO) },
};
static MP_DEFINE_CONST_DICT(picosleep_module_globals, picosleep_module_globals_table);

const mp_obj_module_t mp_module_picosleep = {
    .base = { &mp_type_module },
    .globals = (mp_obj_dict_t *)&picosleep_module_globals,
};

MP_REGISTER_MODULE(MP_QSTR_picosleep, mp_module_picosleep);