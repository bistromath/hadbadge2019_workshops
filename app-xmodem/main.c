#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "mach_defines.h"
#include "sdk.h"
#include "gfx_load.h"
#include "cache.h"
#include "badgetime.h"
#include "xmodem.h"

//The bgnd.png image got linked into the binary of this app, and these two chars are the first
//and one past the last byte of it.
extern char _binary_bgnd_png_start;
extern char _binary_bgnd_png_end;

extern char _binary_tilemap_tmx_start;
extern char _binary_tilemap_tmx_end;

//Pointer to the framebuffer memory.
uint8_t *fbmem;

#define FB_WIDTH 512
#define FB_HEIGHT 320

//time function for delays
uint32_t time() {
    uint32_t cycles;
    asm volatile ("rdcycle %0" : "=r"(cycles));
    return cycles;
}

void wait(uint32_t duration_counts) {
    uint32_t timenow = time();
    while (time() - timenow <= duration_counts*48000){;}
}

//prototypes for the _inbyte and _outbyte functions.
//we can't use fread/fwrite because they lack timeouts.
volatile uint32_t* IRDA_DATA = (uint32_t*) 0x10000010;

int _inbyte(unsigned short timeout) { // msec timeout
    uint32_t timenow = time();
    uint32_t irda;

    do {
        irda = *IRDA_DATA;
        if(time() - timenow >= (timeout*48000)) break;
    } while(irda & (1<<31));

    if(irda & (1<<31)) {
        return -2;
    }

    return irda & 0xFF;
}

void _outbyte(int c) {
    *IRDA_DATA = c;
}

extern volatile uint32_t MISC[];
#define MISC_REG(i) MISC[(i)/4]

