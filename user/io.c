#include <user/io.h>
#include <user/syscall.h>
#include <events.h>
#include <utils.h>
#include <priority.h>
#include <string.h>

#define NOTIFICATION 0
#define PUTCHAR      1
#define GETCHAR      2

typedef
struct IOReq {
    int type;
    int data; // (jason) cannot use char, volatile pointer may get stored
} IOReq;

// train io and monitor io
static int tioRecvSID = -1;
static int tioSendSID = -1;
static int mioRecvSID = -1;
static int mioSendSID = -1;

int Getc(int channel) {
    // Check channel type
    if (channel != COM1 && channel != COM2) return -1;

    // Prepare request
    IOReq req = {
        .type = GETCHAR
    };
    int server_tid = (channel == COM1) ? tioRecvSID : mioRecvSID;
    int ret = Send(server_tid, &req, sizeof(req), &(req.data), sizeof(req.data));
    return ret < 0 ? ret : req.data;
}

int Putc(int channel, char c) {
// Check channel type
    if (channel != COM1 && channel != COM2) return -1;
    // puts the character into sendServer's buffer
    // Prepare request
    IOReq req = {
        .type = PUTCHAR,
        .data = c
    };
    int server_tid = (channel == COM1) ? tioSendSID : mioSendSID;
    int ret = Send(server_tid, &req, sizeof(req), &(req.data), sizeof(req.data));
    return (ret < 0) ? -1 : 0;
}

int PutString(int channel, String *s) {
    // wrapper
    char *buf = sbuf(s);
    for(unsigned i = 0; i < slen(s); i++) {
        if(Putc(channel, buf[i])) {
            return -1;
        }
    }
    return slen(s);
}

int GetString(int channel, String *s) {
    // wrapper
    sinit(s);
    int ret = Getc(channel);
    sputc(s, ret);
}

static void receiveNotifier() {
    // messenger that waits for tio or mio to produce a character
    // and delivers to receiveServer
    int pid = MyParentTid();
    IOReq req = {
        .type = NOTIFICATION
    };

    for (;;) {
        // FIXME: change UART2_RECV_EVENT to also include UART1
        req.data = AwaitEvent(UART2_RECV_EVENT);
        Send(pid, &req, sizeof(req), 0, 0);
    }
}


static void receiveServer() {
    // server that waits for receiveNotifier to deliver a character
    int tid = 0;
    char taskb[128];
    char charb[1024];
    CBuffer taskBuffer;
    CBuffer charBuffer;
    CBufferInit(&taskBuffer, (void*)taskb, 128);
    CBufferInit(&charBuffer, (void*)charb, 1024);

    IOReq req = {
        .type = -1,
        .data = '\0'
    };

    // Spawn notifier
    Create(PRIORITY_RECV_NOTIFIER, &receiveNotifier);
    int ret;
    for (;;) {
        ret = Receive(&tid, &req, sizeof(req));
        if(ret != sizeof(req)) {
            // complain here
            continue;
        }
        switch (req.type) {
        case NOTIFICATION:
        // the notifier sent us a character
            Reply(tid, 0, 0);
            if(CBufferIsEmpty(&taskBuffer)) {
            // no tasks waiting, put char in a buffer
                CBufferPush(&charBuffer, req.data);
            }
            else {
            // there are tasks waiting for a char
            // unblock a task and hand it a char
                int popped_tid = CBufferPop(&taskBuffer);
                char ch = CBufferPop(&charBuffer);
                Reply(popped_tid, &ch, sizeof(ch));
            }
            break;

        case GETCHAR:
        // some user task requested get char
            if(CBufferIsEmpty(&charBuffer)) {
                CBufferPush(&taskBuffer, tid);
                // no reply to getC
            }
            else {
            // characters in the buffer, return it
                char ch = CBufferPop(&charBuffer);
                Reply(tid, &ch, sizeof(ch));
            }
            break;
        default:
            break;
        }
    }
}

static void sendNotifier() {
    int pid = MyParentTid();
    IOReq req = {
        .type = NOTIFICATION
    };

    for (;;) {
        // FIXME: change UART1_XMIT_EVENT to also include UART1
        req.data = AwaitEvent(UART2_XMIT_EVENT);
        Send(pid, &req, sizeof(req), 0, 0);
    }
}

static void sendServer() {
  // server that waits for sendNotifier to deliver a character
    int tid = 0;
    char charb[1024];
    CBuffer charBuffer;
    CBufferInit(&charBuffer, (void*)charb, 1024);

    IOReq req = {
        .type = -1,
        .data = '\0'
    };

    // Spawn notifier
    Create(PRIORITY_SEND_NOTIFIER, &sendNotifier);
    int ret;
    for (;;) {
        ret = Receive(&tid, &req, sizeof(req));
        if(ret != sizeof(req)) {
            // complain here
            continue;
        }
        switch (req.type) {
        case NOTIFICATION:
            if(CBufferIsEmpty(&charBuffer)) { // nothing to send
                // don't get stuck in the interrupt loop
                // turn interrupts off until we have a byte to send
                // TODO: What do?
            }
            else { // there is data to send
                Reply(tid, 0, 0);
                char ch = CBufferPop(&charBuffer);
            // write out char to volatile address
                *((unsigned int *)req.data) = ch;
            }
            break;

        case PUTCHAR:
            Reply(tid, 0, 0);
            CBufferPush(&charBuffer, req.data);
            break;
        default:
            break;
        }
    }
}

void initIO() {
    tioRecvSID = Create(PRIORITY_TIO_RECV_SERVER, receiveServer);
    tioSendSID = Create(PRIORITY_TIO_SEND_SERVER, sendServer);
    mioRecvSID = Create(PRIORITY_MIO_RECV_SERVER, receiveServer);
    mioSendSID = Create(PRIORITY_MIO_SEND_SERVER, sendServer);

}
