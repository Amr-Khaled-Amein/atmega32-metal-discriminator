#define F_CPU 11059200UL
#include <avr/io.h>
#include <util/delay.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <avr/eeprom.h>

/* ============================================================
 * PIN DEFINITIONS
 * ============================================================ */
#define TX_PIN     PD4
#define BUZZ_PIN   PD2
#define BTN_PIN    PD3   
#define BTN_PORT   PORTD
#define BTN_DDR    DDRD
#define BTN_PINS   PIND
#define BTN_PRESSED() (!(BTN_PINS & (1 << BTN_PIN)))
#define HALF_DELAY 100

/* ============================================================
 * TUNING
 * ============================================================ */
#define ZERO_TOTAL   30
#define ZERO_WARMUP   20
#define ZERO_STABLE   10

#define SCAN_TOTAL   30
#define SCAN_WARMUP   20
#define SCAN_STABLE   10

/* ============================================================
 * METAL IDs
 * ============================================================ */
#define METAL_NONE          0xFF
#define METAL_UNK_NONFERR   0
#define METAL_STEEL         1
#define METAL_STAINLESS     2
#define METAL_COPPER        3
#define METAL_SILVER        4
#define METAL_ALUMINUM      5
#define METAL_GOLD          6
#define METAL_UNK_FERROUS   7

/* ============================================================
 * SCAN HISTORY ? last 3 results (RAM)
 * ============================================================ */
uint8_t history_metal[3] = { METAL_NONE, METAL_NONE, METAL_NONE };
uint8_t history_conf[3]  = { 0, 0, 0 };
float   history_diff[3]  = { 0.0f, 0.0f, 0.0f };

/* ============================================================
 * EEPROM LAYOUT
 * ============================================================ */
#define EEPROM_BASE  0x00

typedef struct {
    uint8_t  metal;
    uint8_t  conf;
    uint16_t diff_x10;
} EEPROMEntry;

void eeprom_save_history(void) {
    for (uint8_t i = 0; i < 3; i++) {
        uint16_t addr = EEPROM_BASE + i * sizeof(EEPROMEntry);
        EEPROMEntry e;
        e.metal    = history_metal[i];
        e.conf     = history_conf[i];
        e.diff_x10 = (uint16_t)(history_diff[i] * 10.0f);
        eeprom_write_block(&e, (void*)addr, sizeof(EEPROMEntry));
    }
}

void eeprom_load_history(void) {
    for (uint8_t i = 0; i < 3; i++) {
        uint16_t addr = EEPROM_BASE + i * sizeof(EEPROMEntry);
        EEPROMEntry e;
        eeprom_read_block(&e, (const void*)addr, sizeof(EEPROMEntry));
        if (e.metal == 0xFF) {
            history_metal[i] = METAL_NONE;
            history_conf[i]  = 0;
            history_diff[i]  = 0.0f;
        } else {
            history_metal[i] = e.metal;
            history_conf[i]  = e.conf;
            history_diff[i]  = (float)e.diff_x10 / 10.0f;
        }
    }
}

void history_push(uint8_t metal, uint8_t conf, float diff) {
    history_metal[2] = history_metal[1];
    history_conf[2]  = history_conf[1];
    history_diff[2]  = history_diff[1];
    history_metal[1] = history_metal[0];
    history_conf[1]  = history_conf[0];
    history_diff[1]  = history_diff[0];
    history_metal[0] = metal;
    history_conf[0]  = conf;
    history_diff[0]  = diff;
    eeprom_save_history();
}

/* ============================================================
 * LED MAP (PORTB)
 * ============================================================ */
uint8_t metal_to_led(uint8_t metal) {
    switch (metal) {
        case METAL_STEEL:       return (1 << PB0);
        case METAL_ALUMINUM:    return (1 << PB1);
        case METAL_COPPER:      return (1 << PB2);
        case METAL_STAINLESS:   return (1 << PB3);
        case METAL_SILVER:      return (1 << PB4);
        case METAL_GOLD:        return (1 << PB5);
        case METAL_UNK_FERROUS: return (1 << PB6);
        case METAL_UNK_NONFERR: return (1 << PB7);
        default:                return 0x00;
    }
}

/* ============================================================
 * UART ? 9600 8N1 @ 11.059200 MHz
 * ============================================================ */
