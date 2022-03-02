network:	network.cpp
	g++ -c -fPIC network.cpp -o network.o
	g++ -shared -Wl,-soname,libtwnetwork.so -o libtwnetwork.so network.o

clean:
	rm *.o
	rm *.so

