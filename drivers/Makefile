
$(shell mkdir -p build/test)

PROG?=../build/wmbusmeters

database:
	@./generate.sh $(PROG)

web:
	@mkdir -p ../build/web
	@./generate_index.sh
	@echo "Generated ../build/web/index.html and files."

tests:
	@echo -n "// Generated " > build/generated_tests.xmq
	@date +%Y-%m-%d_%H:%M >> build/generated_tests.xmq
	@for i in $(wildcard src/*) ; do xmq $$i select /driver/tests/test to-xmq >> build/generated_tests.xmq; done

install: database tests
	@grep -v "// Generated " < build/generated_database.cc > build/a
	@grep -v "// Generated " < ../src/generated_database.cc > build/b
	@if ! diff build/a build/b ; then \
            cp build/generated_database.cc ../src ; echo "Installed db"; else echo "No changes db." ; fi
	@grep -v "// Generated " < build/generated_tests.xmq > build/a
	@grep -v "// Generated " < ../tests/generated_tests.xmq > build/b
	@if ! diff build/a build/b ; then \
            cp build/generated_tests.xmq ../tests/generated_tests.xmq ; echo "Installed tests." ; else echo "No changes test." ; fi

generate_ha_discovery:
	@mkdir -p build/ha_mqtt_discovery
	@for i in $(wildcard src/*) ; do echo $$i ; xmq $$i transform to-ha-discovery.xslq to-json | jq . > build/ha_mqtt_discovery/$$(basename $${i%%.xmq}.json); done

pretty:
	@for i in $(wildcard src/*.xmq) ; do xmq $$i > tmp ; mv tmp $$i ; done

test:
	@for i in $(wildcard src/*) ; do ./test.sh $(PROG) $$i ; done

.PHONY: database web tests install test
