
/*++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
                               tty.c
++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
                                                    Forrest Yu, 2005
++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++*/

#include "type.h"
#include "const.h"
#include "protect.h"
#include "string.h"
#include "proc.h"
#include "tty.h"
#include "console.h"
#include "global.h"
#include "keyboard.h"
#include "proto.h"

#define TTY_FIRST	(tty_table)
#define TTY_END		(tty_table + NR_CONSOLES)

PRIVATE void init_tty(TTY* p_tty);
PRIVATE void tty_do_read(TTY* p_tty);
PRIVATE void tty_do_write(TTY* p_tty);
PRIVATE void put_key(TTY* p_tty, u32 key);

/*======================================================================*
                           task_tty
 *======================================================================*/
// 这就是进程体。
PUBLIC void task_tty()
{
	TTY*	p_tty;

	init_keyboard();
	// 初始化所有终端：1.缓存；2.CONSOLE。
	for (p_tty=TTY_FIRST;p_tty<TTY_END;p_tty++) {
		init_tty(p_tty);
	}
	// 第0个TTY是当前终端。
	select_console(0);
	while (1) {
		// 和A的进程体一样，最外层也是一个循环。
		// 每个TTY轮流运行：从键盘缓冲区读取数据到TTY的缓冲区--->从TTY的缓冲区读取数据写入显存。
		// 键盘缓冲区的数据是的数据是当前TTY的数据，所以，只有当前TTY才从键盘缓冲区读取数据。
		for (p_tty=TTY_FIRST;p_tty<TTY_END;p_tty++) {
			tty_do_read(p_tty);
			tty_do_write(p_tty);
		}
	}
}

/*======================================================================*
			   init_tty
 *======================================================================*/
PRIVATE void init_tty(TTY* p_tty)
{
	// 初始化缓冲区
	p_tty->inbuf_count = 0;
	p_tty->p_inbuf_head = p_tty->p_inbuf_tail = p_tty->in_buf;
	// 初始化TTY的console。
	init_screen(p_tty);
}

/*======================================================================*
				in_process
 *======================================================================*/
PUBLIC void in_process(TTY* p_tty, u32 key)
{
        // 也可以使用Memeset(output, 0, 2)
        char output[2] = {'\0', '\0'};

        if (!(key & FLAG_EXT)) {
    	// 把字符放入TTY的缓冲区
		put_key(p_tty, key);
        }
        else {
                int raw_code = key & MASK_RAW;
                switch(raw_code) {
            	// 换行符
                case ENTER:
			put_key(p_tty, '\n');
			break;
				// 退格符
                case BACKSPACE:
			put_key(p_tty, '\b');
			break;
				// 向上滚屏
                case UP:
                        if ((key & FLAG_SHIFT_L) || (key & FLAG_SHIFT_R)) {
				scroll_screen(p_tty->p_console, SCR_DN);
                        }
			break;
		// 向下滚屏
		case DOWN:
			if ((key & FLAG_SHIFT_L) || (key & FLAG_SHIFT_R)) {
				scroll_screen(p_tty->p_console, SCR_UP);
			}
			break;
		case F1:
		case F2:
		case F3:
		case F4:
		case F5:
		case F6:
		case F7:
		case F8:
		case F9:
		case F10:
		case F11:
		case F12:
			/* Alt + F1~F12 */
			if ((key & FLAG_ALT_L) || (key & FLAG_ALT_R)) {
				select_console(raw_code - F1);
			}
			break;
                default:
                        break;
                }
        }
}

/*======================================================================*
			      put_key
*======================================================================*/
// 逻辑和放入字符到键盘缓冲区一致。
PRIVATE void put_key(TTY* p_tty, u32 key)
{
	if (p_tty->inbuf_count < TTY_IN_BYTES) {
		*(p_tty->p_inbuf_head) = key;
		p_tty->p_inbuf_head++;
		if (p_tty->p_inbuf_head == p_tty->in_buf + TTY_IN_BYTES) {
			p_tty->p_inbuf_head = p_tty->in_buf;
		}
		p_tty->inbuf_count++;
	}
}


/*======================================================================*
			      tty_do_read
 *======================================================================*/
PRIVATE void tty_do_read(TTY* p_tty)
{
	if (is_current_console(p_tty->p_console)) {
		keyboard_read(p_tty);
	}
}


/*======================================================================*
			      tty_do_write
 *======================================================================*/
PRIVATE void tty_do_write(TTY* p_tty)
{
	if (p_tty->inbuf_count) {
		char ch = *(p_tty->p_inbuf_tail);
		p_tty->p_inbuf_tail++;
		if (p_tty->p_inbuf_tail == p_tty->in_buf + TTY_IN_BYTES) {
			p_tty->p_inbuf_tail = p_tty->in_buf;
		}
		p_tty->inbuf_count--;

		out_char(p_tty->p_console, ch);
	}
}

/*======================================================================*
                              tty_write
*======================================================================*/
PUBLIC void tty_write(TTY* p_tty, char* buf, int len)
{
        char* p = buf;
        int i = len;

        while (i) {
                out_char(p_tty->p_console, *p++);
                i--;
        }
}

/*======================================================================*
                              sys_write
*======================================================================*/
PUBLIC int sys_write(char* buf, int len, PROCESS* p_proc)
{
        tty_write(&tty_table[p_proc->nr_tty], buf, len);
        return 0;
}

