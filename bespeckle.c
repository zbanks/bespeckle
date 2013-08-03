#include "bespeckle.h"

#include <stdlib.h>

#ifdef __DEBUG_LOCAL__
#include <stdio.h>

#else

#endif

void _setup_one_color(Effect* eff, canpacket_t* data){
    *(rgba_t*) eff->data = hsva_to_rgba(*(hsva_t*) (data->data));
}

bool_t _tick_nothing(Effect* eff, fractick_t ft){
    return CONTINUE;
}

bool_t _tick_flash(Effect* eff, fractick_t ft){
    if(ft == 0){
        rgba_t * rgba = (rgba_t*) eff->data;
        rgba->r ^= 0xff;
        rgba->g ^= 0xff;
        rgba->b ^= 0xff;
    }
    return CONTINUE;
}

rgba_t _pixel_solid(Effect* eff, position_t pos){
    return *((rgba_t*) (eff->data));
}

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

void _msg_nothing(Effect* eff, canpacket_t* data){
    return;
}

// Table containing all the possible effects & their virtual tables
EffectTable effect_table[NUM_EFFECTS] = {
    // Solid color 
    {0, sizeof(rgba_t), _setup_one_color, _tick_nothing, _pixel_solid, _msg_nothing},
    // Flash solid
    {1, sizeof(rgba_t), _setup_one_color, _tick_flash, _pixel_solid, _msg_nothing},
    // Stripes
    {2, sizeof(rgba_t), _setup_one_color, _tick_nothing, _pixel_stripe, _msg_nothing}
};

// Effects stack (initially empty)
Effect* effects = NULL;

rgb_t pack_rgba(rgba_t in){
    return (((in.r >> 3 << RGBA_R_SHIFT) & RGBA_R_MASK) | 
            ((in.g >> 3 << RGBA_G_SHIFT) & RGBA_G_MASK) | 
            ((in.b >> 3 << RGBA_B_SHIFT) & RGBA_B_MASK));
}

rgba_t unpack_rgb(rgb_t packed){
    rgba_t out;
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
    // This can be *very* optimized once we decide on sizeof(a), etc
    rgb_t out = RGB_EMPTY;
    out |= (((bot & RGBA_R_MASK) * (0xff - top.a) + ((top.r * top.a) << RGBA_R_SHIFT)) / 0xff) & RGBA_R_MASK;
    out |= (((bot & RGBA_G_MASK) * (0xff - top.a) + ((top.g * top.a) << RGBA_R_SHIFT)) / 0xff) & RGBA_G_MASK;
    out |= (((bot & RGBA_B_MASK) * (0xff - top.a) + ((top.b * top.a) << RGBA_R_SHIFT)) / 0xff) & RGBA_B_MASK;
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
    _stack->next = eff;
}

inline void free_effect(Effect* eff){
    // Deallocate space for effect
    free(eff);
}

void message(canpacket_t* data){
    if(data->cmd & COMMAND_FLAG){
        switch(data->cmd){
            //TODO
            default:
            break;
        }
    }else{
        int i;
        for(i = 0; i < NUM_EFFECTS; i++){
            if(effect_table[i].eid == data->cmd){
                // Found a match. Attempt to malloc
                // TODO: this might be 1-4 bytes larger than nessassary? 
                Effect* eff = (Effect *) malloc(sizeof(Effect) + effect_table[i].size);
                if(eff == NULL){
                    // malloc failed! :(
                    return;
                }
                // Setup effect; add to stack
                eff->uid = data->uid;
                eff->table = effect_table+i;
                effect_table[i].setup(eff, data);
                push_effect(&effects, eff);
            }
        }
    }
}

#ifdef __DEBUG_LOCAL__

void print_color(rgb_t color){
    rgba_t unpacked = unpack_rgb(color);
    printf("#%02x%02x%02x ", (unpacked.r), (unpacked.g), (unpacked.b));
}

void print_ucolor(rgba_t unpacked){
    printf("#%02x%02x%02x ", (unpacked.r), (unpacked.g), (unpacked.b));
}

void stack_length(Effect * eff){
    for(;eff; eff = eff->next){
        printf("%c", *eff->data);
    }
    printf("  ");
}

void print_strip(){
    int i;
    rgb_t strip[STRIP_LENGTH];
    compose_all(effects, strip);        
    for(i = 0; i < STRIP_LENGTH; i++){
        print_color(strip[i]);
    }
}

int main(){ 
    int i;
    canpacket_t msg1 = {0x02, 0x00, {'a', 0xcd,  0xef, 0x00, 0x00, 0x00}};
    hsva_t color = {0, 255, 255, 0};
    printf("<style>div{ width: 500px; height: 10px; margin: 0; }</style>\n\n");
    for(i = 0; i < 256; i++){
        color.h = i & ~0x3;
        printf("<div style='background-color:");
        print_color(pack_rgba(hsva_to_rgba(color)));
        printf("'>&nbsp;</div>");
        printf("\n");
    }
    message(&msg1);

    for(i = 0; i < 10; i++){
        //stack_length(effects);
        (*msg1.data)++;
        tick_all(effects, 0);
        print_strip();
        printf("\n");
    }
    return 0; 
}

#endif
