// A printf utilizing a function pointer to determine the output stream.
#include "printf.h"
#include "types.h"
#include "defs.h"
#include "x86.h"

static void
printint(fnprintf_putc putc, int xx, int base, int sign)
{
	static char digits[] = "0123456789abcdef";
	char buf[16];
	int i;
	uint x;

	if(sign && (sign = xx < 0))
		x = -xx;
	else
		x = xx;

	i = 0;
	do{
		buf[i++] = digits[x % base];
	}while((x /= base) != 0);

	if(sign)
		buf[i++] = '-';

	while(--i >= 0)
		putc(buf[i]);
}

void
fnvprintf (fnprintf_putc putc, const char* fmt, va_list args)
{
	int i, c;
	const char* s;
	for(i = 0; (c = fmt[i] & 0xff) != 0; i++){
		if(c != '%'){
			putc(c);
			continue;
		}
		c = fmt[++i] & 0xff;
		if(c == 0)
			break;
		switch(c){
		case 'd':
			printint(putc, va_arg(args, int), 10, 1);
			break;
		case 'x':
			printint(putc, va_arg(args, int), 16, 0);
			break;
		case 'p': {
			void* arg = va_arg(args, void*);
			int reinterpreted;
			memmove(&reinterpreted, &arg, sizeof(arg));

#if __STDC_VERSION__ > 201112L
			_Static_assert(sizeof(arg) == sizeof(reinterpreted),
				       "we're assuming ILP32");
			// HACK: printint also assumes 2C, but whatever
#endif

			printint(putc, reinterpreted, 16, 0);
		}
			break;
		case 's':
			if((s = va_arg(args, char*)) == 0)
				s = "(null)";
			for(; *s; s++)
				putc(*s);
			break;
		case '%':
			putc('%');
			break;
		default:
			// Print unknown % sequence to draw attention.
			putc('%');
			putc(c);
			break;
		}
	}
}


// A debugging printf that works in QEMU without any locks.  Requires an
// isa-debugcon device to be configured.
void
e9putc(int x)
{
	outb(0xe9, x);
}

void
e9printf(const char* fmt, ...)
{
	va_list args;

	va_start(args, fmt);
	fnvprintf(e9putc, fmt, args);
	va_end(args);
}
