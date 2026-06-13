shtemp: shtemp.c
	gcc shtemp.c -o shtemp -march=native `pkg-config --cflags --libs gtk4`

clean:
	rm shtemp *~
