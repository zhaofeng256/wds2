#include <stdio.h>

int main(int argc, char **argv)
{

	FILE * fp = fopen(argv[1], "r");
	if (!fp) {
		printf("open file fail\n");
		return -1;
	}
	fclose(fp);
	return 0;
}

