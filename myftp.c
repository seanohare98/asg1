#include "myftp.h"

// network protocol to host conversion
struct message_s *ntohp(struct message_s *packet)
{
  struct message_s *converted =
      (struct message_s *)malloc(sizeof(struct message_s));
  converted->type = ntohs(packet->type);
  converted->length = ntohs(packet->length);
  return converted;
}