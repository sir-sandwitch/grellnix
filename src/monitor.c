#include <monitor.h>
#include <common.h>

uint8_t foreground_color = 0x02;
uint8_t background_color = 0x06;

uint8_t cursor_x = 0;
uint8_t cursor_y = 0;

uint16_t *video_memory = (uint16_t *)0xB8000;

void move_cursor()
{
    uint16_t cursorLocation = cursor_y * 80 + cursor_x;
    outb(0x3D4, 14);                  // Tell the VGA board we are setting the high cursor byte.
    outb(0x3D5, cursorLocation >> 8); // Send the high cursor byte.
    outb(0x3D4, 15);                  // Tell the VGA board we are setting the low cursor byte.
    outb(0x3D5, cursorLocation);      // Send the low cursor byte.
}

void scroll()
{
    // Get a space character with the default colour attributes.
    uint8_t attributeByte = (0 /*black*/ << 4) | (15 /*white*/ & 0x0F);
    uint16_t blank = 0x20 /* space */ | (attributeByte << 8);
    
    // Row 25 is the end, this means we need to scroll up
    if(cursor_y >= 25)
    {
        // Move the current text chunk that makes up the screen
        // back in the buffer by a line
        int i;
        for (i = 0*80; i < 24*80; i++)
        {
            video_memory[i] = video_memory[i+80];
        }
        
        // The last line should now be blank. Do this by writing
        // 80 spaces to it.
        for (i = 24*80; i < 25*80; i++)
        {
            video_memory[i] = blank;
        }
        // The cursor should now be on the last line.
        cursor_y = 24;
    }
}

void monitor_put(char c){
    uint8_t back_color = background_color;
    uint8_t fore_color = foreground_color;
    uint8_t attributeByte = (back_color << 4) | (fore_color & 0x0F);
    uint16_t attribute = attributeByte << 8;
    uint16_t *location;
    
    // Handle a backspace, by moving the cursor back one space
    if (c == 0x08 && cursor_x)
    {
        cursor_x--;
    }
    // Handle a tab by increasing the cursor's X, but only
    // to a point where it is divisible by 8.
    else if (c == 0x09)
    {
        cursor_x = (cursor_x+8) & ~(8-1);
    }
    // Handle carriage return
    else if (c == '\r')
    {
        cursor_x = 0;
    }
    // Handle newline by moving cursor back to left and increasing the row
    else if (c == '\n')
    {
        cursor_x = 0;
        cursor_y++;
    }
    // Handle any other printable character.
    else if(c >= ' ')
    {
        location = video_memory + (cursor_y*80 + cursor_x);
        *location = c | attribute;
        cursor_x++;
    }
    
    // Check if we need to insert a new line because we have reached the end
    // of the screen.
    if (cursor_x >= 80)
    {
        cursor_x = 0;
        cursor_y ++;
    }
    
    // Scroll the screen if needed.
    scroll();
    // Move the hardware cursor.
    move_cursor();
}

void monitor_clear(){
    uint8_t back_color = background_color;
    uint8_t fore_color = foreground_color;
    uint8_t attributeByte = (back_color << 4) | (fore_color & 0x0F);
    uint16_t blank = 0x20 /* space */ | (attributeByte << 8);
    
    int i;
    for (i = 0; i < 80*25; i++)
    {
        video_memory[i] = blank;
    }
    
    cursor_x = 0;
    cursor_y = 0;
    move_cursor();
}

void monitor_write(char *c){
    int i = 0;
    while(c[i]){
        monitor_put(c[i++]);
    }
}

void monitor_write_hex(uint32_t n){
    int tmp;
    monitor_write("0x");
    char noZeroes = 1;
    
    int i;
    for (i = 28; i > 0; i -= 4)
    {
        tmp = (n >> i) & 0xF;
        if (tmp == 0 && noZeroes != 0)
        {
            continue;
        }
        
        if (tmp >= 0xA)
        {
            noZeroes = 0;
            monitor_put (tmp-0xA+'a' );
        }
        else
        {
            noZeroes = 0;
            monitor_put( tmp+'0' );
        }
    }
    
    tmp = n & 0xF;
    if (tmp >= 0xA)
    {
        monitor_put (tmp-0xA+'a');
    }
    else
    {
        monitor_put (tmp+'0');
    }
}

void monitor_write_dec(uint32_t n){
    if (n == 0)
    {
        monitor_put('0');
        return;
    }
    
    int32_t acc = n;
    char c[32];
    int i = 0;
    while (acc > 0)
    {
        c[i] = '0' + acc%10;
        acc /= 10;
        i++;
    }
    c[i] = 0;
    
    char c2[32];
    c2[i--] = 0;
    int j = 0;
    while(i >= 0)
    {
        c2[i--] = c[j++];
    }
    monitor_write(c2);
}

void monitor_printf(char *format, ...){
    va_list ap;
    va_start(ap, format);
    
    int i = 0;
    while(format[i]){
        if(format[i] == '%'){
            i++;
            switch(format[i]){
                case 'c':
                    monitor_put(va_arg(ap, int));
                    break;
                case 'd':
                    monitor_write_dec(va_arg(ap, uint32_t));
                    break;
                case 'x':
                    monitor_write_hex(va_arg(ap, uint32_t));
                    break;
                case 's':
                    monitor_write(va_arg(ap, char*));
                    break;
                default:
                    monitor_put('%');
                    monitor_put(format[i]);
                    break;
            }
        }else{
            monitor_put(format[i]);
        }
        i++;
    }
    
    va_end(ap);
}