all:
	@echo "Build with CPPFLAGS=-DSHORT_NOTATION to convert to the short notation of adding or getting primitives:"
	@echo "Short notation: peer->getProperty("foo") Long notation: peer->properties()->getProperty("foo")."
	@echo "Short notation: this->addProperty( foo ) Long notation: this->properties()->addProperty( foo )."
	@echo "  make CPPFLAG=-DSHORT_NOTATION
	@echo ""
	$(CXX) -Isrc -O0 -g3 -Wall -c -fmessage-length=0 $(CFLAGS) $(CPPFLAGS) -osrc/rtt2-converter.o src/rtt2-converter.cpp
	$(CXX) -ortt2-converter src/rtt2-converter.o $(LDFLAG) -lboost_regex

clean:
	rm -f src/rtt2-converter.o rtt2-converter