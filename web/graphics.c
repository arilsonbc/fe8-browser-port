#include "palette.h"
static int livePalette;
static const u8 *Pal(const u8 *original,int bank) {
    return livePalette?(const u8 *)(gPaletteBuffer+bank*16):original;
}
#include "generated/title_assets.h"

// First graphics backend: decode a 4bpp tile sheet into RGBA pixels.
// This is an asset viewer, not the complete title-screen compositor.
static u8 pixels[256 * 64 * 4];
u8 *fe8_graphics_pixels(void) {
    int x, y;
    for (y = 0; y < 64; ++y) for (x = 0; x < 256; ++x) {
        int tile = (y / 8) * 32 + x / 8;
        int packed = title_tiles[tile * 32 + (y % 8) * 4 + (x % 8) / 2];
        int index = (packed >> ((x & 1) * 4)) & 15;
        int color = title_palette[index * 2] | (title_palette[index * 2 + 1] << 8);
        int offset = (y * 256 + x) * 4;
        int r = color & 31, g = (color >> 5) & 31, b = (color >> 10) & 31;
        pixels[offset] = (r << 3) | (r >> 2);
        pixels[offset + 1] = (g << 3) | (g >> 2);
        pixels[offset + 2] = (b << 3) | (b >> 2);
        pixels[offset + 3] = index ? 255 : 0;
    }
    return pixels;
}

static u8 title_frame[240 * 160 * 4];
static u8 title_coverage[240 * 160];
u8 *fe8_title_pixels(void) {
    static const int widths[3][4] = {{8,16,32,64},{16,32,32,64},{8,8,16,32}};
    static const int heights[3][4] = {{8,16,32,64},{8,8,16,32},{16,32,32,64}};
    int i, x, y;
    for (i = 0; i < 240 * 160; ++i) {
        title_frame[i*4] = title_frame[i*4+1] = title_frame[i*4+2] = 0;
        title_frame[i*4+3] = 255;title_coverage[i]=0;
    }
    for (i = 0; i < title_sprite[0]; ++i) {
        int a = title_sprite[1+i*3], b = title_sprite[2+i*3], c = title_sprite[3+i*3];
        int shape = (a >> 14) & 3, size = (b >> 14) & 3;
        int ox = (b & 511), oy = a & 255;
        int width, height;
        if (shape == 3) continue;
        if (ox >= 256) ox -= 512;
        if (oy >= 128) oy -= 256;
        ox += 4; oy += 48; // Original DrawTitleSprites_Loop coordinates.
        width = widths[shape][size]; height = heights[shape][size];
        for (y = 0; y < height; ++y) for (x = 0; x < width; ++x) {
            int sx = (b & 4096) ? width-1-x : x;
            int sy = (b & 8192) ? height-1-y : y;
            int tile = (c & 1023) + (sy/8)*32 + sx/8; // Original 2D OBJ tile layout.
            int address = tile*32 + (sy%8)*4 + (sx%8)/2;
            int index, color, offset, r, g, blue;
            if (ox+x < 0 || ox+x >= 240 || oy+y < 0 || oy+y >= 160) continue;
            if (address < 0 || address >= sizeof(title_tiles)) continue;
            index = (title_tiles[address] >> ((sx&1)*4)) & 15;
            if (!index) continue;
            const u8 *pal=Pal(title_palette,18);
            color = pal[index*2] | (pal[index*2+1]<<8);
            title_coverage[(oy+y)*240+ox+x]=1;
            offset = ((oy+y)*240+ox+x)*4;
            r=color&31; g=(color>>5)&31; blue=(color>>10)&31;
            title_frame[offset]=(r<<3)|(r>>2);
            title_frame[offset+1]=(g<<3)|(g>>2);
            title_frame[offset+2]=(blue<<3)|(blue>>2);
        }
    }
    return title_frame;
}