#define UART_BAUD  9600
#define UART_UBRR  ((F_CPU / (16UL * UART_BAUD)) - 1)

void uart_init(void) {
    UBRRH = (uint8_t)(UART_UBRR >> 8);
    UBRRL = (uint8_t)(UART_UBRR);
    UCSRB = (1 << TXEN);
    UCSRC = (1 << URSEL) | (1 << UCSZ1) | (1 << UCSZ0);
}
void uart_putc(char c) {
    while (!(UCSRA & (1 << UDRE)));
    UDR = c;
}
void uart_puts(const char *s) { while (*s) uart_putc(*s++); }
void uart_putf(float f) {
    char buf[16];
    int whole = (int)f;
    int frac  = (int)((f - (float)whole) * 100.0f);
    if (frac < 0) frac = -frac;
    itoa(whole, buf, 10); uart_puts(buf);
    uart_putc('.');
    if (frac < 10) uart_putc('0');
    itoa(frac, buf, 10); uart_puts(buf);
}
void uart_log(const char *label, float value) {
    uart_puts(label); uart_putf(value); uart_puts("\r\n");
}
void uart_sep(const char *title) {
    uart_puts("\r\n--- "); uart_puts(title); uart_puts(" ---\r\n");
}

/* ============================================================
 * BUZZER
 * ============================================================ */
void buzz(uint16_t ms) {
    PORTD |= (1 << BUZZ_PIN);
    for (uint16_t i = 0; i < ms; i++) _delay_ms(1);
    PORTD &= ~(1 << BUZZ_PIN);
}
void buzz_beeps(uint8_t n, uint16_t on_ms, uint16_t off_ms) {
    for (uint8_t i = 0; i < n; i++) {
        buzz(on_ms);
        if (i < n - 1)
            for (uint16_t j = 0; j < off_ms; j++) _delay_ms(1);
    }
}
void buzz_result(uint8_t metal) {
    switch (metal) {
        case METAL_UNK_NONFERR:  buzz_beeps(1, 80, 0);                             break;
        case METAL_STEEL:        buzz_beeps(2, 60, 60);                            break;
        case METAL_STAINLESS:    buzz_beeps(3, 60, 60);                            break;
        case METAL_COPPER:       buzz_beeps(4, 60, 60);                            break;
        case METAL_ALUMINUM:     buzz_beeps(5, 60, 60);                            break;
        case METAL_SILVER:       buzz_beeps(5, 60, 60); _delay_ms(200); buzz(250); break;
        case METAL_GOLD:         buzz_beeps(5, 60, 60); _delay_ms(200); buzz(600); break;
        case METAL_UNK_FERROUS:  buzz(600);                                        break;
        default:                 buzz(50);                                         break;
    }
}

/* ============================================================
 * LCD
 * ============================================================ */
void lcd_pulse(void) {
    PORTC |=  (1 << PC0); _delay_us(1);
    PORTC &= ~(1 << PC0); _delay_us(100);
}
void lcd_send4(uint8_t d) {
    PORTC &= 0x0F; PORTC |= (d & 0xF0); lcd_pulse();
}
void lcd_cmd(uint8_t c) {
    PORTC &= ~(1 << PC2); PORTC &= ~(1 << PC1);
    lcd_send4(c); lcd_send4(c << 4); _delay_us(50);
}
void lcd_char(uint8_t c) {
    PORTC |=  (1 << PC2); PORTC &= ~(1 << PC1);
    lcd_send4(c); lcd_send4(c << 4); _delay_us(50);
}
void lcd_init(void) {
    DDRC = 0xFF; _delay_ms(50);
    lcd_send4(0x30); _delay_ms(5);
    lcd_send4(0x30); _delay_us(150);
    lcd_send4(0x20); _delay_us(150);
    lcd_cmd(0x28); lcd_cmd(0x0C); lcd_cmd(0x06); lcd_cmd(0x01);
    _delay_ms(2);
}
void lcd_pos(uint8_t r, uint8_t c) {
    lcd_cmd((r == 0 ? 0x80 : 0xC0) + c);
}
void lcd_str(const char *s)   { while (*s) lcd_char(*s++); }
void lcd_str16(const char *s) {
    uint8_t i = 0;
    while (s[i] && i < 16) { lcd_char(s[i]); i++; }
    while (i < 16)          { lcd_char(' ');  i++; }
}
void lcd_int(int v) { char b[10]; itoa(v, b, 10); lcd_str(b); }

