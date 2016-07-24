#include "t/test.pb-c.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

int main()
{
  Foo__Person person = __FOO__PERSON__INTERNAL_INITIALIZER__;
  Foo__Person *person2;
  unsigned char simple_pad[8];
  size_t size, size2;
  unsigned char *packed;
  ProtobufCBufferSimple bs = PROTOBUF_C_BUFFER_SIMPLE_INIT (simple_pad);

  person.name = "dave b";
  person.id = 42;
  size = foo__person__get_packed_size (&person);
  packed = malloc (size);
  assert (packed);
  size2 = foo__person__pack (&person, packed);
  assert (size == size2);
  foo__person__pack_to_buffer (&person, &bs.base);
  assert (bs.len == size);
  assert (memcmp (bs.data, packed, size) == 0);
  PROTOBUF_C_BUFFER_SIMPLE_CLEAR (&bs);
  person2 = foo__person__unpack (NULL, size, packed);
  assert (person2 != NULL);
  assert (person2->id == 42);
  assert (strcmp (person2->name, "dave b") == 0);

  foo__person__free_unpacked (NULL, person2);
  free (packed);

  printf ("test succeeded.\n");

  return 0;
}
