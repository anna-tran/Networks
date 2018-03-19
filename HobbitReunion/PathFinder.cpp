#include <iostream>
#include <istream>
#include <fstream>
#include <sstream>

#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <string>
#include <string.h>

#include <map>
#include <set>
#include <queue>          // std::priority_queue
#include <vector>         // std::vector
#include <functional>     // std::greater

using std::cout;
using namespace std;

// choose from
// SHP	Shortest Hop Path
// SDP	Shortest Distance Path
// STP	Shortest Time Path
// FTP	Fewest Trolls Path
// #define ROUTING_ALGORITHM "FTP"
// #define DESTINATION 'B'
char ROUTING_ALGORITHM[10];
char DESTINATION;
/*
	Route from source to destination, with information on the distance and time to get to the destination
	including the number of coins and trolls on the way.
*/
struct Route {
	char source;
	char destination;
	int distance;
	int time;
	int coins;
	int trolls;

	Route (char src, char dest, int d, int t, int c, int tr) : source(src), destination(dest), distance(d), time(t), coins(c), trolls(tr) {}

	void printRoute() {
		printf("%c %d %d %d %d\n", destination, distance, time, coins, trolls);
	}
};

/*
	Path for a dwarf to reach Bilbo's home at DESTINATION and a sum of all the hops, distances, times,
	coins and trolls along the way.
*/
struct DwarfPath {
	int id;				// id of start node to use in Dijkstra's shortest path algorithm
	char dwarf[100];
	char home;

	int hops;
	int distance;
	int time;
	int coins;
	int trolls;

	// routes taken to reach the destination
	vector<Route> routes;
};

/*
	Node struct representing a home on the map
*/
struct Node {
	int id;				
	int travel;			// how far/how long it takes/how many trolls from the source node to this node
	Route* prev;		// closest node that can access this

};

/*
	Comparer class for priority queue. Returns whether Node a is farther from the source node than Node b.
*/
class NodeComparer
{
public:
    bool operator() (Node* a, Node* b)
    {
    	if (a-> travel == -1){
    		return true;
    	} else if ((b->travel == -1) || (a->travel < b->travel)) {
    		return false;
    	} else {
    		return true;
    	}
    }
};

/*
	Append an integer value to the end of the src string
*/
void appendInt(string* src, int i) {
	char buffer [33];
	memset(&buffer,sizeof(buffer),0);
	sprintf(buffer, "%d", i);
	src->append(buffer);
}

/*
	Append a double value to the end of the src string
*/
void appendDouble(string* src, double i) {
	char buffer [33];
	memset(&buffer,sizeof(buffer),0);
	sprintf(buffer, "%.2lf", i);
	src->append(buffer);
}

/*
	Get the appropriate hop/distance/time/troll value for a Route r, 
	given they type of routing algorithm.
*/
int getDeterminantValue(struct Route* r) {
	if (strcmp(ROUTING_ALGORITHM,"SHP") == 0)  		// hop
		return 1;
	else if (strcmp(ROUTING_ALGORITHM,"SDP") == 0)	// distance
		return r->distance;
	else if (strcmp(ROUTING_ALGORITHM,"STP") == 0)	// time
		return r->time;
	else if (strcmp(ROUTING_ALGORITHM,"FTP") == 0)	// trolls
		return r->trolls;
	return 0;
}

/*
	Initialize all distances away from the source node to inifinity (represented by -1),
	unless it's the source node, in which case it's set to 0.
*/
void initDistArr(vector<Node>* dist, int sourceId, int distLen) {
	char home;
	for (int i = 0; i < distLen; i++) {
		int distance;
		if (i != sourceId) {
			distance = -1;
		} else {
			distance = 0;
		}
		struct Node n;
		n.id = i;
		n.travel = distance;

		(*dist)[i] = n;
		
	}
}

