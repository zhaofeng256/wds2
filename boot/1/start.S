#define WTCON   0x53000000
#define MPLLCON 0x4C000004
#define CLKDIVN 0x4C000014
#define BWSCON  0x48000000

#define MPLL_400MHZ     ((0x5c<<12)|(0x01<<4)|(0x01))
#define MPLL_200MHZ     ((0x5c<<12)|(0x01<<4)|(0x02))

#define CLKDIV124       0x3
#define CLKDIV148       0x5
#define CLKDIV188       0x4

.text
.global _start
_start:
	#disable watchdog
	ldr r0, =0x0
	ldr r1, =WTCON
	str r0, [r1]


	##fclk:hclk:pclk = 1:4:8
	ldr r0, =CLKDIV148
	ldr r1, =CLKDIVN
	str r0, [r1]

	#If HDIVN is not 0, the CPU bus mode has to be changed 
	#from the fast bus mode to the asynchronousbus mode
	mrc p15,0,r0,c1,c0,0
	orr r0,r0,#0xc0000000
	mcr p15,0,r0,c1,c0,0

	##set pll
	ldr r0, =MPLL_400MHZ
	ldr r1, =MPLLCON
	str r0, [r1]

	#enable instruction cache
	mrc p15,0,r0,c1,c0,0
	orr r0,r0,#(0x1<<12)
	mcr p15,0,r0,c1,c0,0

	#init sdram
	ldr r0, =BWSCON 
	add r1, r0, #13*4
	adr r2, _sdram_cfgs

set_sdram:
	ldr r3, [r2], #4
	str r3, [r0], #4
	cmp r0, r1
	bne set_sdram


	ldr sp, =0x34000000

	bl uart0_init
	#init nand flash
	bl nand_init
	#reloacate code

	ldr r0, =0
	ldr r1, =_start
	ldr r2, =__bss_start
	sub r2, r2, r1
	bl copy_code_to_sdram
	bl clear_bss
	
	ldr lr, =halt
	ldr pc, =main


halt:
	b halt


_sdram_cfgs:
	.long 0x02010000	 //BWSCON
	.long 0x00000700	 //BANKCON0
	.long 0x00000700	 //BANKCON1
	.long 0x00000700	 //BANKCON2
	.long 0x00000700	 //BANKCON3  
	.long 0x00000700	 //BANKCON4
	.long 0x00000700	 //BANKCON5
	.long 0x00018005	 //BANKCON6
	.long 0x00018005	 //BANKCON7
	.long 0x008C04F4	 //REFRESH
	.long 0x000000B1	 //BANKSIZE
	.long 0x00000030	 //MRSRB6
	.long 0x00000000	 //MRSRB7
