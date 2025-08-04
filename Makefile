default: all

test:
	tests/run.sh

.DEFAULT:
	cd src && $(MAKE) $@
