# Hobbit Reunion Path Finder

This program finds the path that a hobbit should travel to the reunion, which is always held at Bilbo's house.
The four implemented routing algorithms are:
* Shortest Hop Path (SHP): This algorithm finds the shortest path from source to destination, where the length of a path refers to the number of hops (i.e., links) traversed. Note that this algorithm ignores the physical distance, travel time, gold coins, and trolls for each link.
* Shortest Distance Path (SDP): This algorithm finds the shortest path from source to destination, where the length of a path refers to the cumulative total distance traveled. Note that this algorithm ignores the number of hops, travel time, as well as gold and trolls.
* Shortest Time Path (STP): This algorithm finds the shortest path from source to destination, where the length of a path refers to the cumulative travel time for traversing the chosen links in the path. Note that this algorithm ignores the number of hops, as well as the distance (although distance and time are often correlated). Gold and trolls are also irrelevant.
* Fewest Trolls Path (FTP): This algorithm finds the path from source to destination that minimizes the number of trolls encountered. Note that this algorithm ignores the number of hops, as well as time, distance, and gold (although gold and trolls are often correlated).

For all algorithms, the ties are broken chronologically (meaning that a tie will not result in a change in the most optimal route thus far).

## Routing performance

There were a few observations made about the relative performance of each of the routing algorithms on the full hobbit map.

* As implied by the algorithm name, of the four algorithms
** SHP generates the fewest number of hops needed by each dwarf to reach their destination
** SDP generates the shortest total distance travelled by each dwarf to their destination
** STP generates the shortest total time of each dwarf to travel to their destination
** FTP generates the fewest number of trolls encountered by each dwarf on the way to their destination

* For each of the four algorithm, the average number of hops was always about the same. Thus, it may be more important to use the other algorithms, as they reduce some additional parameter.

* Using the STP generated the least amount of gold among the four algorithms.

* Avoiding the trolls using FTP increases the average total distance to travel. However, the average amount of gold collected is higher than the other routing algorithms.

* There is a time/distance tradeoff between the SDP and STP outcomes.


## Compilation

To compile and run this program:

1. Run

	g++ PathFinder.cpp

2. Run 

	./a.out <hobbit home file> <map file>

3. When prompted, choose the algorithm (acronym) to use for finding the path from each dwarf's home to Bilbo's home.

