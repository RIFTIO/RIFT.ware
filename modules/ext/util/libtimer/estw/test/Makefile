#
#
#
INCLUDE = -I. \
           -I../../safe_base/include \
           -I../include

LIB =   ../lib/stw_timer.a 

SRC =   demo_stw.c

OBJ = $(SRC:.c=.o)

IMAGE =	d_stw  

CCC = gcc  
DEBUG = -g
CFLAGS =  -Wall -c $(DEBUG)
LFLAGS = -Wall $(DEBUG)

$(IMAGE): 
	$(CCC) $(INCLUDE) $(LFLAGS) $(SRC) $(LIB) -o $(IMAGE)

clean:
	rm -f $(OBJ) $(IMAGE)  

# DO NOT DELETE 

