build:
	mkdir -p build


tests:
	echo "Nothing to do"


make_symlinks:
	shopt -s nullglob; for dir in */; do \
	  [ "$${dir%/}" = "Unity" ] && continue; \
	  ln -sr Unity/src/unity.c $${dir}; \
	  ln -sr Unity/src/unity.h $${dir}; \
	  ln -sr Unity/src/unity_internals.h $${dir}; \
	done

clean:
	rm -rf build/
