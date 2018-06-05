TEMPLATE = subdirs
CONFIG += ordered

CONFIG += c++11

DEFINES += _SILENCE_STDEXT_HASH_DEPRECATION_WARNINGS

# Libraries
SUBDIRS += NURBS
SUBDIRS += TopoBlenderLib

SUBDIRS += test   # Main plugin for topo-blending

test.depends = NURBS TopoBlenderLib
