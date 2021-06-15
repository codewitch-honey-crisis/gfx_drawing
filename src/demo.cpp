extern "C" { void app_main(); }

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "spi_master.hpp"
#include "esp_spiffs.h"
#include "ili9341.hpp"
#include "gfx_drawing.hpp"
#include "gfx_image.hpp"
#include "gfx_drawing.hpp"
#include "stream.hpp"
#include "gfx_color_cpp14.hpp"

using namespace espidf;
using namespace io;
using namespace gfx;
// the following is configured for the ESP-WROVER-KIT
// make sure to set the pins to your set up.
#if defined(ESP_WROVER_KIT)
#define LCD_HOST    VSPI_HOST
#define DMA_CHAN   2
#define PIN_NUM_MISO GPIO_NUM_25
#define PIN_NUM_MOSI GPIO_NUM_23
#define PIN_NUM_CLK  GPIO_NUM_19
#define PIN_NUM_CS   GPIO_NUM_22

#define PIN_NUM_DC   GPIO_NUM_21
#define PIN_NUM_RST  GPIO_NUM_18
#define PIN_NUM_BCKL GPIO_NUM_5
#else
#define LCD_HOST    VSPI_HOST
#define DMA_CHAN    2
#define PIN_NUM_MISO GPIO_NUM_19
#define PIN_NUM_MOSI GPIO_NUM_23
#define PIN_NUM_CLK  GPIO_NUM_18
#define PIN_NUM_CS   GPIO_NUM_5

#define PIN_NUM_DC   GPIO_NUM_2
#define PIN_NUM_RST  GPIO_NUM_4
#define PIN_NUM_BCKL GPIO_NUM_15
#endif
// configure the spi bus. Must be done before the driver
spi_master spi_host(nullptr,
                    LCD_HOST,
                    PIN_NUM_CLK,
                    PIN_NUM_MISO,
                    PIN_NUM_MOSI,
                    GPIO_NUM_NC,
                    GPIO_NUM_NC,
                    4096,
                    DMA_CHAN);
// set up the driver type
using lcd_type = ili9341<LCD_HOST,
                        PIN_NUM_CS,
                        PIN_NUM_DC,
                        PIN_NUM_RST,
                        PIN_NUM_BCKL>;
// instantiate the driver
lcd_type lcd;

// easy access to lcd color enumeration
using lcd_color = color<typename lcd_type::pixel_type>;

