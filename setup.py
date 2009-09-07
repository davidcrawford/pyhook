from distutils.core import setup, Extension

module1 = Extension('pyhook',
                     sources = ['src/pyhookmodule.c'],
                     include_dirs = ['./include'],
                     libraries = ['wpsapi'])

setup(name = 'pyhook',
      version = '1.0',
      description = 'SkyHook wireless python wrapper package',
      ext_modules = [module1])
