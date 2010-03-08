all:
	$(CXX) -Isrc -O0 -g3 -Wall -c -fmessage-length=0 -osrc/rtt2-converter.o src/rtt2-converter.cpp
	$(CXX) -ortt2-converter src/rtt2-converter.o -lboost_regex

clean:
	rm -f src/rtt2-converter.o rtt2-converter