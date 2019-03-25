CC=/home/zf/work/cross_compile/4.3.2/bin/arm-none-linux-gnueabi-gcc 

fb_test: fb_test.c
	$(CC) -o $@ $^ 
	sudo cp fb_test /nfs/s3c2440_new/

clean:
	rm -f *.o fb_test
