#include "drivers/keyboard.h"
#include "drivers/mouse.h"
#include "drivers/tty.h"
#include "drivers/vga/vga.h"
#include "drivers/serial.h"
#include "drivers/devices/net/e1000.h"
#include "drivers/net/l1/arp.h"
#include "drivers/net/l2/udp.h"
#include "drivers/net/l2/tcp.h"
#include "drivers/net/l3/dhcp.h"
#include "drivers/net/l3/dns.h"
#include "drivers/net/l3/http.h"
#include "drivers/net/l3/time.h"
#include "drivers/net/l3/tls.h"
#include "lib/string.h"
#include "hw/cpu/cpuid.h"
#include "hw/cpu/interrupts.h"
#include "hw/cpu/isr.h"
#include "hw/cpu/idt.h"
#include "hw/cpu/irq.h"
#include "hw/cpu/pic.h"
#include "hw/cpu/gdt.h"
#include "hw/mem.h"
#include "hw/timer.h"
#include "hw/pci.h"
#include "multiboot.h"
#include "hw/cpu/system.h"
#include "user/shell.h"
#include "user/scheduler.h"
#include "user/status.h"
#include "user/gui/gui.h"

extern uint32_t start_text;
extern uint32_t end_data;
extern uint32_t end_kernel;
extern uint32_t end_safe;

void kernel_main(multiboot_info_t *mbd, uint32_t magic)
{
	if (magic != MULTIBOOT_BOOTLOADER_MAGIC)
		panic("invalid magic number!");

	if (!(mbd->flags >> 6 & 0x1))
		panic("invalid memory map given by GRUB bootloader");

	mem_segment_t biggest_mem_segment;
	biggest_mem_segment.len = 0;
	biggest_mem_segment.start = 0;

	int i;
	for (i = 0; i < mbd->mmap_length; i += sizeof(multiboot_memory_map_t))
	{
		multiboot_memory_map_t *mmmt = (multiboot_memory_map_t *)(mbd->mmap_addr + i);
		if (mmmt->type == MULTIBOOT_MEMORY_AVAILABLE)
		{
			if (mmmt->addr > (uintptr_t)&end_safe)
			{
				if (mmmt->len > biggest_mem_segment.len)
				{
					biggest_mem_segment.start = (uint64_t *)(uintptr_t)mmmt->addr;
					biggest_mem_segment.len = mmmt->len;
				}
			}
			else if (mmmt->addr + mmmt->len > (uintptr_t)&end_safe)
			{
				if ((mmmt->addr + mmmt->len) - (uintptr_t)&end_safe + 1 > biggest_mem_segment.len)
				{
					biggest_mem_segment.start = ((uint64_t *)((uintptr_t)&end_safe));
					biggest_mem_segment.len = (mmmt->addr + mmmt->len) - (uintptr_t)&end_safe;
				}
			}
		}
	}

	init_mem(&biggest_mem_segment);
	tty_init();
	tty_clear_screen(vga_get_tty());
	vga_get_tty()->tty_print(TTYColor_WHITE, "\n");

	// for (i = 0; i < mbd->mmap_length; i += sizeof(multiboot_memory_map_t))
	// {
	// 	multiboot_memory_map_t *mmmt = (multiboot_memory_map_t *)(mbd->mmap_addr + i);
	// 	printf("mmapseg %x %x %x\n", (uint32_t)(mmmt->addr), (uint32_t)(mmmt->addr) + (uint32_t)(mmmt->len), mmmt->type);
	// }

	printf("            _____      \n\
           |____ |     \n\
  ___  ___     / /     \n\
 / _ \\/ __|    \\ \\  \n\
| (_) \\__ \\____/ /   \n\
 \\___/|___/\\____/    \n");

	// printf("%s\n", (char *)mbd->cmdline);

	status_init();
	status_update();

	dprintf(0, "[MEM] using mem region %p - %p\n", biggest_mem_segment.start, biggest_mem_segment.start + biggest_mem_segment.len);

	get_cpu_info();
	gdt_init();
	idt_init();
	isr_init();
	irq_init();
	pic_remap(32, 40);
	io_wait();

	interrupts_enable();
	keyboard_init();
	mouse_init();
	timer_init();

	udp_init();
	arp_init();
	tcp_init();
	dns_init();

	pci_init();
	if (ethernet_first_netdev())
	{
		dhcp_init(ethernet_first_netdev());
		if (ethernet_first_netdev()->ip_c.ip)
		{
			// status_update_wan_ip();
			time_request(ethernet_first_netdev());
			// sendTLSHandshake(ethernet_first_netdev());
		}
	}

	// vga_switch_mode(VGA_GUI);
	// gui_init();

	serial_int_enable();
	shell_init();
	scheduler_init();

	while (1)
		scheduler_tick();
}