.PHONY: default clean

default: libmemmanager.a

libmemmanager.a: init.o vmm.o logger.o
	$(AR) rcs $@ $^

%.o: %.c
	$(CC) $(CPPFLAGS) $(CFLAGS) -o $@ -c $< $(LDFLAGS) $(LDLIBS)

clean:
	rm -f libmemmanager.a *.o
