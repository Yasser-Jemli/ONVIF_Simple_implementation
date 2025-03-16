LIBS = `curl-config --libs`

mycurlapp : mycurlapp.o
	$(CC) -g -o $@ $< $(LIBS)

mycurlapp.o : mycurlapp2.c
	$(CC) -g -c -o $@ $<


clean:
	rm -rf mycurlapp mycurlapp.o