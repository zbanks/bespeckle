#include "bespeckle.h"
#include "string.h"

// Number of effects 
#define NUM_EFFECTS  6

/* Structs used to store data for effects
 * Useful when you need to store more than a single value, and don't want to explicity code
 * out the pointer arithmetic. Makes code more readable.
 */

typedef struct edata_char4 {
    uint8_t xs[4];
} edata_char4;

typedef struct edata_rgba1_char4 {
    rgba_t cs[1]; 
    uint8_t xs[4];
} edata_rgba1_char4;

/* Effect functions
 *
 * void setup(Effect*, canpacket_t*)
 *  Called when the effect is first created. Space for data has been allocated, but not initialized
 *  The first argument is the effect struct.
 *
 * bool_t tick(Effect*, fractick_t) 
 *  Called on a 'tick', or fraction of a beat from 0-239. Tick 0 is *always* called once per beat.
 *  Return `CONTINUE` or `STOP`. `STOP` means the effect is done and can be removed from the stack.
 *
 * rgba_t pixel(Effect*, position_t)
 *  Called once per pixel once per frame. Should be *very* fast!!!
 *  The second argument is the index of the pixel along the strip
 *
 * bool_t msg(Effect*, canpacket_t*)
 *  Called when the controller recieves a message for an existing effect.
 *  The unmodified packet is sent as a second argument.
 *
 */

// setup - Treat the data as an HSVA value, convert it to RGBA, and store it in the effect data
void _setup_one_color(Effect* eff, canpacket_t* data){

    *(rgba_t*) eff->data = hsva_to_rgba(*(hsva_t*) (data->data));
}

// setup - Copy the 6 bytes from the packet into the effect data
void _setup_copy(Effect* eff, canpacket_t* data){
    memcpy(eff->data, data->data, 6);
}

// tick - do nothing, never stop.
bool_t _tick_nothing(Effect* eff, fractick_t ft){
    return CONTINUE;
}

// tick - increment the first byte of the effect data, never stop
bool_t _tick_increment(Effect* eff, fractick_t ft){
    edata_char4 *edata = (edata_char4*)eff->data;
    if(ft == 0){
        edata->xs[0]++;
    }
    return CONTINUE;
}

// tick - increment the 5th byte of the effect data mod STRIP_LENGTH, never stop
// effect data holds an RGBA value followed by a counter
bool_t _tick_inc_chase(Effect* eff, fractick_t ft){
    edata_rgba1_char4 *edata = (edata_rgba1_char4*)eff->data;
    if(ft == 0){
        if(edata->xs[0] == 0 || edata->xs[0] + (edata->xs[1] & 0x7f) >= STRIP_LENGTH){
            edata->xs[1] ^= 0x80;
        }
        edata->xs[0] += edata->xs[1] & 0x80 ? -1 : 1;
    }
    if(edata->xs[1] & 0x80){
        edata->xs[2] = edata->cs[0].a * ft / 240;
        edata->xs[3] = edata->cs[0].a - edata->xs[2];
    }else{
        edata->xs[3] = edata->cs[0].a * ft / 240;
        edata->xs[2] = edata->cs[0].a - edata->xs[3];
    }
    return CONTINUE;
}

// tick - flash the alpha channel on every beat, never stop
bool_t _tick_flash(Effect* eff, fractick_t ft){
    if(ft == 0){
        rgba_t * rgba = (rgba_t*) eff->data;
        rgba->a ^= 0xff;
    }
    return CONTINUE;
}

// pixel - solid color across the strip: the first bytes of effect data
rgba_t _pixel_solid(Effect* eff, position_t pos){
    return *((rgba_t*) eff->data);
}

// pixel - similar to _pixel_solid, but with inverted colors every 3 pixels
rgba_t _pixel_stripe(Effect* eff, position_t pos){
    if(pos % 3){
        rgba_t rgba = *((rgba_t*) eff->data);
        rgba.r ^= 0xff;
        rgba.g ^= 0xff;
        rgba.b ^= 0xff;
        return rgba;
    }else{
        return *((rgba_t*) eff->data);
    }
}

// pixel - clear in most pixels, but the stored color at a given position (see _tick_inc_chase)
rgba_t _pixel_chase(Effect* eff, position_t pos){
    const static rgba_t clear = {0,0,0,0};
    edata_rgba1_char4 *edata = (edata_rgba1_char4*)eff->data;
    rgba_t color = edata->cs[0];

    if(pos == edata->xs[0]){
        color.a = edata->xs[2];
        return color;
    }else if(pos > edata->xs[0] && pos < edata->xs[0] + (edata->xs[1] & 0x7f)){
        return color;
    }else if(pos == edata->xs[0] + (edata->xs[1] & 0x7f)){
        color.a = edata->xs[3];
        return color;
    }
    return clear;
}

// pixel - rainbow! first byte of effect data is offset, second byte is 'rate' and multiplied by position.
rgba_t _pixel_rainbow(Effect* eff, position_t pos){
    static hsva_t color = {0x00, 0xff, 0xff, 0xff};
    edata_char4 *edata = (edata_char4*)eff->data;
    color.h = (edata->xs[0] * edata->xs[1] + pos * edata->xs[2]) & 0xff;
    //color.h = pos;
    return hsva_to_rgba(color);
}

// pixel - color across the strip where xs[0] <= pos <= xs[1]. Useful for vu meter
rgba_t _pixel_vu(Effect* eff, position_t pos){
    const static rgba_t clear = {0,0,0,0};
    edata_rgba1_char4 *edata = (edata_rgba1_char4*)eff->data;
    if(edata->xs[0] <= pos && pos <= edata->xs[1]){
        return edata->cs[0];
    }else{
        return clear;
    }
}

// msg - do nothing, continue
bool_t _msg_nothing(Effect* eff, canpacket_t* data){
    return CONTINUE;
}

// msg - do nothing, stop
bool_t _msg_stop(Effect* eff, canpacket_t* data){
    return STOP;
}

// msg - copy first 4 bytes to xs; use 5th byte for stop
bool_t _msg_store_char4(Effect* eff, canpacket_t* data){
    edata_char4 *edata = (edata_char4*)eff->data;
    if(data->data[5]){
        return STOP;
    }
    memcpy(edata->xs, data->data, 4);
    return CONTINUE;
}

/* End Effect Functions */

/* Effect Table containing all the possible effects & their virtual functions
 * id - effect id. enables a device to not implement a particular effect. must be unique
 * size - size of `data` array in the effect struct. How much data does the effect need?
 * setup, tick, pixel, msg - functions, as described above
 *
 *  id  size                          setup             tick             pixel           msg
 */
EffectTable const effect_table[NUM_EFFECTS] = {
    // Solid color 
    {0, sizeof(rgba_t),               _setup_one_color, _tick_nothing,   _pixel_solid,   _msg_stop},
    // Flash solid                   
    {1, sizeof(rgba_t),               _setup_one_color, _tick_flash,     _pixel_solid,   _msg_stop},
    // Stripes                       
    {2, sizeof(rgba_t),               _setup_one_color, _tick_nothing,   _pixel_stripe,  _msg_stop},
    // Rainbow!                      
    {3, 6,                            _setup_copy,      _tick_increment, _pixel_rainbow, _msg_stop},
    // Chase
    {4, sizeof(edata_rgba1_char4),    _setup_copy,      _tick_inc_chase, _pixel_chase,   _msg_stop},
    // VU meter
    {5, sizeof(edata_rgba1_char4),    _setup_copy,      _tick_nothing,   _pixel_vu,      _msg_store_char4},
};

