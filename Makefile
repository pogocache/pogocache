default: all

test:
	tools/tests/run.sh

package:
	tools/package.sh

.DEFAULT:
	cd src && $(MAKE) $@
