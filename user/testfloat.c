#include <inc/lib.h>

void print_float(float a){
	unsigned int num = (unsigned int)a;
}

float test_func(float a){

	return a * 2.5;
}	

float exponent(float base, unsigned int exp){
	float product = 1.0;
	int count = 0;
	while (count < exp){
		product *= base;
		count++;
	}
	return product;
}

void calc_pi(int n){
	int k = 0;
	float running_sum = 0.0;
	float coeff = 0;

	float garbage = 2.75;	
	print_float(garbage);
	if (n){
		for (; k < n; k++){
			coeff = (1.0 / (exponent(16,k))) * 
			(
			(4.0/(8*k + 1)) -
			(2.0/(8*k + 4)) -
			(1.0/(8*k + 5)) -
			(1.0/(8*k + 6))
			);
			running_sum += coeff;
			cprintf("Approximation #%d: %f\n",k,running_sum);	
		}
		
	}
}

void umain(int argc, char **argv){


	calc_pi(10);
}
