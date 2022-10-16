# this is python 3, not python 2

__all__ = ["read_png", "write_png", "ImageBuffer", "SimplePngError"]

import struct
import zlib
import sys
import time
import collections

# adapted from http://stackoverflow.com/a/25835368/367916

magic_number = b"\x89PNG\r\n\x1A\n"

IHDR_fmt = "!IIBBBBB"
color_type_mask_INDEXED = 1
color_type_mask_COLOR = 2
color_type_mask_ALPHA = 4

def I4(value):
  return struct.pack("!I", value)

def write_png(f, image):
  height = image.height
  width = image.width

  f.write(magic_number)

  # IHDR
  color_type = color_type_mask_COLOR | color_type_mask_ALPHA
  bit_depth = 8
  compression = 0
  filter_method = 0
  interlaced = 0
  IHDR = struct.pack(IHDR_fmt, width, height, bit_depth, color_type, compression, filter_method, interlaced)
  Chunk(b"IHDR", IHDR).write_to(f)

  # IDAT
  raw = []
  for y in range(height):
    raw.append(b"\x01") # filter type is difference from previous value
    previous_value = 0
    for x in range(width):
      value = image.data[y * width + x]
      raw.append(I4(subtract_bytes(value, previous_value)))
      previous_value = value
  raw = b"".join(raw)
  compressor = zlib.compressobj()
  compressed = compressor.compress(raw)
  compressed += compressor.flush()
  block = b"IDAT" + compressed
  f.write(I4(len(compressed)) + block + I4(zlib.crc32(block)))

  # IEND
  block = b"IEND"
  f.write(I4(0) + block + I4(zlib.crc32(block)))

class Chunk:
  def __init__(self, type_code, body):
    self.type_code = type_code
    self.body = body
  def write_to(self, f):
    block = self.type_code + self.body
    f.write(I4(len(self.body)) + block + I4(zlib.crc32(block)))
def read_chunk(f):
  [length] = struct.unpack("!I", f.read(4))
  type_code = f.read(4)
  body = f.read(length)
  crc32 = struct.unpack("!I", f.read(4))
  return Chunk(type_code, body)

