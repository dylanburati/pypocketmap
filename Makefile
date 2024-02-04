ctest:
	cd microdict/tests && \
	python ./generate.py . && \
	gcc $(EXTRA_GCC_FLAGS) -fsanitize=address -I. -o suite *.c && \
	./suite
