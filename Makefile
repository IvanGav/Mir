main: src/.
# 	LLVM_DISABLE_ABI_BREAKING_CHECKS_ENFORCING=1
# 	g++ src/main.cpp $(llvm-config --cxxflags --ldflags --libs core) -std=c++20 -O0 -g
	g++ src/main.cpp `llvm-config --cxxflags --ldflags --libs core` -std=c++20 -O0 -g -DLLVM_DISABLE_ABI_BREAKING_CHECKS_ENFORCING
# 	g++ -Wall src/main.cpp -g3 -O0 -std=c++20
# 	g++ src/main.cpp $(llvm-config --cxxflags --ldflags --libs core) -o mainllvm.o -std=c++20 -O0 -g -c
# 	g++ mainllvm.o $(llvm-config --ldflags --libs core) -lpthread -o mainllvm