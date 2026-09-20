
$(shell mkdir -p build/test)

PROG?=../build/wmbusmeters

database: pretty library
	@LC_ALL=C ./generate.sh $(PROG)

# Generate src/generated_library.cc from library.xmq
# This is compiled into wmbusmeters providing
# MeterCommonImplementation::addOptionalLibraryFields().
library:
	@mkdir -p build
	@LC_ALL=C ./generate_library.sh

web:
	@mkdir -p ../build/web
	@LC_ALL=C ./generate_index.sh

tests:
	@echo "// Generated" > build/generated_tests.xmq
	@SOURCES=$$(ls src | LC_ALL=C sort) ; for i in $$SOURCES ; do xmq src/$$i select /driver/tests/test to-xmq >> build/generated_tests.xmq; done

install: database tests
	@grep -v "// Generated " < build/generated_database.cc > build/a
	@grep -v "// Generated " < ../src/generated_database.cc > build/b
	@if ! diff build/a build/b ; then \
            cp build/generated_database.cc ../src ; echo "Installed db"; else echo "No changes db." ; fi
	@if ! diff build/generated_library.cc ../src/generated_library.cc ; then \
            cp build/generated_library.cc ../src ; echo "Installed library"; else echo "No changes library." ; fi
	@grep -v "// Generated " < build/generated_tests.xmq > build/a
	@grep -v "// Generated " < ../tests/generated_tests.xmq > build/b
	@if ! diff build/a build/b ; then \
            cp build/generated_tests.xmq ../tests/generated_tests.xmq ; echo "Installed tests." ; else echo "No changes test." ; fi

generate_ha_discovery:
# This is not working yet....
	@mkdir -p build/ha_mqtt_discovery
	@for i in $(wildcard src/*) ; do echo $$i ; xmq $$i transform to-ha-discovery.xslq to-json | jq . > build/ha_mqtt_discovery/$$(basename $${i%%.xmq}.json); done

pretty:
	@for i in $(wildcard src/*.xmq) ; do xmq $$i > tmp ; mv tmp $$i ; done

test:
	@for i in $(wildcard src/*) ; do ./test.sh $(PROG) $$i ; done

testonly:
	@if [ -z "$(DRIVER)" ]; then echo "Usage: make testonly DRIVER=abc"; exit 1; fi
	@./test.sh $(PROG) src/$(DRIVER).xmq

clean:
	rm -f generated.test.json

.PHONY: database web tests install library test clean
