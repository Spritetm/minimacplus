#Makes test app.

OBJS:=basiliskif.o ethertalk.o hexdump.o
TARGET:=etalktst
CFLAGS:=-DTESTAPP

$(TARGET): $(OBJS)
	$(CC) -o $@ $^ $(LDFLAGS)

clean:
	rm -f $(TARGET) $(OBJS)
