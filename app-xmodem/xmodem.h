#pragma once
int xmodemReceive(unsigned char *dest, int destsz);
int xmodemTransmit(unsigned char *src, int srcsz);
int _inbyte(unsigned short timeout);
void _outbyte(int c);
