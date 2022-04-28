# Project Name
TARGET = alphabit

# Sources
CPP_SOURCES = alphabit.cpp

# Library Locations
LIBDAISY_DIR = /Users/stephenjohnson/Documents/DaisyExamples/libDaisy/
DAISYSP_DIR = /Users/stephenjohnson/Documents/DaisyExamples/DaisySP/

# Core location, and generic Makefile.
SYSTEM_FILES_DIR = $(LIBDAISY_DIR)/core
include $(SYSTEM_FILES_DIR)/Makefile
