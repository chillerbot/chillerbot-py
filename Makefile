network:	libnetwork/network.cpp
	g++ $(DEBUG) -c -fPIC libnetwork/network.cpp -o network.o
	g++ $(DEBUG) -shared -Wl,-soname,libtwnetwork.so -o libtwnetwork.so network.o

debug: DEBUG=-g

debug: network

clean:
	rm *.o
	rm *.so
	rm *.gch

