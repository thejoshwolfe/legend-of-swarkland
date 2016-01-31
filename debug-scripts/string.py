print("loaded " + __file__)

def string_matcher(value):
  if value.type.code == gdb.TYPE_CODE_PTR:
    type = value.type.target().strip_typedefs()
    if type.tag == "StringImpl":
      return StringPrinter(value.dereference())
  if value.type.strip_typedefs().tag == "Reference<StringImpl>":
    return StringPrinter(value["_target"].dereference())
  return None

class StringPrinter(object):
  def __init__(self, value):
    self.value = value
  def display_hint(self):
    return "string"
  def to_string(self):
    chars_list = self.value["_chars"]
    length = int(chars_list["_length"])
    chars_pointer = chars_list["_items"]
    # TODO: we should be using unichr() instead of chr(), but I get a NameError 'unichr' is not defined.
    return u"".join(chr(int((chars_pointer + x).dereference())) for x in range(length))

gdb.current_objfile().pretty_printers.append(string_matcher)
