#include <setjmp.h>
#include <stdio.h>

int i = 0;
jmp_buf buf;

int main()
{
/*
	int j;
	if (setjmp(buf)){
		printf("for0 i %d\n", i ); 
		printf("for0 j %d\n", j ); 
		for (j=0; j<5; j++){
				i++;
				printf("for1 i %d\n", i ); 
				printf("for1 j %d\n", j );
		}
	}
	else {
		for (j=0; j<5; j++)
			i--;
		printf("for2av i %d\n", i );
		printf("for2av j %d\n", j );
		longjmp(buf,~0);
		printf("for2ap i %d\n", i ); 
		printf("for2ap j %d\n", j ); 
	}
	printf("%d\n", i ); /* return 0 */
/*	printf("%d\n", j ); /* return 5 */


	int j =0;
	if (setjmp(buf)){
		printf("for0 i %d\n", i );
		printf("for0 j %d\n", j ); 
		for (; j<5; j++){
				i++;
				printf("for1 i %d\n", i );
				printf("for1 j %d\n", j );
		}
	}
	else {
		for (; j<5; j++)
			i--;
		printf("for2av i %d\n", i );
		printf("for2av j %d\n", j ); 
		longjmp(buf,~0);
		printf("for2ap i %d\n", i ); 
		printf("for2ap j %d\n", j ); 
	}
	printf("%d\n", i ); /* return -5 */
	printf("%d\n", j ); /* return 5 */


	return 0;

}
