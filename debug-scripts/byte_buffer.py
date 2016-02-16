print("loaded " + __file__)

def matcher(value):
  if value.type.tag == "ByteBuffer":
    return ByteBufferPrinter(value)
  return None

class ByteBufferPrinter(object):
  def __init__(self, value):
    self.value = value
  def children(self):
    yield ("_buffer", self.value["_buffer"])
  def to_string(self):
    buffer_value = self.value["_buffer"]
    length_value = buffer_value["_length"]
    items_pointer = buffer_value["_items"]
    range_thing = range(int(length_value) - 1)
    s = "".join(chr((items_pointer + i).dereference()) for i in range_thing)
    return repr(s)

gdb.current_objfile().pretty_printers.append(matcher)
