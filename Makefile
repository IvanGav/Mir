# This is not good Makefile practice. I'm aware. It's a personal project with only me being a contributor.

dev: src/.
	g++ src/main.cpp -o mirc -std=c++20 -O0 -g -Wall

good: src/.
	g++ src/main.cpp -o mirc -std=c++20 -O3 -Wall

assemble: mir.s
	gcc -nostdlib -no-pie -o mir.out mir.s

graph: graph.gv
	dot -Tpng -O graph.gv

clean:
	rm mirc graph.gv mir.s mir.out graph.gv.png