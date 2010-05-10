import Options
from os import unlink, symlink, popen
from os.path import exists 

srcdir = "."
blddir = "build"
VERSION = "0.0.3"

def set_options(opt):
  opt.tool_options("compiler_cxx")
  opt.tool_options("compiler_cc")

  opt.add_option('--with-gzip', dest='gzip', action='store_true', default=True)
  opt.add_option('--no-gzip', dest='gzip', action='store_false')
  opt.add_option('--with-bzip', dest='bzip', action='store_true', default=False)
  opt.add_option('--no-bzip', dest='bzip', action='store_false')

def configure(conf):
  conf.check_tool("compiler_cxx")
  conf.check_tool("compiler_cc")
  conf.check_tool("node_addon")

  conf.env.DEFINES = []
  conf.env.USELIB = []

  if Options.options.gzip:
    conf.check_cxx(lib='z',
                   uselib_store='ZLIB',
                   mandatory=True)
    conf.env.DEFINES += [ 'WITH_GZIP' ]
    conf.env.USELIB += [ 'ZLIB' ]

  if Options.options.bzip:
    conf.check_cxx(lib='bz2',
                   uselib_store='BZLIB',
                   mandatory=True)
    conf.env.DEFINES += [ 'WITH_BZIP' ]
    conf.env.USELIB += [ 'BZLIB' ]

def build(bld):
  obj = bld.new_task_gen("cxx", "shlib", "node_addon")
  obj.target = "compress-bindings"
  obj.source = "src/compress.cc"
  obj.defines = bld.env.DEFINES
  obj.uselib = bld.env.USELIB
  

def shutdown():
  # HACK to get compress.node out of build directory.
  # better way to do this?
  if Options.commands['clean']:
    if exists('compress.node'): unlink('compress.node')
  else:
    if exists('build/default/compress.node') and not exists('compress.node'):
      symlink('build/default/compress.node', 'compress.node')
