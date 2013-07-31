#include "bespeckle.h"

rgb_t RGBA::pack(){
    return (((r << RGBA_R_SHIFT) & RGBA_R_MASK) | 
            ((g << RGBA_G_SHIFT) & RGBA_G_MASK) | 
            ((b << RGBA_B_SHIFT) & RGBA_B_MASK));
}

void RGBA::unpack(rgb_t packed){
    r = (uint8_t) ((packed & RGBA_R_MASK) >> RGBA_R_SHIFT);
    g = (uint8_t) ((packed & RGBA_G_MASK) >> RGBA_G_SHIFT);
    b = (uint8_t) ((packed & RGBA_B_MASK) >> RGBA_B_SHIFT);
    a = 0xFF;
}

void RGBA::mix(RGBA top){
    // This can be *very* optimized once we decide on sizeof(a), etc
    r = (uint8_t) ((r * (0xff - top.a) + top.r * top.a) / 0xff);
    g = (uint8_t) ((g * (0xff - top.a) + top.g * top.a) / 0xff);
    b = (uint8_t) ((b * (0xff - top.a) + top.b * top.a) / 0xff);
    a = 0xFF; // Alpha gets lost when mixing
}

rgb_t RGBA::mixOn(rgb_t bot){
    // Mix this color on top of another in packed format
    // This can be *very* optimized once we decide on sizeof(a), etc
    rgb_t out = RGB_EMPTY;
    out |= (((bot & RGBA_R_MASK) * (0xff - a) + r * a) / 0xff) & RGBA_R_MASK;
    out |= (((bot & RGBA_G_MASK) * (0xff - a) + g * a) / 0xff) & RGBA_G_MASK;
    out |= (((bot & RGBA_B_MASK) * (0xff - a) + b * a) / 0xff) & RGBA_B_MASK;
    return out;
}

hsva_t HSVA::pack(){
    return (((h << HSVA_H_SHIFT) & HSVA_H_MASK) | 
            ((s << HSVA_S_SHIFT) & HSVA_S_MASK) | 
            ((v << HSVA_V_SHIFT) & HSVA_V_MASK) | 
            ((a << HSVA_A_SHIFT) & HSVA_A_MASK));
}

void HSVA::unpack(hsva_t packed){
    h = (uint8_t) ((packed & HSVA_H_MASK) >> HSVA_H_SHIFT);
    s = (uint8_t) ((packed & HSVA_S_MASK) >> HSVA_S_SHIFT);
    v = (uint8_t) ((packed & HSVA_V_MASK) >> HSVA_V_SHIFT);
    a = (uint8_t) ((packed & HSVA_A_MASK) >> HSVA_A_SHIFT);
}

RGBA HSVA::toRGBA(void){
    //TODO
    RGBA rgba;
    return rgba;
}

RGBA Effect::pixel_rgba(position_t pos){
    return pixel_hsva(pos).toRGBA();
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
            if(eff->tick(ft)){
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
            eff->tick(ft);
            eff = eff->next;
        }
    }
    return start;
}

void compose(Effect* eff, rgb_t* strip){ 
    // Compose a list of effects onto a strip

    Effect* eff_head = eff; // keep reference to head of stack
    RGBA color;
    
    for(int i = 0; i < STRIP_LENGTH; i++, strip++){
        color = RGBA_EMPTY;
        for(eff = eff_head; eff; eff = eff->next){
            color.mix(eff->pixel_rgba(i));
        }
        *strip = color.pack();
    }
}


template<int N> 
struct _{ operator char() { return N+ 256; } }; //always overflow

int main(){
    //char(_<sizeof(RGBA)>());
    char(_<sizeof(rgb_t[100])>());
    return 0;
}
