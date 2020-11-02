#include<stdio.h>
#include<sys/types.h>
#include <stdlib.h> 
#include <pthread.h>

long cnt;

typedef struct m0_atomic64 {
	long a_value;
} atomic64_t;

atomic64_t a = {1000};
int atomic_test = 1;
int double_arg = 0;
static int64_t (*atomic_double_func)(long delta, atomic64_t *a);
static int64_t (*atomic_single_func)(atomic64_t *a);
static int64_t  (*nonatomic_test_func)(long *delta);

static int64_t  nonatomic64_add(long *i) 
{
	*i = *i + 1;
}                   

static int64_t nonatomic64_sub(long *i) 
{
	*i = *i - 1;
}

static int64_t atomic64_add(long i, atomic64_t *v)                    
{                                                                       
        long result;                                                    
        unsigned long tmp;
	asm volatile("//atomic64_add \n"			\
	"       prfm    pstl1strm, %2\n"			\
	"1:     ldxr    %0, %2\n"				\
	"       add     %0, %0, %3\n"				\
	"       stxr    %w1, %0, %2\n"                          \
	"       cbnz    %w1, 1b"				\
        : "=&r" (result), "=&r" (tmp), "+Q" (v->a_value)	\
        : "Ir" (i));
                                                  
}              

static int64_t m0_atomic64_add_return(long delta, atomic64_t *a)
{
        long result;                                                    
        unsigned long tmp;                                              
                                                                        
        asm volatile("// atomic64_add_return \n"	\
"       prfm    pstl1strm, %2\n"		\
"1:     ldxr    %0, %2\n"			\
"       add     %0, %0, %3\n"		\
"       stlxr    %w1, %0, %2\n"		\
"      	cbnz    %w1, 1b\n"		\
"	dmb ish"			\
        : "=&r" (result), "=&r" (tmp), "+Q" (a->a_value)	\
        : "Ir" (delta)				\
        : "memory");				
                                                                        
        return result;                                                  
}

static int64_t m0_atomic64_sub(int64_t num, struct m0_atomic64 *v)
{
        long result;
        unsigned long tmp;
        asm volatile("// atomic64_sub \n"                       \
        "       prfm    pstl1strm, %2\n"                        \
        "1:     ldxr    %0, %2\n"                               \
        "       sub     %0, %0, %3\n"                           \
        "       stxr    %w1, %0, %2\n"                          \
        "       cbnz    %w1, 1b"                                \
        : "=&r" (result), "=&r" (tmp), "+Q" (v->a_value)        \
        : "Ir" (num));
}

static inline int64_t m0_atomic64_sub_return(int64_t delta, atomic64_t *a)
{
        int64_t result;
        unsigned long tmp;

        asm volatile("// atomic64_sub_return \n"        \
"       prfm    pstl1strm, %2\n"                	\
"1:     ldxr    %0, %2\n"                       	\
"       sub     %0, %0, %3\n"           		\
"       stlxr    %w1, %0, %2\n"         		\
"       cbnz    %w1, 1b\n"              		\
"       dmb ish"                        		\
        : "=&r" (result), "=&r" (tmp), "+Q" (a->a_value)        \
        : "Ir" (delta)                          \
        : "memory");

        return result;
}

static inline int64_t m0_atomic64_inc(struct m0_atomic64 *a)
{
	 atomic64_add((int64_t)1, a);
	return 0;
}

/**
 atomically decrement counter

 @param a pointer to atomic counter

 @return none
 */
static inline int64_t m0_atomic64_dec(struct m0_atomic64 *a)
{
	 m0_atomic64_sub((int64_t)1, a);
	 return 0;

}

static inline int64_t  m0_atomic64_inc_and_test(struct m0_atomic64 *a)
{
	return (m0_atomic64_add_return(1, a) == 0);
}

static inline int64_t m0_atomic64_dec_and_test(struct m0_atomic64 *a)
{

	return (m0_atomic64_sub_return(1, a) == 0);

}


void* thfn(void* thr_data) {

	int n = 0;
	for( n = 0; n < 10000; ++n) {
	/* ATOMIC opertation vs Non atomic op*/
		if (atomic_test && double_arg)
			atomic_double_func(1,&a);
		else if(atomic_test) {
			if (atomic_single_func(&a)) printf("\n Value test succedded\n");
		}
		else
			nonatomic_test_func(&cnt);
	}

	return 0;
}
 
int main(int argc, char *argv[])
{
     	pthread_t thr[10];
	int n, k;
	int input_func = 0;

	if (argc < 2)
		goto help;
	for ( k = 1; k < argc; k++) {

		if ((!strcmp(argv[k], "atomic64_add"))){
			atomic_double_func = atomic64_add;
			double_arg = 1;
			input_func = k;
		} else if ((!strcmp(argv[k], "atomic64_add_return"))){
			double_arg = 1;
			atomic_double_func = m0_atomic64_add_return;
			input_func = k;
		} else if ((!strcmp(argv[k], "atomic64_sub"))){
			double_arg = 1;
			atomic_double_func = m0_atomic64_sub;
			input_func = k;
		} else if ((!strcmp(argv[k], "atomic64_sub_return"))){
			double_arg = 1;
			atomic_double_func = m0_atomic64_sub_return;
			input_func = k;
		} else if ((!strcmp(argv[k], "atomic64_inc"))){
			atomic_single_func = m0_atomic64_inc;
			input_func = k;
		} else if ((!strcmp(argv[k], "atomic64_inc_and_test"))){
			atomic_single_func = m0_atomic64_inc_and_test;
			input_func = k;
		} else if ((!strcmp(argv[k], "atomic64_dec"))){
			atomic_single_func = m0_atomic64_dec;
			input_func = k;
		} else if ((!strcmp(argv[k], "atomic64_dec_and_test"))){
			atomic_single_func = m0_atomic64_dec_and_test;
			input_func = k;
		} else if ((!strcmp(argv[k], "normal_sub"))){
			atomic_test = 0;
			nonatomic_test_func = nonatomic64_sub;
			input_func = k;
		} else if ((!strcmp(argv[k], "normal_add"))){
			atomic_test = 0;
			nonatomic_test_func = nonatomic64_add;
			input_func = k;
		} else {
			help:
			printf("\nValid arguments: \n					\
				\r\t1. atomic64_add \n					\
				\r\t2. atomic64_add_return \n				\
				\r\t3. atomic64_sub \n					\
				\r\t4. atomic64_sub_return \n				\
				\r\t5. atomic64_inc \n					\
				\r\t6. atomic64_inc_and_test \n				\
				\r\t7. atomic64_dec \n					\
				\r\t8. atomic64_dec_and_test \n				\
				\r\t9. normal_add\n					\
				\r\t10.normal_sub\n");
			return 0;
		}
	}

	for( n = 0; n < 10; ++n)
		pthread_create(&thr[n], NULL, thfn, NULL); 
    
	for( n = 0; n < 10; ++n)
        	pthread_join(thr[n], NULL);
		if (atomic_test)
			printf("\n %s : The final value = %d\n", argv[input_func], a.a_value);
		else
			printf("\n %s : The final value = %d\n", argv[input_func], cnt);
}
