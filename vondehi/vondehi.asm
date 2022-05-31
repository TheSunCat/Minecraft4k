
; vim: set ft=nasm noet:

;%define USE_GZIP

%define STDIN_FILENO	0
%define STDOUT_FILENO	1
%define STDERR_FILENO	2

%define AT_EMPTY_PATH	0x1000
%define P_ALL			4

%define SYS_memfd_create	356
%define SYS_fork			2
%define SYS_waitpid			7
%define SYS_execve			11
%define SYS_open			5
%define SYS_lseek			19
%define SYS_dup2			63
%define SYS_execveat		358
%define SYS_vfork			190

%define EBP_bias			((_start-ehdr)-(__strempty-__self))

bits 32

org 0xEBDB0000

ehdr: ;~e_ident
	; jg short 0x47 (inc ebp) ; dec esp ; inc esi
	db 0x7F,"ELF"	;!E_MAGIC
_parent.0:
	xor ebx, ebx
	xchg eax, edi ;edi is zero now and will be overwritten eventually
	mov al, SYS_waitpid
	int 0x80
	lea esi, [esp+0x10]

	db 0x3D ; cmp eax, ...
	dw 2			;!e_type
	dw 3			;!e_machine
_parent.1:
	mov edx, esp
	lea ecx, [ebp+__strempty-__self+EBP_bias]
	add bl, bl
	db 0xEB ; jmp short _parent.2
	;dd _start		;!e_entry
	dd phdr-ehdr	;!e_phoff		; 0x33
_start:
	mov ax, SYS_memfd_create
	mov ebx, esp
	jmp short _start.1
	;dd 0			; e_shoff
	;dd 0			; e_flags
	dw ehdr.end-ehdr;!e_ehdrsize	; 0x34 0x00
	dw phdr.end-phdr;!e_phentsize	; 0x20 0x00

	dw 1			;!e_phnum
_start.1:
	int 0x80
	pop eax
	jmp short _start.2
	;dw 0			; e_shentsize
	;dw 0			; e_shnum
	;dw 0			; e_shstrndx
phdr:
	db 1			;!p_type
ehdr.end:
	times 3 db 0
	dd 0			;!p_offset
	dd ehdr			;!p_vaddr
_start.2:
	mov ebp, __self-EBP_bias
	;dd ehdr			; p_paddr
	jmp short _start.3
	db 0,0xEB
	;dd filesize		;~p_filesz
	jmp short _start.3+4
	db 0
	;dd filesize	;~p_memsz
	db 5			;~p_flags
_start.3:
%ifdef USE_VFORK
	mov al, SYS_vfork
%else
	mov al, SYS_fork
%endif
	jmp short _start.4

	;dd 5			;~p_flags
	;db 0			; p_align
_parent.2:
	mov bl, 3
phdr.endm1:
phdr.end equ phdr.endm1 + 1

;_parent.3:
	mov ax, SYS_execveat
	mov di, AT_EMPTY_PATH
	;int 0x80 ;; fallthru to _start.4

;_start:
;	mov ax, SYS_memfd_create
;	mov ebx, esp
;	int 0x80

;	pop eax
;	mov ebp, __self-EBP_bias
;	mov al, SYS_fork

_start.4:
	int 0x80

	test eax, eax
	jnz short _parent.0
	;jz short _child

;_parent:
;	xor ebx, ebx
;	mov ax, SYS_waitid
;	lea esi, [ebx+P_ALL] ; smaller than mov si, P_ALL
;	int 0x80

;	mov edx, esp ; use our args in args ; edx == argv
;	lea ecx, [ebp+__strempty-__self] ; doesn't like a NULL
;	mov bl, 3 ; __memfd
;	lea esi, [esp+24]
;	mov ax, SYS_execveat
;	mov di, AT_EMPTY_PATH ; can be smaller, reg sucks
;	int 0x80

_child:
	;mov cl, 0 ; fix argc
	;mov dl, 0

	lea ebx, [ebp+EBP_bias]
	;mov ebx, ebp
	mov al, SYS_open
	int 0x80

	; fd1
	push eax

	; seek
	mov al, SYS_lseek
	pop ebx
	push ebx
	mov cl, payload - ehdr
	int 0x80

	; dup2 demo->stdout
	dec ebx
	mov al, SYS_dup2
	mov cl, STDOUT_FILENO
	int 0x80

	; dup2 self->stdin
	mov al, SYS_dup2
	pop ebx
	dec ecx ; zero it out -> STDIN_FILENO
	int 0x80

	; execve
	mov al, SYS_execve

	lea ebx, [ebp+__zip-__self+EBP_bias]
%ifndef USE_GZIP
	push ecx
	push ebx
%endif
	mov ecx, esp
	int 0x80

__self:
	db '/proc/self/exe'
__strempty:
	db 0
__zip:
%ifdef USE_GZIP
	db '/bin/zcat',0
%else
%ifndef NO_UBUNTU_COMPAT
	db '/usr'
%endif
	db '/bin/xzcat',0
%endif

; if you insist
%ifdef TAG
	db TAG
%endif

END:

filesize equ END - ehdr

payload:

%if (_parent.2 - ehdr) != 0x50
%error "_parent.2: bad offset"
%endif

