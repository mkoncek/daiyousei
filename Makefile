MAKEFLAGS += -r

CXXFLAGS ?= -g -Wall -Wextra -Wpedantic
CXXFLAGS += -std=c++23 -Isrc

Dependency_file = $(addprefix target/dependencies/,$(addsuffix .mk,$(subst /,.,$(basename $(1)))))
Object_file = $(addprefix target/object_files/,$(addsuffix .o,$(subst /,.,$(basename $(1)))))

.PHONY: clean compile test-compile test-serialization test-deserialization test-streaming-deserialization test-unit test-server coverage

compile: target/bin/daiyousei

%/:
	@mkdir -p $@

clean:
	@rm -rfv target

target/bin/cat: target/bin/daiyousei | target/bin/
	@ln -s ./daiyousei $@
target/bin/whoami: target/bin/daiyousei | target/bin/
	@ln -s ./daiyousei $@
target/bin/%: | target/bin/
	$(CXX) -o $@ $(CPPFLAGS) $(CXXFLAGS) $(LDFLAGS) $(LDLIBS) $<

$(call Object_file,%): src/%.cpp | target/object_files/ target/dependencies/
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -MMD -MP -MF $(call Dependency_file,$(<F)) -MT $(call Object_file,$(<F)) -c -o $(call Object_file,$(<F)) $(addprefix src/,$(<F))

target/bin/test_%: LDFLAGS += -Ltarget/lib
target/bin/test_%: LDLIBS += -ltesting
target/bin/test_bencode_serialization: test/bencode_serialization.cpp src/bencode.hpp target/lib/libtesting.a
target/bin/test_bencode_deserialization: test/bencode_deserialization.cpp src/bencode.hpp target/lib/libtesting.a
target/bin/test_bencode_streaming_deserialization: test/bencode_streaming_deserialization.cpp src/bencode.hpp target/lib/libtesting.a

target/lib/libtesting.a: $(call Object_file,testing.cpp) | target/lib/
	$(AR) -rcs $@ $<

target/bin/daiyousei: $(call Object_file,main.cpp)

test-serialization: target/bin/test_bencode_serialization
	@./$<
test-deserialization: target/bin/test_bencode_deserialization
	@./$<
test-streaming-deserialization: target/bin/test_bencode_streaming_deserialization
	@./$<

target/doc/daiyousei.html: doc/daiyousei.adoc | target/doc/
	@asciidoctor -D target/doc $<

test-compile: CXXFLAGS += -fsanitize=undefined,address -D_GLIBCXX_ASSERTIONS -D_GLIBCXX_DEBUG
test-compile: LDFLAGS += -fsanitize=undefined,address
test-compile: compile target/bin/test_bencode_serialization target/bin/test_bencode_deserialization target/bin/test_bencode_streaming_deserialization

test-unit: test-compile test-serialization test-deserialization test-streaming-deserialization

test-server: test/server.py test-compile target/bin/daiyousei
	@./$<

coverage: CXXFLAGS += --coverage -fno-elide-constructors -fno-default-inline
coverage: LDFLAGS += --coverage
coverage: test-unit test-server | target/coverage/
	@lcov --ignore-errors mismatch --output-file target/coverage.info --directory target/bin --directory target/object_files --capture --exclude '/usr/include/*' --exclude 'testing.cpp' --exclude 'testing.hpp'
	@genhtml -o target/coverage target/coverage.info

-include target/dependencies/*.mk