/*
	Implementation of Dijkstra's shortest path algorithm
*/
void Dijkstra(vector<Node>* dist, int sourceId, int destId, 
						vector<vector<Route> >* hobbitMap, map<char, DwarfPath>* dwarfPaths) {
	// printf("\nLooking for path of %d to %d\n", sourceId, destId);
	// set of visited nodes
	set<int> visited;	
	// queue of nodes to visit
	priority_queue< struct Node*, vector<struct Node*>, NodeComparer > queue;	
	initDistArr(dist, sourceId, hobbitMap->size());

	// initially push source node into queue
	queue.push(&(*dist)[sourceId]);
	
	const struct Node* v;
	while (!queue.empty()) {
		// looking at node v
		v = queue.top();
		queue.pop();

		// evaluate distances for node v only if it has not been visited
		if (visited.find(v->id) != visited.end()) {
			continue;
		}

		visited.insert(v->id);

		// for each neighbor u of v
		vector<Route>* routes = &((*hobbitMap)[v->id]);
		for (int j = 0; j < routes->size(); j++) {
			// route r connecting v to u
			Route* r = &(*routes)[j];
			// get id of node u
			int uId = (*dwarfPaths)[r->destination].id;
			Node* u = &(*dist)[uId];
			// if distance from the source node through v to u is smaller than what is
			// currently recorded, then replace it
			int altDist = (*dist)[v->id].travel + getDeterminantValue(r);
			if (altDist < u->travel || u->travel == -1) {
				// printf("lowered distance of %d from %d to %d\n", uId, u->travel, altDist);
				u->travel = altDist;
				u->prev = r;
				
			}
			// push only the nodes not yet visited
			if (visited.find(uId) == visited.end()) {
				queue.push(u);
			}			
			
		}

	}
}


/*
	Find the shortest path for a given dwarf, using the hobbit map and the mapping of
	dwarf homes to their node IDs
*/
void findShortestPath(struct DwarfPath* path, const char destination,
						vector<vector<Route> >* hobbitMap, map<char, DwarfPath>* dwarfPaths) {

	int sourceId = path->id;
	int destId = (*dwarfPaths)[destination].id;
	// printf("-- sourceId %d --\n", sourceId);
	vector<Route> newRoutes;
	path->routes = newRoutes;
	vector<Node> dist(hobbitMap->size());		// distance from source to node v

	char aHome = path->home;
	// skip finding the shortest path if we're looking at the destination point	
	if (aHome == destination) {					
		return;
	}
	Dijkstra(&dist, sourceId, destId, hobbitMap, dwarfPaths);

	struct Node* n;
	struct Route* r;
	n = &dist[destId];
	// trace backwards from the destination node to the start node and fill up the dwarf's path
	while (n->id != sourceId) {
		r = n->prev;
		path->routes.push_back(*r);
		int srcId = (*dwarfPaths)[r->source].id;
		n = &dist[srcId];
	}

}

/*
	Initialize a DwarfPath with its integer ID and home, and add it to the DwarfPath mapping.
	Set the dwarf's name to the empty string, indicating that there is no known dwarf living at
	that node as of now.
*/
void createDwarfPath( map<char, DwarfPath>* dwarfPaths, int id, char home) {
	struct DwarfPath dp;
	dp.id = id;
	dp.home = home;
	strcpy(dp.dwarf,"");
	(*dwarfPaths)[dp.home] = dp;
}

/*
	Create the hobbit map and initialize the DwarfPath mapping from the map file information.
*/
void createHobbitMap(ifstream* mapFile, vector<vector<Route> >* hobbitMap, map<char, DwarfPath>* dwarfPaths) {
	char line[256];
	int id = 0;
	while (mapFile->getline(line,sizeof(line))) {
		// get Route information
		char* source = strtok(line," ");
		char* dest = strtok(NULL, " ");
		char* distance = strtok(NULL, " ");
		char* time = strtok(NULL, " ");
		char* coins = strtok(NULL, " ");
		char* trolls = strtok(NULL, " ");

		Route aRoute (*source,*dest,atoi(distance),atoi(time),atoi(coins), atoi(trolls));
		Route oppRoute (*dest,*source,atoi(distance),atoi(time),atoi(coins), atoi(trolls));

		// if node does not exist in the DwarfPath mapping, insert it
		// otherwise get the appropriate node ID for the existing mapping
		int id1, id2;
		if (dwarfPaths->find(*source) == dwarfPaths->end()) {
			id1 = id;
			vector<Route> v;
			hobbitMap->push_back(v);
			createDwarfPath(dwarfPaths,id,(*source));
			id++;
		} else {
			id1 = (*dwarfPaths)[*source].id;
		}

		if (dwarfPaths->find(*dest) == dwarfPaths->end()) {
			id2 = id;
			vector<Route> v;
			hobbitMap->push_back(v);
			createDwarfPath(dwarfPaths,id,(*dest));
			id++;
		} else {
			id2 = (*dwarfPaths)[*dest].id;
		}

		// add the routes to the hobbit map
		(*hobbitMap)[id1].push_back(aRoute);
		(*hobbitMap)[id2].push_back(oppRoute);

	}
}

