print("hello world from asdffdsa")

import re

list_regex = re.compile("List<.*>")

def value_matcher(value):
  tag = value.type.tag
  if tag == None: return None
  if list_regex.match(value.type.tag): return ListPrinter(value)
  return None

class ListPrinter(object):
  def __init__(self, value):
    self.value = value
  def children(self):
    length_value = self.value["_length"]
    i = 0
    while i < length_value:
      item = (self.value["_items"] + i).dereference()
      yield (str(i), item)
      i += 1
  def display_hint(self):
    return "array"
  def to_string(self):
    return "List[" + str(self.value["_length"]) + "/" + str(self.value["_capacity"]) + "]"


gdb.current_objfile().pretty_printers.append(value_matcher)
