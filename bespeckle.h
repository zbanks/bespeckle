#include <stdint.h>

#define RGBA_R_MASK  0xF800 // 5 bits
#define RGBA_R_SHIFT 11
#define RGBA_G_MASK  0x0760 // 5 bits
#define RGBA_G_SHIFT 6
#define RGBA_B_MASK  0x003E // 5 bits
#define RGBA_B_SHIFT 1

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

// Length of LED strip
// sizeof(position_t) > STRIP_LENGTH
#define STRIP_LENGTH 25

// Default color with no effects (black)
#define RGB_EMPTY    0xFFFF

// Default color with no effects (black) for RGBA
#define RGBA_EMPTY   (RGBA){0, 0, 0, 0xFF}

// Number of effects 
#define NUM_EFFECTS  1

//
#define COMMAND_FLAG 0x80

#define CONTINUE     0
#define STOP		 1

#define FOUND        0
#define NOT_FOUND    1

#define NULL         0



// Packed RGB value, as sent to the LED strip. RRRRRGGG GGBBBBB0?
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
		unsigned h:8;
		unsigned s:5;
		unsigned v:5;
		unsigned a:5;
} hsva_t;

// Convert between different color formats
rgb_t pack_rgba(rgba_t);
rgba_t unpack_rgb(rgb_t);
rgba_t hsva_to_rgba(hsva_t);

// Mix/composite color values according to alpha channel
rgba_t mix_rgba(rgba_t, rgba_t);
rgb_t mix_rgb(rgba_t, rgb_t);


// Structure of incomming CAN packets
// sizeof(canpacket_t) == 8
typedef struct {
	uint8_t cmd;
	uint8_t uid;
	uint8_t data[6];
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
	uint8_t uid;
	struct EffectTable* table;
	char data[];
} Effect;

typedef struct EffectTable {
	uint8_t eid;
	uint8_t size;
	bool_t (* tick)(struct Effect *, fractick_t);
	rgba_t (* pixel)(struct Effect *, position_t);
	void (* msg)(struct Effect *, canpacket_t*);
} EffectTable;

// Calls `tick` on every Effect in the linked list;
// Removes Effects from the list that have nonzero return values when fractick == 0
// Always calls tick with fractick = 0 for every beat
Effect* tick_all(Effect*, fractick_t);

// Composites a list of effects into a single set of packed pixels
void compose_all(Effect*, rgb_t*);

// Sends (continuation) message to the correct Effect
bool_t msg_all(Effect*, canpacket_t*);

// Add Effect ot the top of an Effect stack
void push_effect(Effect*, Effect*);

// Free effect memory. Malloc is part of creating an effect from a CAN msg
inline void free_effect(Effect*);