/*
	Initialize the homes from the given home file in the DwarfPath mapping
*/
void initDwarfPath(ifstream* homesFile, map<char, DwarfPath>* dwarfPaths) {
	char line[512];
	while (homesFile->getline(line,sizeof(line))) {
		// get dwarf name and home
		char* name = strtok(line," \n");
		char* home = strtok(NULL," \n");

		if (strcmp(name,"Bilbo") == 0) {
			DESTINATION = *home;
		}
		struct DwarfPath* dp = &((*dwarfPaths)[*home]);
		strcpy(dp->dwarf,name);
		dp->hops = 0;
		dp->distance = 0;
		dp->time = 0;
		dp->coins = 0;
		dp->trolls = 0;

	}
}


/*
	Get the full algorithm name from the acronym
*/
const char* getAlgorithmName() {
	if (strcmp(ROUTING_ALGORITHM, "SHP") == 0) {
		return "SHORTEST_HOP_PATH (SHP)";
	} else if (strcmp(ROUTING_ALGORITHM, "SDP") == 0) {
		return "SHORTEST_DISTANCE_PATH (SDP)";
	} else if (strcmp(ROUTING_ALGORITHM, "STP") == 0) {
		return "SHORTEST_TIME_PATH (STP)";
	} else if (strcmp(ROUTING_ALGORITHM, "FTP") == 0) {
		return "FEWEST_TROLLS_PATH (FTP)";
	}
	return "";
}


/*
	Print the header for the route summary table
*/
void printHeader() {
	string output;
	output.append(getAlgorithmName());
	output.push_back('\n');
	output.append("Destination is always Bilbo's home at node ");
	output.push_back(DESTINATION);
	output.append("\n\nDwarf\tHome\tHops\tDist\tTime\tGold\tTrolls\tPath\n");
	output.append("------------------------------------------------------------------------\n");
	printf("%s", output.c_str());
}

/*
	Boolean function for sorting strings alphabetically
*/
bool wayToSort(string& i, string& j) { 
	if (i.compare(j) < 0) {
		return true;
	}
	return false;
}

/*
	Print the average values for hops, distances, time, coins, and trolls
*/
void printTotals(int totalHops, int totalDistance, int totalTime, int totalCoins, int totalTrolls, double totalDwarfs) {
	string output;
	output.append("------------------------------------------------------------------------\nAVERAGE\t\t");
	appendDouble(&output, totalHops/totalDwarfs);
	output.push_back('\t');
	appendDouble(&output, totalDistance/totalDwarfs);
	output.push_back('\t');
	appendDouble(&output, totalTime/totalDwarfs);
	output.push_back('\t');
	appendDouble(&output, totalCoins/totalDwarfs);
	output.push_back('\t');
	appendDouble(&output, totalTrolls/totalDwarfs);

	printf("%s\n\n", output.c_str());
}