void main(int argc, char **argv) {
	//We're running in app context. We have full control over the badge and can do with the hardware what we want. As
	//soon as main() returns, however, we will go back to the IPL.
    MISC_REG(MISC_GPEXT_W2C_REG) = 1<<30; //enable IRDA

	printf("Hello World app: main running\n");
	//Blank out fb while we're loading stuff by disabling all layers. This just shows the background color.
	GFX_REG(GFX_BGNDCOL_REG)=0x202020; //a soft gray
	GFX_REG(GFX_LAYEREN_REG)=0; //disable all gfx layers
	
	//First, allocate some memory for the background framebuffer. We're gonna dump a fancy image into it. The image is
	//going to be 8-bit, so we allocate 1 byte per pixel.
	fbmem=calloc(FB_WIDTH,FB_HEIGHT);
	printf("Hello World: framebuffer at %p\n", fbmem);
	
	//Tell the GFX hardware to use this, and its pitch. We also tell the GFX hardware to use palette entries starting
	//from 128 for the frame buffer; the tiles left by the IPL will use palette entries 0-16 already.
	GFX_REG(GFX_FBPITCH_REG)=(128<<GFX_FBPITCH_PAL_OFF)|(FB_WIDTH<<GFX_FBPITCH_PITCH_OFF);
	//Set up the framebuffer address
	GFX_REG(GFX_FBADDR_REG)=((uint32_t)fbmem);

	//Now, use a library function to load the image into the framebuffer memory. This function will also set up the palette entries,
	//we tell it to start writing from entry 128.
	int png_size=(&_binary_bgnd_png_end-&_binary_bgnd_png_start);
	int i=gfx_load_fb_mem(fbmem, &GFXPAL[128], 8, FB_WIDTH, &_binary_bgnd_png_start, png_size);
	if (i) printf("gfx_load_fb_mem: error %d\n", i);

	//Flush the memory region to psram so the GFX hw can stream it from there.
	cache_flush(fbmem, fbmem+FB_WIDTH*FB_HEIGHT);

	//The IPL leaves us with a tileset that has tile 0 to 127 map to ASCII characters, so we do not need to
	//load anything specific for this. In order to get some text out, we can use the /dev/console device
	//that will use these tiles to put text in a tilemap. It uses escape codes to do so, see 
	//ipl/gloss/console_out.c for more info.
	FILE *f;
	f=fopen("/dev/console", "w");
	setvbuf(f, NULL, _IONBF, 0); //make console line unbuffered
	//Note that without the setvbuf command, no characters would be printed until 1024 characters are
	//buffered. You normally don't want this.
	fprintf(f, "\033C"); //clear the console. Note '\033' is the escape character.
	fprintf(f, "\0331X"); //set Xpos to 5
	fprintf(f, "\0338Y"); //set Ypos to 8
	fprintf(f, "XMODEM: press A to receive, B to send"); // Print a nice greeting.
	
	//The user can still see nothing of this graphics goodness, so let's re-enable the framebuffer and
	//tile layer A (the default layer for the console). Also indicate the framebuffer we have is
	//8-bit.
	GFX_REG(GFX_LAYEREN_REG)=GFX_LAYEREN_FB_8BIT|GFX_LAYEREN_FB|GFX_LAYEREN_TILEA;

	gfx_load_tilemap_mem(GFXTILEMAPA, 64, 64, 1, &_binary_tilemap_tmx_start, &_binary_tilemap_tmx_end-&_binary_tilemap_tmx_start, 0);
	
	printf("Hello World ready. Press a button to exit.\n");
	//Wait until all buttons are released
	wait_for_button_release();

	//Wait until a button is pressed
    uint8_t buttons;
    while((buttons = MISC_REG(MISC_BTN_REG)) == 0x00);
    if(buttons & BUTTON_A) {
        GFX_REG(GFX_LAYEREN_REG)=0;
        fprintf(f, "\033C"); //clear the console. Note '\033' is the escape character.
        fprintf(f, "\0331X"); //set Xpos to 1
        fprintf(f, "\0338Y"); //set Ypos to 8
        fprintf(f, "Receving file!");
        GFX_REG(GFX_LAYEREN_REG)=GFX_LAYEREN_FB_8BIT|GFX_LAYEREN_FB|GFX_LAYEREN_TILEA;
        FILE *rxf = fopen("/cart/hello.elf", "wb");

        char *buf = malloc(512);
        int st = xmodemReceive(buf, 512);
        if(st < 0) {
            GFX_REG(GFX_LAYEREN_REG)=0;
            fprintf(f, "\033C"); //clear the console. Note '\033' is the escape character.
            fprintf(f, "\0331X"); //set Xpos to 1
            fprintf(f, "\0338Y"); //set Ypos to 8
            fprintf(f, "Error %i", st);
            GFX_REG(GFX_LAYEREN_REG)=GFX_LAYEREN_FB_8BIT|GFX_LAYEREN_FB|GFX_LAYEREN_TILEA;
        } else {
            GFX_REG(GFX_LAYEREN_REG)=0;
            fprintf(f, "\033C"); //clear the console. Note '\033' is the escape character.
            fprintf(f, "\0331X"); //set Xpos to 1
            fprintf(f, "\0338Y"); //set Ypos to 8
            fprintf(f, "Successfully received!", st);
            GFX_REG(GFX_LAYEREN_REG)=GFX_LAYEREN_FB_8BIT|GFX_LAYEREN_FB|GFX_LAYEREN_TILEA;
            fwrite(buf, 1, st, rxf);
        }
        free(buf);
        fclose(rxf);
        wait(1000);
    }
    else if(buttons & BUTTON_B) {
        GFX_REG(GFX_LAYEREN_REG)=0;
        fprintf(f, "\033C"); //clear the console. Note '\033' is the escape character.
        fprintf(f, "\0331X"); //set Xpos to 1
        fprintf(f, "\0338Y"); //set Ypos to 8
        fprintf(f, "Sending file!");
        GFX_REG(GFX_LAYEREN_REG)=GFX_LAYEREN_FB_8BIT|GFX_LAYEREN_FB|GFX_LAYEREN_TILEA;
        char *sbuf = malloc(512);
        sprintf(sbuf, "BUTT HORK %i", time());
        int st = xmodemTransmit(sbuf, strlen(sbuf));

        if(st < 0) {
            GFX_REG(GFX_LAYEREN_REG)=0;
            fprintf(f, "\033C"); //clear the console. Note '\033' is the escape character.
            fprintf(f, "\0331X"); //set Xpos to 1
            fprintf(f, "\0338Y"); //set Ypos to 8
            fprintf(f, "Error %i", st);
            GFX_REG(GFX_LAYEREN_REG)=GFX_LAYEREN_FB_8BIT|GFX_LAYEREN_FB|GFX_LAYEREN_TILEA;
        } else {
            GFX_REG(GFX_LAYEREN_REG)=0;
            fprintf(f, "\033C"); //clear the console. Note '\033' is the escape character.
            fprintf(f, "\0331X"); //set Xpos to 1
            fprintf(f, "\0338Y"); //set Ypos to 8
            fprintf(f, "Successfully sent!", st);
            GFX_REG(GFX_LAYEREN_REG)=GFX_LAYEREN_FB_8BIT|GFX_LAYEREN_FB|GFX_LAYEREN_TILEA;
        }
        free(sbuf);
        wait(1000);
    }
}

