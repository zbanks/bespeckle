#include "bespeckle.h"

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

    *(rgba_t*) eff->data = (rgba_t) hsva_to_rgba(*(hsva_t*) (data->data));
}

// setup - Copy the 6 bytes from the packet into the effect data
void _setup_copy(Effect* eff, canpacket_t* data){
    //TODO optimize
    int i;
    for(i = 0; i < 6; i++){
        *((uint8_t *) eff->data + i) = *((uint8_t*) data->data + i);
    }
}

// tick - do nothing, never stop.
bool_t _tick_nothing(Effect* eff, fractick_t ft){
    return CONTINUE;
}

// tick - increment the first byte of the effect data, never stop
bool_t _tick_increment(Effect* eff, fractick_t ft){
    if(ft == 0){
        *((uint8_t *) eff->data) = 1 + *((uint8_t *) eff->data);
    }
    return CONTINUE;
}

// tick - increment the 5th byte of the effect data mod STRIP_LENGTH, never stop
// effect data holds an RGBA value followed by a counter
bool_t _tick_inc_chase(Effect* eff, fractick_t ft){
    if(ft == 0){
        *((uint8_t *) eff->data + sizeof(rgba_t)) = (1 + *((uint8_t *) eff->data + sizeof(rgba_t))) % STRIP_LENGTH;
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
    static rgba_t clear = {0,0,0,0};
    if(pos == *((uint8_t*) eff->data + sizeof(rgba_t))){
        return *((rgba_t*) eff->data);
    }
    return clear;
}

// pixel - rainbow! first byte of effect data is offset, second byte is 'rate' and multiplied by position.
rgba_t _pixel_rainbow(Effect* eff, position_t pos){
    static hsva_t color = {0x00, 0xff, 0xff, 0xff};
    color.h = (*((uint8_t*) eff->data) + pos * *((uint8_t*) eff->data + 1) ) & 0xff;
    //color.h = pos;
    return hsva_to_rgba(color);
}

// msg - do nothing, continue
bool_t _msg_nothing(Effect* eff, canpacket_t* data){
    return CONTINUE;
}

/* End Effect Functions */

/* Effect Table containing all the possible effects & their virtual functions
 * id - effect id. enables a device to not implement a particular effect. must be unique
 * size - size of `data` array in the effect struct. How much data does the effect need?
 * setup, tick, pixel, msg - functions, as described above
 *
 *  id  size           setup             tick           pixel         msg
 */
EffectTable effect_table[NUM_EFFECTS] = {
    // Solid color 
    {0, sizeof(rgba_t), _setup_one_color, _tick_nothing, _pixel_solid, _msg_nothing},
    // Flash solid
    {1, sizeof(rgba_t), _setup_one_color, _tick_flash, _pixel_solid, _msg_nothing},
    // Stripes
    {2, sizeof(rgba_t), _setup_one_color, _tick_nothing, _pixel_stripe, _msg_nothing},
    // Rainbow!
    {3, 2,              _setup_copy, _tick_increment, _pixel_rainbow, _msg_nothing},
    // Chase
    {4, sizeof(rgba_t)+1, _setup_copy, _tick_inc_chase, _pixel_chase, _msg_nothing},
};

