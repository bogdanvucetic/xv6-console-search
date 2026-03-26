// Console input and output.
// Input is from the keyboard or serial port.
// Output is written to the screen and serial port.

#include "types.h"
#include "defs.h"
#include "traps.h"
#include "spinlock.h"
#include "file.h"
#include "memlayout.h"
#include "proc.h"
#include "x86.h"
#include "printf.h"

#include "search.h"

#include <stdarg.h>

static void consputc(int);

static int panicked = 0;

static struct {
	struct spinlock lock;
	int locking;
} cons;

// Print to the console. only understands %d, %x, %p, %s.
void
cprintf(char *fmt, ...)
{
	int locking;
	va_list args;

	locking = cons.locking;
	if(locking)
		acquire(&cons.lock);

	if (fmt == 0)
		panic("null fmt");

	va_start(args, fmt);

	fnvprintf(consputc, fmt, args);

	va_end(args);

	if(locking)
		release(&cons.lock);
}

void
panic(char *s)
{
	int i;
	uint pcs[10];

	cli();
	cons.locking = 0;
	// use lapiccpunum so that we can call panic from mycpu()
	cprintf("lapicid %d: panic: ", lapicid());
	cprintf(s);
	cprintf("\n");
	getcallerpcs(&s, pcs);
	for(i=0; i<10; i++)
		cprintf(" %p", pcs[i]);
	panicked = 1; // freeze other CPU
	for(;;)
		;
}

#define BACKSPACE 0x100
#define CRTPORT 0x3d4
static ushort *crt = (ushort*)P2V(0xb8000);  // CGA memory

static void
cgaputc(int c)
{
	int pos;

	// Cursor position: col + 80*row.
	outb(CRTPORT, 14);
	pos = inb(CRTPORT+1) << 8;
	outb(CRTPORT, 15);
	pos |= inb(CRTPORT+1);

	if(c == '\n')
		pos += 80 - pos%80;
	else if(c == BACKSPACE){
		if(pos > 0) --pos;
	} else
		crt[pos++] = (c&0xff) | 0x0700;  // black on white

	if(pos < 0 || pos > 25*80)
		panic("pos under/overflow");

	if((pos/80) >= 24){  // Scroll up.
		memmove(crt, crt+80, sizeof(crt[0])*23*80);
		pos -= 80;
		memset(crt+pos, 0, sizeof(crt[0])*(24*80 - pos));
	}

	outb(CRTPORT, 14);
	outb(CRTPORT+1, pos>>8);
	outb(CRTPORT, 15);
	outb(CRTPORT+1, pos);
	crt[pos] = ' ' | 0x0700;
}

void
consputc(int c)
{
	if(panicked){
		cli();
		for(;;)
			;
	}

	if(c == BACKSPACE){
		uartputc('\b'); uartputc(' '); uartputc('\b');
	} else
		uartputc(c);

	cgaputc(c);
}

#define INPUT_BUF 128
struct {
	char buf[INPUT_BUF];
	uint r;  // Read index
	uint w;  // Write index
	uint e;  // Edit index
} input;

#define C(x)  ((x)-'@')  // Control-x

#define ALT_P	0x1C
#define ALT_F	0x1D
#define ALT_N	0x1E

void
put_query_char(int c)
{
	int pos;

	// Cursor position: col + 80*row.
	outb(CRTPORT, 14);
	pos = inb(CRTPORT+1) << 8;
	outb(CRTPORT, 15);
	pos |= inb(CRTPORT+1);

	if (c == BACKSPACE){
		if(pos > 0) --pos;
	} else
		crt[pos++] = (c&0xff) | 0x0700;

	if(pos < 24*80 || pos > 25*80)
		panic("pos under/overflow");

	outb(CRTPORT, 14);
	outb(CRTPORT+1, pos>>8);
	outb(CRTPORT, 15);
	outb(CRTPORT+1, pos);
	crt[pos] = ' ' | 0x0700;
}

void
clear_highlights()
{
	for (int i = 0; i < 1920; i++) {
		crt[i] = (crt[i] & 0xFF) | SEARCH_COLOR;
	}
}

void
paint_highlighs()
{
	clear_highlights();

	int addr;

	for (int i = 0; i < mnum; i++) {
		short color = i == curr_highlighted ? 0x2F00 : HIGHLIGHT_COLOR;

		for (int j = 0; j < qlen; j++) {
			addr = matches[i] + j;
			crt[addr] = (crt[addr] & 0xFF) | color;
		}
	}
}

void
find_matches()
{
	mnum = 0;
	int matching;

	if (qlen == 0) {
		curr_highlighted = -1;
		return;
	}

	for (int i = 0; i < 1920 - qlen + 1; i++) {
		matching = 1;

		for (int j = 0; j < qlen; j++) {
			char ch = crt[i + j] & 0xFF;

			int match_exists = (ch == query[j]) ||
			('a' <= ch && ch <= 'z' && ch == query[j] + 32) ||
			('A' <= ch && ch <= 'Z' && ch == query[j] - 32);

			if (!match_exists) {
				matching = 0;
				break;
			}
		}

		if (matching) {
			matches[mnum++] = i;
		}
	}

	if (mnum == 0) curr_highlighted = -1;
	else curr_highlighted = 0;
}

