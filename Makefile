CC=gcc
CFLAGS=-O2 -Wall
#LDFLAGS=-static
LDFLAGS=

#apps=synochecksum
apps=synochecksum-flip1 synochecksum-emu1

all: $(apps)

clean:
	rm -vf $(apps) $(objs) *~

synochecksum-flip1: synochecksum.c
	$(CC) $(CFLAGS) -DSYNOCKVARIANT=1 $< $(LDFLAGS) -o $@

synochecksum-emu1: synochecksum.c
	$(CC) $(CFLAGS) -DSYNOCKVARIANT=2 $< $(LDFLAGS) -o $@


%: %.c
	$(CC) $(CFLAGS) $< $(LDFLAGS) -o $@

