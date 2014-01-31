#ifndef __BESPECKLE_H__
#define __BESPECKLE_H__

#include <stdint.h>
#ifndef NOHARDWARE
//#include "led_strip_driver.h"
#else
// Defined STRIP_LENGTH in effects.c

#endif

#define RGBA_R_SHIFT 5
#define RGBA_R_MASK  (0x1f << RGBA_R_SHIFT) // 5 bits
#define RGBA_G_SHIFT 0
#define RGBA_G_MASK  (0x1f << RGBA_G_SHIFT) // 5 bits
#define RGBA_B_SHIFT 10
#define RGBA_B_MASK  (0x1f << RGBA_B_SHIFT) // 5 bits


#define HSVA_H_MASK  0xFC00 // 6 bits
#define HSVA_H_SHIFT 10
#define HSVA_S_MASK  0x0380 // 3 bits
#define HSVA_S_SHIFT 7
#define HSVA_V_MASK  0x0070 // 3 bits
#define HSVA_V_SHIFT 4
#define HSVA_A_MASK  0x000F // 4 bits
#define HSVA_A_SHIFT 0

// Number of fracticks per tick
// sizeof(fractick_t) > TICK_LENGTH
#define TICK_LENGTH  240


//midway point
#define HALF_LENGTH (STRIP_LENGTH/2)

// Default color with no effects (black)
#define RGB_EMPTY    0x8000

// Default color with no effects (black) for RGBA
#define RGBA_EMPTY   (RGBA){0, 0, 0, 0xFF}

// CommandsA
#define FLAG_CMD     0x80
#define FLAG_CMD_MSG 0x40 // This gives 6 bits for additional msg parameters

#define CMD_SYNC     0x80
#define CMD_MSG      0x81
#define CMD_STOP     0x82

#define CMD_RESET    0x83
#define CMD_REBOOT   0x84 // TODO
#define CMD_PARAM    0x85

#define CMD_TICK     0x88

#define CONTINUE     0
#define STOP         1

#define FOUND        0
#define NOT_FOUND    1

//#define NULL         0

// Reserve PARAM_LEN bytes for global 'parameters'
#define PARAM_LEN    4


// Packed RGB value, as sent to the LED strip. 1BBB BBRR RRRG GGGG
typedef uint16_t rgb_t;
// Fractional Tick, out of TICK_LENGTH. If TICK_LENGTH >= 255, use uint16_t
typedef uint8_t fractick_t; // sizeof(fractick_t) > TICK_LENGTH
// Position on light strip (in pixels)
typedef uint8_t position_t; // sizeof(position_t) > STRIP_LENGTH
// Boolean
typedef uint8_t bool_t;

// Internal representation of RGBA values
// XXX Subject to change when optimized!
typedef struct {
		uint8_t r;
		uint8_t g;
		uint8_t b;
		uint8_t a;
} rgba_t;

// Representation of HSV as sent over the wire
// XXX Subject to change when optimized!
typedef struct {
	uint8_t h;
	uint8_t s;
	uint8_t v;
	uint8_t a;
	/*
	unsigned h:8;
	unsigned s:5;
	unsigned v:5;
	unsigned a:5;
	*/
} hsva_t;

void init_effects_heap(void);

// Convert between different color formats
rgb_t pack_rgba(rgba_t);
rgba_t unpack_rgb(rgb_t);
rgba_t hsva_to_rgba(hsva_t);

// Mix/composite color values according to alpha channel
rgba_t mix_rgba(rgba_t, rgba_t);
rgb_t mix_rgb(rgba_t, rgb_t);
rgb_t filter_rgb(rgb_t, uint8_t, uint8_t, uint8_t, uint8_t);


// Structure of incomming CAN packets
// sizeof(canpacket_t) == 8
#define CAN_DATA_SIZE 6
typedef struct {
	uint8_t cmd;
	uint8_t uid;
	uint8_t data[CAN_DATA_SIZE];
} canpacket_t;

void message(canpacket_t*);

// Base struct for an Effect
// All effects MUST start with these pointers
// `next` is used as a pointer to the next Effect in the linked list
// `tick` is a function called multiple times per beat to update the effect
// `pixel` is a function called to get the color value of a single pixel

struct EffectTable;

typedef struct Effect {
	struct Effect * next;
	struct EffectTable* table;
	uint8_t uid;
	uint8_t data[32] __attribute__ ((aligned(4))); // I'm a bad person XXX
} Effect;

typedef struct EffectTable {
	uint8_t eid;
	uint8_t size;
	void (* setup)(struct Effect *, canpacket_t*);
	bool_t (* tick)(struct Effect *, fractick_t);
	rgba_t (* pixel)(struct Effect *, position_t);
	bool_t (* msg)(struct Effect *, canpacket_t*);
} EffectTable;

typedef struct tick_t {
    uint32_t tick:24;
    fractick_t frac:8;
} tick_t;

extern tick_t clock;

void time_add(tick_t*, uint32_t, uint8_t);
int32_t time_sub(tick_t, tick_t);

// Calls `tick` on every Effect in the linked list;
// Removes Effects from the list that have nonzero return values when fractick == 0
// Always calls tick with fractick = 0 for every beat
Effect* tick_all(Effect*, fractick_t, uint8_t);

// Composites a list of effects into a single set of packed pixels
void compose_all(Effect*, rgb_t*);
void populate_strip(rgb_t*);

// Sends (continuation) message to the correct Effect
Effect* msg_all(Effect*, canpacket_t*);

// Remove an Effect from the Effect stack via uid
void pop_effect(Effect**, uint8_t);

// Add Effect ot the top of an Effect stack
void push_effect(Effect**, Effect*);

// Free effect memory. Malloc is part of creating an effect from a CAN msg
void free_effect(Effect*);

#endif