/*
	Print the hops, distances, time, coins, and trolls information for each dwarf for the path
	it takes to the destination.
*/
void printPaths(map<char, DwarfPath>* dwarfPaths) {
	printHeader();
	std::vector<string> v;
	string output;

	// total count for ALL dwarf paths
	int totalHops = 0;
	int totalDistance = 0;
	int totalTime = 0;
	int totalCoins = 0;
	int totalTrolls = 0;
	int numDwarfs = 0;
	// print all route information for each dwarf
	for (map<char,DwarfPath>::iterator it = dwarfPaths->begin(); it != dwarfPaths->end(); it++) {
		output.clear();
		char home = it->first;
		// only print information if there is a dwarf living at the current Node
		// and only if that node is not the destination
		if (home != DESTINATION && (strcmp((it->second).dwarf,"") != 0)) {
			output.append((it->second).dwarf);
			output.push_back('\t');
			output.push_back((it->second).home);
			output.push_back('\t');

			string destRoute;
			destRoute.push_back(home);
			// calculate the total hops, distances, time, coins, and trolls information given the dwarf's path
			for (auto route = (it->second).routes.rbegin(); route != (it->second).routes.rend(); route++) {
				destRoute.append(" ");
				(it->second).hops += 1;
				(it->second).distance += route->distance;
				(it->second).time += route->time;
				(it->second).coins += route->coins;
				(it->second).trolls += route->trolls;
				destRoute.push_back(route->destination);
			}

			// add values from dwarf's path to the total values
			totalHops += (it->second).hops;
			totalDistance += (it->second).distance;
			totalTime += (it->second).time;
			totalCoins += (it->second).coins;
			totalTrolls += (it->second).trolls;
			
			appendInt(&output, (it->second).hops);
			output.push_back('\t');
			appendInt(&output, (it->second).distance);
			output.push_back('\t');
			appendInt(&output, (it->second).time);
			output.push_back('\t');
			appendInt(&output, (it->second).coins);
			output.push_back('\t');
			appendInt(&output, (it->second).trolls);
			output.push_back('\t');
			output.append(destRoute);
			v.push_back(output);
			numDwarfs++;
		}
		
	}
	// sort the output alphabetically by dwarf name before printing
	sort(v.begin(), v.end(), wayToSort);
	for (auto it = v.begin(); it != v.end(); it++) {
		printf("%s\n", it->c_str());
	}

	printTotals(totalHops, totalDistance, totalTime, totalCoins, totalTrolls, (double) numDwarfs);
}

/*
    Closes the open sockets and file descriptors and exits the program
*/
void my_handler(int s){
    exit(0);
}


int main(int argc, char** argv) {

	if (argc < 3) {
		printf("Usage:\n\t ./pathfinder <list of homes> <map>\n");
		return 0;
	}


    // set up safe exit on CTRL-C
    struct sigaction sig_int_handler;

    sig_int_handler.sa_handler = my_handler;
    sigemptyset(&sig_int_handler.sa_mask);
    sig_int_handler.sa_flags = 0;

    sigaction(SIGINT, &sig_int_handler, NULL);

    printf("\nType in one of the following routing algorithms"
			"\n> SHP\n> SDP\n> STP\n> FTP\n"
			"Or \"QUIT\" to exit the program\n\n");
    // default is SHP
    strcpy(ROUTING_ALGORITHM, "SHP");
	while (true) {
		memset(ROUTING_ALGORITHM,sizeof(ROUTING_ALGORITHM),0);
		
		printf("> ");
		scanf("%s",ROUTING_ALGORITHM);
		if (strcmp(ROUTING_ALGORITHM,"QUIT") == 0) {
			break;
		}
		// get file names and open the respective files
		char* homesFileName = argv[1];
		char* mapFileName = argv[2];

		ifstream homesFile (homesFileName);
		ifstream mapFile (mapFileName);

		map<char, DwarfPath> dwarfPaths;	
		vector<vector<Route> > hobbitMap;

		// get initial map information from files
		createHobbitMap(&mapFile, &hobbitMap, &dwarfPaths);
		initDwarfPath(&homesFile, &dwarfPaths);

		homesFile.close();
		mapFile.close();
		for (auto it = dwarfPaths.begin(); it != dwarfPaths.end(); it++) {
			char home = it->first;
			if (home != DESTINATION && (strcmp((it->second).dwarf,"") != 0)) {
				// printf("finding path for %s\n", (it->second).dwarf);
				findShortestPath(&(it->second), DESTINATION, &hobbitMap, &dwarfPaths);
			}

		}

		printPaths(&dwarfPaths);
	}
	return 0;

}
