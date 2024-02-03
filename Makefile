ctest:
	cd microdict/tests && \
	python ./generate.py . && \
	gcc $(EXTRA_GCC_FLAGS) -I. -o suite *.c && \
	./suite