/* ============================================================
 * ADC & MEASURE
 * ============================================================ */
void adc_init(void) {
    ADMUX  = (1 << REFS0);
    ADCSRA = (1 << ADEN) | (1 << ADPS2) | (1 << ADPS1) | (1 << ADPS0);
}
uint16_t adc_read(void) {
    ADCSRA |= (1 << ADSC);
    while (ADCSRA & (1 << ADSC));
    return ADC;
}
float measure_once(void) {
    uint32_t sum = 0;
    for (uint8_t i = 0; i < 64; i++) {
        PORTD |=  (1 << TX_PIN);
        volatile uint16_t d = 30; while (d--);
        PORTD &= ~(1 << TX_PIN);
        _delay_us(7);
        sum += adc_read();
        _delay_us(300);
    }
    return (float)sum / 64.0f;
}

/* ============================================================
 * CLASSIFIER
 * ============================================================ */
typedef struct {
    uint8_t id;
    float   low;
    float   center;
    float   high;
} MetalRange;

static const MetalRange RANGES[] = {
    { METAL_UNK_NONFERR,  30.0f,  70.0f,  90.0f  },
    { METAL_COPPER,       90.0f, 103.0f, 120.0f   },
    { METAL_STAINLESS,   120.0f, 135.0f, 155.0f   },
    { METAL_ALUMINUM,    240.0f, 255.0f, 275.0f   },
    { METAL_SILVER,      155.0f, 165.0f, 190.0f   },
    { METAL_GOLD,        190.0f, 235.0f, 240.0f   },
    { METAL_STEEL,       275.0f, 300.0f, 340.0f   },
    { METAL_UNK_FERROUS, 340.0f, 400.0f, 500.0f   },
};
#define NUM_RANGES  (sizeof(RANGES) / sizeof(RANGES[0]))

uint8_t classify(float d, float baseline, uint8_t *confidence_out) {
    if (d < 10.0f) {
        *confidence_out = 0;
        return METAL_NONE;
    }
    for (uint8_t i = 0; i < NUM_RANGES; i++) {
        if (d > RANGES[i].low && d <= RANGES[i].high) {
            float half   = (RANGES[i].high - RANGES[i].low) / 2.0f;
            float dist   = d - RANGES[i].center;
            if (dist < 0.0f) dist = -dist;
            float conf_f = (1.0f - (dist / half)) * 100.0f;
            if (conf_f < 0.0f)   conf_f = 0.0f;
            if (conf_f > 100.0f) conf_f = 100.0f;
            
            
            *confidence_out = (uint8_t)conf_f;
            return RANGES[i].id;
        }
    }
    *confidence_out = 0;
    return METAL_NONE;
}

const char* metal_name(uint8_t m) {
    switch (m) {
        case METAL_UNK_NONFERR: return "Unk.Non-Fe";
        case METAL_STEEL:       return "Steel";
        case METAL_STAINLESS:   return "Stainless";
        case METAL_COPPER:      return "Copper";
        case METAL_SILVER:      return "Silver";
        case METAL_ALUMINUM:    return "Aluminum";
        case METAL_GOLD:        return "Gold";
        case METAL_UNK_FERROUS: return "Unk.Ferrous";
        default:                return "No Metal";
    }
}

/* ============================================================
 * LCD RESULT DISPLAY
 * Line 0: "Copper      65%"
 * Line 1: "D:120  B:480   "
 * ============================================================ */
void lcd_show_result(uint8_t metal, uint8_t confidence,
                     float final_diff, float baseline_mean) {
    lcd_pos(0, 0);
    if (metal == METAL_NONE) {
        lcd_str16("No Metal        ");
    } else {
        const char *name = metal_name(metal);
        uint8_t nc = 0;
        while (name[nc] && nc < 11) { lcd_char(name[nc]); nc++; }
        while (nc < 11) { lcd_char(' '); nc++; }
        char cbuf[5];
        itoa(confidence, cbuf, 10);
        uint8_t cl = 0;
        while (cbuf[cl]) cl++;
        uint8_t pad = 3 - cl;
        while (pad--) lcd_char(' ');
        lcd_str(cbuf);
        lcd_char('%');
    }
    lcd_pos(1, 0);
    lcd_str("D:");  lcd_int((int)final_diff);
    lcd_str(" B:"); lcd_int((int)baseline_mean);
    lcd_str("      ");
}

