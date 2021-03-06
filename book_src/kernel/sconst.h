!这段代码是结合汇编代码为访问进程表中的数据加速
! Miscellaneous constants used in assembler code.
W		=	_WORD_SIZE	! Machine word size.

! Offsets in struct proc. They MUST match proc.h.
P_STACKBASE	=	0
GSREG		=	P_STACKBASE
FSREG		=	GSREG + 2	! 386 introduces FS and GS segments
ESREG		=	FSREG + 2
DSREG		=	ESREG + 2
DIREG		=	DSREG + 2
SIREG		=	DIREG + W
BPREG		=	SIREG + W
STREG		=	BPREG + W	! hole for another SP
BXREG		=	STREG + W
DXREG		=	BXREG + W
CXREG		=	DXREG + W
AXREG		=	CXREG + W
RETADR		=	AXREG + W	! return address for save() call
PCREG		=	RETADR + W
CSREG		=	PCREG + W
PSWREG		=	CSREG + W
SPREG		=	PSWREG + W
SSREG		=	SPREG + W
P_STACKTOP	=	SSREG + W
P_LDT_SEL	=	P_STACKTOP
P_LDT		=	P_LDT_SEL + W

Msize		=	9		! size of a message in 32-bit words
 
