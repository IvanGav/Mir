dev: src/.
	g++ src/main.cpp -std=c++20 -O0 -g -Wall

good: src/.
	g++ src/main.cpp -std=c++20 -O3 -Wall

graph:
	dot -Tpng -O graph.gv

clean:
	rm a.out main graph.gv