print("loaded " + __file__)

field_values = []
def init():
  for field in gdb.lookup_type("VisionTypesBits").fields():
    name = field.name
    if name.startswith("VisionTypes_"):
      # this should always be the case
      name = name[len("VisionTypes_"):]
    field_values.append((field.enumval, name))
  field_values.sort()
init()

def matcher(value):
  if value.type.name == "VisionTypes":
    return VisionTypesPrinter(value)
  return None

class VisionTypesPrinter(object):
  def __init__(self, value):
    self.value = value
  def to_string(self):
    value = int(self.value)
    if value == 0:
      return "0"
    names = []
    for (bit, name) in field_values:
      if value & bit:
        value &= ~bit
        names.append(name)
    if value != 0:
      # this should never happen
      names.append(hex(value))
    return " | ".join(names)

gdb.current_objfile().pretty_printers.append(matcher)
