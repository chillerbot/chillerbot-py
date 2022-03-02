network:	libnetwork/network.cpp
	g++ -c -fPIC libnetwork/network.cpp -o network.o
	g++ -shared -Wl,-soname,libtwnetwork.so -o libtwnetwork.so network.o

clean:
	rm *.o
	rm *.so
	rm *.gch

