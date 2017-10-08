
#define MAX_DATA_LENGTH 16
struct comm_struct{
  uint8_t id;
  uint8_t op;
  uint8_t dt[MAX_DATA_LENGTH];
};

#define REQUEST 0x00
#define RESPONSE 0x80
#define OP_TYPE 0x80
#define OP_CODE 0x7F

#define OP_SAMPLE (0x00)
#define OP_GETNTP (0x01)
#define OP_IPADDR (0x02)

#define OP_HISTST (0x10)
#define OP_HISTCO (0x11)
#define OP_HISTFI (0x12)

