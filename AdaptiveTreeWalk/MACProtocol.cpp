#include <stdio.h>
#include <stdlib.h>
#include <time.h>       /* time */
#include <string>
#include <map>
#include <queue>
#include <utility> 		/* pair */
#include <math.h>       /* pow */
#include <set>
#include <fstream>


#define NUM_STATIONS 1024
#define NUM_TESTS 100

using namespace std;

/*
	Represents the information of success, collision and idle probe rates.
*/
struct ProbeRate {
	float success;
	float collision;
	float idle;
	float successRate;
	float collisionRate;
	float idleRate;
};

/*
	Initiate map of level to ProbeRate, with zeros in all fields.
*/
void initMapWithI(int* is, int isLen, map<int,ProbeRate>* iCount) {
	for (int j = 0; j < isLen; j++) {
		int i = *(is+j);
		ProbeRate pr;
		pr.success = 0;
		pr.collision = 0;
		pr.idle = 0;
		pr.successRate = 0;
		pr.collisionRate = 0;
		pr.idleRate = 0;
		(*iCount)[i] = pr;
	}
}

/*
	Create a random set of exactly 'numActive' active nodes
*/
void generateRandCase(int* nodes, int numStations, int numActive){
	set<int> stationSet;
	// get random list of nodes
	for (int i = 0; i < numActive; i++) {
		int randInt = rand() % numStations;
		while (stationSet.find(randInt) != stationSet.end()) {
			randInt = rand() % numStations;
		}
		stationSet.insert(randInt);
		
		nodes[randInt] = 1;
	}

}

/*
	Recalculate number of sections at the new level, and the number of nodes per section.
*/
void getSectionInfo(int power, int* numSections, int* sizeSection, int sizeNodes) {
	(*numSections) = (int) pow(2,power);
	(*sizeSection) = sizeNodes/(*numSections);
}

/*
	Scan the section for active/ready nodes and return the number of active nodes (max 2).
*/
int scanSection(int* nodes, int sizeNodes, int start, int end) {
	int numActive = 0;
	if (start < 0 || end > sizeNodes) {
		printf("start %d end %d\n", start, end);
		perror("indices not within range");
		exit(0);
	}
	for (int i = start; i < end; i++) {
		if (*(nodes+i) == 1) {
			numActive++;
		}
		// stop scanning once you know that there's more than one active node
		if (numActive > 1) {
			i = end;
		}
	}
	return numActive;

}

/*
	Check if there's a collision using the number of active child nodes 
	from the left and right sides
*/
bool hasCollision(int numActiveLeft, int numActiveRight) {
	return (numActiveLeft > 0 && numActiveRight > 0)
			|| (numActiveLeft > 1 || numActiveRight > 1);
}

/*
	Check if there's a success using the number of active child nodes 
	from the left and right sides
*/
bool hasSuccess(int numActiveLeft, int numActiveRight) {
	return ((numActiveLeft == 1 && numActiveRight == 0)
			|| (numActiveLeft == 0 && numActiveRight == 1));
}

/*
	Initiate the queue of nodes to probe with all nodes defined by the number
	of sections and the size of each section
*/
void initQueue(queue< pair<int,int> >* q, int numSections, int sizeSection) {
	int start, end;
	for (int i = 0; i < numSections; i++) {
		start = i * sizeSection;
		end = (i+1) * sizeSection;
		pair<int,int> p (start,end);
		q->push(p);
	}	
}


/*
	Push a node onto the queue if there are multiple active child nodes on either side of
	the current node
*/
void pushIfMultActive(queue< pair<int,int> >* q, 
						pair<int,int>* leftSection, pair<int,int>* rightSection, 
						int numActiveLeft, int numActiveRight) {
	// >1 active nodes on both sides
	if (numActiveLeft > 0 && numActiveRight > 0) {
		q->push(*leftSection);
		q->push(*rightSection);
	// >1 active nodes on the left side
	} else if (numActiveLeft > 1) {
		q->push(*leftSection);
	// >1 active nodes on the right side
	} else if (numActiveRight > 1) {
		q->push(*rightSection);
	}

}

