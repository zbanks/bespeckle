#include "bespeckle.c"
#include "effects.c"

#include <stdio.h>


void print_color(rgb_t color){
    rgba_t unpacked = unpack_rgb(color);
    printf("#%02x%02x%02x ", (unpacked.r), (unpacked.g), (unpacked.b));
}

void stack_length(Effect * eff){
    for(;eff; eff = eff->next){
        printf("%c", eff->uid);
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
void print_strip_html(){
    int i;
    rgb_t strip[STRIP_LENGTH];
    compose_all(effects, strip);        
    printf("<div>\n");
    for(i = 0; i < STRIP_LENGTH; i++){
        printf("\t<span style='background-color:");
        print_color(strip[i]);
        printf("'>%d</span>\n", i);
    }
    printf("</div>\n");
}

int main(){ 
    int i;
    canpacket_t msg1 = {0x03, 'a', {0x80, 20, 23, 0x00, 0x00, 0x00}};
    canpacket_t msg2 = {0x43 , 'b', {0x00, 0x00, 0x00, 0xff, 0x04, 0x00}};
    canpacket_t msg_sync = {CMD_SYNC, 0, {0, 0, 0, 0, 0, 0}};
    canpacket_t msg_tick = {CMD_TICK, 0, {0, 0, 0, 0, 0, 0}};
    //hsva_t color = {0, 255, 255, 0};
    //printf("<style>div{ width: 500px; height: 10px; margin: 0; }</style>\n\n");
    printf("<style>span{ width: 5; height: 5; margin: 0px; padding: 0px; display: inline-block; }\ndiv{font-size: 0; height: 5px; margin-bottom: 0px;}</style>\n\n");
    init_effects_heap();
    message(&msg1);

    for(i = 0; i < 256; i++){
        /*
        ((hsva_t *) msg1.data)->h = i;
        ((hsva_t *) msg1.data)->s = 0xff;
        ((hsva_t *) msg1.data)->v = 0xff;
        ((hsva_t *) msg1.data)->a = 0xff;
        */
#define FT 10
        msg_sync.uid = (i % FT) * (240 / FT);
        if(i % FT == 0){
            message(&msg_tick);
        }
        message(&msg_sync);
        print_strip_html();
        //if((i % 50) == 0 && i > 10){
        //if(i % 11 == 0 && i < 12){
        if(i == 12){
            msg2.data[0] += 8;
            //msg2.uid += 1;
            message(&msg2);
        }

        /*
        data[0] = i;
        color = *((hsva_t*) data);
        //color.h = i;
        printf("<div style='background-color:");
        print_color(pack_rgba(hsva_to_rgba(color)));
        printf("'>&nbsp;</div>");
        printf("\n");
        */
        //*/
    }
    
    //message(&msg1);
    //printf("<style>span{ width: 20px; height: 20px; margin: 0px; padding: 0px; display: inline-block; }\ndiv{font-size: 0; margin-bottom: 3px;}</style>\n\n");

    /*
    for(i = 0; i < 10; i++){
        //stack_length(effects);
        /
        msg1.uid = i + 'a';
        msg1.cmd = i % 3;
        (*msg1.data)++;
        //message(&msg1);
        /
        tick_all(effects, 0);
        print_strip_html();
        //stack_length(effects);
        printf("\n");
    }
    */
    /*
    msg1.cmd = 0xff;
    message(&msg1);
    print_strip_html();
    //stack_length(effects);
    printf("\n");
    */

    return 0; 
}