static u8 screen[240*160*4];
static void Paint(int x,int y,int index,const u8 *palette) {
    int color,offset,c,i;
    if(!index || x<0 || y<0 || x>=240 || y>=160) return;
    color=palette[index*2]|(palette[index*2+1]<<8);
    offset=(y*240+x)*4;
    for(i=0;i<3;i++){ c=(color>>(i*5))&31; screen[offset+i]=(c<<3)|(c>>2); }
}
static void Layer(const u8 *tiles,int length,const u8 *map,const u8 *palette) {
    int x,y,entry,sx,sy,address,index;
    for(y=0;y<160;y++)for(x=0;x<240;x++) {
        entry=map[((y/8)*32+x/8)*2]|(map[((y/8)*32+x/8)*2+1]<<8);
        sx=(entry&1024)?7-x%8:x%8; sy=(entry&2048)?7-y%8:y%8;
        address=(entry&1023)*32+sy*4+sx/2;
        if(address>=length)continue;
        index=(tiles[address]>>((sx&1)*4))&15;
        Paint(x,y,index,palette);
    }
}
static void Sprites(const u16 *layout,int ox,int oy,int base,const u8 *palette) {
    static const int widths[3][4]={{8,16,32,64},{16,32,32,64},{8,8,16,32}};
    static const int heights[3][4]={{8,16,32,64},{8,8,16,32},{16,32,32,64}};
    int i,x,y;
    for(i=0;i<layout[0];i++){
        int a=layout[1+i*3],b=layout[2+i*3],tile=layout[3+i*3]&1023;
        int shape=a>>14,size=b>>14,w,h,dx=b&511,dy=a&255;
        if(shape>=3)continue;
        if(dx>=256)dx-=512; if(dy>=128)dy-=256;
        w=widths[shape][size]; h=heights[shape][size];
        for(y=0;y<h;y++)for(x=0;x<w;x++){
            int sx=(b&4096)?w-1-x:x,sy=(b&8192)?h-1-y:y;
            int address=(base+tile+(sy/8)*32+sx/8)*32+(sy%8)*4+(sx%8)/2;
            if(address<0 || address>=sizeof(extras))continue;
            Paint(ox+dx+x,oy+dy+y,(extras[address]>>((sx&1)*4))&15,palette);
        }
    }
}
u8 *fe8_screen_pixels(int showStart) {
    static u8 background[sizeof(bg1)+sizeof(bg2)];
    int i;
    for(i=0;i<sizeof(bg1);i++)background[i]=bg1[i];
    for(i=0;i<sizeof(bg2);i++)background[sizeof(bg1)+i]=bg2[i];
    for(i=0;i<240*160;i++){
        screen[i*4]=screen[i*4+1]=screen[i*4+2]=0;screen[i*4+3]=255;
    }
    Layer(background,sizeof(background),bg_map,Pal(bg_palette,14));
    Layer(dragon,sizeof(dragon),dragon_map,Pal(dragon_palette,15));
    fe8_title_pixels();
    for(i=0;i<240*160;i++)if(title_coverage[i]){
        screen[i*4]=title_frame[i*4];screen[i*4+1]=title_frame[i*4+1];screen[i*4+2]=title_frame[i*4+2];
    }
    // OBJ memory starts at tile 0x180 for the supplementary graphics.
    Sprites(banner_sprite,16,85,0x1A0-0x180,Pal(banner_palette,19));
    Sprites(copyright_sprite,4,148,0,Pal(copyright_palette,17));
    if(showStart)Sprites(start_sprite,72,124,0x1BA-0x180,Pal(start_palette,16));
    return screen;
}

// Original Press Start palette entry 8 is animated by DrawTitleSprites_Loop.
extern int fe8_title_pulse_color(void);
u8 *fe8_title_render(void) {
    u8 palette[sizeof(start_palette)];
    int i,color=fe8_title_pulse_color();
    fe8_screen_pixels(0);
    for(i=0;i<sizeof(palette);i++)palette[i]=start_palette[i];
    palette[16]=color&255;palette[17]=color>>8;
    Sprites(start_sprite,72,124,0x1BA-0x180,palette);
    return screen;
}

void fe8_graphics_load_title_palettes(void) {
    const u8 *palettes[]={bg_palette,dragon_palette,start_palette,copyright_palette,title_palette,banner_palette};
    const int banks[]={14,15,16,17,18,19};
    for(int p=0;p<6;p++)for(int i=0;i<16;i++)
        gPaletteBuffer[banks[p]*16+i]=palettes[p][i*2]|(palettes[p][i*2+1]<<8);
}
extern int fe8_menu_state(void);
extern u8 *fe8_menu_pixels(void);
extern int fe8_map_active(void);
extern u8 *fe8_map_pixels(void);
u8 *fe8_game_pixels(void) {
    if(fe8_map_active())return fe8_map_pixels();
    if(fe8_menu_state()>=0)return fe8_menu_pixels();
    livePalette=1;fe8_screen_pixels(1);livePalette=0;
    if(!fe8_display_enabled())for(int i=0;i<240*160;i++)
        screen[i*4]=screen[i*4+1]=screen[i*4+2]=0;
    return screen;
}
