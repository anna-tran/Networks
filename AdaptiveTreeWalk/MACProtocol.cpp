#include <stdio.h>
#include <stdlib.h>
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


struct ProbeRate {
	float success;
	float collision;
	float idle;
	float successRate;
};


void initMapWithI(int* is, int isLen, map<int,ProbeRate>* iCount) {
	for (int j = 0; j < isLen; j++) {
		int i = *(is+j);
		ProbeRate pr;
		pr.success = 0;
		pr.collision = 0;
		pr.idle = 0;
		pr.successRate = 0;
		(*iCount)[i] = pr;
	}
}

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

	// for (int i = 0; i < NUM_STATIONS; i++) {
	// 	printf("%d ", *(nodes+i));
	// }
	// printf("\n");
}

void getSectionInfo(int power, int* numSections, int* sizeSection, int sizeNodes) {
	(*numSections) = (int) pow(2,power);
	// printf("power of %d\n",power);
	(*sizeSection) = sizeNodes/(*numSections);
}

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
	}
	return numActive;

}

bool hasCollision(int numActiveLeft, int numActiveRight) {
	return (numActiveLeft > 0 && numActiveRight > 0)
			|| (numActiveLeft > 1 || numActiveRight > 1);
}

bool hasSuccess(int numActiveLeft, int numActiveRight) {
	return ((numActiveLeft == 1 && numActiveRight == 0)
			|| (numActiveLeft == 0 && numActiveRight == 1));
}

void initQueue(queue< pair<int,int> >* q, int numSections, int sizeSection) {
	int start, end;
	for (int i = 0; i < numSections; i++) {
		start = i * sizeSection;
		end = (i+1) * sizeSection;
		pair<int,int> p (start,end);
		q->push(p);
	}	
	// printf("init queue has %d sections\n",numSections);
}

void pushIfMultActive(queue< pair<int,int> >* q, 
						pair<int,int>* leftSection, pair<int,int>* rightSection, 
						int numActiveLeft, int numActiveRight) {
	// active on both sides
	if (numActiveLeft > 0 && numActiveRight > 0) {
		q->push(*leftSection);
		q->push(*rightSection);
	} else if (numActiveLeft > 1) {
		q->push(*leftSection);
	} else if (numActiveRight > 1) {
		q->push(*rightSection);
	}

}

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
		// printf("size of queue %lu\n",q.size());
		// TODO
		// check all left and check all right
		pair<int,int> p = q.front();
		q.pop();
		start = p.first;
		end = p.second;
		if (end-start < sizeSection) {
			// moved to the next level
			power++;
			getSectionInfo(power, &numSections, &sizeSection, sizeNodes);
		}		
		// printf("looking at start: %d end: %d\n", start,end);
		// printf("size section = %d\n", sizeSection);
		// printf("num sections = %d\n", numSections);
		

		int childSectionSize = sizeSection/2;
		int s1 = start;
		int e1 = start+childSectionSize;
		int s2 = start+childSectionSize;
		int e2 = end;
		// printf("s1 %d e1 %d s2 %d e2 %d\n",s1,e1,s2,e2);

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

// 
void calcSuccessRate(map<int,ProbeRate>* iCount) {
	for (auto it = iCount->begin(); it != iCount->end(); it++) {
		ProbeRate* pr = &(it->second);
		// printf("\nbefore dividing\ni: %d\tcol: %.0f suc: %.0f idle: %.0f\n",it->first,pr->collision,pr->success,pr->idle);
		float totalProbes = pr->collision + pr->success + pr->idle;
		pr->successRate = pr->success/totalProbes;
	}
}

string floatToStr(float f) {
	string s(16, '\0');
	auto written = snprintf(&s[0], s.size(), "%.5f", f);
	s.resize(written);
	return s;
}

void appendToOutput(ofstream* ofs, map<int,ProbeRate>* iCount, int k){ 
	string output;
	output.append("For " + to_string(k) + " random active stations:\n");
	output.append("Level\tSuccess Rate\n");

	for (auto it = iCount->begin(); it != iCount->end(); it++) {
		int level = it->first;
		ProbeRate* pr = &(it->second);
		output += to_string(level) 
			+ "\t" + (to_string(pr->successRate)) 
			+ "\n"; 

	}
	output.append("\n");
	(*ofs) << output;

}

int main(int argc,char** argv) {
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
		// printf("finished init\n");
		// generate 100 test cases
		for (int j = 0; j < NUM_TESTS; j++) {
			memset(&nodes,0,sizeof(nodes));
			generateRandCase(nodes, NUM_STATIONS, k);

			// simulate for each level i
			for (size_t ii = 0; ii < numI; ii++) {

				int i = is[ii];
				probeAtI(i, nodes, NUM_STATIONS, &iCount);	
			}
			
		}
		
		calcSuccessRate(&iCount);
		appendToOutput(&ofs, &iCount, k);
	}

	// write this to file later
	// printf("%s\n", output.c_str());
	ofs.close();
	return 0;

}



















