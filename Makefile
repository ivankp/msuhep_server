.PHONY: all clean

ifeq (0, $(words $(findstring $(MAKECMDGOALS), clean))) #############

CXX = g++
CC = gcc
AS = gcc

CFLAGS := -Wall -O3 -flto -fmax-errors=3 -Iinclude
# CFLAGS := -Wall -O0 -g -fmax-errors=3 -Iinclude
CXXFLAGS := -std=c++20 $(CFLAGS)

# generate .d files during compilation
DEPFLAGS = -MT $@ -MMD -MP -MF .build/$*.d

#####################################################################

all: $(patsubst %, bin/%, \
  test \
)
# msuhepserver
# sqlite3

#####################################################################
LIB = -Llib -Wl,-rpath=$(PWD)/lib

bin/test: .build/server.o .build/socket.o
LF_test := -pthread

bin/msuhepserver: $(patsubst %, .build/%.o, \
  server socket whole_file file_cache http zlib \
)
# lib/libsqlite3.so
LF_msuhepserver := -pthread $(LIB)
# L_msuhepserver := -lsqlite3 -lz
L_msuhepserver := -lz

C__sqlite3 := -Iinclude/sqlite3
C_sqlite3/sqlite3 := $(C__sqlite3) -fPIC
lib/libsqlite3.so: $(patsubst %, .build/sqlite3/%.o, \
  sqlite3 )

C_sqlite3/shell := $(C__sqlite3) -DHAVE_READLINE -DSQLITE_HAVE_ZLIB=1
LF_sqlite3 := -pthread $(LIB)
L_sqlite3 := -ldl -lsqlite3 -lreadline -lz

#####################################################################

.PRECIOUS: .build/%.o lib/lib%.so

bin/%: .build/%.o
	@mkdir -pv $(dir $@)
	$(CXX) $(LDFLAGS) $(LF_$*) $(filter %.o,$^) -o $@ $(LDLIBS) $(L_$*)

lib/lib%.so:
	@mkdir -pv $(dir $@)
	$(CC) $(LDFLAGS) $(LF_$*) -shared $(filter %.o,$^) -o $@ $(LDLIBS) $(L_lib$*)

.build/%.o: src/%.cc
	@mkdir -pv $(dir $@)
	$(CXX) $(CXXFLAGS) $(DEPFLAGS) $(C_$*) -c $(filter %.cc,$^) -o $@

.build/%.o: src/%.c
	@mkdir -pv $(dir $@)
	$(CC) $(CFLAGS) $(DEPFLAGS) $(C_$*) -c $(filter %.c,$^) -o $@

.build/%.o: src/%.S
	@mkdir -pv $(dir $@)
	$(AS) $(C_$*) -c $(filter %.S,$^) -o $@

bin/sqlite3: bin/%: .build/sqlite3/shell.o lib/libsqlite3.so
	@mkdir -pv $(dir $@)
	$(CC) $(LDFLAGS) $(LF_$*) $(filter %.o,$^) -o $@ $(L_$*)

-include $(shell [ -d '.build' ] && find .build -type f -name '*.d')

endif ###############################################################

clean:
	@rm -rfv bin lib .build