/* ============================================================
 * SCAN SEQUENCE
 * ============================================================ */
void do_scan(void) {

    /* --- PHASE 1: ZEROING --- */
    uart_sep("ZEROING PHASE");
    uart_puts("idx,raw,note\r\n");

    lcd_pos(0, 0); lcd_str16("Zeroing...      ");
    lcd_pos(1, 0); lcd_str16("                ");

    float z_sum = 0.0f;
    for (uint8_t j = 0; j < ZERO_TOTAL; j++) {
        float sample = measure_once();
        char buf[4]; itoa(j, buf, 10);
        uart_puts(buf); uart_putc(','); uart_putf(sample);
        if (j < ZERO_WARMUP) {
            uart_puts(",WARMUP\r\n");
        } else {
            z_sum += sample;
            float running = z_sum / (float)(j - ZERO_WARMUP + 1);
            uart_putc(','); uart_putf(running); uart_puts("\r\n");
        }
        {
            uint8_t col = (uint8_t)((uint16_t)j * 16 / ZERO_TOTAL);
            lcd_pos(1, col);
            lcd_char(0xFF);
        }
        _delay_ms(70);
    }

    float baseline_mean = z_sum / (float)ZERO_STABLE;

    for (uint8_t k = 0; k < 5; k++) measure_once();

    uart_sep("BASELINE RESULT");
    uart_log("baseline_mean = ", baseline_mean);

    /* --- TRANSITION: two beeps signal scanning is starting --- */
    buzz_beeps(2, 80, 100);

    /* --- PHASE 2: SCANNING --- */
    uart_sep("SCANNING PHASE");
    uart_log("baseline_mean  = ", baseline_mean);
    uart_puts("[WARMUP] idx,raw\r\n");

    lcd_pos(0, 0); lcd_str16("Scanning...     ");
    lcd_pos(1, 0); lcd_str16("                ");

    float s_sum = 0.0f;
    for (uint8_t i = 0; i < SCAN_TOTAL; i++) {
        float sample = measure_once();
        char buf[4]; itoa(i, buf, 10);

        if (i < SCAN_WARMUP) {
            float warmup_diff = sample - baseline_mean;
            if (warmup_diff < 0.0f) warmup_diff = -warmup_diff;
            uart_puts("[WARMUP] ");
            uart_puts(buf); uart_putc(','); uart_putf(sample);
            uart_putc(','); uart_putf(warmup_diff);
            uart_puts("\r\n");
            if (i == SCAN_WARMUP - 1)
                uart_puts("[STABLE] idx,raw,running_mean,diff\r\n");
        } else {
            s_sum += sample;
            uint8_t stable_idx  = i - SCAN_WARMUP;
            float   stable_mean = s_sum / (float)(stable_idx + 1);
            float   diff        = stable_mean - baseline_mean;
            if (diff < 0.0f) diff = -diff;
            uart_puts("[STABLE] ");
            uart_puts(buf); uart_putc(','); uart_putf(sample);
            uart_putc(','); uart_putf(stable_mean);
            uart_putc(','); uart_putf(diff);
            uart_puts("\r\n");
        }

        {
            uint8_t col = (uint8_t)((uint16_t)i * 16 / SCAN_TOTAL);
            lcd_pos(1, col);
            lcd_char(0xFF);
        }
        _delay_ms(70);
    }

    float avg_mean   = s_sum / (float)SCAN_STABLE;
    float final_diff = avg_mean - baseline_mean;
    if (final_diff < 0.0f) final_diff = -final_diff;

    uint8_t confidence = 0;
    uint8_t metal      = classify(final_diff, baseline_mean, &confidence);

    uart_sep("RESULT");
    uart_log("stable_mean    = ", avg_mean);
    uart_log("baseline_mean  = ", baseline_mean);
    uart_log("final_diff     = ", final_diff);
    float final_ratio = (final_diff / baseline_mean) * 100.0f;
    uart_puts("ratio (%)      = "); uart_putf(final_ratio); uart_puts("%\r\n");
    uart_puts("metal          = "); uart_puts(metal_name(metal)); uart_puts("\r\n");
    uart_puts("confidence     = ");
    { char b[4]; itoa(confidence, b, 10); uart_puts(b); }
    uart_puts("%\r\n");

    history_push(metal, confidence, final_diff);

    if (metal != METAL_NONE && confidence < 10) {
        uart_puts("warning        = LOW CONFIDENCE\r\n");
        lcd_pos(0, 0); lcd_str16("Uncertain!      ");
        lcd_pos(1, 0);
        lcd_str("D:"); lcd_int((int)final_diff);
        lcd_str(" B:"); lcd_int((int)baseline_mean);
        lcd_str("      ");
        for (uint8_t f = 0; f < 3; f++) {
            PORTB = 0xFF; _delay_ms(200);
            PORTB = 0x00; _delay_ms(200);
        }
        buzz(400);
    } else {
        lcd_show_result(metal, confidence, final_diff, baseline_mean);
        PORTB = metal_to_led(metal);
        buzz_result(metal);
    }
}

