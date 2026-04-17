from machine import Pin, SPI
from spi_displays import ST7735
import framebuf

SCL=6
SDA=7
RST=10
DC=5
CS=4
BLK=8

spi = SPI(1, baudrate=20_000_000, sck=Pin(SCL), mosi=Pin(SDA), miso=None, phase=0, polarity=0),
tft = ST7735(spi=spi, width=160, height=128, dc=Pin(DC), cs=Pin(CS), rst=Pin(RST), bl=Pin(BLK),rotation=1)
fb = framebuf.FrameBuffer(tft.buffer, tft.width, tft.height, framebuf.RGB565)

fb.fill(0x0000)
# fb.text("Hello st7735", 10, 10, 0xFFFF)
tft.text("Hi", 0, 0, 0xFFFF, 12)
tft.show()