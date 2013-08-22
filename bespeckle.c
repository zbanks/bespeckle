#include "bespeckle.h"
#include "effects.c"

#include <stdlib.h>

// Effects stack (initially empty)
Effect* effects = NULL;

/* Begin color functions */
rgb_t pack_rgba(rgba_t in){
    return ((in.r >> 3 << RGBA_R_SHIFT) & RGBA_R_MASK) | 
           ((in.g >> 3 << RGBA_G_SHIFT) & RGBA_G_MASK) | 
           ((in.b >> 3 << RGBA_B_SHIFT) & RGBA_B_MASK) |
           RGB_EMPTY;
}

rgba_t unpack_rgb(rgb_t packed){
    rgba_t out;
    //FIXME
#define FIVE(x) ((x << 3) | (x >> 2))
    out.r = (uint8_t) FIVE((packed & RGBA_R_MASK) >> RGBA_R_SHIFT);
    out.g = (uint8_t) FIVE((packed & RGBA_G_MASK) >> RGBA_G_SHIFT);
    out.b = (uint8_t) FIVE((packed & RGBA_B_MASK) >> RGBA_B_SHIFT);
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
    // This can be *very* optimized once we decide on sizeof(a), etc TODO
    rgb_t out = RGB_EMPTY;
    out |= (((bot & RGBA_R_MASK) * (0xff - top.a) + ((top.r * top.a) << RGBA_R_SHIFT >> 3)) / 0xff) & RGBA_R_MASK;
    out |= (((bot & RGBA_G_MASK) * (0xff - top.a) + ((top.g * top.a) << RGBA_G_SHIFT >> 3)) / 0xff) & RGBA_G_MASK;
    out |= (((bot & RGBA_B_MASK) * (0xff - top.a) + ((top.b * top.a) << RGBA_B_SHIFT >> 3)) / 0xff) & RGBA_B_MASK;
    return out;
}

rgba_t hsva_to_rgba(hsva_t in){
    // Convert HSVA to RGBA.  Hue from 0-254, sat, val, & alpha are 5 bit (0-31)
    // TODO: Optimize for the values we actually have
    //
    rgba_t out = {0, 0, 0, in.a};

    uint8_t hue = in.h;
    uint8_t sat = (in.s << 3) | (in.s >> 2);
    uint8_t val = (in.v << 3) | (in.v >> 2);

    if (hue == 255) hue = 254;
    uint16_t chroma = (val) * (sat);
    uint16_t m = 255*(val) - chroma;
    signed long X =(42-abs((hue)%85-42));
    X *= chroma;
    
    uint8_t x8b = X/(255 * 42);
    uint8_t c8b = chroma/255;
    uint8_t m8b = m/255;
    
    if (hue < 42) {
        out.r = c8b + m8b;
        out.g = x8b + m8b;
        out.b = m8b;
    } else if (hue < 84) {
        out.r = x8b + m8b;
        out.g = c8b + m8b;
        out.b = m8b;
    } else if (hue < 127) {
        out.r = m8b;
        out.g = c8b + m8b;
        out.b = x8b + m8b;
    } else if (hue < 169) {
        out.r = m8b;
        out.g = x8b + m8b;
        out.b = c8b + m8b;
    } else if (hue < 212) {
        out.r = x8b + m8b;
        out.g = m8b;
        out.b = c8b + m8b;
    } else {
        out.r = c8b + m8b;
        out.g = m8b;
        out.b = x8b + m8b;
    }
    return out;
}

/* End color functions */

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
            if(eff->table->tick(eff, ft)){
                if(prev != NULL){ // && start == eff
                    // In the middle/end
                    prev->next = eff->next;
                    free_effect(eff);
                    eff = prev->next;
                }else{
                    // At the beginning
                    start = eff->next;
                    free_effect(eff);
                    eff = start;
                }
            }else{
                prev = eff;
                eff = eff->next;
            }
        }
    }else{
        // Simple iteration
        while(eff){
            eff->table->tick(eff, ft);
            eff = eff->next;
        }
    }
    return start;
}

void compose_all(Effect* eff, rgb_t* strip){ 
    // Compose a list of effects onto a strip
    Effect* eff_head = eff; // keep reference to head of stack
    position_t i;
    
    for(i = 0; i < STRIP_LENGTH; i++, strip++){
        *strip = RGB_EMPTY;
        for(eff = eff_head; eff; eff = eff->next){
            *strip = mix_rgb(eff->table->pixel(eff, i), *strip);
        }
    }
}

void populate_strip(rgb_t* strip){
    compose_all(effects, strip);
}

Effect* msg_all(Effect* eff, canpacket_t* data){
    // Pass on canpacket data to matching effect
    // Return new top of stack
    Effect* prev = NULL;
    Effect* start = eff;

    while(eff){
        if(eff->uid == data->uid){ // Match uid
            if(eff->table->msg(eff, data)){ // Send message
                // The effect asked to quit
                if(prev != NULL){
                    // In the middle/end
                    prev->next = eff->next;
                    free_effect(eff);
                    eff = prev->next;
                }else{
                    // At the beginning
                    start = eff->next;
                    free_effect(eff);
                    eff = start;
                }
            }
            return start;
        }else{
            prev = eff;
            eff = eff->next;
        }
    }
    return start;
}

void push_effect(Effect** stack, Effect* eff){
    // XXX Clean me :(
    Effect * _stack = *stack;
    Effect * last_stack = NULL;
    // Add to end of stack
    if(*stack == NULL){
        // If the stack was empty, that was easy
        *stack = eff;
        return;
    }

    for(; _stack; _stack = _stack->next){
        // Loop until we get to the end of the stack, or we find one to replace
        if(_stack->uid == eff->uid){
            // Existing stack element with same uid; remove it
            free_effect(_stack);
            if(last_stack == NULL){
                *stack = eff;
            }else{
                last_stack->next = eff;
            }
            return;
        }
        last_stack = _stack;
    }
    last_stack->next = eff;
}

void free_effect(Effect* eff){
    // Deallocate space for effect
    free(eff);
}

void message(canpacket_t* data){
    Effect* e;
    if(data->cmd & FLAG_CMD){
        switch(data->cmd){
            case CMD_TICK:
                effects = tick_all(effects, data->uid);
            break;
            case CMD_MSG:
                effects = msg_all(effects, data);
            break;
            case CMD_RESET:
                // Reset strip, remove all effects
                while(effects){
                    e = effects;
                    effects = e->next;
                    free_effect(e);
                }
            break;
            default:
            break;
        }
    }else{
        int i;
        for(i = 0; i < NUM_EFFECTS; i++){
            if(effect_table[i].eid == data->cmd){
                // Found a match. Attempt to malloc
                // TODO: this might be 1-4 bytes larger than nessassary? 
                Effect* eff = malloc(sizeof(Effect) + effect_table[i].size);
                if(eff == NULL){
                    // malloc failed! :(
                    return;
                }
                // Setup effect; add to stack
                eff->uid = data->uid;
                eff->table = (EffectTable*)(effect_table+i);
                effect_table[i].setup(eff, data);
                eff->next=NULL;
                push_effect(&effects, eff);
            }
        }
    }
}
