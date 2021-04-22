
/*++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
			      console.c
++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
						    Forrest Yu, 2005
++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++*/

/*
	回车键: 把光标移到第一列
	换行键: 把光标前进到下一行
*/


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

PRIVATE void set_cursor(unsigned int position);
PRIVATE void set_video_start_addr(u32 addr);
PRIVATE void flush(CONSOLE* p_con);

/*======================================================================*
			   init_screen
 *======================================================================*/
PUBLIC void init_screen(TTY* p_tty)
{
	// 数组的某个元素的内存地址减去数组的内存地址（数组的第一个元素的内存地址），
	// 结果是，这个元素在数组中大的索引
	int nr_tty = p_tty - tty_table;
	p_tty->p_console = console_table + nr_tty;

	// V_MEM_SIZE是字节，右移动一位，是除以2，结果是字节。
	int v_mem_size = V_MEM_SIZE >> 1;	/* 显存总大小 (in WORD) */

	// 每个TTY的占用的显存大小相同。
	int con_v_mem_size                   = v_mem_size / NR_CONSOLES;
	// 第0个，初始地址是0；第1个，初始地址是con_v_mem_size。
	// 这种边角问题，很让我困惑。紧急时，我会弄错。
	p_tty->p_console->original_addr      = nr_tty * con_v_mem_size;
	p_tty->p_console->v_mem_limit        = con_v_mem_size;
	p_tty->p_console->current_start_addr = p_tty->p_console->original_addr;

	/* 默认光标位置在最开始处 */
	p_tty->p_console->cursor = p_tty->p_console->original_addr;

	if (nr_tty == 0) {
		/* 第一个控制台沿用原来的光标位置 */
		// 有点费解。
		// 第一个控制台也可以不沿用原来的光标位置。
		// 然而，在第一个控制台中，写入字符的初始位置必定是0。
		p_tty->p_console->cursor = disp_pos / 2;
		disp_pos = 0;
	}
	else {
		// 不是第一个控制台，打印控制台编号加#。
		// 例如，第2个控制台，打印'2#'
		out_char(p_tty->p_console, nr_tty + '0');
		out_char(p_tty->p_console, '#');
	}
	// 设置光标
	set_cursor(p_tty->p_console->cursor);
}


/*======================================================================*
			   is_current_console
*======================================================================*/
PUBLIC int is_current_console(CONSOLE* p_con)
{
	// 比较的是内存地址
	return (p_con == &console_table[nr_current_console]);
}


/*======================================================================*
			   out_char
 *======================================================================*/
PUBLIC void out_char(CONSOLE* p_con, char ch)
{
	// 光标位置和当前写入字符的显存地址：后者 = 前者 * 2。
	// 光标的内存地址是1，那么，写入下一个字符的内存地址是：1*2。
	// 为什么？无能力说得太清楚。可这个事实对我来说，已经是不需要思考、不需要问为什么的常识了。
	u8* p_vmem = (u8*)(V_MEM_BASE + p_con->cursor * 2);

	switch(ch) {
	// 理解起来最费劲。
	// 如果光标位置不在控制台的最后一行，才换行。
	// 理解换行，花了很多时间。不应该。这不需要领域知识，只需要基本推理。
	// 为啥理解慢？因为，我的方法不对。只需用具体数值代入计算一次，马上能知道这种计算方法没问题。
	// 再举一组例子吧。
	// 1. cursor在一行的中间。
	// 2. cursor在一行的末尾。末尾-初始地址 = SCREEN_WIDTH - 1。认清这一点，耗费时间最多。
	// cursor在第一行，(cursor - original_addr) / SCREEN_WIDTH = 0。
	// 这么理解：
	// 1. 行号 = (cursor - original_addr) / SCREEN_WIDTH + 1。
	// 2. 如果光标在第N行，那么，输入换行符后，cursor的位置应该是 行号 * SCREEN_WIDTH + original_addr。·
	// 3. 加original_addr + SCREEN_WIDTH等于下一行的初始位置。
	// 4. 加original_addr-1 + SCREEN_WIDTH等于这一行的最后那个位置。
	// 一边听歌一边理解这样的常识，投入产出不成正比。
	case '\n':
		if (p_con->cursor < p_con->original_addr +
		    p_con->v_mem_limit - SCREEN_WIDTH) {
			p_con->cursor = p_con->original_addr + SCREEN_WIDTH * 
				((p_con->cursor - p_con->original_addr) /
				 SCREEN_WIDTH + 1);
		}
		break;
	// 退格。简单。光标后退，把当前位置的字符用空字符代替。
	case '\b':
		if (p_con->cursor > p_con->original_addr) {
			p_con->cursor--;
			*(p_vmem-2) = ' ';
			*(p_vmem-1) = DEFAULT_CHAR_COLOR;
		}
		break;
	// 非空格、非换行。
	// 光标距离控制本TTY的终点至少还有1个字节，写入下一个字符的位置距离本TTY
	// 的终点至少还有2个字节。
	// 这个判断是不是有问题？cursor距离终点只有1个字节，p_vmem是不是早就超过了
	// 界限？
	default:
		if (p_con->cursor <
		    p_con->original_addr + p_con->v_mem_limit - 1) {
			*p_vmem++ = ch;
			*p_vmem++ = DEFAULT_CHAR_COLOR;
			p_con->cursor++;
		}
		// 与PHP的差异，即使是default，break也不能缺少。
		break;
	}

	// 如果光标地址大于一屏，滚屏，直到光标在一屏之内。
	while (p_con->cursor >= p_con->current_start_addr + SCREEN_SIZE) {
		scroll_screen(p_con, SCR_DN);
	}
	// 设置光标位置；设置出现在屏幕左上角的内存地址。
	// 在这里，我忽然疑惑了很久，莫名其妙。
	flush(p_con);
}

