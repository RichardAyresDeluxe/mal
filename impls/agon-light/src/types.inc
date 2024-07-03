struct MalVal
  .next   rb 3
  .flags  rb 1
  .data   rb 3
ends
MalVal.list := MalVal.data
MalVal.number := MalVal.data
MalVal.string := MalVal.data

MASK_TYPE   EQU 0x3F
MASK_FLAGS  EQU 0xC0

FLAG_MARK   EQU 0x80

TYPE_NUMBER EQU 0x11
TYPE_LIST   EQU 0x21
TYPE_STRING EQU 0x31
TYPE_SYMBOL EQU 0x32

struct MalList
  .next       rb 3
  .ref_count  rb 3
  .value      rb 3
ends
