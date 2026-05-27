#include <Arduino.h>
#include <LiquidCrystal_I2C.h>
#include <EEPROM.h>
#include <avr/interrupt.h>

// ---- LCD I2C ----
LiquidCrystal_I2C lcd(0x27, 16, 2);

// ---- Pini ----
#define PIN_BLUE      2   // PD2 - INT0
#define PIN_YEL       3   // PD3 - INT1
#define PIN_RED       5   // PD5 - PCINT21
#define PIN_GREEN_LED A0  // PC0 - doar OUTPUT pt LED
#define PIN_GREEN_BTN A3  // PC3 - PCINT11 pt buton
#define BUZZER        A2  // PC2

const uint8_t PINS[4] = {PIN_RED, PIN_BLUE, PIN_GREEN_LED, PIN_YEL};
const char* NAMES[4]  = {"RED", "BLUE", "GREEN", "YELLOW"};

// ---- Joc ----
#define MAX_LEVEL     20
#define INPUT_TIMEOUT 8000UL
#define SEQ_SHOW_MS   550
#define SEQ_PAUSE_MS  250

uint8_t seq[MAX_LEVEL];
int level = 1;

// ---- Variabile volatile pentru intreruperi ----
volatile int8_t  btn_pressed = -1;
volatile bool    btn_event   = false;

// ---- Timer1: milisecunde ----
volatile uint32_t timer1_ms = 0;

ISR(TIMER1_COMPA_vect)
{
    timer1_ms++;
}

void timer1_init()
{
    TCCR1A = 0;
    TCCR1B = (1 << WGM12) | (1 << CS11) | (1 << CS10);
    OCR1A  = 249;
    TIMSK1 = (1 << OCIE1A);
    timer1_ms = 0;
}

void timer1_reset()
{
    cli();
    timer1_ms = 0;
    TCNT1 = 0;
    sei();
}

uint32_t timer1_get()
{
    uint32_t val;
    cli();
    val = timer1_ms;
    sei();
    return val;
}

// ---- Timer2: buzzer ----
volatile bool     buzzer_active    = false;
volatile uint16_t buzzer_ticks     = 0;
volatile uint16_t buzzer_ticks_max = 0;

ISR(TIMER2_OVF_vect)
{
    if (!buzzer_active)
		return;

    buzzer_ticks++;
    PORTC ^= (1 << PC2);

    if (buzzer_ticks >= buzzer_ticks_max)
	{
        buzzer_active = false;
        PORTC &= ~(1 << PC2);
        TIMSK2 &= ~(1 << TOIE2);
    }
}

void timer2_init()
{
    TCCR2A = 0;
    TCCR2B = (1 << CS22) | (1 << CS21);
    TIMSK2 = 0;
}

void buzz_blocking(uint16_t duration_ms)
{
    digitalWrite(BUZZER, HIGH);
    uint32_t start = timer1_get();
    while (timer1_get() - start < duration_ms);
    digitalWrite(BUZZER, LOW);
}

// ---- Intreruperi butoane ----
#define DEBOUNCE_MS 150
volatile uint32_t last_btn_time = 0;

void btns_as_input()
{
    for (int i = 0; i < 4; i++)
        if (i == 2)
		{
            pinMode(PIN_GREEN_BTN, INPUT_PULLUP);
            pinMode(PIN_GREEN_LED, OUTPUT);
            digitalWrite(PIN_GREEN_LED, LOW);
        } else
            pinMode(PINS[i], INPUT_PULLUP);
}

void interrupts_enable()
{
    btns_as_input();
    
    // INT0 (PD2) 
    EICRA |= (1 << ISC01);
    EICRA &= ~(1 << ISC00);
    EIMSK |= (1 << INT0);
    
    // INT1 (PD3) 
    EICRA |= (1 << ISC11);
    EICRA &= ~(1 << ISC10);
    EIMSK |= (1 << INT1);
    
    // PCINT2 pentru PD5 (RED)
    PCICR  |= (1 << PCIE2);
    PCMSK2 |= (1 << PCINT21);

    // PCINT1 pentru PC3 (GREEN BTN)
    PCICR  |= (1 << PCIE1);
    PCMSK1 |= (1 << PCINT11);

    btn_pressed = -1;
    btn_event   = false;
    sei();
}

void interrupts_disable()
{
    EIMSK  &= ~((1 << INT0) | (1 << INT1));
    PCICR  &= ~((1 << PCIE2) | (1 << PCIE1));
    PCMSK2 &= ~(1 << PCINT21);
    PCMSK1 &= ~(1 << PCINT11);
}

