this program takes the inputfile.txt as an argument. the inputfile.txt has a list of ‘trains’. The requirements for this program is that every train must be dispatched at the same time, and only one train may be on the train tracks at a time, therefor every train must wait until the train infant of it is off the main track.
The first letter in the input file represents the direction the train is going, weather it is going east or west, and if it is capital, then it is a high priority train and must go above low priority trains.
The second letter in the input file represents the loading time of a train on the train track. Those who finish loading first will get to go on the tracks first, or go into the ready queue to wait for it’s turn.
The third letter in the input file represents the time a train spends on the train tracks. There can only be one train on the train tracks at a time, so the trains behind it have to wait that amount of time.

function, then make threads once more
to send them out to the main track. In my implementation now, I have combined this into my thread function. My thread function
has each thread wait, and then loads them, but also has them wait and cross the main track in the same function. 

To implement this program, we have to use pthreads, mutex’s for protection, and linked lists 