class ImageBuffer:
  def __init__(self, width, height):
    self.width = width
    self.height = height
    # data is formatted 0xRRGGBBAA in row-major order
    self.data = [0] * (width * height)
  def set(self, x, y, value):
    self.data[y * self.width + x] = value
  def at(self, x, y):
    return self.data[y * self.width + x]
  def paste(self, other, sx=0, sy=0, dx=0, dy=0, width=None, height=None, flip_h=False, rotate=0):
    if width == None:
      width = min(self.width - dx, other.width - sx)
    if height == None:
      height = min(self.height - dy, other.height - sy)
    if flip_h or rotate != 0:
      other = other.copy(sx=sx, sy=sy, width=width, height=height)
      sx = 0
      sy = 0
      if flip_h: other.flip_h()
      if rotate != 0: other.rotate(rotate)
    for y in range(height):
      for x in range(width):
        value = other.at(sx + x, sy + y)
        alpha = value & 0xff
        if alpha == 0:
          continue
        if alpha < 255:
          value = alpha_blend(value, self.at(dx + x, dy + y))
        self.set(dx + x, dy + y, value)
  def copy(self, sx=0, sy=0, width=None, height=None):
    if width == None: width = self.width
    if height == None: height = self.height
    other = ImageBuffer(width, height)
    for y in range(height):
      for x in range(width):
        other.set(x, y, self.at(x + sx, y + sy))
    return other
  def flip_h(self):
    for y in range(self.height):
      for x in range(self.width // 2):
        x2 = self.width - 1 - x
        tmp = self.at(x, y)
        self.set(x, y, self.at(x2, y))
        self.set(x2, y, tmp)
  def rotate(self, quarter_turns):
    for y in range(self.height // 2):
      y2 = self.height - 1 - y
      for x in range(self.width // 2):
        x2 = self.width - 1 - x
        tmp = self.at(x, y)
        if quarter_turns == 1:
          self.set(x, y, self.at(y, x2))
          self.set(y, x2, self.at(x2, y2))
          self.set(x2, y2, self.at(y2, x))
          self.set(y2, x, tmp)
        elif quarter_turns == -1:
          self.set(x, y, self.at(y2, x))
          self.set(y2, x, self.at(x2, y2))
          self.set(x2, y2, self.at(y, x2))
          self.set(y, x2, tmp)

def add_bytes(a, b):
  return (
    ((a & 0xff000000) + (b & 0xff000000) & 0xff000000) |
    ((a & 0x00ff0000) + (b & 0x00ff0000) & 0x00ff0000) |
    ((a & 0x0000ff00) + (b & 0x0000ff00) & 0x0000ff00) |
    ((a & 0x000000ff) + (b & 0x000000ff) & 0x000000ff)
  )
def subtract_bytes(a, b):
  return (
    ((a & 0xff000000) - (b & 0xff000000) & 0xff000000) |
    ((a & 0x00ff0000) - (b & 0x00ff0000) & 0x00ff0000) |
    ((a & 0x0000ff00) - (b & 0x0000ff00) & 0x0000ff00) |
    ((a & 0x000000ff) - (b & 0x000000ff) & 0x000000ff)
  )
def average_bytes(a, b):
  return (
    ((((a & 0xff000000) + (b & 0xff000000)) >> 1) & 0xff000000) |
    ((((a & 0x00ff0000) + (b & 0x00ff0000)) >> 1) & 0x00ff0000) |
    ((((a & 0x0000ff00) + (b & 0x0000ff00)) >> 1) & 0x0000ff00) |
    ((((a & 0x000000ff) + (b & 0x000000ff)) >> 1) & 0x000000ff)
  )
def alpha_blend(foreground, background):
  back_a = background & 0xff
  if back_a == 0:
    return foreground
  fore_a = foreground & 0xff
  out_a = fore_a + back_a * (0xff - fore_a) // 0xff
  fore_r = ((foreground & 0xff000000) >> 24) & 0xff
  fore_g = ((foreground & 0x00ff0000) >> 16) & 0xff
  fore_b = ((foreground & 0x0000ff00) >>  8) & 0xff
  back_r = ((background & 0xff000000) >> 24) & 0xff
  back_g = ((background & 0x00ff0000) >> 16) & 0xff
  back_b = ((background & 0x0000ff00) >>  8) & 0xff
  out_r = (fore_r * fore_a // 0xff + back_r * back_a * (0xff - fore_a) // 0xff // 0xff) * 0xff // out_a
  out_b = (fore_b * fore_a // 0xff + back_b * back_a * (0xff - fore_a) // 0xff // 0xff) * 0xff // out_a
  out_g = (fore_g * fore_a // 0xff + back_g * back_a * (0xff - fore_a) // 0xff // 0xff) * 0xff // out_a
  return (
    (out_r << 24) |
    (out_g << 16) |
    (out_b <<  8) |
    (out_a <<  0)
  )

class SimplePngError(Exception):
  pass

def read_png(f, verbose=False):
  try:
    first_bytes = f.read(len(magic_number))
  except UnicodeDecodeError:
    # trigger the non-bytes error below
    first_bytes = ""
  if type(first_bytes) != bytes:
    raise SimplePngError("file must be open in binary mode")
  if first_bytes != magic_number:
    raise SimplePngError("not a png image")

  IHDR = read_chunk(f)
  if IHDR.type_code != b"IHDR":
    raise SimplePngError("expected IHDR")
  width, height, bit_depth, color_type, compression, filter_method, interlaced = struct.unpack(IHDR_fmt, IHDR.body)
  if verbose: print("metadata: {}x{}, {}-bit, color_type: {}".format(width, height, bit_depth, color_type))
  if width * height == 0:
    raise SimplePngError("image must have > 0 pixels")
  if bit_depth == 16:
    raise SimplePngError("16-bit color depth is not supported")
  if color_type not in (0, 2, 3, 4, 6):
    raise SimplePngError("unsupported color type: {}".format(color_type))
  if compression != 0:
    raise SimplePngError("unsupported compression method: {}".format(compression))
  if filter_method != 0:
    raise SimplePngError("unsupported filter method: {}".format(filter_method))
  if interlaced not in (0, 1):
    raise SimplePngError("unsupported interlace method: {}".format(interlaced))

  idat_data = []
  decompressor = zlib.decompressobj()
  palette = None
  while True:
    chunk = read_chunk(f)
    if chunk.type_code == b"IEND":
      break
    elif chunk.type_code == b"PLTE":
      if not(color_type & color_type_mask_INDEXED):
        if verbose: print("WARNING: ignoring chunk: {}: color_type does not require a palette".format(repr(chunk.type_code)))
      else:
        palette = [struct.unpack("!I", bytes(rgb + (0xff,)))[0] for rgb in zip(*[iter(chunk.body)]*3)]
    elif chunk.type_code == b"IDAT":
      idat_data.append(decompressor.decompress(chunk.body))
    else:
      if verbose: print("WARNING: ignoring chunk: " + repr(chunk.type_code))
  idat_data.append(decompressor.flush())
  idat_data = b"".join(idat_data)

  if bit_depth == 8:
    def bits_at(index):
      return idat_data[index]
  elif bit_depth == 4:
    def bits_at(index):
      tmp = idat_data[index >> 1]
      shift = (1 - (index & 1)) << 2
      return (tmp >> shift) & 0xf
  elif bit_depth == 2:
    def bits_at(index):
      tmp = idat_data[index >> 2]
      shift = (3 - (index & 3)) << 1
      return (tmp >> shift) & 0x3
  elif bit_depth == 1:
    def bits_at(index):
      tmp = idat_data[index >> 3]
      shift = 7 - (index & 7)
      return (tmp >> shift) & 0x1
  else:
    raise SimplePngError("unsupported bit depth: {}".format(bit_depth))

  image = ImageBuffer(width, height)
  data = image.data

  def reconstruct_none(v, coord_transform, x, y):
    return v
  def reconstruct_sub(v, coord_transform, x, y):
    if x == 0: return v
    else:      return add_bytes(data[coord_transform(x - 1, y)], v)
  def reconstruct_up(v, coord_transform, x, y):
    if y == 0: return v
    else:      return add_bytes(data[coord_transform(x, y - 1)], v)
  def reconstruct_average(v, coord_transform, x, y):
    if x == 0: sub = 0
    else:      sub = data[coord_transform(x - 1, y)]
    if y == 0: up = 0
    else:      up = data[coord_transform(x, y - 1)]
    return add_bytes(average_bytes(sub, up), v)
  def reconstruct_paeth(v, coord_transform, x, y):
    def get_predictor(a, b, c):
      p = a + b - c
      pa = abs(p - a)
      pb = abs(p - b)
      pc = abs(p - c)
      if pa <= pb and pa <= pc: return a
      if pb <= pc: return b
      return c
    def paeth_byte(v, shift):
      v = (v >> shift) & 0xff
      if x == 0: sub = 0
      else:      sub = (data[coord_transform(x - 1, y)] >> shift) & 0xff
      if y == 0: up = 0
      else:      up = (data[coord_transform(x, y - 1)] >> shift) & 0xff
      if x*y==0: diag = 0
      else:      diag = (data[coord_transform(x - 1, y - 1)] >> shift) & 0xff
      return ((get_predictor(sub, up, diag) + v) & 0xff) << shift
    return (
      paeth_byte(v, 24) |
      paeth_byte(v, 16) |
      paeth_byte(v,  8) |
      paeth_byte(v,  0)
    )
  reconstruct_funcs = [
    reconstruct_none,    # 0
    reconstruct_sub,     # 1
    reconstruct_up,      # 2
    reconstruct_average, # 3
    reconstruct_paeth,   # 4
  ]

  if interlaced == 0:
    # no interlacing
    passes = [(width, height, lambda x, y: y * width + x)]
  else:
    # Adam7 interlacing
    passes = [
      ((width + 7) // 8, (height + 7) // 8, lambda x, y: (y * 8    ) * width + x * 8    ),
      ((width + 3) // 8, (height + 7) // 8, lambda x, y: (y * 8    ) * width + x * 8 + 4),
      ((width + 3) // 4, (height + 3) // 8, lambda x, y: (y * 8 + 4) * width + x * 4    ),
      ((width + 1) // 4, (height + 3) // 4, lambda x, y: (y * 4    ) * width + x * 4 + 2),
      ((width + 1) // 2, (height + 1) // 4, lambda x, y: (y * 4 + 2) * width + x * 2    ),
      ( width      // 2, (height + 1) // 2, lambda x, y: (y * 2    ) * width + x * 2 + 1),
      ( width          ,  height      // 2, lambda x, y: (y * 2 + 1) * width + x        ),
    ]
    assert width * height == sum(w * h for w, h, _ in passes)

  if verbose: filter_type_histogram = collections.Counter()
  in_cursor = 0
  for pass_width, pass_height, coord_transform in passes:
    if pass_width == 0: continue
    for y in range(pass_height):
      try: filter_type = idat_data[in_cursor]
      except IndexError: raise SimplePngError("expected more IDAT data")
      in_cursor += 1
      try: reconstruct = reconstruct_funcs[filter_type]
      except IndexError: raise SimplePngError("unrecognized filter type: {}".format(filter_type))
      if verbose: filter_type_histogram.update([filter_type])
      for x in range(pass_width):
        # get rbga from bits
        if color_type & color_type_mask_INDEXED:
          # palette
          value = palette[bits_at(in_cursor)]
          in_cursor += 1
          if filter_type != 0:
            if verbose: print("WARNING: ignoring filter_type: {}: color_type is indexed".format(filter_type))
        else:
          if color_type & color_type_mask_COLOR:
            # rgb
            r = bits_at(in_cursor+0)
            g = bits_at(in_cursor+1)
            b = bits_at(in_cursor+2)
            in_cursor += 3
          else:
            # grayscale
            r = g = b = bits_at(in_cursor)
            in_cursor += 1
          if color_type & color_type_mask_ALPHA:
            a = bits_at(in_cursor)
            in_cursor += 1
          else:
            # opaque
            a = 255
          value = (
            (r << 24) |
            (g << 16) |
            (b << 8) |
            (a << 0)
          )
          value = reconstruct(value, coord_transform, x, y)
        if not (color_type & color_type_mask_ALPHA):
          # alpha is always opaque
          value |= 0xff
        data[coord_transform(x, y)] = value
  if verbose: print("filter types used: " + "   ".join("{}:{}".format(*x) for x in sorted(filter_type_histogram.items())))

  return image

if __name__ == "__main__":
  print("reading 1...")
  with open(sys.argv[1], "rb") as f:
    image1 = read_png(f, verbose=True)
  print("reading 2...")
  with open(sys.argv[2], "rb") as f:
    image2 = read_png(f, verbose=True)
  print("compositing...")
  image1.paste(image2, 0, 0, 0, 0)
  print("writing...")
  with open(sys.argv[3], "wb") as f:
    write_png(f, image1)
  print("done")

