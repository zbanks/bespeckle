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

#define NULL         0

typedef uint16_t rgb_t;
typedef uint16_t hsva_t;
typedef uint8_t fractick_t; // sizeof(fractick_t) > TICK_LENGTH
typedef uint8_t position_t; // sizeof(position_t) > STRIP_LENGTH

class RGBA {
	public:
		uint8_t r;
		uint8_t g;
		uint8_t b;
		uint8_t a;

		rgb_t pack(void);
		void unpack(rgb_t);
		void mix(RGBA);
		rgb_t mixOn(rgb_t);
};

class HSVA {
	public:
		uint8_t h;
		uint8_t s;
		uint8_t v;
		uint8_t a;
		
		RGBA toRGBA(void);

		hsva_t pack(void);
		void unpack(hsva_t);
};

class Effect {
	public:
		Effect* next;
		uint8_t id;
		uint8_t type;

		virtual bool tick(fractick_t);
		//virtual rgb_t pixel_packed(position_t); // No alpha, no compositing?
		virtual RGBA pixel_rgba(position_t);
		virtual HSVA pixel_hsva(position_t);
};

Effect* tick_all(Effect*, fractick_t);
void compose(Effect*, rgb_t*);