ISR(INT0_vect)
{
    if (timer1_get() - last_btn_time < DEBOUNCE_MS)
		return;
    last_btn_time = timer1_get();
    btn_pressed = 1;
    btn_event   = true;
}

ISR(INT1_vect)
{
    if (timer1_get() - last_btn_time < DEBOUNCE_MS)
		return;
    last_btn_time = timer1_get();
    btn_pressed = 3;
    btn_event   = true;
}

ISR(PCINT2_vect)
{
    static uint8_t prev = 0;
    static bool initialized = false;

    if (!initialized)
	{
        prev = PIND;
        initialized = true;
        return;
    }

    uint8_t curr = PIND;
    uint8_t fallen = prev & ~curr;
    prev = curr;

    if (fallen & (1 << PD5))
	{ 
		// RED
        if (timer1_get() - last_btn_time < DEBOUNCE_MS) 
			return;
        last_btn_time = timer1_get();
        btn_pressed = 0;
        btn_event   = true;
    }
}

ISR(PCINT1_vect)
{
    static uint8_t prev = 0;
    static bool initialized = false;

    if (!initialized)
	{
        prev = PINC;
        initialized = true;
        return;
    }

    uint8_t curr = PINC;
    uint8_t fallen = prev & ~curr;
    prev = curr;

    if (fallen & (1 << PC3))
	{
		// GREEN BTN
        if (timer1_get() - last_btn_time < DEBOUNCE_MS)
			return;
        last_btn_time = timer1_get();
        btn_pressed = 2;
        btn_event   = true;
    }
}

// ---- LED control ----
void led_on(int idx)
{
    interrupts_disable();
    btns_as_input();
    
    if (idx == 2)
        digitalWrite(PIN_GREEN_LED, HIGH);
	else
	{
        pinMode(PINS[idx], OUTPUT);
        digitalWrite(PINS[idx], HIGH);
    }
    sei();
}

void led_off(int idx)
{
    if (idx == 2)
        digitalWrite(PIN_GREEN_LED, LOW);
	else
        pinMode(PINS[idx], INPUT_PULLUP);
}

void all_leds_off()
{
    btns_as_input();
}

void flash_led(int idx, uint32_t ms)
{
    interrupts_disable();
    led_on(idx);
    uint32_t start = timer1_get();
    while (timer1_get() - start < ms);
    led_off(idx);
    interrupts_enable();
}

// ---- EEPROM ----
#define EEPROM_MAGIC 0xAB
#define HS_COUNT 5

void hs_init()
{
    if (EEPROM.read(0) != EEPROM_MAGIC)
	{
        EEPROM.write(0, EEPROM_MAGIC);
        for (int i = 0; i < HS_COUNT; i++)
		{
            EEPROM.write(1 + i * 2, 0);
            EEPROM.write(2 + i * 2, 0);
        }
    }
}

int hs_get(int i)
{
    return ((int)EEPROM.read(1 + i * 2) << 8) | EEPROM.read(2 + i * 2);
}

void hs_put(int i, int val)
{
    EEPROM.write(1 + i * 2, val >> 8);
    EEPROM.write(2 + i * 2, val & 0xFF);
}

void hs_save(int score)
{
    hs_init();
    int scores[HS_COUNT];
    for (int i = 0; i < HS_COUNT; i++)
		scores[i] = hs_get(i);

    for (int i = 0; i < HS_COUNT; i++)
        if (score > scores[i])
		{
            for (int j = HS_COUNT - 1; j > i; j--)
                scores[j] = scores[j - 1];
            scores[i] = score;
            break;
        }

    for (int i = 0; i < HS_COUNT; i++)
		hs_put(i, scores[i]);
}

void show_highscores()
{
    hs_init();
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("-- Highscores --");
    uint32_t t = timer1_get();
    while (timer1_get() - t < 1000);
    for (int i = 0; i < HS_COUNT; i++)
	{
        lcd.clear();
        lcd.setCursor(0, 0);
        lcd.print(i + 1);
        lcd.print(". Score: ");
        lcd.print(hs_get(i));
        t = timer1_get();
        while (timer1_get() - t < 1500);
    }
}

// ---- Secventa ----
void gen_sequence()
{
    randomSeed(timer1_get());
    for (int i = 0; i < MAX_LEVEL; i++)
        seq[i] = random(4);
}

