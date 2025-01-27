MAKEFLAGS += -r

CXXFLAGS ?= -g -Wall -Wextra
CXXFLAGS += -std=c++23 -Isrc

%/:
	@mkdir -p $@

clean:
	@rm -rfv target

target/bin/%: | target/bin/
	$(CXX) -o $@ $(CPPFLAGS) $(CXXFLAGS) $(LDFLAGS) $(LDLIBS) $<

target/bin/test_%: CPPFLAGS = -D_GLIBCXX_ASSERTIONS -D_GLIBCXX_DEBUG
target/bin/test_%: LDLIBS = -lboost_unit_test_framework
target/bin/test_bencode_serialization: test/bencode_serialization.cpp src/bencode.hpp
target/bin/test_bencode_deserialization: test/bencode_deserialization.cpp src/bencode.hpp

target/bin/main: src/main.cpp src/bencode.hpp

main: target/bin/main
	@./$<

test-serialization: target/bin/test_bencode_serialization
	@./$<
test-deserialization: target/bin/test_bencode_deserialization
	@./$<

target/doc/interface.html: doc/interface.adoc | target/doc/
	asciidoctor -D target/doc $<

test: test-serialization test-deserialization

coverage: CXXFLAGS += --coverage -fno-elide-constructors -fno-default-inline
coverage: LDFLAGS += --coverage
coverage: test | target/coverage/
	@lcov --ignore-errors mismatch --output-file target/coverage.info --directory target/bin --capture --exclude '/usr/include/*'
	@genhtml -o target/coverage target/coverage.info
