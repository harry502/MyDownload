cc = g++
prom = ../bin/http_down_main
obj = http_down.o http_down_main.o

$(prom): $(obj)
	$(cc) -o $(prom) $(obj) -g -lpthread

%.o: %.cpp
	$(cc) -c $< -o $@

clean:
	rm *.o
	rm ../bin/*