/*
	Probe nodes at level i if to determine how many probes needed to allow all
	nodes to send their information
*/
void probeAtI(int level, int* nodes, int sizeNodes, map<int,ProbeRate>* iCount) {
	// number of sections to check initially 2^level
	int power = level, numSections, sizeSection;
	getSectionInfo(power, &numSections, &sizeSection, sizeNodes);
	
	int start, end;
	// queue of whole section range covered by node	
	queue< pair<int,int> > q;
	initQueue(&q, numSections, sizeSection);

	ProbeRate* pr = &(iCount->at(level));
	// int i = 8;
	while (!q.empty()) {
		// get the node to look at
		pair<int,int> p = q.front();
		q.pop();
		start = p.first;
		end = p.second;
		if (end-start < sizeSection) {
			// moved to the next level
			power++;
			getSectionInfo(power, &numSections, &sizeSection, sizeNodes);
		}		
		

		int childSectionSize = sizeSection/2;
		int s1 = start;
		int e1 = start+childSectionSize;
		int s2 = start+childSectionSize;
		int e2 = end;

		int numActiveLeft = scanSection(nodes, sizeNodes, s1, e1);
		int numActiveRight = scanSection(nodes, sizeNodes, s2, e2);

		// if collision
		if (hasCollision(numActiveLeft, numActiveRight)) {
			pr->collision++;
			// get new start end indices and add to queue
			pair<int,int> pleft (s1,e1);
			pair<int,int> pright (s2,e2);
			pushIfMultActive(&q, &pleft, &pright, numActiveLeft, numActiveRight);
		} else if (hasSuccess(numActiveLeft,numActiveRight)) {
			pr->success++;
		} else {
			pr->idle++;
		}
	}
	

}

/*
	Calculate the success, collision and idle rates for all levels in the map
*/ 
void calcProbeRates(map<int,ProbeRate>* iCount) {
	for (auto it = iCount->begin(); it != iCount->end(); it++) {
		ProbeRate* pr = &(it->second);
		float totalProbes = pr->collision + pr->success + pr->idle;
		pr->successRate = pr->success/totalProbes;
		pr->collisionRate = pr->collision/totalProbes;
		pr->idleRate = pr->idle/totalProbes;
	}
}


/*
	Convert a float to a string, to two decimal places
*/
string floatToStr(float f) {
	string s(16, '\0');
	auto written = snprintf(&s[0], s.size(), "%.2f", f);
	s.resize(written);
	return s;
}

/*
	Append the output to an output file stream.
	The output consists of all the probe rates for each level, for a given
	k number of active nodes.
*/
void appendToOutput(ofstream* ofs, map<int,ProbeRate>* iCount, int k){ 
	string output;
	output.append("For " + to_string(k) + " random active stations:\n");
	output.append("Level\tSuccess\tCollision\tIdle\n");

	for (auto it = iCount->begin(); it != iCount->end(); it++) {
		int level = it->first;
		ProbeRate* pr = &(it->second);
		output += to_string(level) 
			+ "\t" + (floatToStr(100 * pr->successRate)) 
			+ "\t" + (floatToStr(100 * pr->collisionRate)) 
			+ "\t" + (floatToStr(100 * pr->idleRate)) 
			+ "\n"; 

	}
	output.append("\n");
	(*ofs) << output;

}

int main(int argc,char** argv) {
	// randomize the seed for RNG
	srand (time(NULL));
	ofstream ofs ("TreeWalk.txt", ofstream::out);

	// ks	number of stations ready to send
	// is	levels to begin probing
	int numK = 11;
	int numI = 6;
	int ks [11] = {1, 2, 4, 8, 16, 32, 64, 128, 256, 512, 1024};
	int is [6] = {0, 2, 4, 6, 8, 10};


	int nodes[NUM_STATIONS];
	string output;
	// simulate for every value of k
	for (int ki = 0; ki < numK; ki++) {
		int k = ks[ki];
		// key		i (level)
		// value 	number of probes
		map<int,ProbeRate> iCount;
		initMapWithI(is,numI,&iCount);
		// generate 100 test cases
		for (int j = 0; j < NUM_TESTS; j++) {
			memset(&nodes,0,sizeof(nodes));
			generateRandCase(nodes, NUM_STATIONS, k);

			// simulate for each level i
			for (int ii = 0; ii < numI; ii++) {
				int i = is[ii];
				probeAtI(i, nodes, NUM_STATIONS, &iCount);	
			}
			
		}
		
		calcProbeRates(&iCount);
		appendToOutput(&ofs, &iCount, k);
	}

	ofs.close();
	return 0;

}