void show_sequence(int len)
{
    for (int i = 0; i < len; i++)
	{
        if (i > 0 && seq[i] == seq[i - 1])
		{
            lcd.clear();
            lcd.setCursor(0, 0);
            lcd.print("   ---   ");
            uint32_t t = timer1_get();
            while (timer1_get() - t < 400);
        }
        lcd.clear();
        lcd.setCursor(0, 0);
        lcd.print("Watch:");
        lcd.setCursor(0, 1);
        lcd.print(NAMES[seq[i]]);
        flash_led(seq[i], SEQ_SHOW_MS);
        uint32_t t = timer1_get();
        while (timer1_get() - t < SEQ_PAUSE_MS);
        lcd.clear();
    }
}

int wait_button(uint32_t timeout_ms)
{
    btn_event   = false;
    btn_pressed = -1;
    interrupts_enable();
    uint32_t start = timer1_get();
    while (timer1_get() - start < timeout_ms)
        if (btn_event)
		{
            int b = btn_pressed;
            btn_event   = false;
            btn_pressed = -1;
            flash_led(b, 120);
            return b;
        }

    return -1;
}

// ---- Game over ----
void game_over(int score, bool timeout)
{
    interrupts_disable();
    all_leds_off();
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print(timeout ? "TIME OUT!" : "WRONG! -_-");
    lcd.setCursor(0, 1);
    lcd.print("Score: ");
    lcd.print(score);
    buzz_blocking(700);
    hs_save(score);
    uint32_t t = timer1_get();
    while (timer1_get() - t < 2500);
}

// ---- Run game ----
void run_game()
{
    level = 1;
    gen_sequence();

    while (level <= MAX_LEVEL)
	{
        lcd.clear();
        lcd.setCursor(0, 0);
        lcd.print("Level: ");
        lcd.print(level);
        lcd.setCursor(0, 1);
        lcd.print("Score: ");
        lcd.print(level - 1);
        uint32_t t = timer1_get();
        while (timer1_get() - t < 1200);

        show_sequence(level);

        lcd.clear();
        lcd.setCursor(0, 0);
        lcd.print("Your turn! (");
        lcd.print(level);
        lcd.print(")");
        lcd.setCursor(0, 1);
        lcd.print("Score: ");
        lcd.print(level - 1);

        bool lost = false;
        for (int i = 0; i < level; i++)
		{
            int btn = wait_button(INPUT_TIMEOUT);
            if (btn == -1)
			{
                game_over(level - 1, true);
                lost = true;
                break;
            }
            if (btn != (int)seq[i])
			{
                game_over(level - 1, false);
                lost = true;
                break;
            }
            lcd.setCursor(0, 1);
            lcd.print("OK ");
            lcd.print(i + 1);
            lcd.print("/");
            lcd.print(level);
            lcd.print("    ");
        }

        if (lost)
			return;

        interrupts_disable();
        all_leds_off();
        lcd.clear();
        lcd.setCursor(0, 0);
        lcd.print("  Good job! +1  ");
        t = timer1_get();
        while (timer1_get() - t < 900);
        level++;
    }

    interrupts_disable();
    all_leds_off();
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("  YOU WIN!!!  ");
    lcd.setCursor(0, 1);
    lcd.print("Score: ");
    lcd.print(MAX_LEVEL);
    hs_save(MAX_LEVEL);
    uint32_t t = timer1_get();
    while (timer1_get() - t < 4000);
}

// ---- Meniu ----
void show_menu()
{
    interrupts_disable();
    all_leds_off();
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("RED  = Start");
    lcd.setCursor(0, 1);
    lcd.print("BLUE = Scores");
    btns_as_input();

    while (true)
	{
        if (digitalRead(PIN_RED) == LOW)
		{
            uint32_t t = timer1_get();
            while (timer1_get() - t < 200);
            return;
        }
        if (digitalRead(PIN_BLUE) == LOW)
		{
            uint32_t t = timer1_get();
            while (timer1_get() - t < 200);
            show_highscores();
            lcd.clear();
            lcd.setCursor(0, 0);
            lcd.print("RED  = Start");
            lcd.setCursor(0, 1);
            lcd.print("BLUE = Scores");
            btns_as_input();
        }
    }
}

// ---- Setup / Loop ----
void setup()
{
    timer1_init();
    timer2_init();
    btns_as_input();
    pinMode(BUZZER, OUTPUT);
    digitalWrite(BUZZER, LOW);
    hs_init();
    lcd.init();
    lcd.backlight();
    lcd.setCursor(0, 0);
    lcd.print(" Memory Rush ");
    lcd.setCursor(0, 1);
    lcd.print("  Loading...  ");
    uint32_t t = timer1_get();
    while (timer1_get() - t < 1500);
}

void loop()
{
    show_menu();
    run_game();
}