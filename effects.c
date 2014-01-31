#include "bespeckle.h"
#include "effects.h"
#include "string.h"

#ifdef NOHARWARE
#include <stdio.h>
#define DEBUG(x...) fprintf(stderr, x)
#else
#define DEBUG(x...) 
#endif

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

typedef struct edata_rgba1_char4_int4 {
    rgba_t cs[1];
    uint8_t xs[4];
    uint32_t ys[4];
} edata_rgba1_char4_int4;

typedef struct edata_rgba1_char4_time1 {
    rgba_t cs[1];
    uint8_t xs[4];
    tick_t ts[1];
} edata_rgba1_char4_time1;

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

// setup - Copy the bytes from the packet into the effect data. Checks size! Zeros everything else
void _setup_copy(Effect* eff, canpacket_t* data){
    if(eff->table->size < CAN_DATA_SIZE){
        memcpy(eff->data, data->data, eff->table->size);
    }else{
        memcpy(eff->data, data->data, CAN_DATA_SIZE);
        // Set remaining bits to 0 since the uC won't do that for us
        memset(eff->data + CAN_DATA_SIZE, 0x00, eff->table->size - CAN_DATA_SIZE);
    }
}

// setup - Setup pulse by 
void _setup_pulse(Effect* eff, canpacket_t* data){
    edata_rgba1_char4_int4 *edata = (edata_rgba1_char4_int4 *) eff->data;
    memcpy(eff->data, data->data, CAN_DATA_SIZE);
    memset(eff->data + CAN_DATA_SIZE, 0x00, eff->table->size - CAN_DATA_SIZE);

    edata->xs[0] = 1;

    if(edata->xs[1] & 0x8){
        edata->ys[3] = 0x1;
        edata->ys[1] = 0x1;
    }else{
        edata->ys[2] = (0x1) << (STRIP_LENGTH - 33);
        edata->ys[0] = (0x1) << (STRIP_LENGTH - 33);
    }
    //_tick_fadeacross(eff, 0);

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
    edata->xs[3] = ft;
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

bool_t _tick_inc_spr(Effect* eff, fractick_t ft){
    edata_rgba1_char4 *edata = (edata_rgba1_char4 *) eff->data;
    if(ft == 0){
        if(edata->xs[0] >= STRIP_LENGTH){
            edata->xs[0] = 0;
        } else {
            edata->xs[0]++;
        }
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

// tick - fade in. xs[1] controls rate; xs[0] is state after each beat; x[2] is the value
bool_t _tick_fadein(Effect* eff, fractick_t ft){
    edata_rgba1_char4 *edata = (edata_rgba1_char4*)eff->data;
    uint8_t last_val = edata->xs[0];
    int val;
    // Approximate as 255 ticks/beat
    // This is a good template for timing 
    if(last_val != 0xff){
        if(ft == 0){
            edata->xs[0] += 0xff >> (edata->xs[1] & 0x7);
        }
        val = edata->xs[0] + (ft >> (edata->xs[1] & 0x7));

        if(val >= 0xff || edata->xs[0] < last_val){
            // Overflow; stop the fade 
            edata->xs[0] = 0xff;
            edata->xs[2] = 0xff;
        }else{
            edata->xs[2] = val & 0xff;
        }
    }
    return CONTINUE;
}

// tick - pulse. ys[0] is the state used by _pixel_pulse, ys[1] is the state since the last full beat. 
//        xs[1] is the rate, xs[2] is the alpha for pixels where the pulse is
bool_t _tick_pulse(Effect* eff, fractick_t ft){
    edata_rgba1_char4_int4 *edata = (edata_rgba1_char4_int4 *) eff->data;
    uint8_t rate = 1 << (edata->xs[1] & 0x7);
    uint8_t mv;
    if(ft == 0){
        mv = (0xff << 3) / (STRIP_LENGTH * rate);
        if(edata->xs[1] & 0x8){
            edata->ys[2] <<= mv;
            edata->ys[2] |= (edata->ys[3] >> (32 - mv));
            edata->ys[3] <<= mv;
        }else{
            edata->ys[3] >>= mv;
            edata->ys[3] |= (edata->ys[2] << (32 - mv));;
            edata->ys[2] >>= mv;
        }
        edata->ys[0] = edata->ys[2];
        edata->ys[1] = edata->ys[3];
        edata->xs[2] = ((uint32_t) edata->cs[0].a * (0xff % (STRIP_LENGTH * rate)) / (STRIP_LENGTH * rate));
        //edata->xs[3] = edata->cs[0].a - edata->xs[2];
    }else{
        mv = ((uint16_t) ft << 3) / (STRIP_LENGTH * rate);
        if(edata->xs[1] & 0x8){
            edata->ys[0] = edata->ys[2] << mv;
            edata->ys[1] = edata->ys[3] << mv;
            //XXX
            if(mv > 0){
                edata->ys[0] |= (edata->ys[3] >> (32 - mv));
            }
        }else{
            edata->ys[0] = edata->ys[2] >> mv;
            edata->ys[1] = edata->ys[3] >> mv;
            // Fuck it; #yolo XXX
            if(mv > 0){
                edata->ys[1] |= (edata->ys[2] << (32 - mv));
            }
            //edata->ys[1] |= (edata->ys[2] << (31));
        }
        edata->xs[2] = ((uint32_t) edata->cs[0].a * (ft % (STRIP_LENGTH * rate)) / (STRIP_LENGTH * rate));
        //edata->xs[3] = edata->cs[0].a - edata->xs[2];
    }
    return CONTINUE;
}

bool_t _tick_fadeacross(Effect* eff, fractick_t ft){
    edata_rgba1_char4_int4 *edata = (edata_rgba1_char4_int4 *) eff->data;
    uint8_t rate = 1 << (edata->xs[1] & 0x7);
    uint8_t l;
    if(ft == 0){
        l = ((240 << 3) / (STRIP_LENGTH * rate));
        if(edata->xs[1] & 0x8){
            for(; l; l--){
                edata->ys[2] = edata->ys[2] | (edata->ys[2] << 1) | (edata->ys[3] >> 31);
                edata->ys[3] = edata->ys[3] | (edata->ys[3] << 1);
            }
        }else{
            for(; l; l--){
                edata->ys[3] = edata->ys[3] | (edata->ys[3] >> 1) | (edata->ys[2] << 31);
                edata->ys[2] = edata->ys[2] | (edata->ys[2] >> 1);
            }
        }
        edata->ys[0] = edata->ys[2];
        edata->ys[1] = edata->ys[3];
    }else{
        l = ((uint16_t) ft << 3) / (STRIP_LENGTH * rate);
        edata->ys[0] = edata->ys[2];
        edata->ys[1] = edata->ys[3];
        if(edata->xs[1] & 0x8){
            for(; l; l--){
                edata->ys[0] = edata->ys[0] | (edata->ys[0] << 1) | (edata->ys[1] >> 31);
                edata->ys[1] = edata->ys[1] | (edata->ys[1] << 1);
            }
        }else{
            for(; l; l--){
                edata->ys[1] = edata->ys[1] | (edata->ys[1] >> 1) | (edata->ys[0] << 31);
                edata->ys[0] = edata->ys[0] | (edata->ys[0] >> 1);
            }
        }
        edata->xs[2] = (((uint32_t) edata->cs[0].a * ((rate * ft) % STRIP_LENGTH)) / STRIP_LENGTH);
        edata->xs[3] = edata->cs[0].a - edata->xs[2];
    }
    return CONTINUE;
}

// tick - strobes
bool_t _tick_strobe(Effect* eff, fractick_t ft){
    edata_rgba1_char4 *edata = (edata_rgba1_char4 *) eff->data;
    edata->xs[2] = ft;
    if(ft == 0){
        edata->xs[3]++;
    }
    return CONTINUE;
}

// tick - decrement xs[0] for each fractick
bool_t _tick_subdecrement(Effect* eff, fractick_t ft){
    edata_rgba1_char4 *edata = (edata_rgba1_char4 *) eff->data;
    if(edata->xs[2] > ft){
        edata->xs[0] = 240 - (edata->xs[2] - ft) ;
    }else{
        edata->xs[0] = ft - edata->xs[2] ;
    }
    edata->xs[2] = ft;
    return CONTINUE;
}

// tick - die if clock > time
bool_t _tick_timeout(Effect* eff, fractick_t ft){
    edata_rgba1_char4_time1 *edata = (edata_rgba1_char4_time1 *) eff->data;
    //if(((uint32_t) edata->ts[0]) > ((uint32_t) clock)){
    if(!edata->xs[3]){
        // Start
        edata->ts[0].frac = clock.frac;
        edata->ts[0].tick = clock.tick;
        time_add(&(edata->ts[0]), edata->xs[0], edata->xs[1]);
        edata->xs[3] = 1;
    }else{
        if(edata->ts[0].tick < clock.tick){
            return STOP;
        }else if(edata->ts[0].tick == clock.tick && edata->ts[0].frac < clock.frac){
            return STOP;
        }
    }
    return CONTINUE;
}

bool_t _tick_timeout_scroll(Effect* eff, fractick_t ft){
    edata_rgba1_char4_time1 *edata = (edata_rgba1_char4_time1 *) eff->data;
    int32_t time_left;
    int32_t time_total;
    int32_t t;
    //if(((uint32_t) edata->ts[0]) > ((uint32_t) clock)){
    if(!edata->xs[2]){
        // Start
        edata->ts[0].frac = clock.frac;
        edata->ts[0].tick = clock.tick;
        time_add(&(edata->ts[0]), edata->xs[0] & 0x7f, edata->xs[1]);
    }
    //if(edata->xs[3] > STRIP_LENGTH){
    time_left = time_sub(edata->ts[0], clock); 
    if(time_left < 0){
        if(eff->table->eid == 0x42){
            return STOP;
        }else{
            edata->xs[3] = (edata->xs[0] & 0x80) ? 0 : 0xff;
            return CONTINUE;
        }
    }

    time_total = (edata->xs[0] & 0x7f) * TICK_LENGTH + edata->xs[1];
    
    //DEBUG("clock: %d, %d\n", clock.tick, clock.frac);
    //DEBUG("end: %d, %d, %d, %d\n", edata->ts[0].tick, edata->ts[0].frac, edata->xs[0], edata->xs[1]);
    //DEBUG("time: %d, %d\n", time_left, time_total);
    if(edata->xs[0] & 0x80){
        t = time_left;
    }else{
        t = time_total - time_left;
    }

    edata->xs[3] = (t * STRIP_LENGTH)  / time_total;
    edata->xs[2] = (((t * STRIP_LENGTH) % time_total) * edata->cs[0].a) / time_total; 
    //edata->xs[2] = edata->cs[0].a;
    
    if(edata->xs[2] < 1){
        edata->xs[2] = 1;
    }
    
    return CONTINUE;
}

bool_t _tick_timeout_fade(Effect* eff, fractick_t ft){
    edata_rgba1_char4_time1 *edata = (edata_rgba1_char4_time1 *) eff->data;
    int32_t time_left;
    int32_t time_total;
    int32_t t;
    //if(((uint32_t) edata->ts[0]) > ((uint32_t) clock)){
    if(!edata->xs[2]){
        // Start
        edata->ts[0].frac = clock.frac;
        edata->ts[0].tick = clock.tick;
        time_add(&(edata->ts[0]), edata->xs[0] & 0x7f, edata->xs[1]);
        edata->xs[2] = edata->cs[0].a;
    }
    time_left = time_sub(edata->ts[0], clock); 
    if(edata->xs[3] == 0xff || time_left < 0){
        edata->xs[3] = 0xff;
        edata->cs[0].a = (edata->xs[0] & 0x80) ? 0 : edata->xs[2];
        return CONTINUE;
    }

    time_total = (edata->xs[0] & 0x7f) * TICK_LENGTH + edata->xs[1];
    if(edata->xs[0] & 0x80){
        t = time_left;
    }else{
        t = time_total - time_left;
    }

    edata->cs[0].a = (t * edata->xs[2])  / time_total;
    //edata->xs[2] = ((((time_total - time_left) * STRIP_LENGTH) % time_total) * edata->cs[0].a) / time_total; 
    //edata->xs[2] = edata->cs[0].a;
    
    if(edata->xs[2] < 1){
        edata->xs[2] = 1;
    }
    
    return CONTINUE;
}


// pixel - solid color across the strip: the first bytes of effect data
rgba_t _pixel_solid(Effect* eff, position_t pos){
    return *((rgba_t*) eff->data);
}

// pixel - solid color across the strip: the first bytes of effect data; alpha controlled by xs[2]
rgba_t _pixel_solid_alpha2(Effect* eff, position_t pos){
    edata_rgba1_char4 *edata = (edata_rgba1_char4*)eff->data;
    rgba_t out = edata->cs[0];
    out.a = ((int) (out.a * edata->xs[2])) >> 8;
    if(edata->xs[1] & 0x8){
        // Fade in opposite direction
        out.a = 0xff - out.a;
    }
    return out;
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
// pixel - 
rgba_t _pixel_ltr(Effect* eff, position_t pos){
    const static rgba_t clear = {0,0,0,0};
    edata_rgba1_char4 *edata = (edata_rgba1_char4*)eff->data;
    rgba_t color = edata->cs[0];

    if(pos < edata->xs[0]){
        return color;
    }
    return clear; 
}

// pixel - 
rgba_t _pixel_rtl(Effect* eff, position_t pos){
    const static rgba_t clear = {0,0,0,0};
    edata_rgba1_char4 *edata = (edata_rgba1_char4*)eff->data;
    rgba_t color = edata->cs[0];

    if(STRIP_LENGTH - pos < edata->xs[0]){
        return color;
    }
    return clear; 
}

rgba_t _pixel_spr(Effect* eff, position_t pos){
    const static rgba_t clear = {0,0,0,0};
    edata_rgba1_char4 *edata = (edata_rgba1_char4*)eff->data;
    rgba_t color = edata->cs[0];

    if(HALF_LENGTH- edata->xs[0]< pos && pos < HALF_LENGTH + edata->xs[0]){
        return color;
    }
    return clear; 
}

rgba_t _pixel_shr(Effect* eff, position_t pos){
    const static rgba_t clear = {0,0,0,0};
    edata_rgba1_char4 *edata = (edata_rgba1_char4*)eff->data;
    rgba_t color = edata->cs[0];

    if(HALF_LENGTH - edata->xs[0] < pos || pos > HALF_LENGTH + edata->xs[0]){
        return color;
    }
    return clear; 
}



// pixel - rainbow! first byte of effect data is offset, second byte is 'rate' and multiplied by position.
rgba_t _pixel_rainbow(Effect* eff, position_t pos){
    static hsva_t color = {0x00, 0xff, 0xff, 0xff};
    edata_char4 *edata = (edata_char4*)eff->data;
    color.h = (edata->xs[0] * edata->xs[1] + (edata->xs[3] / edata->xs[1]) + pos * edata->xs[2]) & 0xff;
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

rgba_t _pixel_pulse(Effect* eff, position_t pos){
    const static rgba_t clear = {0,0,0,0};
    edata_rgba1_char4_int4 *edata = (edata_rgba1_char4_int4*) eff->data;
    //rgba_t color = edata->cs[0];
    static rgba_t color;
    uint32_t cmp = edata->ys[(~(pos >> 5)) & 0x1];
    pos &= 0x1f;

    color.r = edata->cs[0].r;
    color.g = edata->cs[0].g;
    color.b = edata->cs[0].b;
    color.a = edata->cs[0].a;

    if(edata->xs[1] & 0x8){
        if(cmp & (1 << pos)){
            return color;
        }
        if((cmp & ((1 << pos) >> 1))){
            color.a = edata->xs[2];
            return color;
        }
        if(cmp & ((1 << pos) << 1)){
            color.a = edata->cs[0].a - edata->xs[2];
            return color;
        }
    }else{
        if(cmp & (1 << pos)){
            return color;
        }
        if((cmp & ((1 << pos) << 1))){
            color.a = edata->xs[2];
            return color;
        }
        if(cmp & ((1 << pos) >> 1)){
            color.a = edata->cs[0].a - edata->xs[2];
            return color;
        }
    }
    return clear;
}

rgba_t _pixel_er_pulse(Effect* eff, position_t pos){
    const static rgba_t clear = {0,0,0,0};
    edata_rgba1_char4_time1 *edata = (edata_rgba1_char4_time1*) eff->data;
    //rgba_t color = edata->cs[0];
    static rgba_t color;
    uint8_t target = edata->xs[3];

    color.r = edata->cs[0].r;
    color.g = edata->cs[0].g;
    color.b = edata->cs[0].b;
    color.a = edata->cs[0].a;

    if(pos == target){
        return color;
    }
    if(eff->table->eid == 0x42){
        // Chaser
        if(pos == (target+1)){
            color.a = edata->xs[2];
            return color;
        }
        if(pos == (target-1)){
            color.a -= edata->xs[2];
            return color;
        }
    }else{
        // Fade Across
        if(edata->xs[0] & 0x80){
            if(pos >= target){
                return color;
            }
            if(pos == (target-1)){
                color.a -= edata->xs[2];
                return color;
            }
        }else{
            if(pos < target){
                return color;
            }
            if(pos == (target+1)){
                color.a = edata->xs[2];
                return color;
            }
        }
    }
    return clear;
}


// pixel - strobe solid color across the strip
rgba_t _pixel_strobe(Effect* eff, position_t pos){
    const static rgba_t clear = {0,0,0,0};
    edata_rgba1_char4 *edata = (edata_rgba1_char4*)eff->data;
    if(edata->xs[3] % edata->xs[1] == 0 && (edata->xs[0] % edata->xs[2]) < 5){
        return edata->cs[0];
    }
    return clear;
}
// pixel - color on if xs[0] <= xs[2] < xs[1]; else clear
rgba_t _pixel_conditional_range(Effect* eff, position_t pos){
    const static rgba_t clear = {0,0,0,0};
    edata_rgba1_char4 *edata = (edata_rgba1_char4*)eff->data;
    if(edata->xs[0] <= edata->xs[2] && edata->xs[2] < edata->xs[1]){
        return edata->cs[0];
    }
    return clear;
}
// pixel - color on if xs[0] >= 0
rgba_t _pixel_conditional_x1(Effect* eff, position_t pos){
    const static rgba_t clear = {0,0,0,0};
    edata_rgba1_char4 *edata = (edata_rgba1_char4*)eff->data;
    if(edata->xs[0]){
        return edata->cs[0];
    }
    return clear;
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

// msg - copy data bytes over the effect data 
bool_t _msg_copy(Effect* eff, canpacket_t* data){
    if(eff->table->size < CAN_DATA_SIZE){
        memcpy(eff->data, data->data, eff->table->size);
    }else{
        memcpy(eff->data, data->data, CAN_DATA_SIZE);
        // Set remaining bits to 0 since the uC won't do that for us
        memset(eff->data + CAN_DATA_SIZE, 0x00, eff->table->size - CAN_DATA_SIZE);
    }
    return CONTINUE;
}

// msg - copy data bytes over the effect data 
bool_t _msg_copytick(Effect* eff, canpacket_t* data){
    // data->data[0:3] RGBA color
    // data->data[4] # ticks later to start
    // data->data[5] # ticks later to stop
    // eff->data[6] tick number
    memcpy(eff->data, data->data, 4);
    eff->data[4] = data->data[4];
    //eff->data[4] = data->data[4] + eff->data[6];
    //eff->data[5] = data->data[5] + eff->data[6];
    return CONTINUE;
}

bool_t _msg_pulse(Effect* eff, canpacket_t* data){
    edata_rgba1_char4_int4 *edata = (edata_rgba1_char4_int4*) eff->data;
    uint32_t mask = (1 << (data->data[0] + 1)) - 1;

    /*
    // Reverse
    #ifdef NOHARDWARE
        output = ((input * 0x80200802ULL) & 0x0884422110ULL) * 0x0101010101ULL >> 32;
    #else
        __asm__("rbit %0, %1\n" : "=r"(output) : "r"(input));
    #endif
    */
    
    
    if(edata->xs[1] & 0x8){
        edata->ys[3] |= mask;
    }else{
        edata->ys[2] |= mask << (32 - data->data[0] - 1);
    }
    return CONTINUE;
}

bool_t _msg_strobe(Effect* eff, canpacket_t* data){
    edata_rgba1_char4 *edata = (edata_rgba1_char4 *) eff->data;

    switch(data->cmd & 0x7){
        case 0:
            // set color
            memcpy(edata->cs, data->data, 4);
        break;
        case 1:
            // set params
            memcpy(edata->xs, data->data, 2);
        break;
        default:
        break;
    }
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
    {5, sizeof(edata_rgba1_char4),    _setup_copy,      _tick_nothing, _pixel_vu,   _msg_store_char4},
    // expand
    {6, sizeof(edata_rgba1_char4),    _setup_copy,      _tick_inc_spr, _pixel_spr,   _msg_stop},
	//  shrink
	{7, sizeof(edata_rgba1_char4),    _setup_copy,      _tick_inc_spr, _pixel_shr, _msg_stop}, 
	// ltr
	{8, sizeof(edata_rgba1_char4),    _setup_copy,      _tick_inc_spr, _pixel_ltr,    _msg_stop}, 
	//rtl
    {9, sizeof(edata_rgba1_char4),    _setup_copy,      _tick_inc_spr, _pixel_rtl,    _msg_stop}, 
	// scattering
	//{10, sizeof(edata_rgba1_char4),   _setup_copy,      _tick_flash,   _pixel_scat,   _msg_stop), 
	// slide in left(pos)+stop
	//slide in right
	//dashed line ltr
	
	//give all signal for colorchange, speedchange
    // Solid color; RGBA; msg changes color
    {0x10, sizeof(rgba_t),               _setup_copy, _tick_nothing,   _pixel_solid,   _msg_copy},
    // Fade in/out; RGBA; msg changes color; data[5] is start, data[6] is 'rate' & direction
    {0x12, sizeof(edata_rgba1_char4),    _setup_copy, _tick_fadein,    _pixel_solid_alpha2,   _msg_copy},
    // Pulse; RGBA; msg sends pulse; data[5] is nothing, data[6] is 'rate' & direction
    {0x14, sizeof(edata_rgba1_char4_int4), _setup_copy, _tick_pulse,    _pixel_pulse,   _msg_pulse},
    // Fade across; RGBA; msg changes color & sends pulse; data[5] is nothing, data[6] is 'rate' & direction
    // Not efficiently implemented, but lets us reuse a lot of code
    {0x16, sizeof(edata_rgba1_char4_int4), _setup_pulse, _tick_fadeacross,    _pixel_pulse,   _msg_pulse},

    // Strobe; RGBA; msg changes color/rate
    {0x18, sizeof(edata_rgba1_char4), _setup_copy, _tick_strobe, _pixel_strobe, _msg_strobe},
    // Strobe to pattern; RGBA; msg sets color, on & off times
    {0x20, sizeof(edata_rgba1_char4), _setup_copy, _tick_strobe, _pixel_conditional_range, _msg_copy},
    // Solid color for n ticks; msg sets color & on time
    {0x21, sizeof(edata_rgba1_char4), _setup_copy, _tick_subdecrement, _pixel_conditional_x1, _msg_copy},



    // Synchronous events that @ervanalb wants
    // Strobe
    {0x40, sizeof(edata_rgba1_char4_time1), _setup_copy, _tick_timeout, _pixel_solid, _msg_stop},
    // Fade across
    {0x41, sizeof(edata_rgba1_char4_time1), _setup_copy, _tick_timeout_scroll, _pixel_er_pulse, _msg_stop},
    // Chaser
    {0x42, sizeof(edata_rgba1_char4_time1), _setup_copy, _tick_timeout_scroll, _pixel_er_pulse, _msg_stop},
    // Fade in
    {0x43, sizeof(edata_rgba1_char4_time1), _setup_copy, _tick_timeout_fade, _pixel_solid, _msg_stop},
};

