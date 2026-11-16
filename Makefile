.PHONY: all run web dataset test clean

all:
	g++ -std=c++11 -Wall -o smart_revision main.cpp

run: all
	mkdir -p data
	./smart_revision

web:
	python3 web_app.py

dataset:
	python3 import_dataset.py

test:
	g++ -std=c++11 -Wall -o test_mylib test_mylib.cpp
	./test_mylib
	g++ -std=c++11 -Wall -o test_core test_core.cpp
	./test_core
	python3 test_web.py

clean:
	rm -f smart_revision test_mylib test_core
	rm -rf data_test test_data
