build:
	mkdir -p build


tests: build build/plc-resource-protector

build/plc-resource-protector: plc-resource-protector/plc-resource-protector.ino ../build/libplc-peripherals.a build
	gcc $(CPPFLAGS) $(CFLAGS) -o $@ -x c $< -I../include -I./Unity/src ./Unity/src/unity.c $(LDFLAGS)


.PHONY: make_symlinks clean


make_symlinks:
	shopt -s nullglob; for dir in */; do \
	  [ "$${dir%/}" = "Unity" ] && continue; \
	  ln -sr Unity/src/unity.c $${dir}; \
	  ln -sr Unity/src/unity.h $${dir}; \
	  ln -sr Unity/src/unity_internals.h $${dir}; \
	done

clean:
	rm -rf build/
