#include "../myftp.h"

#define IP_ADDRESS "127.0.0.1"
#define PORT 12345
#define U_INPUTLEN 1024

//User command structure
struct command
{
};

struct command *getUserCommand(char *);