/* ============================================================
 * MAIN
 * ============================================================ */
int main(void) {
    DDRD |=  (1 << TX_PIN) | (1 << BUZZ_PIN);
    DDRD &= ~(1 << BTN_PIN);
    BTN_PORT |=  (1 << BTN_PIN);
    PORTD    &= ~(1 << BUZZ_PIN);
    DDRB = 0xFF; PORTB = 0x00;
    DDRA = 0x00;

    adc_init();
    uart_init();
    lcd_init();

    eeprom_load_history();

    uart_puts("\r\n=============================\r\n");
    uart_puts("  Metal Detector - UART Log  \r\n");
    uart_puts("  9600 8N1                   \r\n");
    uart_puts("  Zero  warmup: ");
    { char b[4]; itoa(ZERO_WARMUP, b, 10); uart_puts(b); }
    uart_puts(" samples\r\n  Zero  stable: ");
    { char b[4]; itoa(ZERO_STABLE, b, 10); uart_puts(b); }
    uart_puts(" samples\r\n  Scan  warmup: ");
    { char b[4]; itoa(SCAN_WARMUP, b, 10); uart_puts(b); }
    uart_puts(" samples\r\n  Scan  stable: ");
    { char b[4]; itoa(SCAN_STABLE, b, 10); uart_puts(b); }
    uart_puts(" samples\r\n=============================\r\n");

    lcd_pos(0, 0); lcd_str16("Metal Detector  ");
    lcd_pos(1, 0); lcd_str16("  Initializing..");
    buzz(150);
    _delay_ms(1000);

    while (1) {

        /* --- READY SCREEN --- */
        lcd_pos(0, 0); lcd_str16("Ready!          ");
        lcd_pos(1, 0); lcd_str16("Btn -> Scan     ");
        for (uint8_t t = 0; t < 20; t++) {
            if (BTN_PRESSED()) { _delay_ms(50); if (BTN_PRESSED()) goto do_scan_label; }
            _delay_ms(100);
        }

        /* --- HISTORY --- */
        for (uint8_t h = 0; h < 3; h++) {
            lcd_pos(0, 0);
            lcd_char('['); lcd_char('0' + h + 1); lcd_char(']'); lcd_char(' ');

            if (history_metal[h] == METAL_NONE) {
                uint8_t nc = 4;
                while (nc < 16) { lcd_char(' '); nc++; }
                lcd_pos(1, 0); lcd_str16("No scan yet     ");
            } else {
                const char *name = metal_name(history_metal[h]);
                uint8_t nc = 4, ni = 0;
                while (name[ni] && nc < 16) { lcd_char(name[ni]); ni++; nc++; }
                while (nc < 16) { lcd_char(' '); nc++; }
                lcd_pos(1, 0);
                lcd_str("D:"); lcd_int((int)history_diff[h]);
                lcd_str(" C:"); lcd_int(history_conf[h]);
                lcd_char('%');
                lcd_str("      ");
            }

            for (uint8_t t = 0; t < 20; t++) {
                if (BTN_PRESSED()) { _delay_ms(50); if (BTN_PRESSED()) goto do_scan_label; }
                _delay_ms(100);
            }
        }

        continue;

        do_scan_label:
        uart_puts("\r\n>>> SCAN STARTED <<<\r\n");
        do_scan();
        uart_puts("\r\n>>> SCAN COMPLETE <<<\r\n");
        while (BTN_PRESSED());
        _delay_ms(5000);
        PORTB = 0x00;
    }
}