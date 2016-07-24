#
#
# 
SRC =	stw_timer.c stw_system_timer.c

OBJ = $(SRC:.c=.o)

OUT = ../lib/stw_timer.a

INCLUDE = -I. \
           -I../include/ \
           -I../../safe_base/include

CCFLAGS = -g
CCC = gcc
LDFLAGS = -g
.SUFFIXES: .c


.c.o:
	$(CCC) $(INCLUDE) $(CCFLAGS) -c $< -o $@

$(OUT): $(OBJ)
	ar rcs $(OUT) $(OBJ)

clean:
	rm -f $(OBJ)  

# DO NOT DELETE


