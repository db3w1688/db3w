#include <stdio.h>

typedef unsigned long long int word64;
#ifdef __alpha
typedef unsigned int word32;
#else
typedef unsigned int word32;
#endif
typedef unsigned char byte;

typedef unsigned char octet[8];

word32 table[4*256][2];
static octet *table_ch = (octet*) table;

main()
{
  gen("Tiger - A Fast New Hash Function, by Ross Anderson and Eli Biham", 5);
  print_table();
}

/* Big endian:                                         */
#if !(defined(__alpha)|| defined(__i386__)|| defined(__x86_64__) || defined(__vax__))
#define BIG_ENDIAN
#endif

gen(word32 str[16], int passes)
{
  word64 state[3];
  octet *state_ch = (octet*) state;
  byte tempstr[64];

  int i;
  int j;
  int cnt;
  int sb;
  int col;
  int abc;

  state[0]=0x0123456789ABCDEFLL;
  state[1]=0xFEDCBA9876543210LL;
  state[2]=0xF096A5B4C3B2E187LL;

#ifdef BIG_ENDIAN
      for(j=0; j<64; j++)
        tempstr[j^7] = ((byte*)str)[j];
#else
      for(j=0; j<64; j++)
        tempstr[j] = ((byte*)str)[j];
#endif
  for(i=0; i<1024; i++)
    for(col=0; col<8; col++)
      table_ch[i][col] = i&255;

  abc=2;
  for(cnt=0; cnt<passes; cnt++)
    for(i=0; i<256; i++)
      for(sb=0; sb<1024; sb+=256)
	{
	  abc++;
	  if(abc == 3)
	    {
	      abc = 0;
	      tiger_compress(tempstr, state);
	    }
	  for(col=0; col<8; col++) {
	      byte tmp = table_ch[sb+i][col];
	      table_ch[sb+i][col] = table_ch[sb+state_ch[abc][col]][col];
	      table_ch[sb+state_ch[abc][col]][col] = tmp;
	    }
	}  
}

/* Big endian:                                         */
#if !(defined(__alpha)||defined(__i386__)||defined(__vax__))
#define BIG_ENDIAN
#define M 0
#define L 1
#else
#define M 1
#define L 0
#endif

#ifdef __alpha
#define MASK 0x00000000FFFFFFFFLL
#else
#define MASK 0xFFFFFFFF
#endif

print_table()
{
  int i;

  printf("/* sboxes.c: Tiger S boxes */\n");
  printf("typedef unsigned long long int word64;\n");
  printf("word64 table[4*256] = {\n");
  for(i=0; i<1024; i++)
    {
      printf("    0x%08lX%08lXLL   /* %4d */",
	     table[i][M]&MASK, table[i][L]&MASK, i);
      if(i < 1023)
	printf(",");
      else
	printf("};");
      if(i&1)
	printf("\n");
    }

  printf("\n\n\n");

  printf("/* sboxes32.c: Tiger S boxes for 32-bit-only compilers */\n");
  printf("typedef unsigned long word32;\n");
  printf("word32 table[4*256][2] = {\n");
  for(i=0; i<1024; i++)
    {
      printf("    0x%08lX, 0x%08lX /* %4d */",
	     table[i][L]&MASK, table[i][M]&MASK, i);
      if(i < 1023)
	printf(",");
      else
	printf("};");
      if(i&1)
	printf("\n");
    }
}

fatal(char *str)
{
  fprintf(stdout, "Fatal error: %s\n", str);
  exit(2);
}
