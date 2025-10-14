.PHONY: make_symlinks

make_symlinks:
	shopt -s nullglob; for dir in */; do \
	  [ "$${dir%/}" = "Unity" ] && continue; \
	  ln -sr Unity/src/unity.c $${dir}; \
	  ln -sr Unity/src/unity.h $${dir}; \
	  ln -sr Unity/src/unity_internals.h $${dir}; \
	done
