#include "bespeckle.h"

#include <stdlib.h>

bool_t _tick_nothing(Effect* eff, fractick_t ft){
    return CONTINUE;
}

rgba_t _pixel_solid(Effect* eff, position_t pos){
    return *((rgba_t*) (eff->data));
}

void _msg_nothing(Effect* eff, canpacket_t* data){
    return;
}

EffectTable effect_table[NUM_EFFECTS] = {
    // Solid color 
    {0, sizeof(rgba_t), _tick_nothing, _pixel_solid, _msg_nothing}
};

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

void message(canpacket_t* data){
    if(data->cmd & COMMAND_FLAG){
        switch(data->cmd){
            default:
            break;
        }
    }else{

    }
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

bool_t msg_all(Effect* eff, canpacket_t* data){
    for(; eff; eff = eff->next){
        if(eff->uid == data->uid){
            eff->table->msg(eff, data);
            return FOUND;
        }
    }
    return NOT_FOUND;
}

void push_effect(Effect* stack, Effect* eff){
    // Add to end of stack
    for(; stack->next; stack = stack->next){
        if(stack->next->uid == eff->uid){
            // Existing stack element with same uid; remove it
            free_effect(stack->next);
            break;
        }
    }
    stack->next = eff;
}

inline void free_effect(Effect* eff){
    // Deallocate space for effect
    free(eff);
}


int main(){ 
    return 0; 
}