void
update_query(int c)
{
	if(c == BACKSPACE){
		if (qlen <= 0) return;

		query[--qlen] = '\0';

		if(qlen == 0){
			put_query_char(c);
			clear_highlights();
			return;
		}
	} else {
		query[qlen++] = c;
	}

	put_query_char(c);

	find_matches();
	paint_highlighs();
}

void
searchintr(int (*getc)(void))
{
	int c = 0;

	acquire(&cons.lock);
	while((c = getc()) >= 0){
		switch(c){
			case '\n':
				break;
			case C('H'): case '\x7f':  // Backspace
				update_query(BACKSPACE);
				break;
			case ALT_F: // Exit
				search_mode_active = 0;

				for (int i = 0; i < 2000; i++) {
					crt[i] = prev_content[i];
				}

				outb(CRTPORT, 14);
				outb(CRTPORT+1, old_pos>>8);
				outb(CRTPORT, 15);
				outb(CRTPORT+1, old_pos);
				crt[old_pos] = ' ' | 0x0700;

				break;
			case ALT_P: // Prev
				curr_highlighted = (curr_highlighted + mnum - 1) % mnum;
				paint_highlighs();
				break;
			case ALT_N: // Next
				curr_highlighted = (curr_highlighted + mnum + 1) % mnum;
				paint_highlighs();
				break;
			default:
				if(c != 0 && qlen < query_capacity){
					update_query(c);
				}
				break;
		}
	}
	release(&cons.lock);
}

void
consoleintr(int (*getc)(void))
{
	if(search_mode_active){
		searchintr(getc);
		return;
	}

	int c, doprocdump = 0;

	acquire(&cons.lock);
	while((c = getc()) >= 0){
		switch(c){
		case C('P'):  // Process listing.
			// procdump() locks cons.lock indirectly; invoke later
			doprocdump = 1;
			break;
		case C('U'):  // Kill line.
			while(input.e != input.w &&
			      input.buf[(input.e-1) % INPUT_BUF] != '\n'){
				input.e--;
				consputc(BACKSPACE);
			}
			break;
		case C('H'): case '\x7f':  // Backspace
			if(input.e != input.w){
				input.e--;
				consputc(BACKSPACE);
			}
			break;
		case ALT_F:
			search_mode_active = 1;

			char prompt[] = PROMPT;
			int pos = 24 * 80;

			for (int i = 0; i < 24 * 80; i++) {
				prev_content[i] = crt[i];
				crt[i] = (crt[i] & 0xFF) | SEARCH_COLOR;
			}

			for (int i = 0; i < PROMPT_LEN; i++) {
				prev_content[pos + i] = crt[pos + i];
				crt[pos++] = (prompt[i] & 0xFF) | 0x0200;
			}

			for (int i = 0; i < qlen; i++) {
				prev_content[pos + i] = crt[pos + i];
				crt[pos++] = (query[i] & 0xFF) | 0x0700;
			}

			find_matches();
			paint_highlighs();

			outb(CRTPORT, 14);
			old_pos = inb(CRTPORT+1) << 8;
			outb(CRTPORT, 15);
			old_pos |= inb(CRTPORT+1);

			outb(CRTPORT, 14);
			outb(CRTPORT+1, pos>>8);
			outb(CRTPORT, 15);
			outb(CRTPORT+1, pos);
			crt[pos] = ' ' | 0x0700;

			break;
		case ALT_P: case ALT_N:
			break;
		default:
			if(c != 0 && input.e-input.r < INPUT_BUF){
				c = (c == '\r') ? '\n' : c;
				input.buf[input.e++ % INPUT_BUF] = c;
				consputc(c);
				if(c == '\n' || c == C('D') || input.e == input.r+INPUT_BUF){
					input.w = input.e;
					wakeup(&input.r);
				}
			}
			break;
		}
	}
	release(&cons.lock);
	if(doprocdump) {
		procdump();  // now call procdump() wo. cons.lock held
	}
}

int
consoleread(struct inode *ip, char *dst, int n)
{
	uint target;
	int c;

	iunlock(ip);
	target = n;
	acquire(&cons.lock);
	while(n > 0){
		while(input.r == input.w){
			if(myproc()->killed){
				release(&cons.lock);
				ilock(ip);
				return -1;
			}
			sleep(&input.r, &cons.lock);
		}
		c = input.buf[input.r++ % INPUT_BUF];
		if(c == C('D')){  // EOF
			if(n < target){
				// Save ^D for next time, to make sure
				// caller gets a 0-byte result.
				input.r--;
			}
			break;
		}
		*dst++ = c;
		--n;
		if(c == '\n')
			break;
	}
	release(&cons.lock);
	ilock(ip);

	return target - n;
}

int
consolewrite(struct inode *ip, char *buf, int n)
{
	int i;

	iunlock(ip);
	acquire(&cons.lock);
	for(i = 0; i < n; i++)
		consputc(buf[i] & 0xff);
	release(&cons.lock);
	ilock(ip);

	return n;
}

void
consoleinit(void)
{
	initlock(&cons.lock, "console");

	devsw[CONSOLE].write = consolewrite;
	devsw[CONSOLE].read = consoleread;
	cons.locking = 1;

	ioapicenable(IRQ_KBD, 0);
}
