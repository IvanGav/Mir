dev: src/.
	g++ src/main.cpp -std=c++20 -O0 -g -Wall

good: src/.
	g++ src/main.cpp -std=c++20 -O3 -Wall