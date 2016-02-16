print("loaded " + __file__)

import re

uint_oversized_regex = re.compile("uint_oversized<(\d+)>")

def matcher(value):
  type = value.type.strip_typedefs()
  if type.tag == None: return None
  match = uint_oversized_regex.match(type.tag)
  if match: return UintOversizedPrinter(value, int(match.group(1)))
  return None

class UintOversizedPrinter(object):
  def __init__(self, value, size64):
    self.value = value
    self.size64 = size64
  def to_string(self):
    result = ""
    for i in range(self.size64):
      n = int(self.value["values"][i]) & 0xffffffffffffffff
      result += hex(n)[2:].zfill(16)
    return result

gdb.current_objfile().pretty_printers.append(matcher)
