.PHONY: clean

# defalut
SocketThread:
	gcc -o ddupserver -std=c99 -pthread `xml2-config --cflags` ddupserver.c `xml2-config --libs` repoutil.c -lssl -lcrypto
	gcc -std=c99 -pthread ddupclient.c -o ddupclient

clean:
	$(RM) *.out