// if you want to make a method that draws to any draw destination
// make it a template method with a Destination type parameter
// and then take a *reference* to that in your routine
template<typename Destination>
void draw_happy_face(Destination& bmp,float alpha,const srect16& bounds) {
    // we'll be needing alpha channels
    using pixel_type=rgba_pixel<32>;

    pixel_type yellow = color<pixel_type>::yellow;
    yellow.channelr<channel_name::A>(alpha);
    pixel_type black = color<pixel_type>::black;
    black.channelr<channel_name::A>(alpha);

    // draw the face
    draw::filled_ellipse(bmp,bounds,yellow);
    
    // draw the left eye
    srect16 eye_bounds_left(spoint16(bounds.width()/5+bounds.left(),bounds.height()/5+bounds.top()),ssize16(bounds.width()/5,bounds.height()/3));
    draw::filled_ellipse(bmp,eye_bounds_left,black);
    // draw the right eye
    srect16 eye_bounds_right=eye_bounds_left;
    eye_bounds_right.x1 = bounds.right()-(eye_bounds_left.x1-bounds.x1)-eye_bounds_left.width();
    eye_bounds_right.x2 = eye_bounds_right.x1 + eye_bounds_left.width()-1;
      
    draw::filled_ellipse(bmp,eye_bounds_right,black);
    
    // draw the mouth
    srect16 mouth_bounds=bounds.inflate(-bounds.width()/7,-bounds.height()/8).normalize();
    // we need to clip part of the circle we'll be drawing
    srect16 mouth_clip(mouth_bounds.x1,mouth_bounds.y1+mouth_bounds.height()/(float)1.6,mouth_bounds.x2,mouth_bounds.y2);
    draw::ellipse(bmp,mouth_bounds,black,&mouth_clip);
}
void app_main(void)
{
    // check to make sure SPI was initialized successfully
    if(!spi_host.initialized()) {
        printf("SPI host initialization error.\r\n");
        abort();
    }

    // we want to do alpha blending but our display doesn't support reading.
    // bitmaps do, however. One thing we can do is create a framebuffer
    // for the entire display off screen. This is useful if you're going
    // to do heavy animation. Normally, you can't just allocate a bitmap
    // of this size. However, large_bitmap<> can use non-contiguous
    // memory, meaning heap fragmentation doesn't prevent allocation
    // of an entire framebuffer like it usually does

    // declare the frame buffer
    // all draw targets have a pixel_type member
    
    // CHOOSE:

    // Uncomment this to use a 16 color EGA palette
    //using fb_pal_type = ega_palette<typename lcd_type::pixel_type>;
    //using fb_type = large_bitmap<indexed_pixel<4>,fb_pal_type>;
    //fb_pal_type pal;
    //fb_type fb(lcd.dimensions(),1,&pal);

    // Uncomment this to use the lcd's native pixel type
    using fb_type = large_bitmap<typename lcd_type::pixel_type>;
    fb_type fb(lcd.dimensions(),1);

    // END CHOOSE

    while(true) {
        // draw a checkerboad pattern
        draw::filled_rectangle(fb,(srect16)fb.bounds(),lcd_color::black);
        for(int y = 0;y<lcd.dimensions().height;y+=16) {
            for(int x = 0;x<lcd.dimensions().width;x+=16) {
                if(0!=((x+y)%32)) {
                    draw::filled_rectangle(fb,
                                        srect16(
                                            spoint16(x,y),
                                            ssize16(16,16)),
                                        lcd_color::white);
                }
            }
        }
        int c = 1+(rand()%8);
        for(int i = 0;i<c;++i) {
            // fb is 16-bit color with no alpha channel.
            // it can still do alpha blending on draw but two
            // things must occur: you must use a pixel with
            // an alpha channel, and the destination for the 
            // draw must be able to be read, which bitmaps can.
            
            // create a 3-bit RGB pixel with an 8 bit alpha channel
            // this makes all of our colors dumb down to something
            // "primary ish"
            pixel<channel_traits<channel_name::R,1>,channel_traits<channel_name::G,1>,channel_traits<channel_name::B,1>,channel_traits<channel_name::A,8>> px;
            // set the color channels to random values:
            px.channel<channel_name::R>(rand()%2);
            px.channel<channel_name::G>(rand()%2);
            px.channel<channel_name::B>(rand()%2);
            // set the alpha channel to a constrained value
            // so none are "too transparent"
            px.channel<channel_name::A>((256-192)+(rand()%192));
            switch(i%4) {
                case 0:
                    draw::filled_rectangle(fb,srect16(rand()%lcd.width,rand()%lcd.height,rand()%lcd.width,rand()%lcd.height),px);
                    break;
                case 1:
                    draw::filled_ellipse(fb,
                        srect16(
                            rand()%lcd.dimensions().width,
                            rand()%lcd.dimensions().height,
                            rand()%lcd.dimensions().width,
                            rand()%lcd.dimensions().height),
                        px);
                    break;
                case 2: {
                        const float scalex = (rand()%101)/100.0;
                        const float scaley = (rand()%101)/100.0;
                        const uint16_t w = lcd.dimensions().width,
                                    h = lcd.dimensions().height;
                        spoint16 path_points[] = {
                            // lower left corner
                            spoint16(0,scaley*h),
                            // upper middle corner
                            spoint16(scalex*w/2,0),
                            // lower right corner
                            spoint16(scalex*w,scaley*h)
                            // the final point is implicit
                            // as polygons automatically
                            // connect the final point to
                            // the first point    
                        };
                        spath16 poly_path(3,path_points);
                        // position it on the screen
                        poly_path.offset_inplace(rand()%w,rand()%h);
                        // now draw it
                        draw::filled_polygon(fb,poly_path,px);
                    }
                    break;
                case 3:
                    draw_happy_face(fb,(rand()%51)/100.0+.5,srect16(rand()%lcd.width,rand()%lcd.height,rand()%lcd.width,rand()%lcd.height).normalize());
                    break;
            }
        }
        // send the frame buffer to the screen
        draw::bitmap(lcd,(srect16)lcd.bounds(),fb,fb.bounds());

        // we need this to prevent the ESP32 wdt from being triggered:
        vTaskDelay(1);
    }   
}