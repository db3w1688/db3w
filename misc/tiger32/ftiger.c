typedef unsigned int word32;
typedef unsigned char byte;

#define BUFSIZ	1024
#define TBUFSIZ	64

#include <stdio.h>

main(int argc, char *argv[])
{
	FILE *infp;
	char buf[BUFSIZ],*tp;
	word32 res[6];
	int i;

	if (argc>2) {
		fprintf(stderr,"Usage: %s [filename]\n",argv[0]);
		exit(1);
	}
	if (argc==2) {
		infp = fopen(argv[1],"r");
	}
	else {
		printf("Enter input filename: ");
		if (!fgets(buf, BUFSIZ, stdin)) {
			fprintf(stderr,"Cannot get input\n");
			exit(1);
		}
		tp = buf + strlen(buf);
		if (*(tp-1) == '\n') *(tp-1) = '\0';
		infp = fopen(buf,"r");
	}
	if (!infp) {
		fprintf(stderr,"Cannot open %s\n",argv[1]);
		exit(1);
	}
	res[0]=0x89ABCDEF;
	res[1]=0x01234567;
	res[2]=0x76543210;
	res[3]=0xFEDCBA98;
	res[4]=0xC3B2E187;
	res[5]=0xF096A5B4;
	while (!feof(infp) && !ferror(infp)) {
		for (i=0; i<TBUFSIZ; ++i) {
			int c;
			if ((c = fgetc(infp)) == EOF) break;
			buf[i] = c;
		}
		if (i<TBUFSIZ) buf[i] = '\0';
		tiger_compress(buf, res);
	}
	fclose(infp);
	printf("%08X%08X %08X%08X %08X%08X\n",res[1],res[0],res[3],res[2],res[5],res[4]);
}
