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
  tiling_group = parser.add_mutually_exclusive_group(required=True)
  tiling_group.add_argument("--tile-size", type=int)
  tiling_group.add_argument("--slice-tiles", metavar="WxH")
  parser.add_argument("--spritesheet-path", required=True)
  parser.add_argument("--defs-path", required=True)
  parser.add_argument("--deps")

  args = parser.parse_args()
  source_dir = args.DIR
  glob_pattern = args.glob
  tile_size = args.tile_size
  if args.slice_tiles != None:
    import re
    match = re.match(r"^(\d+)x(\d+)$", args.slice_tiles)
    if match == None:
      parser.error("--slice-tiles wrong format: " + repr(args.slice_tiles))
    slice_dimensions = tuple(int(x) for x in match.groups())
  else:
    slice_dimensions = None
  spritesheet_path = args.spritesheet_path
  defs_path = args.defs_path
  deps_path = args.deps

  if glob_pattern != "*.png":
    parser.error("you must specify glob pattern '*.png'")

  main(source_dir, tile_size, slice_dimensions, spritesheet_path, defs_path, deps_path)

def main(source_dir, tile_size, slice_dimensions, spritesheet_path, defs_path, deps_path):
  item_dimensions = slice_dimensions or (tile_size, tile_size)

  dependencies = [source_dir]
  # find all the input files
  input_paths = []
  for (input_path, is_file) in find_files(source_dir):
    if not is_file:
      dependencies.append(os.path.join(source_dir, input_path))
      continue
    if not input_path.endswith(".png"): continue
    input_paths.append(input_path)
    dependencies.append(os.path.join(source_dir, input_path))
  input_paths.sort()

  input_images = []
  item_count = 0
  for input_path in input_paths:
    with open(os.path.join(source_dir, input_path), "rb") as f:
      input_image = simplepng.read_png(f)
    if tile_size != None:
      if (input_image.width, input_image.height) != item_dimensions:
        sys.exit("ERROR: {}: expected {}x{}. found {}x{}".format(
          input_path, tile_size, tile_size, input_image.width, input_image.height
        ))
      item_count += 1
    else:
      if (input_image.width % item_dimensions[0], input_image.height % item_dimensions[1]) != (0, 0):
        sys.exit("ERROR: {}: expected multiple of {}x{}. found {}x{}".format(
          input_path, item_dimensions[0], item_dimensions[1], input_image.width, input_image.height
        ))
      item_count += (input_image.width // item_dimensions[0]) * (input_image.height // item_dimensions[1])
    input_images.append(input_image)

  # output should be the smallest power-of-2-sized square to fit everything
  output_size = 1
  while True:
    sprites_per_row = output_size // item_dimensions[0]
    sprites_per_col = output_size // item_dimensions[1]
    if sprites_per_row * sprites_per_col >= item_count: break
    output_size <<= 1
  spritesheet_image = simplepng.ImageBuffer(output_size, output_size)

  # composite spritesheet
  dx, dy = 0, 0
  defs_items = []
  for input_path, input_image in zip(input_paths, input_images):
    sx, sy = 0, 0
    coordinates = []
    while True:
      spritesheet_image.paste(input_image, sx=sx, sy=sy, dx=dx, dy=dy, width=item_dimensions[0], height=item_dimensions[1])

      coordinates.append((dx, dy))

      dx += item_dimensions[0]
      if dx > output_size - item_dimensions[0]:
        dx = 0
        dy += item_dimensions[1]

      sx += item_dimensions[0]
      if sx >= input_image.width:
        sx = 0
        sy += item_dimensions[1]

      if sy >= input_image.height:
        # next input image
        break
    defs_items.append((mangle_item_name(input_path), coordinates))

  # output spritesheet in RGBA8888 format
  with open(spritesheet_path + ".tmp", "wb") as f:
    f.write(b"".join(struct.pack("<I", pixel) for pixel in spritesheet_image.data))
  with open(spritesheet_path + "_reference.png", "wb") as f:
    simplepng.write_png(f, spritesheet_image)

  # check defs
  if tile_size != None:
    defs_item_format = "pub const %s = Rect.{.x = %i, .y = %i, .width = %i, .height = %i};";
    defs_contents = defs_format.format(
      os.path.relpath(spritesheet_path, os.path.dirname(defs_path)),
      spritesheet_image.width,
      spritesheet_image.height,
      "\n".join(
        defs_item_format % (defs_item[0], defs_item[1][0][0], defs_item[1][0][1], item_dimensions[0], item_dimensions[1])
        for defs_item in defs_items
      )
    )
  else:
    defs_item_format       = "pub const %s = []Rect.{\n%s};";
    defs_item_child_format = "    Rect.{.x = %i, .y = %i, .width = %i, .height = %i},\n";
    defs_contents = defs_format.format(
      os.path.relpath(spritesheet_path, os.path.dirname(defs_path)),
      spritesheet_image.width,
      spritesheet_image.height,
      "\n".join(
        defs_item_format % (defs_item[0],
          "".join(
            defs_item_child_format % (child[0], child[1], item_dimensions[0], item_dimensions[1])
            for child in defs_item[1]
          )
        ) for defs_item in defs_items
      )
    )

  def maybe_update_defs():
    try:
      with open(defs_path, "r") as f:
        if f.read() == defs_contents: return
    except IOError: pass
    with open(defs_path + ".tmp", "w") as f:
      f.write(defs_contents)
    os.rename(defs_path + ".tmp", defs_path)
  maybe_update_defs()

  # output dependencies
  if deps_path != None:
    with open(deps_path, "w") as f:
      f.write(spritesheet_path + ": " + " ".join(dependencies) + "\n")
      f.write("".join("{}:\n".format(dependency) for dependency in dependencies))

  # now we're done
  os.rename(spritesheet_path + ".tmp", spritesheet_path)

defs_format = """\
const Rect = @import("core").geometry.Rect;

pub const buffer = @embedFile("./{}");

pub const width = {};
pub const height = {};

{}
"""

def mangle_item_name(input_path):
  name = input_path[:-len(".png")]
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
