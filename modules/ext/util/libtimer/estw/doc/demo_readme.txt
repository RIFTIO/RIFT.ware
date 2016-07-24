

/* Build the demo program */ 

$ make -f make_demo_stw
gcc   -I. -I../../safe_base/include -I../include -Wall -g demo_stw.c ../lib/stw_timer.a  -o d_stw  


/* Run the demo program */ 

$ ./d_stw

a_repeating_timer timer a count=1 
a_repeating_timer timer a count=2 
a_repeating_timer timer a count=3 
a_repeating_timer timer a count=4 
b_timer timer b count=1 
a_repeating_timer timer a count=5 
a_repeating_timer timer a count=6 
a_repeating_timer timer a count=7 
a_repeating_timer timer a count=8 
a_repeating_timer timer a count=9 
b_timer timer b count=2 
a_repeating_timer timer a count=10 
a_repeating_timer timer a count=11 
a_repeating_timer timer a count=12 
a_repeating_timer timer a count=13 
a_repeating_timer timer a count=14 
b_timer timer b count=3 
a_repeating_timer timer a count=15 
a_repeating_timer timer a count=16 
a_repeating_timer timer a count=17 
a_repeating_timer timer a count=18 
a_repeating_timer timer a count=19 
b_timer timer b count=4 
a_repeating_timer timer a count=20 

Demo Timer Wheel 
       Granularity=100
        Wheel Size=512
        Tick Count=20
       Spoke Index=20
     Active timers=2
    Expired timers=24
      Hiwater mark=2
    Started timers=2
  Cancelled timers=0

a_repeating_timer timer a count=1 
a_repeating_timer timer a count=2 
a_repeating_timer timer a count=3 
a_repeating_timer timer a count=4 
b_timer timer b count=5 
a_repeating_timer timer a count=5 
a_repeating_timer timer a count=6 
a_repeating_timer timer a count=7 
a_repeating_timer timer a count=8 
a_repeating_timer timer a count=9 
b_timer timer b count=6 
a_repeating_timer timer a count=10 
a_repeating_timer timer a count=11 




