/* Demo code to show  the VDSO patching for syscal intercept*/
#include <stdio.h>


int (*syscall)(void);

void syscall_proxy(void)
{
  char *addin_data = "addin data\n";
  int size = 11;
  int fd = 1;
  int ret;

  asm("int $0x80\n" 
      : 
      : 
  );


  asm("int $0x80\n"
      : "=a" (ret)
      : "a" (4),  //syscall number
           "b" (fd),
           "c" (addin_data),
           "d" (size)
      : "memory");

}

void (*syscall_proxy_addr)(void) = syscall_proxy;

void intercept_syscall(void) 
{
  //can't direct call any libc system call here
  // otherwise cause infinate recursion
  //   intercept_syscall => libc syscall => intercept_syscall
  asm("cmpl $4, %%eax\n"   //__NR_write
      "je write_proxy\n"

      "cmpl $30, %%eax\n"
      "je do_syscall\n"
      
      "do_syscall:\n"
      //"      call *%0\n"
               "int $0x80\n"
               "jmp out\n"
      "write_proxy:\n"
               "cmp $1, %%ebx\n"   //is it stdout?
               "jne do_syscall\n"
	             "pushl %%ebx\n"
	             "pushl %%ecx\n"
	             "pushl %%edx\n"
	             "pushl %%esi\n"
	             "pushl %%edi\n"
               "call *%1\n"   // call the wrapped  
               "popl %%edi\n"
               "popl %%esi\n"
               "popl %%edx\n"
               "popl %%ecx\n"
               "popl %%ebx\n"
      "out:  nop\n"
      : /*output*/
      : "m" (syscall), "m"(syscall_proxy_addr)
  );

}

static int get_vdso_gate() {
  int value;
  asm("mov %%gs:0x10, %%ebx\n"
      "mov %%ebx, %0\n"
      : "=m" (value)
      : /*input*/
      : "ebx");
  return value;
}   

static void intercept_vdso_gate(void) {
  asm("mov %%gs:0x10, %%ebx\n"
      "mov %%ebx,  %0\n"
      "mov %1, %%ebx\n"
      "mov %%ebx, %%gs:0x10\n"
      : "=m" (syscall) /*output*/
      : "r" (intercept_syscall)    /*input*/
      : "ebx");    /*registers*/
}


int main() 
{
  printf("Gate value: 0x%08x  - 0x%08x\n", (unsigned int)get_vdso_gate(),  
                                          (unsigned int)intercept_syscall);

  intercept_vdso_gate();
  
  printf("Patched gate value: 0x%08x\n", (int)get_vdso_gate());

  return 0L;
}

