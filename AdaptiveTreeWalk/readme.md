# Adaptive Tree Walk MAC Protocol

This program simulates the average case performance of the Adaptive Tree Walk MAC Protocol on a set of 2^10 = 1024 stations. It generates 100 test cases in which there are k stations ready to send data frames, where k = 1, 2, 4, 8, 16, 32, 64, 128, 256, 512, and 1024. For each value of k, it probes starting at level i if there is a collision between nodes trying to send data simulataneously and moves downwards for every collision. These levels are i = 0, 2, 4, 6, 8, and 10.

 Stations generate frames randomly and independently from each other, and each station is equally likely to generate a frame in each round. 

## Compilation

To compile this program,

    g++ MACProtocol.cpp

To run this program,

    ./a.out

Running the program will produce a file called TreeWalk.txt containing the average success, collision and probe rates for each call of the simulation. These numbers will vary for each run of the program since the random number generator seed is based on the current calendar time.

## Testing

To test this program, I ran it a few times and checked the results of the
output. While the values were different each time, they were roughly within the
same range of 1-2% differences in value.

## Results and Observations

The successful probe rates are graphed and all probe rates are visualized in
a table in the 'TreeWalk Results' PDF file. Observations concerning the results are also noted in this document.
