
__attribute__((__naked__,__used__))
int _start() {
    asm volatile("mov $60, %%al\npush $42\npop %%rdi\nsyscall\n":::);
}

