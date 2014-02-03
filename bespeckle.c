#define mallog_getpagesize 256
#define VALUE_TO_STRING(x) #x
#define VALUE(x) VALUE_TO_STRING(x)
#define VAR_NAME_VALUE(var) #var "="  VALUE(var)

#include "bespeckle.h"
#include "effects.h"

#include <stdlib.h>

// Effects stack (initially empty)
#define EFFECTS_HEAP_SIZE 50
#define EFFECT_UNUSED 0xffffffff
Effect* effects = NULL;
Effect effects_heap[EFFECTS_HEAP_SIZE]; //XXX

void init_effects_heap(){
    for(int i = 0; i < EFFECTS_HEAP_SIZE; i++){
        free_effect(effects_heap+i);
    }
    effects_running = 0;
}

// Parameters
uint8_t parameters[PARAM_LEN] = {0xff,0xff,0xff,0xff};

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
    if(top.a == 0){
        // Optimzation
        return bot;
    }
    out |= (((bot & RGBA_R_MASK) * (0xff - top.a) + ((top.r * top.a) << RGBA_R_SHIFT >> 3)) / 0xff) & RGBA_R_MASK;
    out |= (((bot & RGBA_G_MASK) * (0xff - top.a) + ((top.g * top.a) << RGBA_G_SHIFT >> 3)) / 0xff) & RGBA_G_MASK;
    out |= (((bot & RGBA_B_MASK) * (0xff - top.a) + ((top.b * top.a) << RGBA_B_SHIFT >> 3)) / 0xff) & RGBA_B_MASK;
    return out;
}

rgb_t filter_rgb(rgb_t color, uint8_t rf, uint8_t gf, uint8_t bf, uint8_t kf){
    // Filter R, G, B, (all) channels for color correction, etc
    // Probably should be optimized
    rgb_t out = RGB_EMPTY;
    out |= (((color & RGBA_R_MASK) * rf * kf) / 0xfe01) & RGBA_R_MASK;
    out |= (((color & RGBA_G_MASK) * gf * kf) / 0xfe01) & RGBA_G_MASK;
    out |= (((color & RGBA_B_MASK) * bf * kf) / 0xfe01) & RGBA_B_MASK;
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
;

void time_add(tick_t* c, uint32_t b, uint8_t t){
    uint8_t f = c->frac + t;
    c->tick += b;
    if(f < c->frac || f >= TICK_LENGTH){
        c->tick++;
        c->frac = f + (0xff - TICK_LENGTH) + 1; //XXX OBO error?
    }else{
        c->frac = f;
    }
}

int32_t time_sub(tick_t end, tick_t start){
    // TODO: Test
    return ((int32_t) end.tick - (int32_t) start.tick) * TICK_LENGTH + ((int32_t) end.frac -(int32_t)  start.frac);
}

tick_t clock = {0, 0};

Effect* tick_all(Effect* eff, fractick_t ft, uint8_t beat){
    // Send a tick event to every effect
    // If ft is 0, check for deleted effects
    // Return new top of stack
    Effect* prev = NULL;
    Effect* start = eff;

    if(beat){
        clock.tick++;
        clock.frac = 0;
    }else{
        // Check for time dilation
        if(clock.frac < ft){
            clock.frac = ft;
        }
    }

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
    return start;
}

void compose_all(Effect* eff, rgb_t* strip){ 
    // Compose a list of effects onto a strip
    Effect* eff_head = eff; // keep reference to head of stack
    position_t i;
    rgb_t px;
    
    for(i = 0; i < STRIP_LENGTH; i++, strip++){
        px = RGB_EMPTY;
        for(eff = eff_head; eff; eff = eff->next){
            px = mix_rgb(eff->table->pixel(eff, i), px);
        }
        // Apply color correction
        px = filter_rgb(px, parameters[0], parameters[1], parameters[2], parameters[3]);
        // Buffer pixel to prevent flicker while sending pixel buffer
        // Now the failure mode is tearing
        *strip = px;
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

void pop_effect(Effect** stack, uint8_t uid){
    Effect * _stack = *stack;
    Effect * last_stack = NULL;
    for(; _stack != NULL; last_stack = _stack, _stack = _stack->next){
        if(_stack->uid == uid){
            if(last_stack == NULL){
                *stack = _stack->next;
            }else{
                last_stack->next = _stack->next;
            }
            free_effect(_stack);
            return;
        }
    }
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
    //free(eff);
    eff->next = (Effect*) EFFECT_UNUSED;
    if(effects_running >= 1){
        effects_running--;
    }
}

void message(canpacket_t* data){
    Effect* e;
    if(data->cmd & FLAG_CMD){
        if(data->cmd & FLAG_CMD_MSG){
            effects = msg_all(effects, data);
        }else{
            switch(data->cmd){
                case CMD_SYNC:
                    effects = tick_all(effects, data->uid, 0);
                break;
                case CMD_TICK:
                    effects = tick_all(effects, data->uid, 1);
                break;
                case CMD_MSG:
                    effects = msg_all(effects, data);
                break;
                case CMD_STOP:
                    pop_effect(&effects, data->uid);
                break;
                case CMD_RESET:
                case CMD_REBOOT:
                    // Reset strip, remove all effects
                    while(effects){
                        e = effects;
                        effects = e->next;
                        free_effect(e);
                    }
                    int i;
                    for(i = 0; i < PARAM_LEN; i++){
                        parameters[i] = 0xff;
                    }
                    // Reset clock
                    clock.tick = 0;
                    clock.frac = 0;
                break;
                case CMD_PARAM:
                    if(data->uid < PARAM_LEN){
                        parameters[data->uid] = data->data[0];
                    }
                break;
                default:
                break;
            }
        }
    }else{
        int i;
        for(i = 0; i < NUM_EFFECTS; i++){
            if(effect_table[i].eid == data->cmd){
                // Remove effect with the same uid
                pop_effect(&effects, data->uid);
                // Found a match. Attempt to malloc
                // TODO: this might be 1-4 bytes larger than nessassary? 
                //Effect* eff = malloc(sizeof(Effect) + effect_table[i].size);
                Effect* eff = NULL;
                for(int i = 0; i < EFFECTS_HEAP_SIZE; i++){
                    if(effects_heap[i].next == (Effect*) EFFECT_UNUSED){
                        eff = effects_heap + i;
                        break;
                    }
                }
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
                effects_running++;
            }
        }
    }
}
