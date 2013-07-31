#include "bespeckle.h"

rgb_t pack_rgba(rgba_t in){
    return (((in.r << RGBA_R_SHIFT) & RGBA_R_MASK) | 
            ((in.g << RGBA_G_SHIFT) & RGBA_G_MASK) | 
            ((in.b << RGBA_B_SHIFT) & RGBA_B_MASK));
}

rgba_t unpack_rgb(rgb_t packed){
    rgba_t out;
    out.r = (uint8_t) ((packed & RGBA_R_MASK) >> RGBA_R_SHIFT);
    out.g = (uint8_t) ((packed & RGBA_G_MASK) >> RGBA_G_SHIFT);
    out.b = (uint8_t) ((packed & RGBA_B_MASK) >> RGBA_B_SHIFT);
    out.a = 0xFF;
    return out;
}

rgba_t mix_rgba(rgba_t top, rgba_t bot){
    // This can be *very* optimized once we decide on sizeof(a), etc
    bot.r = (uint8_t) ((bot.r * (0xff - top.a) + top.r * top.a) / 0xff);
    bot.g = (uint8_t) ((bot.g * (0xff - top.a) + top.g * top.a) / 0xff);
    bot.b = (uint8_t) ((bot.b * (0xff - top.a) + top.b * top.a) / 0xff);
    bot.a = 0xFF; // Alpha gets lost when mixing
    return bot;
}

rgb_t mix_rgb(rgba_t top, rgb_t bot){
    // Mix this color on top of another in packed format
    // This can be *very* optimized once we decide on sizeof(a), etc
    rgb_t out = RGB_EMPTY;
    out |= (((bot & RGBA_R_MASK) * (0xff - top.a) + top.r * top.a) / 0xff) & RGBA_R_MASK;
    out |= (((bot & RGBA_G_MASK) * (0xff - top.a) + top.g * top.a) / 0xff) & RGBA_G_MASK;
    out |= (((bot & RGBA_B_MASK) * (0xff - top.a) + top.b * top.a) / 0xff) & RGBA_B_MASK;
    return out;
}

rgba_t hsva_to_rgba(hsva_t in){
    //TODO
    rgba_t out;
    return out;
}

Effect* tick_all(Effect* eff, fractick_t ft){
    // Send a tick event to every effect
    // If ft is 0, check for deleted effects
    // Return new top of stack
    Effect* prev = NULL;
    Effect* start = eff;
    static fractick_t last_tick = TICK_LENGTH;

    if(last_tick > ft){
        // Don't miss 0
        ft = 0;
    }
    last_tick = ft;

    if(ft == 0){
        while(eff){
            if(eff->tick(eff, ft)){
                if(prev != NULL){ // && start == eff
                    eff = prev->next = eff->next;
                }else{
                    eff = start = eff->next;
                }
            }else{
                prev = eff;
                eff = eff->next;
            }
        }
    }else{
        // Simple iteration
        while(eff){
            eff->tick(eff, ft);
            eff = eff->next;
        }
    }
    return start;
}

void compose(Effect* eff, rgb_t* strip){ 
    // Compose a list of effects onto a strip
    Effect* eff_head = eff; // keep reference to head of stack
    
    for(position_t i = 0; i < STRIP_LENGTH; i++, strip++){
        *strip = RGB_EMPTY;
        for(eff = eff_head; eff; eff = eff->next){
            *strip = mix_rgb(eff->pixel(eff, i), *strip);
        }
    }
}

int main(){ return 0; }
