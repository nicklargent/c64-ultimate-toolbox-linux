#pragma once

#include <QString>
#include <QList>

struct BasicSample {
    QString name;
    QString code;
};

inline QList<BasicSample> basicSamples() {
    return {
        {"Hello World",
R"BAS(10 print "{clr}"
20 print "{down}{down}{down}"
30 print "  {rvs on} hello from c64 ultimate toolbox! {rvs off}"
40 print "{down}"
50 for i=1 to 16
60 print "{rvs on}                                        {rvs off}"
70 next i
80 goto 80)BAS"},

        {"Color Bars",
R"BAS(10 print "{clr}"
20 c$="{blk}{wht}{red}{cyn}{pur}{grn}{blu}{yel}"
30 c$=c$+"{org}{brn}{lred}{dgry}{mgry}{lgrn}{lblu}{lgry}"
40 for y=0 to 24
50 for x=0 to 15
60 poke 1024+y*40+x*2,160
70 poke 1024+y*40+x*2+1,160
80 poke 55296+y*40+x*2,x
90 poke 55296+y*40+x*2+1,x
100 next x
110 next y
120 goto 120)BAS"},

        {"Bouncing Ball",
R"BAS(10 print "{clr}"
20 x=1:y=1:dx=1:dy=1
30 poke 1024+y*40+x,81
40 poke 55296+y*40+x,1
50 for t=1 to 20:next t
60 poke 1024+y*40+x,32
70 x=x+dx:y=y+dy
80 if x<1 or x>38 then dx=-dx
90 if y<1 or y>23 then dy=-dy
100 goto 30)BAS"},

        {"Number Guessing Game",
R"BAS(10 print "{clr}"
20 print "*** number guessing game ***"
30 print
40 n=int(rnd(1)*100)+1
50 g=0
60 print "guess a number (1-100):"
70 input a
80 g=g+1
90 if a<n then print "too low!":goto 60
100 if a>n then print "too high!":goto 60
110 print "correct! you got it in";g;"guesses!"
120 print "play again? (y=1/n=0)"
130 input p
140 if p=1 then goto 10)BAS"},

        {"Maze Generator",
R"BAS(10 print "{clr}"
20 for i=1 to 1000
30 r=int(rnd(1)*2)
40 if r=0 then print "{pur}/";
50 if r=1 then print "{grn}\";
60 next i
70 goto 70)BAS"},

        {"Fibonacci Sequence",
R"BAS(10 print "{clr}"
20 print "fibonacci sequence"
30 print "===================="
40 print
50 a=0:b=1
60 for i=1 to 20
70 print a
80 c=a+b
90 a=b:b=c
100 next i
110 print
120 print "done! first 20 numbers.")BAS"},
    };
}
