#include <stdio.h>
#include <stdint.h>
#include <signal.h>
typedef uint16_t u16;
/*input buffering/ code coppied from: https://www.jmeiners.com/lc3-vm */
#include <Windows.h>
#include <conio.h>
HANDLE hStdin=INVALID_HANDLE_VALUE;DWORD fdwMode,fdwOldMode;
void disable_input_buffering(){hStdin=GetStdHandle(STD_INPUT_HANDLE);
 GetConsoleMode(hStdin,&fdwOldMode);
 fdwMode=fdwOldMode^ENABLE_ECHO_INPUT^ENABLE_LINE_INPUT;
 SetConsoleMode(hStdin, fdwMode);
 FlushConsoleInputBuffer(hStdin);}
void restore_input_buffering(){SetConsoleMode(hStdin,fdwOldMode);}
u16 check_key(){return WaitForSingleObject(hStdin, 1000)==WAIT_OBJECT_0&&_kbhit();}
void handle_interrupt(int signal){restore_input_buffering();printf("\n");exit(-2);}
enum {kbsr=0xfe00,kbdr=0xfe02};
/*sign extension with number n and byte b*/
u16 sext(u16 n,int b){return((n>>(b-1))&1)?(n|(0xFFFF<<b)):n;}
#define fimm(i) (((i)>>5)&1)
#define dr(i) (((i)>>9)&0x7)
#define sr1(i) (((i)>>6)&0x7)
#define sr2(i) ((i)&0x7)
#define imm(i) ((i)&0x1F)
#define simm(i) sext(imm(i),5)
#define p11(i) sext((i)&0x7FF,11)
#define p9(i) sext((i)&0x1FF,9)
#define p6(i) sext((i)&0x3F,6)
/*running(boolean)*/int rn=1;
u16 orig=0;u16 s=0x3000;u16 m[UINT16_MAX]={0};
u16 mr(u16 a){if(a==kbsr){if(check_key()){m[kbsr]=(1<<15);
 m[kbdr]=getchar();}else{m[kbsr]=0;}}return m[a];}
void mw(u16 a,u16 v){m[a]=v;}
enum reg{r0=0,r1,r2,r3,r4,r5,r6,r7,rpc,rc,rs};
u16 r[rs]={0};
enum f{fp=1<<0,fz=1<<1,fn=1<<2};
void uf(enum reg rg){
 if(r[rg]==0)r[rc]=fz;
 else if(r[rg]>>15)r[rc]=fn;
 else r[rc]=fp;}
// trap things
#define trp(i) ((i)&0xFF)
/*0x20*/void tgetc(){r[r0]=(u16)getchar();uf(r[r0]);}
/*0x21*/void toutc(){putc((char)r[r0],stdout);fflush(stdout);}
/*0x22*/void tputs(){u16*c=m+r[r0];while(*c){putc((char)*c,stdout);c++;}fflush(stdout);}
/*0x23*/void tin(){printf("Enter a character: ");char c=getchar();
        putc(c,stdout);fflush(stdout);r[r0]=(u16)c;uf(r[r0]);}
/*0x24*/void tputps(){u16*p=m+r[r0];while(*p){fprintf(stdout,"%c",(char)((*p)&0xFF));
        fprintf(stdout,"%c",(char)((*p)>>8));p++;}fflush(stdout);}
/*0x25*/void thalt(){puts("HALT");fflush(stdout);rn=0;}
enum {to=0x20};
void (*trx[6])()={tgetc,toutc,tputs,tin,tputps,thalt};

// opcodes
/*0000*/void br(u16 i){if(r[rc]&dr(i))r[rpc]+=p9(i);}
/*0001*/void add(u16 i){r[dr(i)]=r[sr1(i)]+(fimm(i)?simm(i):r[sr2(i)]);uf(dr(i));}
/*0010*/void ld(u16 i){r[dr(i)]=mr(r[rpc]+p9(i));uf(dr(i));}
/*0011*/void st(u16 i){mw(r[rpc]+p9(i),r[dr(i)]);}
/*0100*/void jsr(u16 i){r[r7]=r[rpc];r[rpc]=((i>>11)&1)?/*jsrr*/r[rpc]+p11(i):/*jsr*/r[sr1(i)];}
/*0101*/void and(u16 i){r[dr(i)]=r[sr1(i)]&(fimm(i)?simm(i):r[sr2(i)]);uf(dr(i));}
/*0110*/void ldr(u16 i){r[dr(i)]=mr(r[sr1(i)]+p6(i));uf(dr(i));}
/*0111*/void str(u16 i){mw(r[sr1(i)]+p6(i),r[dr(i)]);}
/*1000*/void rti(u16 i){printf("Bad opcode");exit(1);}
/*1001*/void not(u16 i){r[dr(i)]=~r[sr1(i)];uf(dr(i));}
/*1010*/void ldi(u16 i){r[dr(i)]=mr(mr(r[rpc]+p9(i)));uf(dr(i));}
/*1011*/void sti(u16 i){mw(mr(r[rpc]+p9(i)),r[dr(i)]);}
/*1100*/void jmp(u16 i){r[rpc]=r[sr1(i)];}
/*1101*/void res(u16 i){printf("Bad opcode");exit(1);}
/*1110*/void lea(u16 i){r[dr(i)]=r[rpc]+p9(i);uf(dr(i));}
/*1111*/void trap(u16 i){trx[trp(i)-to]();}
//opcode table
void (*op[16])(u16 i)={br,add,ld,st,jsr,and,ldr,str,rti,not,ldi,sti,jmp,res,lea,trap};
// execute code
void exec(){r[rpc]=orig;while(rn){u16 i=mr(r[rpc]++);op[i>>12](i);}}
// swapping (big endiann and little endian)
u16 swap(u16 x){return(x<<8)|(x>>8);}
// reading the file
void ri(const char *f){FILE *fi;fopen_s(&fi,f,"rb");fread(&orig,sizeof(orig),1,fi);
 orig=swap(orig);u16 mx=(UINT16_MAX)-orig;
 u16*p=m+orig;size_t r=fread(p,sizeof(u16),mx,fi);while(r-- >0){*p=swap(*p);++p;}fclose(fi);}

int main(int argc, char **argv) {
 signal(SIGINT,handle_interrupt);disable_input_buffering();
 if(argc<2){fprintf(stdout,"Usage: lc3 [image]\n");exit(1);}
 ri(argv[1]);exec();restore_input_buffering();return 0;}