/*======================================================================*
                           flush
*======================================================================*/
PRIVATE void flush(CONSOLE* p_con)
{
	if (is_current_console(p_con)) {
		set_cursor(p_con->cursor);
		set_video_start_addr(p_con->current_start_addr);
	}
}

/*======================================================================*
			    set_cursor
 *======================================================================*/
PRIVATE void set_cursor(unsigned int position)
{
	disable_int();
	out_byte(CRTC_ADDR_REG, CURSOR_H);
	out_byte(CRTC_DATA_REG, (position >> 8) & 0xFF);
	out_byte(CRTC_ADDR_REG, CURSOR_L);
	out_byte(CRTC_DATA_REG, position & 0xFF);
	enable_int();
}

/*======================================================================*
			  set_video_start_addr
 *======================================================================*/
PRIVATE void set_video_start_addr(u32 addr)
{
	disable_int();
	out_byte(CRTC_ADDR_REG, START_ADDR_H);
	out_byte(CRTC_DATA_REG, (addr >> 8) & 0xFF);
	out_byte(CRTC_ADDR_REG, START_ADDR_L);
	out_byte(CRTC_DATA_REG, addr & 0xFF);
	enable_int();
}



/*======================================================================*
			   select_console
 *======================================================================*/
PUBLIC void select_console(int nr_console)	/* 0 ~ (NR_CONSOLES - 1) */
{
	if ((nr_console < 0) || (nr_console >= NR_CONSOLES)) {
		return;
	}

	nr_current_console = nr_console;

	flush(&console_table[nr_console]);
}

/*======================================================================*
			   scroll_screen
 *----------------------------------------------------------------------*
 滚屏.
 *----------------------------------------------------------------------*
 direction:
	SCR_UP	: 向上滚屏
	SCR_DN	: 向下滚屏
	其它	: 不做处理
 *======================================================================*/
PUBLIC void scroll_screen(CONSOLE* p_con, int direction)
{
	if (direction == SCR_UP) {
		// current_start_addr 是 SCREEN_WIDTH 的倍数。只要大于0，就能向上滚屏。
		if (p_con->current_start_addr > p_con->original_addr) {
			p_con->current_start_addr -= SCREEN_WIDTH;
		}
	}
	else if (direction == SCR_DN) {
		// 剩余的显存足够一屏，就能向下滚屏。
		if (p_con->current_start_addr + SCREEN_SIZE <
		    p_con->original_addr + p_con->v_mem_limit) {
			p_con->current_start_addr += SCREEN_WIDTH;
		}
	}
	else{
	}

	flush(p_con);
}

