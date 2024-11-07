/* Notes from PINTOS Doc:
Unfortunately, Pintos does not support floating-point arithmetic
in the kernel, This means that calculations on real quantities must
be simulated using integers. The fundamental idea is to treat the rightmost
bits of an integer as representing a Fraction.

we can designate the lowest 14 bits of a signed 32-bit integer as
Fractional bits, This is called a 17.14 fixed-point number.

Suppose that we are using a p.q fixed-point format, and let f = 2q. By the
definition above, we can convert an integer or real number into p.q format by
multiplying with f.

To convert a fixed-point value back to an integer, divide by f.
The normal ‘/’ operator in C rounds toward zero, that is, it rounds
positive numbers down and negative numbers up. To round to nearest, add f /2 to a positive
number, or subtract it from a negative number, before dividing.

Let x and y be fixed-point numbers, and let n be an integer.
Then the sum of x and y is x + y
and their difference is x - y.
The sum of x and n is x + n * f;
difference, x - n * f;
product, x * n;
quotient, x / n.

Multiplying two fixed-point values x and y: ((int64_t) x) * y / f.
Dividing two fixed-point values x and y: ((int64_t) x) * f / y.
*/

#ifndef THREADS_FIXED_POINT_H
#define THREADS_FIXED_POINT_H

#define fp_t int
#define P 17
#define Q 14
#define F 1<<(Q)

#if P + Q != 31
#error "FATAL ERROR: P + Q != 31."
#endif

#define FIXED_INT_ADD(x, n) (x) + (n) * (F)
#define FIXED_INT_SUBTRACT(x, n) (x) - (n) * (F)
#define FIXED_INT_DIVIDE(x, n) (x) / (n)
#define FIXED_INT_MULTIPLY(x, n) (x) * (n)
#define CONVERT_TO_FIXED(x) (x) * (F)
#define FIXED_TO_INT_ROUND_TOWARDS_ZERO(x) (x) / (F)
#define FIXED_TO_INT_ROUND_TOWARDS_NEAR(x) ((x) >= 0 ? ((x) + (F) / 2) / (F) : ((x) - (F) / 2) / (F))
#define FIXED_MULTIPLY(x, y) ((int64_t)(x)) * (y) / (F)
#define FIXED_DIVISION(x, y) ((int64_t)(x)) * (F) / (y)
#define FIXED_ADD(x, y) (x) + (y)
#define FIXED_SUB(x, y) (x) - (y)

#endif