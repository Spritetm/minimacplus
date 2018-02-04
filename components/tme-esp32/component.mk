#Component makefile

COMPONENT_SRCDIRS := .
COMPONENT_OBJS := hd.o hexdump.o localtalk.o main.o mpu6050.o mpumouse.o snd.o

ifdef CONFIG_TME_DISP_MIPI
COMPONENT_OBJS += mipi_lcd.o adns9500.o
endif
ifdef CONFIG_TME_DISP_WROVER
COMPONENT_OBJS += spi_lcd.o
endif