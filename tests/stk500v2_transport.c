// Include the real backend so static command/transport functions are tested.
// Only the IO providers are simulated; no production test hooks are needed.
#include "../src/stk500v2.c"
#include <assert.h>

libavrdude_context *cx;
char *progname = "stk500v2-transport-test";
const char *pgmid = "test", *partdesc = "test";
int ovsigck;
int verbose, quell_progress;
int avrdude_message2(FILE *fp, int line, const char *file, const char *func,
  int mode, int level, const char *format, ...) {
  (void)fp; (void)line; (void)file; (void)func; (void)mode; (void)level; (void)format;
  return 0;
}

static unsigned char reply[32], wire[40];
static size_t reply_length, wire_length, position;
static unsigned sends;
static int failed_receive;
static struct pdata *test_data;

static int fake_send(const union filedescriptor *fd, const unsigned char *bytes, size_t n) {
  (void)fd; (void)bytes; (void)n;
  sends++;
  if(test_data->pgmtype == PGMTYPE_STK500) {
    wire[0]=MESSAGE_START; wire[1]=test_data->command_sequence;
    wire[2]=0; wire[3]=(unsigned char)reply_length; wire[4]=TOKEN;
    memcpy(wire+5,reply,reply_length);
    wire[5+reply_length]=0;
    for(size_t i=0;i<5+reply_length;i++) wire[5+reply_length]^=wire[i];
    wire_length=reply_length+6;
  } else {
    memcpy(wire,reply,reply_length);
    wire_length=reply_length;
  }
  position=0;
  return 0;
}
static int fake_receive(const union filedescriptor *fd, unsigned char *bytes, size_t n) {
  (void)fd;
  if(failed_receive) return -1;
  if(test_data->pgmtype == PGMTYPE_STK500) {
    if(position+n>wire_length) return -1;
    memcpy(bytes,wire+position,n); position+=n;
    return 0; // serial provider returns zero on an exact receive
  }
  size_t available=wire_length-position;
  if(available>n) available=n;
  memcpy(bytes,wire+position,available); position+=available;
  return (int)available; // USB providers return the received byte count
}
static struct serial_device fake_device = {.send=fake_send,.recv=fake_receive};

int __wrap_jtagmkII_send(const PROGRAMMER *pgm, unsigned char *bytes, size_t n) {
  (void)pgm; (void)bytes; (void)n; sends++; return 0;
}
int __wrap_jtag3_send(const PROGRAMMER *pgm, unsigned char *bytes, size_t n) {
  return __wrap_jtagmkII_send(pgm,bytes,n);
}
static int jtag_reply(unsigned char **bytes, unsigned char envelope) {
  if(failed_receive) return -1;
  *bytes=mmt_malloc(reply_length+1);
  (*bytes)[0]=envelope;
  memcpy(*bytes+1,reply,reply_length);
  return (int)reply_length+1;
}
int __wrap_jtagmkII_recv(const PROGRAMMER *pgm, unsigned char **bytes) {
  (void)pgm; return jtag_reply(bytes,RSP_SPI_DATA);
}
int __wrap_jtag3_recv(const PROGRAMMER *pgm, unsigned char **bytes) {
  (void)pgm; return jtag_reply(bytes,SCOPE_AVR_ISP);
}

static void exercise(PROGRAMMER *pgm, const unsigned char *answer, size_t length, int ok) {
  unsigned char command[16]={CMD_XPROG,XPRG_CMD_READ_MEM};
  memcpy(reply,answer,length); reply_length=length; sends=0; position=0;
  int result=stk500v2_command(pgm,command,9,4);
  assert((result==0)==ok);
  assert(sends==1);
  if(ok) assert(command[3]==0xA5);
}

int main(void) {
  cx=calloc(1,sizeof *cx);
  assert(cx);
  serdev=&fake_device;
  PROGRAMMER *pgm=pgm_new();
  stk500v2_setup(pgm);
  test_data=pgm->cookie;
  const int transports[]={PGMTYPE_STK500,PGMTYPE_STK600,PGMTYPE_AVRISP_MKII,PGMTYPE_JTAGICE3};
  for(size_t t=0;t<sizeof transports/sizeof *transports;t++) {
    test_data->pgmtype=transports[t];
    exercise(pgm,(unsigned char[]){CMD_XPROG,XPRG_CMD_READ_MEM,0,0xA5},4,1);
    exercise(pgm,(unsigned char[]){CMD_XPROG,XPRG_CMD_READ_MEM,0},3,0);
    exercise(pgm,(unsigned char[]){CMD_XPROG,XPRG_CMD_READ_MEM},2,0);
    exercise(pgm,(unsigned char[]){CMD_XPROG,XPRG_CMD_WRITE_MEM,0,0xA5},4,0);
    exercise(pgm,(unsigned char[]){CMD_XPROG_SETMODE,0},2,0);
    for(unsigned status=1;status<=3;status++)
      exercise(pgm,(unsigned char[]){CMD_XPROG,XPRG_CMD_READ_MEM,(unsigned char)status},3,0);
    failed_receive=1;
    for(unsigned sub=XPRG_CMD_ERASE;sub<=XPRG_CMD_WRITE_MEM;sub++) {
      unsigned char command[16]={CMD_XPROG,(unsigned char)sub};
      sends=0;
      assert(stk500v2_command(pgm,command,9,3)<0 && sends==1);
    }
    failed_receive=0;
    printf("PASS transport %d: lengths, opcode/subcommand, FAILED/COLLISION/TIMEOUT, no erase/write replay\n",transports[t]);
  }
  for(int t=0;t<2;t++) {
    test_data->pgmtype=t ? PGMTYPE_JTAGICE3 : PGMTYPE_JTAGICE_MKII;
    unsigned char buffer[4];
    const unsigned char expected[]={CMD_XPROG,XPRG_CMD_READ_MEM,0,0xA5};
    memcpy(reply,expected,4); reply_length=4;
    assert(stk500v2_recv(pgm,buffer,4)==5 && memcmp(buffer,expected,4)==0);
    memset(reply+4,0xFF,4); reply_length=8; memset(buffer,0,4);
    assert(stk500v2_recv(pgm,buffer,4)==5 && memcmp(buffer,expected,4)==0);
    printf("PASS JTAG wrapper %d: exact and padded reply copy all four requested bytes\n",test_data->pgmtype);
  }
  return 0;
}
