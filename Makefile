ctest:
	cd microdict/tests && \
	python ./generate.py . && \
	gcc -I. -o suite *.c && \
	./suite