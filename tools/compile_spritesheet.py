#!/usr/bin/env python3

import os
import sys
import math
import time
import struct

# make sure you call this script with the right PYTHONPATH
import simplepng

def cli():
  import argparse
  parser = argparse.ArgumentParser()
  parser.add_argument("DIR")
  parser.add_argument("--glob")
  parser.add_argument("--tilesize", type=int, required=True)
  parser.add_argument("--spritesheet", required=True)
  parser.add_argument("--header")
  parser.add_argument("--deps")

  args = parser.parse_args()
  source_dir = args.DIR
  glob_pattern = args.glob
  tilesize = args.tilesize
  spritesheet_path = args.spritesheet
  header_path = args.header
  deps_path = args.deps

  if glob_pattern != "*.png":
    parser.error("you must specify glob pattern '*.png'")

  main(source_dir, tilesize, spritesheet_path, header_path, deps_path)

def main(source_dir, tilesize, spritesheet_path, header_path, deps_path):
  dependencies = [source_dir]
  # find all the input files
  item_paths = []
  for (item_path, is_file) in find_files(source_dir):
    if not is_file:
      dependencies.append(os.path.join(source_dir, item_path))
      continue
    if not item_path.endswith(".png"): continue
    item_paths.append(item_path)
    dependencies.append(os.path.join(source_dir, item_path))
  item_paths.sort()

  # output should be the smallest power-of-2-sized square to fit everything
  sprites_per_row = 1 << math.ceil(math.log(math.sqrt(len(item_paths)), 2))
  spritesheet_image = simplepng.ImageBuffer(sprites_per_row * tilesize, sprites_per_row * tilesize)

  # composite spritesheet
  x = 0
  y = 0
  header_items = []
  for item_path in item_paths:
    with open(os.path.join(source_dir, item_path), "rb") as f:
      item_image = simplepng.read_png(f)
    if not (item_image.width == tilesize and item_image.height == tilesize):
      sys.exit("ERROR: {}: expected {}x{}. found {}x{}".format(item_path, tilesize, tilesize, item_image.width, item_image.height))
    spritesheet_image.paste(item_image, dx=x*tilesize, dy=y*tilesize)

    header_items.append((mangle_item_name(item_path), x * tilesize, y * tilesize))

    x += 1
    if x == sprites_per_row:
      x = 0
      y += 1

  # output spritesheet in RGBA8888 format
  with open(spritesheet_path + ".tmp", "wb") as f:
    f.write(b"".join(struct.pack("<I", pixel) for pixel in spritesheet_image.data))
  with open(spritesheet_path + "_reference.png", "wb") as f:
    simplepng.write_png(f, spritesheet_image)

  # check header
  header_item_format = "pub const %s = Rect{.x = %i, .y = %i, .width = 32, .height = 32};";
  header_contents = header_format.format(
    os.path.relpath(spritesheet_path, os.path.dirname(header_path)),
    sprites_per_row * tilesize,
    sprites_per_row * tilesize,
    "\n".join(
      header_item_format % header_item for header_item in header_items
    )
  )
  def maybe_update_header():
    try:
      with open(header_path, "r") as f:
        if f.read() == header_contents: return
    except IOError: pass
    with open(header_path + ".tmp", "w") as f:
      f.write(header_contents)
    os.rename(header_path + ".tmp", header_path)
  maybe_update_header()

  # output dependencies
  if deps_path != None:
    with open(deps_path, "w") as f:
      f.write(spritesheet_path + ": " + " ".join(dependencies) + "\n")
      f.write("".join("{}:\n".format(dependency) for dependency in dependencies))

  # now we're done
  os.rename(spritesheet_path + ".tmp", spritesheet_path)

header_format = """\
pub const buffer = @embedFile("./{}");

const Rect = @import("../src/geometry.zig").Rect;

pub const width = {};
pub const height = {};

{}
"""

def mangle_item_name(item_path):
  name = item_path[:-len(".png")]
  name = os.path.basename(name)
  return name

def find_files(root):
  for name in os.listdir(root):
    path = os.path.join(root, name)
    if os.path.isdir(path):
      yield (name, False)
      for (child, is_file) in find_files(path):
        yield (os.path.join(name, child), is_file)
    else:
      yield (name, True)

if __name__ == "__main__":
  cli()
