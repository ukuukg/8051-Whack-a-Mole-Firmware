#include "reg51.h"
typedef enum {
    IDLE,
    PREPARING,
    WAIT,
    MOLE_ODD,
    MOLE_EVEN,
    HIT,
    MISS,
    FINISH,
    END
} State;
unsigned char code L7seg[] = {0xC0, 0xF9, 0xA4, 0xB0, 0x99, 0x92, 0x82, 0xF8,
                               0x80, 0x98, 0x88, 0x83, 0xC6, 0xA1, 0x86, 0x8E};

volatile State state;
volatile unsigned char button_clicked;
volatile unsigned char live;
volatile unsigned char score;
volatile unsigned int tick;
volatile unsigned char counting_down;
volatile unsigned int waiting_time;
volatile unsigned char mole_pattern;
volatile unsigned char random_number;
volatile unsigned char num_mole_is_odd;

void LFSR(void) {
    unsigned char tem = random_number ^ (random_number>>1) ^ (random_number>>2);
    tem = tem << 7;
    random_number = tem | (random_number>>1);
}

void Timer0_ISR(void) interrupt 1 using 2 {
    tick++;
}

void INT0_ISR(void) interrupt 0 using 1 {
    EX0 = 0;
    button_clicked = 1;
}

void delay(unsigned int s) {
	unsigned int m;
	for(m=0;m<s;m++);
}

void set_counting_down(unsigned char cd) {
    counting_down = cd;
    P0 = 0xFF;
    ACC = L7seg[cd];
    P1 = ACC;
}

void set_score(unsigned char s){
    score = s;
    ACC = L7seg[s%10];
    P0 = ACC;
    P1 = 0xFF;
    if(s>=10){
        ACC = L7seg[s/10];
        P1 = ACC; 
    }
}

void set_state(State s) {
    state = s;
    button_clicked = 0;
    IE0 = 0;
    if(s==IDLE||s==WAIT||s==MOLE_ODD||s==MOLE_EVEN) {
        EX0 = 1;
        if(s==WAIT)
            waiting_time = 6000 + random_number * 35;
    }
    if(s==MISS){
        P2 &= 0xDF;
        P2 |= 0x0F; 
    }
    if(s==HIT){
        P2 &= 0xEF;
        P2 |= 0x0F;
    }
    tick = 0;
}

void set_mole_pattern(void) {
    mole_pattern = random_number%15;
    P2 &= 0xF0;
    P2 |= mole_pattern;
    num_mole_is_odd = 0x01 & (mole_pattern ^ (mole_pattern >> 1) ^ (mole_pattern >> 2) ^ (mole_pattern >> 3));
}

void set_live(unsigned char l){
    live = l;
    P2 |= 0xC0;
    P2 &= (~(l<<6));
}

void init() {
    set_state(IDLE);

    TMOD &= 0xF0;
    TMOD |= 0x02;
    EA = 1;
    TH0 = 0x06;
    TL0 = 0x06;
    ET0 = 1;
    TR0 = 1;
    IT0 = 1;
    
    P0 = 0xFF;
    P1 = 0xFF;
    P2 = 0xFF;
    P3 = 0xFF;
    
    random_number = 187;
}

void fsm_update() {
    switch (state) {
        case IDLE:
            if (button_clicked) {
                set_counting_down(5);
                set_state(PREPARING);
            }
            break;

        case PREPARING:
            if (tick >= 4000) {
                counting_down--;
                if (counting_down == 0) {
                    set_score(0);
                    set_live(3);
                    set_state(WAIT);
                } else {
                    set_counting_down(counting_down);
                    set_state(PREPARING);
                }
            }
            break;

        case WAIT:
            if (tick >= waiting_time) {
                set_mole_pattern();
                if (num_mole_is_odd) {
                    set_state(MOLE_ODD);
                } else {
                    set_state(MOLE_EVEN);
                }
            } else if (button_clicked) {
                set_state(MISS);
            }
            break;

        case MISS:
            if (tick >= 5000) {
                P2 |= 0x30;
                set_live(live-1);
                if (live == 0) {
                    set_state(FINISH);
                } else {
                    set_state(WAIT);
                }
            }
            break;

        case HIT:
            if (tick >= 5000) {
                P2 |= 0x30;
                set_score(score+1);
                if (score >= 80) {
                    set_state(FINISH);
                } else {
                    set_state(WAIT);
                }
            }
            break;

        case MOLE_ODD:
            if (button_clicked) {
                set_state(HIT);
            } else if (tick >= 2800) {
                set_state(MISS);
            }
            break;

        case MOLE_EVEN:
            if (tick > 2800) {
                set_state(HIT);
            } else if (button_clicked) {
                set_state(MISS);
            }
            break;

        case FINISH:
            if (tick >= 8000) {
                counting_down = 5;
                P0 = 0xFF;
                P1 = 0xFF;
                set_state(END);
            }
            break;

        case END:
            if(tick >= 2000){
                counting_down--;
                if(counting_down == 0){
                    set_score(score);
                    set_state(IDLE);
                }else{
                    if(counting_down%2){
                        P0 = 0xFF;
                        P1 = 0xFF;
                    }else{
                        set_score(score);
                    }
                    set_state(END);
                }
            }
            break;
    }
}

void main() {
    init();
    while (1) {
        fsm_update();
        delay(10);
        LFSR();
    }
}