


run: test
	valgrind  --leak-check=full ./test

test: test.o
	g++ $< -g -o $@

test.o: test.cpp
	g++ -c -g -DTEST $< -o $@ -I./

clean:
	@rm test.o test
