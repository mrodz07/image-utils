CC_FLAGS = -Wall -Wextra -Wpedantic -lm -fopenmp -g -o
OUT_NAME = output

default: run

run: output
	./$(OUT_NAME)

output: image_utils.c
	cc image_utils.c $(CC_FLAGS) $(OUT_NAME)

.PHONY: clean run

clean:
	rm -rf output
